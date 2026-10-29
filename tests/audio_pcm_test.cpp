// The WAV reader (engine/audio/pcm.hpp): 8- and 16-bit PCM, float and the extensible header
// decode to the samples they hold, more channels mix down to one, the loop comes from `smpl`
// before `cue ` with its `ltxt` length before `cue ` alone, a loop outside the data is clamped,
// every failure names the byte and the next step, and a shipped sound reads.
//
// ge:summary Tests for the WAV reader: formats, the downmix, the loop chunks, errors, a real file
// ge:keywords test wav read_wav pcm 8-bit 16-bit float extensible downmix loop cue smpl ltxt
// ge:skill audio testing
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "audio/pcm.hpp"
#include "check.hpp"

namespace {

using namespace ge::audio;
using Bytes = std::vector<uint8_t>;

void put16(Bytes& b, uint32_t v) {
  b.push_back(static_cast<uint8_t>(v));
  b.push_back(static_cast<uint8_t>(v >> 8));
}
void put32(Bytes& b, uint32_t v) {
  put16(b, v & 0xFFFF);
  put16(b, v >> 16);
}
void put_id(Bytes& b, const char* id) { b.insert(b.end(), id, id + 4); }
void chunk(Bytes& b, const char* id, const Bytes& body) {
  put_id(b, id);
  put32(b, static_cast<uint32_t>(body.size()));
  b.insert(b.end(), body.begin(), body.end());
  if (body.size() & 1) b.push_back(0);
}

Bytes fmt_chunk(uint16_t format, int channels, int rate, int bits, bool extensible = false) {
  Bytes f;
  put16(f, extensible ? 0xFFFE : format);
  put16(f, static_cast<uint32_t>(channels));
  put32(f, static_cast<uint32_t>(rate));
  put32(f, static_cast<uint32_t>(rate * channels * bits / 8));
  put16(f, static_cast<uint32_t>(channels * bits / 8));
  put16(f, static_cast<uint32_t>(bits));
  if (extensible) {
    put16(f, 22);
    put16(f, static_cast<uint32_t>(bits));
    put32(f, 0);
    put16(f, format);  // the sub-format GUID's first two bytes
    for (int i = 0; i < 14; i++) f.push_back(0);
  }
  return f;
}

Bytes wav(const Bytes& fmt, const Bytes& data,
          const std::vector<std::pair<const char*, Bytes>>& extra = {}) {
  Bytes body;
  put_id(body, "WAVE");
  chunk(body, "fmt ", fmt);
  for (const auto& [id, b] : extra) chunk(body, id, b);
  chunk(body, "data", data);
  Bytes out;
  put_id(out, "RIFF");
  put32(out, static_cast<uint32_t>(body.size()));
  out.insert(out.end(), body.begin(), body.end());
  return out;
}

Bytes pcm16(const std::vector<int16_t>& s) {
  Bytes b;
  for (int16_t v : s) put16(b, static_cast<uint16_t>(v));
  return b;
}

Bytes cue_chunk(uint32_t id, uint32_t offset) {
  Bytes c;
  put32(c, 1);
  put32(c, id);
  put32(c, 0);
  put_id(c, "data");
  put32(c, 0);
  put32(c, 0);
  put32(c, offset);
  return c;
}

Bytes ltxt_list(uint32_t id, uint32_t length) {
  Bytes l;
  put_id(l, "adtl");
  Bytes t;
  put32(t, id);
  put32(t, length);
  put_id(t, "mark");
  for (int i = 0; i < 8; i++) t.push_back(0);
  chunk(l, "ltxt", t);
  return l;
}

Bytes smpl_chunk(uint32_t start, uint32_t end_inclusive) {
  Bytes s;
  for (int i = 0; i < 7; i++) put32(s, 0);
  put32(s, 1);  // loops
  put32(s, 0);
  put32(s, 0);  // cue id
  put32(s, 0);  // type
  put32(s, start);
  put32(s, end_inclusive);
  put32(s, 0);
  put32(s, 0);
  return s;
}

Pcm read_ok(const Bytes& b) {
  Pcm p;
  std::string error;
  const bool ok = read_wav(b, "test.wav", &p, &error);
  CHECK_MSG(ok, "read_wav failed: " + error);
  return p;
}

}  // namespace

TEST(sixteen_bit_mono_reads_its_samples_and_rate) {
  const Pcm p = read_ok(wav(fmt_chunk(1, 1, 22050, 16), pcm16({0, 16384, -32768, 32767})));
  CHECK(p.rate == 22050 && p.frames() == 4 && !p.loops());
  CHECK(p.samples[0] == 0 && p.samples[1] == 0.5f && p.samples[2] == -1);
  CHECK(p.samples[3] > 0.9999f && p.samples[3] < 1);
}

