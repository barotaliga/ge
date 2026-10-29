// The fragmented MP4 muxer (mp4.hpp).
//
// ge:summary mp4's definitions: the boxes, the sample entries, the fragments, the mfra and patches
// ge:keywords mp4 boxes ftyp moov mvhd trak tkhd mdia mdhd hdlr stsd mvex trex moof traf trun mdat
// ge:skill replay
// ge:test tests/capture_mp4_test.cpp
#include "capture/mp4.hpp"

#include <algorithm>
#include <cstring>

namespace ge::capture {
namespace {

constexpr int64_t kMovieTimescale = 1000;     // the movie's clock (mvhd, mehd): milliseconds
constexpr uint32_t kKeyFlags = 0x02000000;    // depends on no other sample
constexpr uint32_t kDeltaFlags = 0x01010000;  // depends on others, and is not a sync sample
constexpr uint32_t kTrunFlags = 0x000701;     // a data offset; each sample's duration, size, flags
constexpr uint32_t kBaseIsMoof = 0x020000;    // tfhd: the data offsets count from the moof
constexpr size_t kOpusHeadSize = 19;

// Big-endian box writing over a byte vector: `open` reserves a box's 32-bit size and `close`
// writes it.
struct Boxes {
  std::vector<uint8_t> b;

  void u8(uint64_t v) { b.push_back(static_cast<uint8_t>(v)); }
  void u16(uint64_t v) {
    for (int i = 1; i >= 0; i--) u8(v >> (8 * i));
  }
  void u32(uint64_t v) {
    for (int i = 3; i >= 0; i--) u8(v >> (8 * i));
  }
  void u64(uint64_t v) {
    for (int i = 7; i >= 0; i--) u8(v >> (8 * i));
  }
  void zeros(size_t n) { b.resize(b.size() + n, 0); }
  void four(std::string_view t) {
    for (size_t i = 0; i < 4; i++) u8(i < t.size() ? static_cast<uint8_t>(t[i]) : ' ');
  }
  void bin(std::span<const uint8_t> d) { b.insert(b.end(), d.begin(), d.end()); }
  size_t open(std::string_view type) {
    const size_t at = b.size();
    u32(0);
    four(type);
    return at;
  }
  // A full box: its version and 24 bits of flags after the type.
  size_t full(std::string_view type, int version, uint32_t flags) {
    const size_t at = open(type);
    u8(static_cast<uint64_t>(version));
    u8(flags >> 16);
    u8(flags >> 8);
    u8(flags);
    return at;
  }
  void close(size_t at) { put32(at, b.size() - at); }
  void put32(size_t at, uint64_t v) {
    for (int i = 0; i < 4; i++)
      b[at + static_cast<size_t>(i)] = static_cast<uint8_t>(v >> (8 * (3 - i)));
  }
  void matrix() {
    for (const uint32_t m : {0x00010000u, 0u, 0u, 0u, 0x00010000u, 0u, 0u, 0u, 0x40000000u}) u32(m);
  }
};

uint32_t le16(std::span<const uint8_t> d, size_t at) {
  return static_cast<uint32_t>(d[at] | (d[at + 1] << 8));
}

uint32_t le32(std::span<const uint8_t> d, size_t at) {
  return static_cast<uint32_t>(d[at]) | (static_cast<uint32_t>(d[at + 1]) << 8) |
         (static_cast<uint32_t>(d[at + 2]) << 16) | (static_cast<uint32_t>(d[at + 3]) << 24);
}

bool opus_head(std::span<const uint8_t> p) {
  return p.size() >= kOpusHeadSize && std::memcmp(p.data(), "OpusHead", 8) == 0;
}

// The `dOps` box from an OpusHead: the same fields, big-endian, with no magic and version 0.
void dops(Boxes& x, std::span<const uint8_t> head) {
  const size_t at = x.open("dOps");
  x.u8(0);
  x.u8(head[9]);                                          // the output channels
  x.u16(le16(head, 10));                                  // the pre-skip
  x.u32(le32(head, 12));                                  // the input's rate
  x.u16(le16(head, 16));                                  // the output gain
  x.u8(head[18]);                                         // the channel mapping family
  if (head[18] != 0) x.bin(head.subspan(kOpusHeadSize));  // its table, laid out alike
  x.close(at);
}

void hdlr(Boxes& x, std::string_view type, std::string_view name) {
  const size_t at = x.full("hdlr", 0, 0);
  x.u32(0);
  x.four(type);
  x.zeros(12);
  x.bin(std::span(reinterpret_cast<const uint8_t*>(name.data()), name.size()));
  x.u8(0);
  x.close(at);
}

void dinf(Boxes& x) {
  const size_t at = x.open("dinf");
  const size_t dref = x.full("dref", 0, 0);
  x.u32(1);
  x.close(x.full("url ", 0, 1));  // the samples are in this file
  x.close(dref);
  x.close(at);
}

// The sample tables, empty: every sample is in a fragment.
void empty_tables(Boxes& x) {
  x.u32(0);
  for (const char* t : {"stts", "stsc", "stco"}) {
    const size_t at = x.full(t, 0, 0);
    x.u32(0);
    x.close(at);
  }
  const size_t stsz = x.full("stsz", 0, 0);
  x.u32(0);
  x.u32(0);
  x.close(stsz);
}

void video_trak(Boxes& x, uint32_t id, const VideoTrack& v) {
  const size_t trak = x.open("trak");
  const size_t tkhd = x.full("tkhd", 0, 3);  // enabled, in the movie
  x.u32(0);
  x.u32(0);
  x.u32(id);
  x.u32(0);
  x.u32(0);  // the duration: the fragments'
  x.zeros(8);
  x.u16(0);
  x.u16(0);
  x.u16(0);  // a video track's volume
  x.u16(0);
  x.matrix();
  x.u32(static_cast<uint64_t>(v.width) << 16);
  x.u32(static_cast<uint64_t>(v.height) << 16);
  x.close(tkhd);
  const size_t mdia = x.open("mdia");
  const size_t mdhd = x.full("mdhd", 0, 0);
  x.u32(0);
  x.u32(0);
  x.u32(kMovieTimescale);
  x.u32(0);
  x.u16(0x55C4);  // "und"
  x.u16(0);
  x.close(mdhd);
  hdlr(x, "vide", "ge video");
  const size_t minf = x.open("minf");
  const size_t vmhd = x.full("vmhd", 0, 1);
  x.zeros(8);
  x.close(vmhd);
  dinf(x);
  const size_t stbl = x.open("stbl");
  const size_t stsd = x.full("stsd", 0, 0);
  x.u32(1);
  const std::string_view entry = mp4_entry_of(v.codec);
  const size_t e = x.open(entry);
  x.zeros(6);
  x.u16(1);  // the data reference
  x.zeros(16);
  x.u16(static_cast<uint64_t>(v.width));
  x.u16(static_cast<uint64_t>(v.height));
  x.u32(0x00480000);  // 72 dpi
  x.u32(0x00480000);
  x.u32(0);
  x.u16(1);  // a frame a sample
  x.zeros(32);
  x.u16(0x18);
  x.u16(0xFFFF);
  const std::string_view config = entry == "av01" ? "av1C" : entry == "avc1" ? "avcC" : "hvcC";
  const size_t c = x.open(config);
  x.bin(v.codec_private);
  x.close(c);
  x.close(e);
  x.close(stsd);
  empty_tables(x);
  x.close(stbl);
  x.close(minf);
  x.close(mdia);
  x.close(trak);
}

void audio_trak(Boxes& x, uint32_t id, const AudioTrack& a) {
  const std::span<const uint8_t> head = a.codec_private;
  const size_t trak = x.open("trak");
  const size_t tkhd = x.full("tkhd", 0, 3);
  x.u32(0);
  x.u32(0);
  x.u32(id);
  x.u32(0);
  x.u32(0);
  x.zeros(8);
  x.u16(0);
  x.u16(1);  // the alternate group: one of the movie's sounds
  x.u16(0x0100);
  x.u16(0);
  x.matrix();
  x.u32(0);
  x.u32(0);
  x.close(tkhd);
  if (const uint32_t preskip = le16(head, 10); preskip > 0) {
    // The encoder's delay dropped: the presentation starts `preskip` samples into the media.
    const size_t edts = x.open("edts");
    const size_t elst = x.full("elst", 0, 0);
    x.u32(1);
    x.u32(0);  // the edit's duration: the rest of the media
    x.u32(preskip);
    x.u32(0x00010000);
    x.close(elst);
    x.close(edts);
  }
  const size_t mdia = x.open("mdia");
  const size_t mdhd = x.full("mdhd", 0, 0);
  x.u32(0);
  x.u32(0);
  x.u32(static_cast<uint64_t>(a.rate));
  x.u32(0);
  x.u16(0x55C4);
  x.u16(0);
  x.close(mdhd);
  hdlr(x, "soun", "ge sound");
  const size_t minf = x.open("minf");
  const size_t smhd = x.full("smhd", 0, 0);
  x.u32(0);
  x.close(smhd);
  dinf(x);
  const size_t stbl = x.open("stbl");
  const size_t stsd = x.full("stsd", 0, 0);
  x.u32(1);
  const size_t e = x.open("Opus");
  x.zeros(6);
  x.u16(1);
  x.zeros(8);
  x.u16(static_cast<uint64_t>(a.channels));
  x.u16(16);
  x.u32(0);
  x.u32(static_cast<uint64_t>(a.rate) << 16);
  dops(x, head);
  x.close(e);
  x.close(stsd);
  empty_tables(x);
  x.close(stbl);
  x.close(minf);
  x.close(mdia);
  x.close(trak);
}

}  // namespace

std::string_view mp4_entry_of(std::string_view codec) {
  if (codec == "V_AV1") return "av01";
  if (codec == "V_MPEG4/ISO/AVC") return "avc1";
  if (codec == "V_MPEGH/ISO/HEVC") return "hvc1";
  if (codec == "A_OPUS") return "Opus";
  return "";
}

std::string mp4_problem(const MatroskaOptions& o) {
  if (const std::string why = options_problem(o); !why.empty()) return why;
  if (o.video && (mp4_entry_of(o.video->codec).empty() || o.video->codec == "A_OPUS")) {
    return "an MP4 carries AV1, H.264 or HEVC pictures, not " + o.video->codec +
           " (uncompressed pictures go in a .mkv)";
  }
  if (o.video && o.video->codec_private.empty()) {
    return "the " + o.video->codec + " track has no codec private (its decoder's configuration)";
  }
  if (o.video && (o.video->width > 0xFFFF || o.video->height > 0xFFFF)) {
    return "the pictures are over 65535 on a side";
  }
  if (o.audio && o.audio->codec != "A_OPUS") {
    return "an MP4 carries Opus sound, not " + o.audio->codec + " (float PCM goes in a .mkv)";
  }
  if (o.audio && !opus_head(o.audio->codec_private)) {
    return "the Opus track's codec private is not an OpusHead record";
  }
  if (o.audio && o.audio->rate > 0xFFFF) return "the sound's rate is over 65535";
  return "";
}

bool Mp4Writer::open(const std::string& path, const MatroskaOptions& o, std::string& error) {
  if (open_) {
    error = "the muxer is already open";
    return false;
  }
  if (const std::string why = mp4_problem(o); !why.empty()) {
    error = why;
    return false;
  }
  opt_ = o;
  video_ = Track{};
  audio_ = Track{};
  fragment_ms_ = -1;
  sequence_ = 0;
  starts_.clear();
  uint32_t id = 1;
  if (o.video) {
    video_.on = true;
    video_.id = id++;
    video_.timescale = kMovieTimescale;
  }
  if (o.audio) {
    audio_.on = true;
    audio_.id = id++;
    audio_.timescale = o.audio->rate;
  }
  if (!out_.open(path, error)) return false;

  Boxes x;
  const size_t ftyp = x.open("ftyp");
  x.four("isom");
  x.u32(0x200);
  for (const char* brand : {"isom", "iso6", "mp41"}) x.four(brand);
  if (o.video && o.video->codec == "V_AV1") x.four("av01");
  x.close(ftyp);

  const size_t moov = x.open("moov");
  const size_t mvhd = x.full("mvhd", 0, 0);
  x.u32(0);
  x.u32(0);
  x.u32(kMovieTimescale);
  x.u32(0);  // the duration: the fragments' (the mehd's)
  x.u32(0x00010000);
  x.u16(0x0100);
  x.zeros(10);
  x.matrix();
  x.zeros(24);
  x.u32(id);  // the next track's id
  x.close(mvhd);
  if (o.video) video_trak(x, video_.id, *o.video);
  if (o.audio) audio_trak(x, audio_.id, *o.audio);
  const size_t mvex = x.open("mvex");
  const size_t mehd = x.full("mehd", 1, 0);
  mehd_at_ = static_cast<int64_t>(x.b.size());
  x.u64(0);
  x.close(mehd);
  for (const Track* k : {&video_, &audio_}) {
    if (!k->on) continue;
    const size_t trex = x.full("trex", 0, 0);
    x.u32(k->id);
    x.u32(1);  // the sample description
    x.u32(0);
    x.u32(0);
    x.u32(0);
    x.close(trex);
  }
  x.close(mvex);
  x.close(moov);
  open_ = out_.emit(x.b, error);
  return open_;
}

size_t Mp4Writer::held_bytes() const { return video_.data.size() + audio_.data.size(); }

bool Mp4Writer::add(Track& k, int64_t ms, bool key, std::span<const uint8_t> data,
                    std::string& error) {
  if (!open_) {
    error = "the muxer is not open";
    return false;
  }
  if (ms < 0 || ms < k.last_ms) {
    error = "track " + std::to_string(k.id) + "'s sample at " + std::to_string(ms) +
            " ms is before its last at " + std::to_string(k.last_ms) + " ms";
    return false;
  }
  k.last_ms = ms;
  const int64_t t = ms * k.timescale / 1000;
  if (!k.samples.empty()) k.samples.back().duration = std::max<int64_t>(0, t - k.samples.back().t);
  const bool cuts = &k == &video_ || !video_.on;  // the track fragments are cut on
  if (cuts && key) {
    const bool due = fragment_ms_ < 0 || ms - fragment_ms_ >= opt_.cluster_ms ||
                     held_bytes() >= opt_.cluster_bytes;
    if (due) {
      if (fragment_ms_ >= 0 && !flush(error)) return false;
      fragment_ms_ = ms;
    }
  }
  k.samples.push_back({t, -1, key, k.data.size(), data.size()});
  k.data.insert(k.data.end(), data.begin(), data.end());
  return true;
}

bool Mp4Writer::video(int64_t ms, bool key, std::span<const uint8_t> data, std::string& error) {
  if (!opt_.video) {
    error = "the muxer has no video track";
    return false;
  }
  return add(video_, ms, key, data, error);
}

bool Mp4Writer::audio(int64_t ms, std::span<const uint8_t> data, std::string& error) {
  if (!opt_.audio) {
    error = "the muxer has no audio track";
    return false;
  }
  return add(audio_, ms, true, data, error);
}

bool Mp4Writer::flush(std::string& error) {
  // Each track's samples whose durations are known: all but, at most, the newest.
  size_t ready[2] = {0, 0};
  Track* tracks[2] = {&video_, &audio_};
  for (int i = 0; i < 2; i++) {
    const std::vector<Sample>& s = tracks[i]->samples;
    while (ready[i] < s.size() && s[ready[i]].duration >= 0) ready[i]++;
  }
  if (ready[0] == 0 && ready[1] == 0) return true;

  Boxes x;
  const size_t moof = x.open("moof");
  const size_t mfhd = x.full("mfhd", 0, 0);
  x.u32(static_cast<uint64_t>(sequence_ + 1));
  x.close(mfhd);
  size_t offset_at[2] = {0, 0};
  size_t bytes[2] = {0, 0};
  for (int i = 0; i < 2; i++) {
    if (ready[i] == 0) continue;
    const Track& k = *tracks[i];
    const size_t traf = x.open("traf");
    const size_t tfhd = x.full("tfhd", 0, kBaseIsMoof);
    x.u32(k.id);
    x.close(tfhd);
    const size_t tfdt = x.full("tfdt", 1, 0);
    x.u64(static_cast<uint64_t>(k.samples[0].t));
    x.close(tfdt);
    const size_t trun = x.full("trun", 0, kTrunFlags);
    x.u32(ready[i]);
    offset_at[i] = x.b.size();
    x.u32(0);
    for (size_t j = 0; j < ready[i]; j++) {
      const Sample& s = k.samples[j];
      x.u32(static_cast<uint64_t>(s.duration));
      x.u32(s.size);
      x.u32(s.key ? kKeyFlags : kDeltaFlags);
      bytes[i] += s.size;
    }
    x.close(trun);
    x.close(traf);
  }
  x.close(moof);
  const uint64_t body = bytes[0] + bytes[1];
  const bool large = body + 8 > 0xFFFFFFFFull;
  const size_t header = large ? 16 : 8;
  uint64_t data_at = x.b.size() + header;  // from the moof's start, which is byte 0 of `x`
  for (int i = 0; i < 2; i++) {
    if (ready[i] == 0) continue;
    x.put32(offset_at[i], data_at);
    data_at += bytes[i];
  }
  if (large) {
    x.u32(1);
    x.four("mdat");
    x.u64(body + 16);
  } else {
    x.u32(body + 8);
    x.four("mdat");
  }
  const int64_t moof_offset = out_.written();
  for (int i = 0; i < 2; i++) {
    if (ready[i] == 0) continue;
    const Track& k = *tracks[i];
    const Sample& last = k.samples[ready[i] - 1];
    x.bin(std::span(k.data).subspan(k.samples[0].offset,
                                    last.offset + last.size - k.samples[0].offset));
  }
  if (ready[0] > 0) {
    starts_.push_back({video_.samples[0].t, moof_offset});
  } else if (!video_.on) {
    starts_.push_back({audio_.samples[0].t, moof_offset});
  }
  if (!out_.emit(x.b, error)) return false;
  sequence_++;

  // What was written dropped; what is left (the newest of a track, at most) moved to the front.
  for (int i = 0; i < 2; i++) {
    Track& k = *tracks[i];
    if (ready[i] == 0) continue;
    k.last_duration = k.samples[ready[i] - 1].duration;
    const size_t from = ready[i] < k.samples.size() ? k.samples[ready[i]].offset : k.data.size();
    k.data.erase(k.data.begin(), k.data.begin() + static_cast<std::ptrdiff_t>(from));
    k.samples.erase(k.samples.begin(), k.samples.begin() + static_cast<std::ptrdiff_t>(ready[i]));
    for (Sample& s : k.samples) s.offset -= from;
  }
  return true;
}

bool Mp4Writer::close(int64_t end_ms, std::string& error) {
  if (!open_) {
    error = "the muxer is not open";
    return false;
  }
  open_ = false;
  for (Track* k : {&video_, &audio_}) {
    if (k->samples.empty() || k->samples.back().duration >= 0) continue;
    Sample& last = k->samples.back();
    // As long as the sample before it, or to the end when it is the only one.
    int64_t d = k->samples.size() > 1 ? k->samples[k->samples.size() - 2].duration : -1;
    if (d < 0) d = k->last_duration;
    if (d < 0) d = end_ms * k->timescale / 1000 - last.t;
    last.duration = std::max<int64_t>(1, d);
  }
  if (!flush(error)) return false;

  Boxes x;
  const size_t mfra = x.open("mfra");
  const Track& seek = video_.on ? video_ : audio_;
  const size_t tfra = x.full("tfra", 1, 0);
  x.u32(seek.id);
  x.u32(0);  // one byte for each of the traf, trun and sample numbers
  x.u32(starts_.size());
  for (const Start& s : starts_) {
    x.u64(static_cast<uint64_t>(s.t));
    x.u64(static_cast<uint64_t>(s.offset));
    x.u8(1);
    x.u8(1);
    x.u8(1);
  }
  x.close(tfra);
  const size_t mfro = x.full("mfro", 0, 0);
  x.u32(0);
  x.close(mfro);
  x.close(mfra);
  x.put32(x.b.size() - 4, x.b.size());  // the mfro names the mfra's size
  if (!out_.emit(x.b, error)) return false;

  uint8_t duration[8];
  const uint64_t d = static_cast<uint64_t>(std::max<int64_t>(0, end_ms));
  for (int i = 0; i < 8; i++) duration[i] = static_cast<uint8_t>(d >> (8 * (7 - i)));
  if (!out_.patch(mehd_at_, duration, error)) return false;
  return out_.close(error);
}

}  // namespace ge::capture
