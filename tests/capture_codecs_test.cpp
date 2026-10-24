// The export's encoders (engine/capture/codecs.hpp) and the compressed export (av_export.hpp with
// codec av1): the av1C record reads the profile and level off a sequence header; AV1 decodes back
// to the pictures it was given above 30 dB, starts on a keyframe and keys again on its interval;
// Opus decodes back to its tone above 20 dB once its pre-skip is dropped, its packets 20 ms apart;
// the sync fixture exported as a .webm keeps each click on its step and each flash within a frame,
// its blocks in time order; a build without the encoders says so.
//
// ge:summary Tests: AV1 and Opus round trips, the av1C record, the compressed export in sync
// ge:keywords test av1 opus codec encode decode psnr snr av1c webm export sync interleave
// ge:skill replay testing
// ge:test tests/capture_codecs_test.cpp
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
#include "capture/mkv_read.hpp"
#include "capture/sync_fixture.hpp"
#include "capture/yuv.hpp"
#include "check.hpp"
#include "play/record.hpp"

namespace gc = ge::capture;
namespace gp = ge::play;
namespace fs = std::filesystem;

namespace {

constexpr int kW = 96, kH = 64;

// A picture that moves: a diagonal gradient sliding right and a bright square crossing it.
std::vector<uint8_t> moving(int k) {
  std::vector<uint8_t> p(gc::i420_bytes(kW, kH));
  for (int y = 0; y < kH; y++) {
    for (int x = 0; x < kW; x++) {
      const bool square = x >= 10 + 2 * k && x < 30 + 2 * k && y >= 20 && y < 40;
      p[static_cast<size_t>(y * kW + x)] =
          static_cast<uint8_t>(square ? 230 : 16 + (x + y + 3 * k) % 200);
    }
  }
  for (size_t i = static_cast<size_t>(kW) * kH; i < p.size(); i++) p[i] = 128;
  return p;
}

double luma_psnr(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b) {
  double se = 0;
  const size_t n = static_cast<size_t>(kW) * kH;
  for (size_t i = 0; i < n; i++) {
    const double d = static_cast<double>(a[i]) - static_cast<double>(b[i]);
    se += d * d;
  }
  if (se == 0) return 99;
  return 10.0 * std::log10(255.0 * 255.0 / (se / static_cast<double>(n)));
}

std::vector<uint8_t> read_all(const std::string& path) {
  std::ifstream in(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

}  // namespace

TEST(the_av1c_record_reads_the_profile_and_level_off_the_sequence_header) {
  // A sequence header OBU (type 1, with a size): profile 0, not still, not reduced, no timing
  // info, no display delay, one operating point with idc 0 and level 8 (4.0), tier 1.
  // Bits: 000 0 0 0 0 00000 000000000000 01000 1 ...
  const std::vector<uint8_t> seq = {0x0A, 0x04, 0x00, 0x00, 0x00, 0x46};
  const std::vector<uint8_t> c = gc::av1_codec_private(seq);
  CHECK(c.size() == 4 + seq.size());
  CHECK(c[0] == 0x81);
  CHECK(c[1] == 8);              // profile 0, level 8
  CHECK(c[2] == (0x80 | 0x0C));  // tier 1, 4:2:0
  CHECK(c[3] == 0);
  CHECK(c[4] == 0x0A && c[9] == 0x46);
  // Timing info first: the level is left to the stream (31).
  const std::vector<uint8_t> timed = {0x0A, 0x01, 0x04};
  CHECK((gc::av1_codec_private(timed)[1] & 0x1F) == 31);
}

TEST(without_the_encoders_every_open_says_how_to_get_them) {
  if (gc::encoders_built()) return;
  std::string error;
  gc::Av1Encoder e;
  CHECK(!e.open({kW, kH}, error));
  CHECK(error.find("GE_ENCODERS") != std::string::npos);
}

TEST(av1_decodes_back_to_its_pictures_above_30_db_and_keys_on_its_interval) {
  if (!gc::encoders_built()) return;
  std::string error;
  gc::Av1Encoder enc;
  gc::Av1Options o;
  o.width = kW;
  o.height = kH;
  o.fps = 30;
  o.crf = 24;
  o.key_seconds = 1;
  CHECK_MSG(enc.open(o, error), error.c_str());
  CHECK(!enc.sequence_header().empty());
  std::vector<gc::Packet> packets;
  constexpr int kFrames = 45;
  for (int k = 0; k < kFrames; k++) {
    CHECK_MSG(enc.encode(moving(k), gc::frame_ms(k, 30), packets, error), error.c_str());
  }
  CHECK_MSG(enc.flush(packets, error), error.c_str());
  CHECK(packets.size() == kFrames);
  if (packets.size() != kFrames) return;
  CHECK(packets[0].key);
  CHECK(packets[0].ms == 0 && packets[1].ms == 33);
  int keys = 0;
  for (const gc::Packet& p : packets) keys += p.key ? 1 : 0;
  CHECK(keys >= 2 && keys <= 3);  // at 0, then once a second
  // No temporal delimiter left at a packet's start (OBU type 2).
  CHECK(((packets[0].data[0] >> 3) & 0x0F) != 2);
  gc::Av1Decoder dec;
  CHECK_MSG(dec.open(error), error.c_str());
  double worst = 99;
  for (int k = 0; k < kFrames; k++) {
    std::vector<uint8_t> got;
    int w = 0, h = 0;
    bool any = false;
    CHECK_MSG(dec.decode(packets[static_cast<size_t>(k)].data, got, w, h, any, error),
              error.c_str());
    CHECK(any && w == kW && h == kH);
    if (any) worst = std::fmin(worst, luma_psnr(moving(k), got));
  }
  CHECK_MSG(worst > 30, ("worst luma PSNR " + std::to_string(worst)).c_str());
}

TEST(opus_decodes_back_to_its_tone_above_20_db_once_its_preskip_is_dropped) {
  if (!gc::encoders_built()) return;
  std::string error;
  gc::OpusEncoder enc;
  CHECK_MSG(enc.open(128000, error), error.c_str());
  CHECK(enc.preskip() > 0 && enc.preskip() < 1000);
  const std::vector<uint8_t> head = enc.codec_private();
  CHECK(head.size() == 19 && head[9] == 2);
  CHECK((head[10] | head[11] << 8) == enc.preskip());
  constexpr int kN = gc::kOpusRate;  // one second
  std::vector<float> tone(2 * static_cast<size_t>(kN));
  for (int i = 0; i < kN; i++) {
    const auto s = static_cast<float>(0.5 * std::sin(2 * 3.141592653589793 * 440.0 * i / kN));
    tone[2 * static_cast<size_t>(i)] = s;
    tone[2 * static_cast<size_t>(i) + 1] = s;
  }
  std::vector<gc::Packet> packets;
  // In uneven pieces, as an export hands them over.
  size_t at = 0;
  for (const size_t piece : {700u, 1600u, 37u, 20000u}) {
    CHECK(enc.push(tone.data() + 2 * at, piece, packets, error));
    at += piece;
  }
  CHECK(enc.push(tone.data() + 2 * at, static_cast<size_t>(kN) - at, packets, error));
  CHECK_MSG(enc.finish(packets, error), error.c_str());
  CHECK(static_cast<int64_t>(packets.size()) * gc::kOpusPacketSamples >= kN + enc.preskip());
  for (size_t i = 0; i < packets.size(); i++) CHECK(packets[i].ms == static_cast<int64_t>(i) * 20);
  gc::OpusDecoder dec;
  CHECK_MSG(dec.open(error), error.c_str());
  std::vector<float> out;
  for (const gc::Packet& p : packets) CHECK(dec.decode(p.data, out, error));
  const auto skip = static_cast<size_t>(enc.preskip());
  CHECK(out.size() / 2 >= skip + kN);
  if (out.size() / 2 < skip + kN) return;
  double signal = 0, noise = 0;
  for (size_t i = 4800; i < static_cast<size_t>(kN) - 4800; i++) {  // the middle, past the edges
    const double want = tone[2 * i], got = out[2 * (i + skip)];
    signal += want * want;
    noise += (want - got) * (want - got);
  }
  const double snr = 10.0 * std::log10(signal / std::fmax(noise, 1e-12));
  CHECK_MSG(snr > 20, ("Opus SNR " + std::to_string(snr) + " dB").c_str());
}

TEST(the_sync_fixture_as_webm_keeps_each_click_on_its_step_and_each_flash_within_a_frame) {
  if (!gc::encoders_built()) return;
  constexpr int64_t kSteps = 600;
  constexpr double kFps = 30;
  const fs::path dir = fs::temp_directory_path() / "ge_capture_codecs_test";
  fs::create_directories(dir);
  gp::RecordOptions o;
  o.view = "flash";
  o.fps = kFps;
  o.band = false;
  o.thumbs = 0;
  o.encoding.format = "mkv";
  o.encoding.codec = "av1";
  o.sound = &gc::kSyncSound;
  o.video = (dir / "sync.webm").string();
  std::string error;
  CHECK_MSG(gp::record(gc::kSyncGame, gc::sync_log(kSteps), o, error).has_value(), error.c_str());
  const std::vector<uint8_t> bytes = read_all(o.video);
  const auto f = gc::read_mkv(bytes, error);
  CHECK_MSG(f.has_value(), error.c_str());
  if (!f) return;
  CHECK(f->doc_type == "webm");
  CHECK(f->tracks.size() == 2);
  if (f->tracks.size() != 2) return;
  CHECK(f->tracks[0].codec == "V_AV1" && f->tracks[0].codec_private.size() > 4);
  CHECK(f->tracks[1].codec == "A_OPUS" && f->tracks[1].codec_private.size() == 19);
  CHECK(f->tracks[1].codec_delay_ns > 0 && f->tracks[1].seek_pre_roll_ns == 80'000'000);
  CHECK(f->tracks[1].bits == 0);
  // Every block in time order across both tracks.
  int64_t last = -1;
  for (const gc::ReadBlock& b : f->blocks) {
    CHECK(b.ms >= last);
    last = b.ms;
  }
  // Decode both tracks.
  gc::Av1Decoder vd;
  gc::OpusDecoder ad;
  CHECK(vd.open(error) && ad.open(error));
  std::vector<std::pair<int64_t, bool>> pictures;
  std::vector<float> sound;
  for (const gc::ReadBlock& b : f->blocks) {
    const std::span<const uint8_t> data(bytes.data() + b.offset, b.size);
    if (b.track == 1) {
      std::vector<uint8_t> yuv;
      int w = 0, h = 0;
      bool got = false;
      CHECK(vd.decode(data, yuv, w, h, got, error));
      if (got) pictures.emplace_back(b.ms, yuv[static_cast<size_t>(w) * (h / 2) + w / 2] > 128);
    } else {
      CHECK(ad.decode(data, sound, error));
    }
  }
  CHECK(pictures.size() == 151);
  const auto skip = static_cast<size_t>(
      std::llround(static_cast<double>(f->tracks[1].codec_delay_ns) * gc::kOpusRate / 1e9));
  int clicks = 0;
  size_t from = skip;
  for (int64_t step = 1; step <= kSteps; step++) {
    if (!gc::sync_clicks_at(step, 150, 37)) continue;
    clicks++;
    const int64_t due = gc::step_sample(step, 0, 120, ge::audio::kRate);
    size_t loud = from;
    while (loud < sound.size() / 2 && std::fabs(sound[2 * loud]) < 0.05F) loud++;
    from = loud + static_cast<size_t>(ge::audio::kRate / 10);
    const auto played = static_cast<int64_t>(loud - skip);
    // Opus smears a click's onset by a few milliseconds either way; never more than 3.
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
