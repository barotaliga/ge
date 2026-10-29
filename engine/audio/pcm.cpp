// The WAV reader: walk the RIFF chunks once, keep `fmt `, `data`, `smpl`, `cue ` and the
// labelled text in `LIST` `adtl`, then decode the data to mono floats and settle the loop.
//
// ge:summary The WAV reader: RIFF chunks, PCM 8/16 and float decoded to mono, the loop settled
// ge:keywords wav riff chunk fmt data smpl cue ltxt adtl decode downmix loop read_wav
// ge:skill audio
// ge:test tests/audio_pcm_test.cpp
#include "audio/pcm.hpp"

#include <cstring>

namespace ge::audio {

namespace {

uint16_t u16(const uint8_t* p) { return static_cast<uint16_t>(p[0] | (p[1] << 8)); }
uint32_t u32(const uint8_t* p) {
  return static_cast<uint32_t>(p[0]) | static_cast<uint32_t>(p[1]) << 8 |
         static_cast<uint32_t>(p[2]) << 16 | static_cast<uint32_t>(p[3]) << 24;
}
bool is(const uint8_t* p, const char* id) { return std::memcmp(p, id, 4) == 0; }

bool fail(std::string* error, std::string_view where, const std::string& what, size_t at,
          const char* next) {
  if (error) {
    *error =
        std::string(where) + ": " + what + " at byte " + std::to_string(at) + "; next: " + next;
  }
  return false;
}

constexpr uint16_t kFormatPcm = 1;
constexpr uint16_t kFormatFloat = 3;
constexpr uint16_t kFormatExtensible = 0xFFFE;

}  // namespace

bool read_wav(std::span<const uint8_t> b, std::string_view where, Pcm* out, std::string* error) {
  const char* kNotWav = "check the file is a RIFF WAVE file (not compressed audio renamed .wav)";
  if (b.size() < 12 || !is(b.data(), "RIFF") || !is(b.data() + 8, "WAVE")) {
    return fail(error, where, "not a RIFF WAVE header", 0, kNotWav);
  }
  // What the chunks say, filled as they are met.
  const uint8_t* fmt = nullptr;
  size_t fmt_at = 0, fmt_size = 0;
  const uint8_t* data = nullptr;
  size_t data_size = 0;
  bool smpl_loop = false;
  int64_t smpl_start = 0, smpl_end = 0;
  bool cue = false;
  uint32_t cue_id = 0;
  int64_t cue_at = 0;
  int64_t cue_length = -1;
  // `ltxt` entries may come before or after the cue they label: keep the first few.
  struct Label {
    uint32_t id;
    int64_t length;
  };
  Label labels[8];
  int label_count = 0;

  size_t at = 12;
  while (at + 8 <= b.size()) {
    const uint8_t* c = b.data() + at;
    const size_t size = u32(c + 4);
    const size_t body = at + 8;
    // A truncated last chunk is read as far as it goes (some writers get the data's size wrong).
    const size_t have = body + size <= b.size() ? size : b.size() - body;
    const uint8_t* p = b.data() + body;
    if (is(c, "fmt ")) {
      fmt = p;
      fmt_at = body;
      fmt_size = have;
    } else if (is(c, "data")) {
      data = p;
      data_size = have;
    } else if (is(c, "smpl") && have >= 36 + 24 && u32(p + 28) > 0) {
      smpl_loop = true;
      smpl_start = u32(p + 36 + 8);
      smpl_end = static_cast<int64_t>(u32(p + 36 + 12)) + 1;
    } else if (is(c, "cue ") && have >= 4 + 24 && u32(p) > 0) {
      cue = true;
      cue_id = u32(p + 4);
      cue_at = u32(p + 4 + 20);
    } else if (is(c, "LIST") && have >= 4 && is(p, "adtl")) {
      size_t q = 4;
      while (q + 8 <= have) {
        const uint8_t* s = p + q;
        const size_t n = u32(s + 4);
        if (is(s, "ltxt") && n >= 8 && q + 8 + n <= have && label_count < 8) {
          labels[label_count++] = {u32(s + 8), static_cast<int64_t>(u32(s + 12))};
        }
        q += 8 + n + (n & 1);
      }
    }
    at = body + size + (size & 1);
  }
  if (!fmt || fmt_size < 16) {
    return fail(error, where, "no fmt chunk", 12, "the file is damaged or not a WAV: re-export it");
  }
  if (!data) return fail(error, where, "no data chunk", 12, "re-export the file with its samples");
  uint16_t format = u16(fmt);
  const int channels = u16(fmt + 2);
  const int rate = static_cast<int>(u32(fmt + 4));
  const int bits = u16(fmt + 14);
  if (format == kFormatExtensible && fmt_size >= 26) format = u16(fmt + 24);
  const bool pcm8 = format == kFormatPcm && bits == 8;
  const bool pcm16 = format == kFormatPcm && bits == 16;
  const bool float32 = format == kFormatFloat && bits == 32;
  if (!pcm8 && !pcm16 && !float32) {
    return fail(error, where,
                "unsupported sample format " + std::to_string(format) + " at " +
                    std::to_string(bits) + " bits",
                fmt_at, "convert it to 16-bit PCM (8-bit PCM and 32-bit float also read)");
  }
  if (channels < 1 || rate < 1) {
    return fail(
        error, where,
        "fmt says " + std::to_string(channels) + " channels at " + std::to_string(rate) + " Hz",
        fmt_at, "the header is damaged: re-export the file");
  }
  const size_t width = static_cast<size_t>(bits / 8);
  const size_t frame_bytes = width * static_cast<size_t>(channels);
  const size_t frames = data_size / frame_bytes;
  Pcm pcm;
  pcm.rate = rate;
  pcm.samples.resize(frames);
  const float mix = 1.0f / static_cast<float>(channels);
  for (size_t f = 0; f < frames; f++) {
    const uint8_t* s = data + f * frame_bytes;
    float sum = 0;
    for (int ch = 0; ch < channels; ch++, s += width) {
      if (pcm8) {
        sum += static_cast<float>(static_cast<int>(s[0]) - 128) * (1.0f / 128);
      } else if (pcm16) {
        sum += static_cast<float>(static_cast<int16_t>(u16(s))) * (1.0f / 32768);
      } else {
        const uint32_t bitsof = u32(s);
        float v;
        std::memcpy(&v, &bitsof, 4);
        sum += v != v ? 0 : v < -1 ? -1 : v > 1 ? 1 : v;
      }
    }
    pcm.samples[f] = channels == 1 ? sum : sum * mix;
  }
  // The loop, by the order the header states.
  const int64_t n = static_cast<int64_t>(frames);
  int64_t start = -1, end = n;
  if (smpl_loop) {
    start = smpl_start;
    end = smpl_end;
  } else if (cue) {
    start = cue_at;
    for (int i = 0; i < label_count; i++) {
      if (labels[i].id == cue_id) cue_length = labels[i].length;
    }
    end = cue_length > 0 ? cue_at + cue_length : n;
  }
  if (start >= 0) {
    if (end > n) end = n;
    if (start < end) {
      pcm.loop_start = start;
      pcm.loop_end = end;
    }
  }
  *out = std::move(pcm);
  return true;
}

}  // namespace ge::audio
