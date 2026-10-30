// The export's video codec by its word (engine/capture/video.hpp) and the platform's encoders
// behind it (platform_video.hpp, VideoToolbox on macOS): the words and their codec ids, NV12 to
// I420 and back with strides, a build or a machine with no platform encoder answering so and
// `auto` falling back to AV1, a .webm refusing what it cannot carry; and where the platform has
// an encoder, H.264 and HEVC pictures coded and decoded back close to what went in, a packet a
// picture at its time with the first a keyframe and the codec private an avcC or hvcC record,
// and the sync fixture recorded as an H.264 .mp4 decoding to every picture with each click on
// its step.
//
// ge:summary Tests: the video codec by its word, NV12, the fallback, VideoToolbox's H.264 and HEVC
// ge:keywords test h264 hevc videotoolbox platform-encoder auto fallback nv12 avcc hvcc decode
// ge:skill replay testing
// ge:test tests/capture_platform_video_test.cpp
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <span>
#include <string>
#include <vector>

#include "audio/voice.hpp"
#include "capture/av_export.hpp"
#include "capture/codecs.hpp"
#include "capture/mp4_read.hpp"
#include "capture/mux.hpp"
#include "capture/platform_video.hpp"
#include "capture/sync_fixture.hpp"
#include "capture/video.hpp"
#include "capture/yuv.hpp"
#include "check.hpp"
#include "play/record.hpp"

namespace gc = ge::capture;
namespace gp = ge::play;
namespace fs = std::filesystem;

