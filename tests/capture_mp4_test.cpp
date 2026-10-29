// The fragmented MP4 muxer (engine/capture/mp4.hpp), read back by the engine's reader
// (mp4_read.hpp): the brands and the tracks with their sample entries and configuration boxes,
// Opus's pre-skip as the edit list, every sample back at its time with its bytes, its duration
// and its sync flag, fragments cut at keyframes by time and by size, the mfra's seek points and
// the mehd's duration, a file cut short reading to its last whole fragment at every byte, the
// codecs an MP4 cannot carry refused, the container picked by the file's name, and the sync
// fixture recorded as an .mp4 decoding to every picture with each click on its step.
//
// ge:summary Tests: the fragmented MP4 muxer read back: tracks, samples, fragments, mfra, crash
// ge:keywords test mp4 fmp4 muxer read fragment moof trun mfra truncated opus dops elst av1c
// ge:skill replay testing
// ge:test tests/capture_mp4_test.cpp
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <span>
#include <string>
#include <vector>

#include "audio/voice.hpp"
#include "capture/av_export.hpp"
#include "capture/codecs.hpp"
#include "capture/mp4.hpp"
#include "capture/mp4_read.hpp"
#include "capture/mux.hpp"
#include "capture/sync_fixture.hpp"
#include "check.hpp"
#include "play/record.hpp"

namespace gc = ge::capture;
namespace gp = ge::play;
namespace fs = std::filesystem;

