// LAB `capture-run`: FALL LINE'S RACE EXPORTED WITH ITS SOUND (engine/capture; docs/replays.md §
// Video with sound). The pilot races `--seed`, and `--seconds` of the race from `--from` are
// exported through `record --format=mkv` with the game's own sound (fall_line/sound/play_sound),
// the near view at `--size`. The file is then read back by the engine's reader: a strip of
// pictures decoded from it (I420 back to RGB) over the sound's whole wave, so what a viewer of the
// file sees and hears is what the picture shows. The sound must run exactly as long as the
// pictures and must not be silence; when ffmpeg is installed it decodes the same file and must
// count every picture. Then the same take as AV1 and Opus (`--codec=av1`, a .webm): its size and
// export cost against the raw file's, every picture decoded and measured against the raw one's
// (luma PSNR), and its own strip; measured, so the digest is the same without the encoders.
//
//   make lab LAB=capture-run ARGS="--seed=38 --from=0 --seconds=20 --size=320x180"
//
// ge:summary Lab: Fall Line's race exported with its sound, decoded back as a strip and a wave
// ge:keywords lab capture export mkv sound wave strip decode fall-line race ffmpeg loudness
// ge:keywords av1 opus webm psnr compressed size
// ge:skill replay labs
// ge:test tests/fall_line_sound_test.cpp
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
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
#include "capture/yuv.hpp"
#include "core/libm/libm.hpp"
#include "core/num_text.hpp"
#include "fall_line/sim/play.hpp"
#include "fall_line/sound/play_sound.hpp"
#include "lab/lab.hpp"
#include "lab/raster.hpp"
#include "play/game.hpp"
#include "play/record.hpp"
#include "play/session.hpp"