namespace {

bool platform() { return !gc::platform_encoder_name().empty(); }

std::vector<uint8_t> read_all(const std::string& path) {
  std::ifstream in(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

// A smooth moving picture as I420: a gradient that slides a pixel a frame, chroma tilted.
std::vector<uint8_t> moving(int w, int h, int k) {
  std::vector<uint8_t> f(gc::i420_bytes(w, h));
  for (int y = 0; y < h; y++) {
    for (int x = 0; x < w; x++) {
      f[static_cast<size_t>(y) * w + x] = static_cast<uint8_t>(16 + ((x + y + k) * 2) % 220);
    }
  }
  uint8_t* u = f.data() + static_cast<size_t>(w) * h;
  uint8_t* v = u + static_cast<size_t>(w / 2) * (h / 2);
  for (int y = 0; y < h / 2; y++) {
    for (int x = 0; x < w / 2; x++) {
      u[static_cast<size_t>(y) * (w / 2) + x] = static_cast<uint8_t>(64 + (x * 2) % 128);
      v[static_cast<size_t>(y) * (w / 2) + x] = static_cast<uint8_t>(64 + (y * 2) % 128);
    }
  }
  return f;
}

// The mean absolute difference of the luma planes.
double luma_error(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b, int w, int h) {
  double sum = 0;
  const auto n = static_cast<size_t>(w) * static_cast<size_t>(h);
  for (size_t i = 0; i < n; i++) sum += std::fabs(static_cast<double>(a[i]) - b[i]);
  return sum / static_cast<double>(n);
}

// `frames` pictures through `word`'s encoder and back through the decoder its track names.
void round_trip(const char* word, gc::VideoCodec want, const char* record) {
  constexpr int kW = 320, kH = 240, kFrames = 45;
  gc::VideoEncoder enc;
  gc::VideoEncoderOptions o;
  o.width = kW;
  o.height = kH;
  o.fps = 30;
  o.key_seconds = 1;
  o.bitrate = 2'000'000;
  std::string error;
  CHECK_MSG(enc.open(word, o, error), error.c_str());
  CHECK(enc.codec() == want);
  CHECK(enc.encoder().rfind(std::string(gc::platform_encoder_name()), 0) == 0);
  // The record names its kind and starts with its version, 1.
  const std::vector<uint8_t>& priv = enc.codec_private();
  CHECK(priv.size() > 8 && priv[0] == 1);
  std::vector<std::vector<uint8_t>> in;
  std::vector<gc::Packet> packets;
  for (int k = 0; k < kFrames; k++) {
    in.push_back(moving(kW, kH, k));
    const size_t before = packets.size();
    CHECK_MSG(enc.encode(in.back(), gc::frame_ms(k, 30), packets, error), error.c_str());
    CHECK(packets.size() == before + 1);  // a packet a picture, out with it
  }
  CHECK(enc.flush(packets, error));
  CHECK(packets.size() == static_cast<size_t>(kFrames));
  if (packets.size() != static_cast<size_t>(kFrames)) return;
  int keys = 0;
  for (int k = 0; k < kFrames; k++) {
    CHECK(packets[k].ms == gc::frame_ms(k, 30));
    keys += packets[k].key ? 1 : 0;
  }
  CHECK(packets[0].key);
  CHECK(keys >= 2 && keys < kFrames);  // a keyframe a second, not every picture
  gc::VideoDecoder dec;
  CHECK_MSG(dec.open(enc.matroska_codec(), priv, kW, kH, error), error.c_str());
  double worst = 0;
  int decoded = 0;
  for (int k = 0; k < kFrames; k++) {
    std::vector<uint8_t> got;
    int w = 0, h = 0;
    bool any = false;
    CHECK_MSG(dec.decode(packets[k].data, got, w, h, any, error), error.c_str());
    if (!any) continue;
    CHECK(w == kW && h == kH);
    worst = std::max(worst, luma_error(in[static_cast<size_t>(k)], got, kW, kH));
    decoded++;
  }
  std::fprintf(stderr, "%s by %s: %d pictures decoded, worst mean luma error %.2f\n", record,
               enc.encoder().c_str(), decoded, worst);
  CHECK(decoded == kFrames);
  CHECK(worst < 6);
}

}  // namespace

TEST(the_codec_words_and_their_matroska_ids) {
  for (const char* w : {"av1", "h264", "hevc", "auto"}) CHECK(gc::video_codec_word(w));
  for (const char* w : {"", "vp9", "H264", "x264"}) CHECK(!gc::video_codec_word(w));
  CHECK(gc::matroska_codec_of(gc::VideoCodec::kAv1) == "V_AV1");
  CHECK(gc::matroska_codec_of(gc::VideoCodec::kH264) == "V_MPEG4/ISO/AVC");
  CHECK(gc::matroska_codec_of(gc::VideoCodec::kHevc) == "V_MPEGH/ISO/HEVC");
  gc::VideoEncoder e;
  std::string error;
  gc::VideoEncoderOptions o;
  o.width = 64;
  o.height = 48;
  CHECK(!e.open("vp9", o, error));
  CHECK(error.find("av1, h264, hevc and auto") != std::string::npos);
}

TEST(nv12_and_i420_round_trip_through_padded_strides) {
  constexpr int kW = 6, kH = 4;
  const std::vector<uint8_t> i420 = moving(kW, kH, 3);
  constexpr size_t kYStride = 16, kUvStride = 12;  // wider than the rows, as a pixel buffer is
  std::vector<uint8_t> y(kYStride * kH, 0xEE), uv(kUvStride * (kH / 2), 0xEE);
  gc::i420_to_nv12(i420, kW, kH, y.data(), kYStride, uv.data(), kUvStride);
  // The chroma interleaved U then V, and the padding left alone.
  const uint8_t* u = i420.data() + static_cast<ptrdiff_t>(kW) * kH;
  const uint8_t* v = u + static_cast<ptrdiff_t>(kW / 2) * (kH / 2);
  CHECK(uv[0] == u[0] && uv[1] == v[0] && uv[2] == u[1] && uv[3] == v[1]);
  CHECK(y[kW] == 0xEE && uv[kW] == 0xEE);
  std::vector<uint8_t> back;
  gc::nv12_to_i420(y.data(), kYStride, uv.data(), kUvStride, kW, kH, back);
  CHECK(back == i420);
}

TEST(with_no_platform_encoder_h264_says_so_and_auto_takes_av1) {
  if (platform()) return;
  gc::VideoEncoderOptions o;
  o.width = 64;
  o.height = 48;
  std::string error;
  gc::VideoEncoder h264;
  CHECK(!h264.open("h264", o, error));
  CHECK(error.find("no platform video encoder") != std::string::npos);
  CHECK(error.find("--codec=auto") != std::string::npos);
  if (!gc::encoders_built()) return;
  gc::VideoEncoder any;
  error.clear();
  CHECK_MSG(any.open("auto", o, error), error.c_str());
  CHECK(any.codec() == gc::VideoCodec::kAv1);
  CHECK(any.encoder() == "libaom");
  CHECK(!any.hardware());
}

TEST(a_webm_refuses_what_it_cannot_carry) {
  gc::MatroskaOptions m;
  gc::VideoTrack v;
  v.width = 64;
  v.height = 48;
  v.codec = "V_MPEG4/ISO/AVC";
  v.codec_private = {1, 100, 0, 31, 0xFF, 0xE0, 0};
  m.video = v;
  const fs::path dir = fs::temp_directory_path() / "ge_capture_platform_video_test";
  fs::create_directories(dir);
  gc::Mux mux;
  std::string error;
  CHECK(!mux.open((dir / "run.WebM").string(), gc::Container::kMatroska, m, error));
  CHECK(error.find("name it .mp4 or .mkv") != std::string::npos);
  gc::Mux mkv;
  CHECK_MSG(mkv.open((dir / "run.mkv").string(), gc::Container::kMatroska, m, error),
            error.c_str());
  CHECK(mkv.doc_type() == "matroska");
  CHECK(mkv.close(0, error));
}

TEST(the_platforms_h264_codes_and_decodes_close_to_what_went_in) {
  if (!platform()) return;
  round_trip("h264", gc::VideoCodec::kH264, "H.264");
}

TEST(the_platforms_hevc_codes_and_decodes_close_to_what_went_in) {
  if (!platform()) return;
  round_trip("hevc", gc::VideoCodec::kHevc, "HEVC");
}

TEST(auto_takes_the_platforms_h264_where_there_is_one) {
  if (!platform()) return;
  gc::VideoEncoder e;
  gc::VideoEncoderOptions o;
  o.width = 128;
  o.height = 96;
  std::string error;
  CHECK_MSG(e.open("auto", o, error), error.c_str());
  CHECK(e.codec() == gc::VideoCodec::kH264);
}

TEST(the_sync_fixture_as_an_h264_mp4_decodes_every_picture_with_each_click_on_its_step) {
  if (!platform() || !gc::encoders_built()) return;  // Opus is the engine's own
  constexpr int64_t kSteps = 600;
  constexpr double kFps = 30;
  const fs::path dir = fs::temp_directory_path() / "ge_capture_platform_video_test";
  fs::create_directories(dir);
  gp::RecordOptions o;
  o.view = "flash";
  o.fps = kFps;
  o.band = false;
  o.thumbs = 0;
  o.encoding.format = "mkv";
  o.encoding.codec = "h264";
  CHECK(gp::encoding_problem(o.encoding).empty());
  CHECK(gp::extension_of(o.encoding) == "mp4");
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
  const gc::Mp4Track& vt = f->tracks[0];
  CHECK(vt.entry == "avc1" && vt.config_type == "avcC" && f->tracks[1].entry == "Opus");
  const int64_t skip = f->tracks[1].edit_start;
  gc::VideoDecoder vd;
  gc::OpusDecoder ad;
  CHECK_MSG(vd.open("V_MPEG4/ISO/AVC", vt.config, static_cast<int>(vt.width),
                    static_cast<int>(vt.height), error),
            error.c_str());
  CHECK(ad.open(error));
  std::vector<std::pair<int64_t, bool>> pictures;
  std::vector<float> sound;
  for (const gc::Mp4Sample& s : f->samples) {
    const std::span<const uint8_t> data(bytes.data() + s.offset, s.size);
    if (s.track == 1) {
      std::vector<uint8_t> yuv;
      int w = 0, h = 0;
      bool got = false;
      CHECK_MSG(vd.decode(data, yuv, w, h, got, error), error.c_str());
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