TEST(eight_bit_is_unsigned_about_128) {
  const Pcm p = read_ok(wav(fmt_chunk(1, 1, 11025, 8), Bytes{128, 192, 0}));
  CHECK(p.frames() == 3 && p.samples[0] == 0 && p.samples[1] == 0.5f && p.samples[2] == -1);
}

TEST(stereo_mixes_down_and_float_and_extensible_read) {
  const Pcm st = read_ok(wav(fmt_chunk(1, 2, 44100, 16), pcm16({16384, 0, -16384, -16384})));
  CHECK(st.frames() == 2 && st.samples[0] == 0.25f && st.samples[1] == -0.5f);
  Bytes f;
  const float v[] = {0.25f, -2.0f};  // out of range is clamped
  for (float x : v) {
    uint32_t u;
    std::memcpy(&u, &x, 4);
    put32(f, u);
  }
  const Pcm fl = read_ok(wav(fmt_chunk(3, 1, 48000, 32), f));
  CHECK(fl.samples[0] == 0.25f && fl.samples[1] == -1);
  const Pcm ex = read_ok(wav(fmt_chunk(1, 1, 22050, 16, true), pcm16({8192})));
  CHECK(ex.frames() == 1 && ex.samples[0] == 0.25f);
}

TEST(the_loop_comes_from_smpl_then_cue_with_its_length_then_cue_alone) {
  const Bytes data = pcm16(std::vector<int16_t>(100, 1000));
  const Pcm cue = read_ok(wav(fmt_chunk(1, 1, 22050, 16), data, {{"cue ", cue_chunk(1, 30)}}));
  CHECK(cue.loops() && cue.loop_start == 30 && cue.loop_end == 100);
  const Pcm marked = read_ok(wav(fmt_chunk(1, 1, 22050, 16), data,
                                 {{"cue ", cue_chunk(1, 30)}, {"LIST", ltxt_list(1, 50)}}));
  CHECK(marked.loop_start == 30 && marked.loop_end == 80);
  const Pcm other = read_ok(wav(fmt_chunk(1, 1, 22050, 16), data,
                                {{"cue ", cue_chunk(1, 30)}, {"LIST", ltxt_list(2, 50)}}));
  CHECK_MSG(other.loop_end == 100, "an ltxt for another cue is not this loop's length");
  const Pcm smpl = read_ok(wav(fmt_chunk(1, 1, 22050, 16), data,
                               {{"cue ", cue_chunk(1, 30)}, {"smpl", smpl_chunk(10, 59)}}));
  CHECK(smpl.loop_start == 10 && smpl.loop_end == 60);
  const Pcm clamped = read_ok(wav(fmt_chunk(1, 1, 22050, 16), data,
                                  {{"cue ", cue_chunk(1, 30)}, {"LIST", ltxt_list(1, 500)}}));
  CHECK(clamped.loop_end == 100);
  const Pcm past = read_ok(wav(fmt_chunk(1, 1, 22050, 16), data, {{"cue ", cue_chunk(1, 100)}}));
  CHECK_MSG(!past.loops(), "a cue at the data's end leaves nothing to loop");
}

TEST(failures_name_the_byte_and_the_next_step) {
  Pcm p;
  std::string error;
  CHECK(!read_wav(Bytes{'R', 'I', 'F', 'F'}, "a.wav", &p, &error));
  CHECK(error.find("a.wav") == 0 && error.find("at byte 0") != std::string::npos &&
        error.find("next:") != std::string::npos);
  CHECK(!read_wav(wav(fmt_chunk(1, 1, 22050, 24), Bytes(6, 0)), "b.wav", &p, &error));
  CHECK(error.find("24 bits") != std::string::npos &&
        error.find("at byte 20") != std::string::npos);
  Bytes no_data;
  put_id(no_data, "RIFF");
  put32(no_data, 4 + 8 + 16);
  put_id(no_data, "WAVE");
  chunk(no_data, "fmt ", fmt_chunk(1, 1, 22050, 16));
  CHECK(!read_wav(no_data, "c.wav", &p, &error) && error.find("no data") != std::string::npos);
}

TEST(a_shipped_sound_reads) {
  std::ifstream in(ge::test::root_path("games/shooters_edge/content/sounds/weapon/explosion.wav"),
                   std::ios::binary);
  const Bytes b{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
  const Pcm p = read_ok(b);
  CHECK(p.rate == 22050 && p.seconds() > 0.2 && p.seconds() < 10);
  float peak = 0;
  for (float s : p.samples) peak = s > peak ? s : -s > peak ? -s : peak;
  CHECK(peak > 0.1f && peak <= 1);
}