namespace {

// An OpusHead record: stereo, `preskip`, 48 kHz, no gain, family 0.
std::vector<uint8_t> opus_head(int preskip) {
  std::vector<uint8_t> h = {'O', 'p', 'u', 's', 'H', 'e', 'a', 'd', 1, 2};
  h.push_back(static_cast<uint8_t>(preskip & 0xFF));
  h.push_back(static_cast<uint8_t>(preskip >> 8));
  for (const int b : {0x80, 0xBB, 0x00, 0x00, 0x00, 0x00, 0x00})
    h.push_back(static_cast<uint8_t>(b));
  return h;
}

gc::MatroskaOptions small(bool audio = true) {
  gc::MatroskaOptions o;
  gc::VideoTrack v;
  v.codec = "V_AV1";
  v.width = 64;
  v.height = 32;
  v.fps = 30;
  v.codec_private = {0x81, 0x00, 0x0C, 0x00, 0x0A, 0x0B};
  o.video = v;
  if (audio) {
    gc::AudioTrack a;
    a.codec = "A_OPUS";
    a.bits = 0;
    a.codec_private = opus_head(312);
    o.audio = a;
  }
  return o;
}

// `frames` pictures of 12 bytes (picture k filled with k, a key every `key_every`) and the 20 ms
// sound packets due before each (6 bytes filled with 200 + n), as the interleave hands them.
std::vector<uint8_t> write(const gc::MatroskaOptions& o, int frames, int key_every,
                           std::string& error, int64_t* fragments = nullptr) {
  gc::Mp4Writer w;
  if (!w.open("", o, error)) return {};
  int64_t sound = 0;
  for (int k = 0; k < frames; k++) {
    const int64_t ms = k * 1000 / 30;
    while (o.audio && sound * 20 < ms) {
      const std::vector<uint8_t> pkt(6, static_cast<uint8_t>(200 + sound % 50));
      if (!w.audio(sound * 20, pkt, error)) return {};
      sound++;
    }
    const std::vector<uint8_t> picture(12, static_cast<uint8_t>(k));
    if (!w.video(ms, k % key_every == 0, picture, error)) return {};
  }
  if (!w.close(frames * 1000 / 30, error)) return {};
  if (fragments) *fragments = w.fragments();
  return w.bytes();
}

std::vector<uint8_t> read_all(const std::string& path) {
  std::ifstream in(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

}  // namespace

TEST(the_moov_names_both_tracks_with_their_entries_and_configuration) {
  std::string error;
  const std::vector<uint8_t> bytes = write(small(), 30, 15, error);
  CHECK_MSG(!bytes.empty(), error.c_str());
  const auto f = gc::read_mp4(bytes, error);
  CHECK_MSG(f.has_value(), error.c_str());
  if (!f) return;
  CHECK(f->major == "isom");
  CHECK(f->brands.size() == 4 && f->brands[1] == "iso6" && f->brands[3] == "av01");
  CHECK(f->movie_timescale == 1000);
  CHECK(f->fragment_duration == 1000);
  CHECK(!f->cut);
  CHECK(f->tracks.size() == 2);
  if (f->tracks.size() != 2) return;
  const gc::Mp4Track& v = f->tracks[0];
  CHECK(v.id == 1 && v.handler == "vide" && v.entry == "av01" && v.timescale == 1000);
  CHECK(v.width == 64 && v.height == 32);
  CHECK(v.config_type == "av1C" && v.config == small().video->codec_private);
  CHECK(v.edit_start == 0);
  const gc::Mp4Track& a = f->tracks[1];
  CHECK(a.id == 2 && a.handler == "soun" && a.entry == "Opus" && a.timescale == 48000);
  CHECK(a.channels == 2 && a.rate == 48000);
  // dOps: version 0, two channels, the pre-skip, the rate, the gain and the family, big-endian.
  const std::vector<uint8_t> dops = {0, 2, 0x01, 0x38, 0x00, 0x00, 0xBB, 0x80, 0, 0, 0};
  CHECK(a.config_type == "dOps" && a.config == dops);
  CHECK(a.edit_start == 312);
}

TEST(every_sample_comes_back_at_its_time_with_its_bytes_duration_and_sync_flag) {
  std::string error;
  const std::vector<uint8_t> bytes = write(small(), 30, 15, error);
  const auto f = gc::read_mp4(bytes, error);
  CHECK_MSG(f.has_value(), error.c_str());
  if (!f) return;
  std::vector<gc::Mp4Sample> video, sound;
  for (const gc::Mp4Sample& s : f->samples) (s.track == 1 ? video : sound).push_back(s);
  CHECK(video.size() == 30);
  CHECK(sound.size() == 49);  // 0, 20, .., 960 ms: every one due before a picture
  for (size_t k = 0; k < video.size(); k++) {
    const gc::Mp4Sample& s = video[k];
    CHECK(s.ms == static_cast<int64_t>(k) * 1000 / 30);
    CHECK(s.key == (k % 15 == 0));
    CHECK(s.size == 12 && bytes[s.offset] == k && bytes[s.offset + 11] == k);
    const int64_t next = k + 1 < video.size() ? video[k + 1].t : s.t + video[k - 1].duration;
    CHECK(s.duration == next - s.t);
  }
  for (size_t n = 0; n < sound.size(); n++) {
    CHECK(sound[n].t == static_cast<int64_t>(n) * 960 && sound[n].duration == 960);
    CHECK(sound[n].key && sound[n].size == 6 && bytes[sound[n].offset] == 200 + n % 50);
  }
}

TEST(fragments_are_cut_at_keyframes_once_a_second_has_run_and_the_mfra_finds_each) {
  std::string error;
  int64_t written = 0;
  const std::vector<uint8_t> bytes = write(small(), 120, 15, error, &written);  // 4 s
  const auto f = gc::read_mp4(bytes, error);
  CHECK_MSG(f.has_value(), error.c_str());
  if (!f) return;
  CHECK(f->fragments.size() == 4 && written == 4);
  CHECK(f->seeks.size() == f->fragments.size());
  for (size_t i = 0; i < f->seeks.size() && i < f->fragments.size(); i++) {
    CHECK(f->seeks[i].track == 1);
    CHECK(f->seeks[i].moof == f->fragments[i]);
    CHECK(f->seeks[i].t == static_cast<int64_t>(i) * 1000);  // keys at 0, 30, 60, 90 frames
  }
  // Each fragment opens with a sync sample of the video.
  size_t fragment = 0;
  for (const gc::Mp4Sample& s : f->samples) {
    if (s.track != 1) continue;
    if (fragment < f->seeks.size() && s.t == f->seeks[fragment].t) {
      CHECK(s.key);
      fragment++;
    }
  }
  CHECK(fragment == 4);
  // A smaller byte limit cuts at every key instead.
  gc::MatroskaOptions o = small();
  o.cluster_bytes = 64;
  CHECK(!write(o, 120, 15, error, &written).empty());
  CHECK(written == 8);
}

TEST(a_file_cut_at_any_byte_reads_to_its_last_whole_fragment) {
  std::string error;
  const std::vector<uint8_t> bytes = write(small(), 120, 15, error);
  const auto whole = gc::read_mp4(bytes, error);
  CHECK_MSG(whole.has_value(), error.c_str());
  if (!whole || whole->fragments.size() != 4) return;
  const size_t moov_end = static_cast<size_t>(whole->fragments[0]);
  // Inside the header, nothing reads.
  for (size_t cut : {size_t{0}, size_t{20}, moov_end - 1}) {
    const auto f = gc::read_mp4(std::span(bytes.data(), cut), error);
    CHECK(!f.has_value());
  }
  // Where each fragment ends: the next one's moof, and for the last the mfra (its mfro, the
  // file's last four bytes, names its size).
  const size_t n = bytes.size();
  const size_t mfra = n - ((size_t{bytes[n - 4]} << 24) | (size_t{bytes[n - 3]} << 16) |
                           (size_t{bytes[n - 2]} << 8) | bytes[n - 1]);
  std::vector<size_t> ends;
  for (size_t i = 0; i < whole->fragments.size(); i++) {
    ends.push_back(i + 1 < whole->fragments.size() ? static_cast<size_t>(whole->fragments[i + 1])
                                                   : mfra);
  }
  for (size_t cut = moov_end; cut < mfra; cut += 7) {
    const auto f = gc::read_mp4(std::span(bytes.data(), cut), error);
    CHECK_MSG(f.has_value(), error.c_str());
    if (!f) return;
    size_t kept = 0;
    while (kept < ends.size() && ends[kept] <= cut) kept++;
    CHECK(f->fragments.size() == kept);
    const size_t upto = kept > 0 ? ends[kept - 1] : moov_end;  // where the whole part ends
    size_t samples = 0;
    for (const gc::Mp4Sample& s : whole->samples) samples += s.offset < upto ? 1 : 0;
    CHECK(f->samples.size() == samples);
    CHECK(f->cut == (cut != upto));
    CHECK(f->seeks.empty());
  }
}

TEST(what_an_mp4_cannot_carry_is_refused_and_says_where_it_goes) {
  std::string error;
  gc::MatroskaOptions raw;
  raw.video = gc::VideoTrack{};
  raw.video->width = 4;
  raw.video->height = 2;
  gc::Mp4Writer w;
  CHECK(!w.open("", raw, error));
  CHECK(error.find(".mkv") != std::string::npos);
  gc::MatroskaOptions pcm = small();
  pcm.audio = gc::AudioTrack{};
  CHECK(!gc::mp4_problem(pcm).empty());
  gc::MatroskaOptions bare = small();
  bare.video->codec_private.clear();
  CHECK(gc::mp4_problem(bare).find("codec private") != std::string::npos);
  gc::Mp4Writer x;
  CHECK_MSG(x.open("", small(false), error), error.c_str());
  CHECK(x.video(100, true, std::vector<uint8_t>(4, 1), error));
  CHECK(!x.video(50, true, std::vector<uint8_t>(4, 1), error));
  CHECK(error.find("before its last") != std::string::npos);
  CHECK(!x.audio(0, std::vector<uint8_t>(4, 1), error));
}

TEST(the_container_is_picked_by_the_files_name_or_named) {
  CHECK(gc::container_of("run.mp4") == gc::Container::kMp4);
  CHECK(gc::container_of("RUN.M4V") == gc::Container::kMp4);
  CHECK(gc::container_of("run.webm") == gc::Container::kMatroska);
  CHECK(gc::container_of("run") == gc::Container::kMatroska);
  gc::Container c = gc::Container::kMatroska;
  std::string error;
  CHECK(gc::container_named("", "a.mp4", c, error) && c == gc::Container::kMp4);
  CHECK(gc::container_named("mkv", "a.mp4", c, error) && c == gc::Container::kMatroska);
  CHECK(gc::container_named("mp4", "", c, error) && c == gc::Container::kMp4);
  CHECK(!gc::container_named("avi", "", c, error));
}

TEST(the_sync_fixture_as_mp4_decodes_every_picture_and_keeps_each_click_on_its_step) {
  if (!gc::encoders_built()) return;
  constexpr int64_t kSteps = 600;
  constexpr double kFps = 30;
  const fs::path dir = fs::temp_directory_path() / "ge_capture_mp4_test";
  fs::create_directories(dir);
  gp::RecordOptions o;
  o.view = "flash";
  o.fps = kFps;
  o.band = false;
  o.thumbs = 0;
  o.encoding.format = "mkv";
  o.encoding.codec = "av1";
  o.sound = &gc::kSyncSound;
  o.video = (dir / "sync.mp4").string();
  std::string error;
  CHECK_MSG(gp::record(gc::kSyncGame, gc::sync_log(kSteps), o, error).has_value(), error.c_str());
  const std::vector<uint8_t> bytes = read_all(o.video);
  const auto f = gc::read_mp4(bytes, error);
  CHECK_MSG(f.has_value(), error.c_str());
  if (!f) return;
  CHECK(f->tracks.size() == 2 && !f->cut);
  if (f->tracks.size() != 2) return;
  CHECK(f->tracks[0].entry == "av01" && f->tracks[1].entry == "Opus");
  const int64_t skip = f->tracks[1].edit_start;
  CHECK(skip > 0 && skip < 1000);
  gc::Av1Decoder vd;
  gc::OpusDecoder ad;
  CHECK(vd.open(error) && ad.open(error));
  std::vector<std::pair<int64_t, bool>> pictures;
  std::vector<float> sound;
  for (const gc::Mp4Sample& s : f->samples) {
    const std::span<const uint8_t> data(bytes.data() + s.offset, s.size);
    if (s.track == 1) {
      std::vector<uint8_t> yuv;
      int w = 0, h = 0;
      bool got = false;
      CHECK(vd.decode(data, yuv, w, h, got, error));
      if (got) pictures.emplace_back(s.ms, yuv[static_cast<size_t>(w) * (h / 2) + w / 2] > 128);
    } else {
      CHECK(ad.decode(data, sound, error));
    }
  }
  CHECK(pictures.size() == 151);
  int clicks = 0;
  size_t from = static_cast<size_t>(skip);
  for (int64_t step = 1; step <= kSteps; step++) {
    if (!gc::sync_clicks_at(step, 150, 37)) continue;
    clicks++;
    const int64_t due = gc::step_sample(step, 0, 120, ge::audio::kRate);
    size_t loud = from;
    while (loud < sound.size() / 2 && std::fabs(sound[2 * loud]) < 0.05F) loud++;
    from = loud + static_cast<size_t>(ge::audio::kRate / 10);
    const auto played = static_cast<int64_t>(loud) - skip;
    CHECK(std::llabs(played - due) < 3 * ge::audio::kRate / 1000);
    const double click_ms = 1000.0 * static_cast<double>(played) / ge::audio::kRate;
    int64_t lit_ms = -1;
    for (const auto& [ms, lit] : pictures) {
      if (lit && static_cast<double>(ms) + 3 >= click_ms) {
        lit_ms = ms;
        break;
      }
    }
    CHECK(lit_ms >= 0);
    CHECK(static_cast<double>(lit_ms) - click_ms < 1000.0 / kFps + 3);
  }
  CHECK(clicks == 4);
}