namespace {

namespace gc = ge::capture;
namespace gl = ge::lab;
namespace gp = ge::play;
namespace num = ge::core::num;
namespace fs = std::filesystem;
using Clock = std::chrono::steady_clock;

#ifdef _WIN32
constexpr const char* kQuiet = " > NUL 2>&1";
#else
constexpr const char* kQuiet = " > /dev/null 2>&1";
#endif

std::vector<uint8_t> read_all(const std::string& path) {
  std::ifstream in(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

double db_of(double amplitude) {
  return amplitude > 0 ? 20 * ge::core::libm::log(amplitude) / ge::core::libm::log(10.0) : -200;
}

struct Shown {
  int64_t ms = 0;
  gl::Picture picture;
};

// The raw file's pictures, decoded from its I420.
std::vector<Shown> raw_pictures(const std::vector<uint8_t>& bytes, const gc::ReadFile& f, int width,
                                int height) {
  std::vector<Shown> out;
  for (const gc::ReadBlock& b : f.blocks) {
    if (b.track != 1 || b.size < gc::i420_bytes(width, height)) continue;
    out.push_back({b.ms, gc::from_i420(bytes.data() + b.offset, width, height)});
  }
  return out;
}

// `count` of `pictures` spread over the take, in a row over the sound's wave (both channels'
// louder side, min and max a column), each picture's time marked on the wave.
gl::Picture strip(const std::vector<Shown>& pictures, const std::vector<float>& wave, int width,
                  int height, int count) {
  constexpr int kGap = 6, kWave = 120, kPad = 8;
  const int tw = std::max(width, 96), th = tw * height / std::max(width, 1);
  const int w = kPad * 2 + count * tw + (count - 1) * kGap;
  gl::Picture p = gl::create_picture(w, kPad * 3 + th + 14 + kWave + 14, {250, 250, 252, 255});
  const gl::Rgba ink{30, 34, 40, 255}, faint{150, 156, 166, 255}, red{205, 40, 40, 255};
  const double total_ms =
      pictures.empty() ? 1 : static_cast<double>(pictures.back().ms - pictures.front().ms) + 1;
  const double start_ms = pictures.empty() ? 0 : static_cast<double>(pictures.front().ms);
  const int wave_y = kPad * 2 + th + 14, mid = wave_y + kWave / 2;
  for (int k = 0; k < count && !pictures.empty(); k++) {
    const size_t i = pictures.size() == 1 ? 0
                                          : static_cast<size_t>(k) * (pictures.size() - 1) /
                                                static_cast<size_t>(std::max(count - 1, 1));
    const Shown& b = pictures[i];
    const int x = kPad + k * (tw + kGap);
    gl::blit(p, gl::resized(b.picture, tw, th), x, kPad);
    gl::draw_text(p, x, kPad + th + 3, num::to_fixed(static_cast<double>(b.ms) / 1000, 1) + " s",
                  ink);
    const double wx = kPad + (static_cast<double>(b.ms) - start_ms) / total_ms * (w - 2 * kPad);
    gl::draw_line(p, wx, wave_y, wx, wave_y + kWave, red);
  }
  gl::draw_line(p, kPad, mid, w - kPad, mid, faint);
  float peak = 1e-6f;
  for (const float s : wave) peak = std::max(peak, std::fabs(s));
  const double scale = (kWave / 2.0 - 2) / static_cast<double>(peak);
  const size_t n = wave.size();
  for (int x = kPad; x < w - kPad && n > 0; x++) {
    const size_t a = static_cast<size_t>(x - kPad) * n / static_cast<size_t>(w - 2 * kPad);
    const size_t z =
        std::max(a + 1, static_cast<size_t>(x + 1 - kPad) * n / static_cast<size_t>(w - 2 * kPad));
    float lo = 0, hi = 0;
    for (size_t i = a; i < z && i < n; i++) {
      lo = std::min(lo, wave[i]);
      hi = std::max(hi, wave[i]);
    }
    gl::draw_line(p, x, mid - hi * scale, x, mid - lo * scale, ink);
  }
  gl::draw_text(p, kPad, wave_y + kWave + 3,
                "the sound, scaled to its peak (" + num::to_fixed(db_of(peak), 1) + " dB)", faint);
  return p;
}

double luma_psnr(const gl::Picture& a, const gl::Picture& b) {
  std::vector<uint8_t> ya, yb;
  gc::to_i420(a, ya);
  gc::to_i420(b, yb);
  const size_t n = static_cast<size_t>(a.width) * static_cast<size_t>(a.height);
  double se = 0;
  for (size_t i = 0; i < n; i++) {
    const double d = static_cast<double>(ya[i]) - static_cast<double>(yb[i]);
    se += d * d;
  }
  if (se == 0) return 99;
  return 10 * ge::core::libm::log(255.0 * 255.0 / (se / static_cast<double>(n))) /
         ge::core::libm::log(10.0);
}

// The same take as AV1 and Opus (`record --format=mkv --codec=av1`, a .webm): its size and cost
// against the raw file's, every picture decoded and measured against the raw file's (luma PSNR),
// and a strip of the decoded pictures over the decoded sound. All measured: an encoder's bytes
// vary with the machine, and a build without the encoders has the same digest.
void compressed(gl::Lab& lab, const gp::InputLog& log, gp::RecordOptions o, int64_t raw_bytes,
                double raw_ms, const std::vector<Shown>& raw) {
  const std::string shot_id = "race-av1";
  const std::string title = "THE SAME TAKE AS AV1 AND OPUS, DECODED BACK";
  const std::string about =
      "The take exported again with --codec=av1 (AV1 and Opus in a .webm, the engine's own "
      "encoders) and decoded back by the engine: the same moments as above, the sound under them "
      "decoded from Opus.";
  const std::string look =
      "The same pictures as the uncompressed strip with no blocks or smear "
      "you notice at this size, and the same wave.";
  if (!gc::encoders_built()) {
    lab.note("this build has no encoders (GE_ENCODERS is OFF): the AV1 take is skipped");
    gl::Picture blank = gl::create_picture(480, 60, {250, 250, 252, 255});
    gl::draw_text(blank, 8, 24, "NO ENCODERS IN THIS BUILD", {30, 34, 40, 255});
    lab.shot({shot_id, title, about, look, {}}, blank, gl::Det::measured);
    return;
  }
  o.encoding.codec = "av1";
  o.video = fs::path(o.video).replace_extension(".webm").string();
  std::string error;
  const auto t0 = Clock::now();
  const auto rec = gp::record(fall_line::sim::kGame, log, o, error);
  const double ms = std::chrono::duration<double, std::milli>(Clock::now() - t0).count();
  if (!rec) {
    lab.fail("the AV1 export failed: " + error,
             "Run tests/capture_codecs_test.cpp (skill: replay).");
    return;
  }
  const std::vector<uint8_t> bytes = read_all(o.video);
  const auto f = gc::read_mkv(bytes, error);
  gc::Av1Decoder vd;
  gc::OpusDecoder ad;
  if (!f || f->tracks.size() != 2 || !vd.open(error) || !ad.open(error)) {
    lab.fail("the AV1 file did not read back: " + error,
             "Run tests/capture_codecs_test.cpp (skill: replay).");
    return;
  }
  std::vector<Shown> pictures;
  std::vector<float> stereo;
  for (const gc::ReadBlock& b : f->blocks) {
    const std::span<const uint8_t> data(bytes.data() + b.offset, b.size);
    if (b.track == 1) {
      std::vector<uint8_t> yuv;
      int w = 0, h = 0;
      bool got = false;
      if (!vd.decode(data, yuv, w, h, got, error)) break;
      if (got) pictures.push_back({b.ms, gc::from_i420(yuv.data(), w, h)});
    } else if (!ad.decode(data, stereo, error)) {
      break;
    }
  }
  const auto skip = static_cast<size_t>(
      std::llround(static_cast<double>(f->tracks[1].codec_delay_ns) * gc::kOpusRate / 1e9));
  std::vector<float> wave;
  for (size_t i = skip; 2 * i + 1 < stereo.size(); i++) {
    const float l = stereo[2 * i], r = stereo[2 * i + 1];
    wave.push_back(std::fabs(l) >= std::fabs(r) ? l : r);
  }
  double psnr = 0, worst = 99;
  const size_t n = std::min(pictures.size(), raw.size());
  for (size_t i = 0; i < n; i++) {
    const double d = luma_psnr(raw[i].picture, pictures[i].picture);
    psnr += d;
    worst = std::min(worst, d);
  }
  psnr /= static_cast<double>(std::max<size_t>(n, 1));
  const double minutes = static_cast<double>(rec->frames) / o.fps / 60;
  const double frames = static_cast<double>(std::max<int64_t>(rec->frames, 1));
  lab.metric("av1.pictures-decoded", static_cast<double>(pictures.size()), "count",
             gl::Det::measured);
  lab.metric("av1.mb", static_cast<double>(rec->bytes) / 1e6, "MB", gl::Det::measured);
  lab.metric("av1.mb-per-minute", static_cast<double>(rec->bytes) / 1e6 / minutes, "MB",
             gl::Det::measured);
  lab.metric("av1.smaller-by",
             static_cast<double>(raw_bytes) / std::max<double>(1, static_cast<double>(rec->bytes)),
             "x", gl::Det::measured);
  lab.metric("av1.export-ms-per-frame", ms / frames, "ms", gl::Det::measured);
  lab.metric("av1.psnr-db", psnr, "dB", gl::Det::measured);
  lab.metric("av1.worst-psnr-db", worst, "dB", gl::Det::measured);
  lab.table({"The take uncompressed and as AV1 and Opus: size, MB a minute, export a frame, "
             "luma PSNR against the uncompressed pictures (mean, worst)",
             {"file", "MB", "MB/min", "export-ms", "psnr-db", "worst-db"},
             {{"mkv (I420, float)", num::to_fixed(static_cast<double>(raw_bytes) / 1e6, 2),
               num::to_fixed(static_cast<double>(raw_bytes) / 1e6 / minutes, 1),
               num::to_fixed(raw_ms / frames, 1), "-", "-"},
              {"webm (AV1, Opus)", num::to_fixed(static_cast<double>(rec->bytes) / 1e6, 2),
               num::to_fixed(static_cast<double>(rec->bytes) / 1e6 / minutes, 1),
               num::to_fixed(ms / frames, 1), num::to_fixed(psnr, 1), num::to_fixed(worst, 1)}}},
            gl::Det::measured);
  if (static_cast<int64_t>(pictures.size()) != rec->frames || worst < 25) {
    lab.fail("the AV1 file decoded to " + std::to_string(pictures.size()) + " of " +
                 std::to_string(rec->frames) + " pictures, the worst at " +
                 num::to_fixed(worst, 1) + " dB",
             "Run tests/capture_codecs_test.cpp; lower --crf (skill: replay).");
  }
  lab.shot({shot_id, title, about, look, {}}, strip(pictures, wave, o.width, o.height, 6),
           gl::Det::measured);
}

void run(gl::Lab& lab) {
  namespace fl = fall_line::sim;
  const double seed = lab.arg_num("seed"), from = lab.arg_num("from"),
               seconds = lab.arg_num("seconds"), fps = lab.arg_num("fps");
  const std::string size = lab.arg("size");
  const size_t x = size.find('x');
  const int width = x == std::string::npos ? 0 : std::atoi(size.substr(0, x).c_str()) & ~1;
  const int height = x == std::string::npos ? 0 : std::atoi(size.substr(x + 1).c_str()) & ~1;
  if (width < 2 || height < 2) {
    lab.fail("--size must be WxH, not " + lab.arg("size"), "Pass --size=320x180 (skill: labs).");
    return;
  }
  gp::Setup setup;
  std::string error;
  if (!gp::make_setup(fl::kGame, seed, {{"rivals", lab.arg("rivals")}}, setup, error)) {
    lab.fail(error, "Pass --rivals as a number (skill: sim).");
    return;
  }
  const auto played = gp::pilot_run(fl::kGame, setup, error);
  if (!played) {
    lab.fail("the pilot's run of seed " + num::to_string(seed) + " failed: " + error,
             "Run make sim SEEDS=" + num::to_string(seed) + " (skill: sim).");
    return;
  }

  const fs::path dir = fs::temp_directory_path() / "ge-capture-run-lab";
  std::error_code ec;
  fs::create_directories(dir, ec);
  gp::RecordOptions o;
  o.view = "near";
  o.fps = fps;
  o.width = width;
  o.height = height;
  o.from = static_cast<int64_t>(std::llround(from * fl::kGame.hz));
  o.to = o.from + static_cast<int64_t>(std::llround(seconds * fl::kGame.hz));
  o.thumbs = 0;
  o.encoding.format = "mkv";
  o.sound = &fall_line::sound::kSound;
  o.video = (dir / ("fall_line-" + num::to_string(seed) + ".mkv")).string();
  const auto t0 = Clock::now();
  const auto rec = gp::record(fl::kGame, played->log, o, error);
  const double export_ms = std::chrono::duration<double, std::milli>(Clock::now() - t0).count();
  if (!rec) {
    lab.fail("the export failed: " + error,
             "Run tests/capture_av_export_test.cpp and tests/fall_line_sound_test.cpp (skill: "
             "replay).");
    return;
  }
  const std::vector<uint8_t> bytes = read_all(o.video);
  const auto f = gc::read_mkv(bytes, error);
  if (!f) {
    lab.fail("the engine's reader refused its own file: " + error,
             "Run tests/capture_matroska_test.cpp (skill: replay).");
    return;
  }

  // The sound read back: the louder channel a sample, its peak and its RMS.
  std::vector<float> wave;
  double sum = 0;
  float peak = 0;
  int64_t pictures = 0;
  for (const gc::ReadBlock& b : f->blocks) {
    if (b.track == 1) {
      pictures++;
      continue;
    }
    const size_t n = b.size / (2 * sizeof(float));
    for (size_t i = 0; i < n; i++) {
      float lr[2] = {};
      std::memcpy(lr, bytes.data() + b.offset + 2 * i * sizeof(float), sizeof lr);
      const float s = std::fabs(lr[0]) >= std::fabs(lr[1]) ? lr[0] : lr[1];
      wave.push_back(s);
      sum += static_cast<double>(s) * s;
      peak = std::max(peak, std::fabs(s));
    }
  }
  const double rms = wave.empty() ? 0 : std::sqrt(sum / static_cast<double>(wave.size()));
  const int64_t expected = gc::frame_sample(rec->frames, fps, ge::audio::kRate);

  lab.metric("frames", static_cast<double>(rec->frames), "count");
  lab.metric("pictures-read", static_cast<double>(pictures), "count");
  lab.metric("samples", static_cast<double>(rec->samples), "count");
  lab.metric("samples-read", static_cast<double>(wave.size()), "count");
  lab.metric("mb", static_cast<double>(rec->bytes) / 1e6, "MB");
  lab.metric("peak-db", db_of(peak), "dB");
  lab.metric("rms-db", db_of(rms), "dB");
  lab.metric("export-ms-per-frame",
             export_ms / static_cast<double>(std::max<int64_t>(rec->frames, 1)), "ms",
             gl::Det::measured);
  if (pictures != rec->frames || rec->samples != expected ||
      static_cast<int64_t>(wave.size()) != rec->samples) {
    lab.fail("the file holds " + std::to_string(pictures) + " pictures and " +
                 std::to_string(wave.size()) + " samples for " + std::to_string(rec->frames) +
                 " frames (" + std::to_string(expected) + " samples due)",
             "Read av_export.cpp's frame and close (skill: replay).");
  }
  if (db_of(peak) < -60) {
    lab.fail("the race's sound is silence (peak " + num::to_fixed(db_of(peak), 1) + " dB)",
             "Run tests/fall_line_sound_test.cpp; read play_sound.cpp's mix (skill: audio).");
  }

  const bool ffmpeg = std::system((std::string("ffprobe -version") + kQuiet).c_str()) == 0;
  if (ffmpeg) {
    const std::string cmd =
        "ffprobe -v error -count_frames -select_streams v:0 -show_entries "
        "stream=nb_read_frames -of csv=p=0 \"" +
        o.video + "\"";
#ifdef _WIN32
    std::FILE* pipe = _popen(cmd.c_str(), "r");
#else
    std::FILE* pipe = popen(cmd.c_str(), "r");  // NOLINT(cert-env33-c): the decoder is the point
#endif
    char line[64] = {};
    const bool got = pipe && std::fgets(line, sizeof line, pipe) != nullptr;
    if (pipe) {
#ifdef _WIN32
      _pclose(pipe);
#else
      pclose(pipe);
#endif
    }
    const double read = got ? static_cast<double>(std::atoll(line)) : -1;
    lab.metric("ffmpeg-frames", read, "count", gl::Det::measured);
    if (read != static_cast<double>(rec->frames)) {
      lab.fail("ffmpeg read " + num::to_string(read) + " of " + std::to_string(rec->frames) +
                   " pictures",
               "Compare the muxer's elements with ffprobe -show_packets (skill: replay).");
    }
  }
  lab.note(ffmpeg ? "ffprobe found: it counted the file's pictures too"
                  : "ffprobe not found: the file is read by the engine's reader alone");
  lab.note("the file: " + o.video);

  lab.shot({"race",
            "SEED " + num::to_string(seed) + "'S RACE, READ BACK FROM ITS FILE",
            "The pilot's race exported with the game's sound (record --format=mkv) and read back "
            "by the engine's reader: pictures decoded from the file's I420 spread over the take, "
            "each one's time a red line on the sound's wave under them.",
            "Pictures that follow the run (the band's clock rising), and a wave that is never "
            "flat: the wind and the edges louder as the skier gathers speed.",
            {{"seed", num::to_string(seed)},
             {"from", num::to_string(from)},
             {"seconds", num::to_string(seconds)},
             {"fps", num::to_string(fps)},
             {"size", lab.arg("size")}}},
           strip(raw_pictures(bytes, *f, width, height), wave, width, height, 6));

  compressed(lab, played->log, o, rec->bytes, export_ms, raw_pictures(bytes, *f, width, height));
}

const gl::LabSpec kSpec{
    "capture-run",
    gl::LabKind::scenario,
    "game:fall_line",
    "engine/capture, engine/play/record.cpp, games/fall_line/sound/play_sound.cpp",
    "replay",
    "Does Fall Line's race export with its own sound, and does the file hold every picture and "
    "every sample of it?",
    {{"seed", "38", "the race's seed"},
     {"rivals", "0", "bots on the grid beside the pilot"},
     {"from", "0", "where the take starts, s into the run"},
     {"seconds", "20", "how long the take is"},
     {"fps", "30", "the frame rate"},
     {"size", "320x180", "the pictures' size, WxH"}},
    "--from=8 --seconds=1 --fps=12 --size=96x54",
    run,
};
GE_LAB(kSpec);

}  // namespace
