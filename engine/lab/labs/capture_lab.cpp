// LAB `capture`: IS AN EXPORT'S SOUND IN SYNC WITH ITS PICTURES? (engine/capture; docs/replays.md
// § Video with sound). The sync fixture (capture/sync_fixture.hpp) flashes and clicks on the same
// steps; it is exported through `record --format=mkv` at each frame rate of `--fps`, and the
// engine's reader takes the file apart: every click's first sound against its step's sample (it
// must be on it), and the first bright picture after it (it must follow by less than a frame).
// When ffmpeg is installed it decodes the same file: every picture and every sample must come out
// (its numbers are measured, since a machine may not have it).
//
// The cost of the uncompressed path is measured too: a 1280x720 picture to I420 and into the
// muxer, a frame, and the bytes a minute of it takes. Then the COMPRESSED export (AV1 and Opus,
// capture/codecs.hpp): the same sync check through the decoders, with ffmpeg decoding the .webm,
// the same again in the engine's fragmented MP4 (and that file cut short, read to its last
// whole fragment), and what the AV1 encoder costs at 720p and 1080p at three qualities. Last the
// PLATFORM'S OWN ENCODERS (capture/platform_video.hpp: VideoToolbox's H.264 and HEVC on a Mac):
// which encoder coded, the sync fixture as an .mp4 through them and back, and a 720p frame's cost.
// Those numbers are all measured, so a build without the encoders (GE_ENCODERS=OFF) or a machine
// without a platform encoder has the same digest.
//
//   make lab LAB=capture ARGS="--seconds=30 --fps=24,25,30,60"
//
// ge:summary Lab: an export's sound against its pictures, a click and a flash on one step
// ge:keywords lab capture export sync flash click lag mkv ffmpeg decode i420 cost av1 opus webm
// ge:keywords encoder-cost psnr crf h264 hevc videotoolbox platform-encoder hardware-encoder
// ge:skill replay labs
// ge:test tests/capture_av_export_test.cpp
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
#include "capture/matroska.hpp"
#include "capture/mkv_read.hpp"
#include "capture/mp4_read.hpp"
#include "capture/platform_video.hpp"
#include "capture/sync_fixture.hpp"
#include "capture/video.hpp"
#include "capture/yuv.hpp"
#include "core/libm/libm.hpp"
#include "core/num_text.hpp"
#include "lab/lab.hpp"
#include "lab/raster.hpp"
#include "play/record.hpp"

namespace {

namespace gc = ge::capture;
namespace gl = ge::lab;
namespace gp = ge::play;
namespace num = ge::core::num;
namespace fs = std::filesystem;
using Clock = std::chrono::steady_clock;

constexpr int64_t kPeriod = 150, kFirst = 37;

struct Click {
  int64_t step = 0;
  double due_ms = 0;    // its step's sample, as time
  double sound_ms = 0;  // its first sound in the file
  double lit_ms = -1;   // the first bright picture at or after it
};

struct Take {
  double fps = 0;
  int64_t frames = 0, samples = 0, bytes = 0;
  std::vector<Click> clicks;
  std::vector<std::pair<int64_t, bool>> pictures;  // ms, lit
  std::vector<float> left;                         // the left channel
  bool ffmpeg_decodes = false;
  int64_t ffmpeg_frames = -1;
  std::string error;
};

std::vector<uint8_t> read_all(const std::string& path) {
  std::ifstream in(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

// The shell's sink for a command's output.
#ifdef _WIN32
constexpr const char* kQuiet = " > NUL 2>&1";
#else
constexpr const char* kQuiet = " > /dev/null 2>&1";
#endif

bool tool_found(const char* tool) {
  return std::system((std::string(tool) + " -version" + kQuiet).c_str()) == 0;
}

// ffprobe's count of the video's frames in `path`, or -1.
int64_t probe_frames(const std::string& path) {
  const std::string cmd =
      "ffprobe -v error -count_frames -select_streams v:0 -show_entries "
      "stream=nb_read_frames -of csv=p=0 \"" +
      path + "\"";
#ifdef _WIN32
  std::FILE* p = _popen(cmd.c_str(), "r");
#else
  std::FILE* p = popen(cmd.c_str(), "r");  // NOLINT(cert-env33-c): the decoder is the point
#endif
  if (!p) return -1;
  char line[64] = {};
  const bool got = std::fgets(line, sizeof line, p) != nullptr;
#ifdef _WIN32
  _pclose(p);
#else
  pclose(p);
#endif
  return got ? std::atoll(line) : -1;
}

// A file's blocks, whichever container: the track (1 the pictures, 2 the sound), the time and
// the bytes; and the samples a player drops from the sound's start.
struct Block {
  uint64_t track = 0;
  int64_t ms = 0;
  size_t offset = 0, size = 0;
};

// The video track as a decoder opens it: its Matroska codec id, codec private and size.
struct VideoInfo {
  std::string codec;
  std::vector<uint8_t> codec_private;
  int width = 0, height = 0;
};

std::string matroska_id_of(const std::string& entry) {
  if (entry == "avc1") return "V_MPEG4/ISO/AVC";
  if (entry == "hvc1") return "V_MPEGH/ISO/HEVC";
  return entry == "av01" ? "V_AV1" : entry;
}

bool blocks_of(const std::vector<uint8_t>& bytes, bool mp4, std::vector<Block>& out, size_t& skip,
               VideoInfo& video, std::string& error) {
  skip = 0;
  if (mp4) {
    const auto f = gc::read_mp4(bytes, error);
    if (!f) return false;
    for (const gc::Mp4Sample& s : f->samples) out.push_back({s.track, s.ms, s.offset, s.size});
    if (!f->tracks.empty()) {
      const gc::Mp4Track& v = f->tracks[0];
      video = {matroska_id_of(v.entry), v.config, static_cast<int>(v.width),
               static_cast<int>(v.height)};
    }
    // The pre-skip is the sound track's edit list.
    if (f->tracks.size() == 2) skip = static_cast<size_t>(f->tracks[1].edit_start);
    return true;
  }
  const auto f = gc::read_mkv(bytes, error);
  if (!f) return false;
  for (const gc::ReadBlock& b : f->blocks) out.push_back({b.track, b.ms, b.offset, b.size});
  if (!f->tracks.empty()) {
    const gc::ReadTrack& v = f->tracks[0];
    video = {v.codec, v.codec_private, static_cast<int>(v.width), static_cast<int>(v.height)};
  }
  if (f->tracks.size() == 2) {
    // The track's CodecDelay, as a player drops it.
    skip = static_cast<size_t>(
        std::llround(static_cast<double>(f->tracks[1].codec_delay_ns) * gc::kOpusRate / 1e9));
  }
  return true;
}

// The sync fixture exported at `fps`, uncompressed (`codec` "") or coded (capture/video.hpp's word,
// with Opus) in a .webm or (`mp4`) the engine's fragmented MP4, and read back: the raw file by its
// bytes, the coded one through the decoders its tracks name.
Take take(double fps, int64_t steps, const fs::path& dir, bool ffmpeg, const std::string& codec,
          bool mp4 = false) {
  Take t;
  t.fps = fps;
  gp::RecordOptions o;
  o.view = "flash";
  o.fps = fps;
  o.band = false;
  o.thumbs = 0;
  o.encoding.format = "mkv";
  o.encoding.codec = codec;
  o.sound = &gc::kSyncSound;
  o.video = (dir / ("sync-" + (codec.empty() || codec == "av1" ? "" : codec + "-") +
                    num::to_string(fps) + "fps" +
                    (codec.empty() ? ".mkv"
                     : mp4         ? ".mp4"
                                   : ".webm")))
                .string();
  const auto rec = gp::record(gc::kSyncGame, gc::sync_log(steps, kPeriod, kFirst), o, t.error);
  if (!rec) return t;
  const std::vector<uint8_t> bytes = read_all(o.video);
  std::vector<Block> blocks;
  size_t skip = 0;
  VideoInfo info;
  if (!blocks_of(bytes, mp4, blocks, skip, info, t.error)) return t;
  t.frames = rec->frames;
  t.samples = rec->samples;
  t.bytes = rec->bytes;
  const bool coded = !codec.empty();
  gc::VideoDecoder vd;
  gc::OpusDecoder ad;
  std::vector<float> stereo;
  if (coded && (!vd.open(info.codec, info.codec_private, info.width, info.height, t.error) ||
                !ad.open(t.error))) {
    return t;
  }
  for (const Block& b : blocks) {
    const std::span<const uint8_t> data(bytes.data() + b.offset, b.size);
    if (coded && b.track == 1) {
      std::vector<uint8_t> yuv;
      int w = 0, h = 0;
      bool got = false;
      if (!vd.decode(data, yuv, w, h, got, t.error)) return t;
      // The middle pixel's luma: the flash fills the picture.
      if (got) t.pictures.emplace_back(b.ms, yuv[static_cast<size_t>(w) * (h / 2) + w / 2] > 128);
    } else if (coded) {
      if (!ad.decode(data, stereo, t.error)) return t;
    } else if (b.track == 1) {
      t.pictures.emplace_back(b.ms, bytes[b.offset] > 128);
    } else {
      const size_t n = b.size / (2 * sizeof(float));
      for (size_t i = 0; i < n; i++) {
        float l = 0;
        std::memcpy(&l, bytes.data() + b.offset + 2 * i * sizeof(float), sizeof(float));
        t.left.push_back(l);
      }
    }
  }
  if (coded) {
    // The decoded sound with the encoder's delay dropped, as a player drops it.
    for (size_t i = skip; 2 * i + 1 < stereo.size(); i++) t.left.push_back(stereo[2 * i]);
  }
  constexpr double kRate = ge::audio::kRate;
  // Silence is exactly 0 in the raw file; Opus leaves a faint floor and smears a click's onset.
  const float loud = coded ? 0.05F : 1e-6F;
  const double early_ms = coded ? 3 : 1;
  size_t from = 0;
  for (int64_t s = 1; s <= steps; s++) {
    if (!gc::sync_clicks_at(s, kPeriod, kFirst)) continue;
    Click c;
    c.step = s;
    c.due_ms = 1000.0 *
               static_cast<double>(gc::step_sample(s, 0, gc::kSyncGame.hz, ge::audio::kRate)) /
               kRate;
    size_t i = from;
    while (i < t.left.size() && std::fabs(t.left[i]) < loud) i++;
    if (i >= t.left.size()) break;  // the last click past the last picture's sound
    c.sound_ms = 1000.0 * static_cast<double>(i) / kRate;
    from = i + static_cast<size_t>(kRate / 10);
    for (const auto& [ms, lit] : t.pictures) {
      if (lit && static_cast<double>(ms) + early_ms >= c.sound_ms) {
        c.lit_ms = static_cast<double>(ms);
        break;
      }
    }
    t.clicks.push_back(c);
  }
  if (ffmpeg) {
    t.ffmpeg_decodes =
        std::system(("ffmpeg -v error -i \"" + o.video + "\" -f null -" + kQuiet).c_str()) == 0;
    t.ffmpeg_frames = probe_frames(o.video);
  }
  return t;
}

// Each frame rate's first click, 120 ms either side: the pictures as boxes on the time line
// (white when lit), the sound's wave under them, the click's step as a red line.
gl::Picture strips(const std::vector<Take>& takes, const std::string& title) {
  constexpr int kW = 960, kRow = 92, kHead = 24, kLeft = 90;
  constexpr double kSpan = 240;  // ms across
  gl::Picture p = gl::create_picture(kW, kHead + kRow * static_cast<int>(takes.size()) + 20,
                                     {250, 250, 252, 255});
  const gl::Rgba ink{30, 34, 40, 255}, faint{150, 156, 166, 255}, red{205, 40, 40, 255};
  gl::draw_text(p, 8, 8, title, ink);
  const double px = (kW - kLeft - 16) / kSpan;
  for (size_t r = 0; r < takes.size(); r++) {
    const Take& t = takes[r];
    if (t.clicks.empty()) continue;
    const Click& c = t.clicks.front();
    const double t0 = c.due_ms - kSpan / 2;
    const int y0 = kHead + kRow * static_cast<int>(r);
    gl::draw_text(p, 8, y0 + 18, num::to_string(t.fps) + " FPS", ink, 2);
    const auto x_of = [&](double ms) { return kLeft + (ms - t0) * px; };
    const double frame_ms = 1000.0 / t.fps;
    for (const auto& [ms, lit] : t.pictures) {
      const double m = static_cast<double>(ms);
      if (m + frame_ms < t0 || m > t0 + kSpan) continue;
      const double x = std::max<double>(kLeft, x_of(m));
      const double w = std::min<double>(x_of(m + frame_ms), kW - 16) - x;
      gl::fill_rect(p, x, y0 + 4, w - 1, 30,
                    lit ? gl::Rgba{255, 255, 255, 255} : gl::Rgba{44, 48, 56, 255});
      gl::draw_line(p, x, y0 + 4, x, y0 + 34, faint);
    }
    gl::draw_line(p, kLeft, y0 + 34, kW - 16, y0 + 34, faint);
    const double mid = y0 + 60;
    for (int x = kLeft; x < kW - 16; x++) {
      const double ms0 = t0 + (x - kLeft) / px, ms1 = t0 + (x + 1 - kLeft) / px;
      float lo = 0, hi = 0;
      for (auto i = static_cast<int64_t>(ms0 * ge::audio::kRate / 1000.0);
           i < static_cast<int64_t>(ms1 * ge::audio::kRate / 1000.0); i++) {
        if (i < 0 || i >= static_cast<int64_t>(t.left.size())) continue;
        lo = std::min(lo, t.left[static_cast<size_t>(i)]);
        hi = std::max(hi, t.left[static_cast<size_t>(i)]);
      }
      gl::draw_line(p, x, mid - hi * 40, x, mid - lo * 40, ink);
    }
    gl::draw_line(p, x_of(c.due_ms), y0 + 2, x_of(c.due_ms), y0 + kRow - 6, red);
    gl::draw_text(p, x_of(c.due_ms) + 4, y0 + kRow - 16,
                  "step " + std::to_string(c.step) + ", picture +" +
                      num::to_fixed(c.lit_ms - c.sound_ms, 1) + " ms",
                  red);
  }
  const int yb = kHead + kRow * static_cast<int>(takes.size()) + 4;
  gl::draw_text(p, kLeft, yb, "-120 ms", faint);
  gl::draw_text(p, kW / 2.0 + 30, yb, "the click's step", faint);
  gl::draw_text(p, kW - 70, yb, "+120 ms", faint);
  return p;
}

// A picture that costs an encoder something: blocks of hashed grey scrolling 3 px a frame over a
// gradient, with a bright square crossing it.
gl::Picture pattern(int w, int h, int k) {
  gl::Picture p = gl::create_picture(w, h, {0, 0, 0, 255});
  for (int y = 0; y < h; y++) {
    for (int x = 0; x < w; x++) {
      const int sx = x + 3 * k;
      auto n =
          (static_cast<uint32_t>(sx / 8) * 73856093u) ^ (static_cast<uint32_t>(y / 8) * 19349663u);
      n = (n ^ (n >> 13)) * 0x5bd1e995u;
      const auto g = static_cast<uint8_t>(40 + (n >> 24) % 120 + (x + y) % 60);
      const bool square = x > w / 4 + 4 * k % (w / 2) && x < w / 4 + 4 * k % (w / 2) + w / 8 &&
                          y > h / 3 && y < h / 3 + h / 6;
      const size_t at =
          4 * (static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x));
      p.rgba[at] = square ? 250 : g;
      p.rgba[at + 1] = square ? 220 : g;
      p.rgba[at + 2] = square ? 120 : static_cast<uint8_t>(std::min(255, g + 20));
    }
  }
  return p;
}

double luma_psnr(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b, int w, int h) {
  double se = 0;
  const size_t n = static_cast<size_t>(w) * static_cast<size_t>(h);
  for (size_t i = 0; i < n; i++) {
    const double d = static_cast<double>(a[i]) - static_cast<double>(b[i]);
    se += d * d;
  }
  if (se == 0) return 99;
  // 10 * log10(x) as ln(x) * 10 / ln(10), on the engine's own libm.
  return 10.0 * ge::core::libm::log(255.0 * 255.0 / (se / static_cast<double>(n))) /
         ge::core::libm::log(10.0);
}

// The MP4 as laid out on disk: the header, every fragment (its first picture's time on it) and
// the mfra across the file's bytes, the cut halfway through the last fragment as a red line, and
// under it how far the engine's reader and ffmpeg each got through the cut file.
gl::Picture file_map(const gc::Mp4File& whole, size_t size, size_t cut_at, int64_t read_pictures,
                     int64_t ffmpeg_pictures, int64_t frames, double fps) {
  constexpr int kW = 960, kLeft = 16, kBarW = kW - 2 * kLeft;
  gl::Picture p = gl::create_picture(kW, 200, {250, 250, 252, 255});
  const gl::Rgba ink{30, 34, 40, 255}, faint{120, 126, 136, 255}, red{205, 40, 40, 255};
  gl::draw_text(p, kLeft, 8,
                "THE MP4 ON DISK AT " + num::to_string(fps) + " FPS, " +
                    num::to_fixed(static_cast<double>(size) / 1e3, 1) + " KB, CUT SHORT",
                ink);
  const auto x_of = [&](size_t at) {
    return kLeft + static_cast<double>(at) * kBarW / static_cast<double>(size);
  };
  const double y = 32, h = 44;
  const size_t first = whole.fragments.empty() ? size : static_cast<size_t>(whole.fragments[0]);
  gl::fill_rect(p, x_of(0), y, std::max(2.0, x_of(first) - x_of(0)), h, {70, 110, 190, 255});
  // The last fragment's mdat ends where its last sample's bytes end; the mfra follows.
  size_t mfra = 0;
  for (const gc::Mp4Sample& s : whole.samples) mfra = std::max(mfra, s.offset + s.size);
  for (size_t i = 0; i < whole.fragments.size(); i++) {
    const size_t a = static_cast<size_t>(whole.fragments[i]);
    const size_t b =
        i + 1 < whole.fragments.size() ? static_cast<size_t>(whole.fragments[i + 1]) : mfra;
    const gl::Rgba shade = i % 2 ? gl::Rgba{176, 182, 192, 255} : gl::Rgba{206, 210, 218, 255};
    gl::fill_rect(p, x_of(a), y, x_of(b) - x_of(a), h, shade);
    gl::draw_line(p, x_of(a), y, x_of(a), y + h, faint);
    if (i < whole.seeks.size() && x_of(b) - x_of(a) > 30) {
      gl::draw_text(p, x_of(a) + 3, y + h / 2 - 4,
                    num::to_fixed(static_cast<double>(whole.seeks[i].t) / 1000.0, 0) + "s", ink);
    }
  }
  gl::fill_rect(p, x_of(mfra), y, std::max(2.0, x_of(size) - x_of(mfra)), h, {150, 90, 170, 255});
  gl::draw_text(p, kLeft, y + h + 6,
                "ftyp + moov (blue), a fragment a box (moof + mdat, its first "
                "picture's time), mfra (purple)",
                faint);
  gl::draw_line(p, x_of(cut_at), y - 4, x_of(cut_at), y + h + 4, red);
  gl::draw_text(p, x_of(cut_at) + 4, y - 2, "cut", red);
  const auto bar = [&](double yy, int64_t pictures, const std::string& who, gl::Rgba c) {
    const double share =
        frames > 0 ? static_cast<double>(pictures) / static_cast<double>(frames) : 0;
    gl::fill_rect(p, kLeft, yy, std::max(0.0, share) * kBarW, 18, c);
    gl::draw_text(p, kLeft + 4, yy + 5,
                  who + ": " + (pictures < 0 ? std::string("-") : std::to_string(pictures)) +
                      " of " + std::to_string(frames) + " pictures",
                  ink);
  };
  bar(130, read_pictures, "THE ENGINE'S READER, WHOLE FRAGMENTS ONLY", {120, 196, 140, 255});
  bar(156, ffmpeg_pictures, "FFMPEG, ALSO WHAT IT CAN OF THE CUT ONE", {236, 196, 110, 255});
  return p;
}

// The compressed export again in the engine's fragmented MP4 (capture/mp4.hpp): the same sync
// check, and the file cut halfway through its last fragment (as a crash would leave it) read to its
// last whole fragment by the engine's reader and, when it is installed, played by ffmpeg to at
// least as far. Measured, like the rest of the compressed export.
void in_mp4(gl::Lab& lab, const std::vector<double>& rates, int64_t steps, const fs::path& dir,
            bool ffmpeg) {
  gl::LabTable table{
      "AV1 and Opus in the engine's fragmented MP4: every frame rate's pictures, KB, worst lag, "
      "sound off its step, ffmpeg's decode, and the file cut short: the pictures in its whole "
      "fragments, and ffmpeg's count (at least as many: it also plays what it can of the cut one)",
      {"fps", "frames", "KB", "fragments", "worst-lag-ms", "sound-off-ms", "ffmpeg", "cut: read",
       "cut: ffmpeg"},
      {}};
  int64_t bad = 0;
  bool shown = false;
  for (const double fps : rates) {
    Take t = take(fps, steps, dir, ffmpeg, "av1", true);
    if (!t.error.empty()) {
      lab.fail("the MP4 export at " + num::to_string(fps) + " fps failed: " + t.error,
               "Run tests/capture_mp4_test.cpp (skill: replay).");
      continue;
    }
    double worst = 0, off = 0;
    for (const Click& c : t.clicks) {
      off = std::max(off, std::fabs(c.sound_ms - c.due_ms));
      worst = std::max(worst, c.lit_ms < 0 ? 1e9 : c.lit_ms - c.due_ms);
    }
    if (worst >= 1000.0 / fps + 3 || off > 3 || t.clicks.empty()) bad++;
    if (ffmpeg && (!t.ffmpeg_decodes || t.ffmpeg_frames != t.frames)) bad++;
    // Cut halfway through its last fragment, as a crash while writing it would leave it.
    const std::string path = (dir / ("sync-" + num::to_string(fps) + "fps.mp4")).string();
    const std::vector<uint8_t> bytes = read_all(path);
    std::string error;
    const auto whole = gc::read_mp4(bytes, error);
    size_t cut_at = bytes.size() / 2;
    if (whole && !whole->fragments.empty()) {
      const auto last = static_cast<size_t>(whole->fragments.back());
      size_t end = last;
      for (const gc::Mp4Sample& s : whole->samples) end = std::max(end, s.offset + s.size);
      cut_at = last + (end - last) / 2;
    }
    const std::string cut_path = (dir / ("cut-" + num::to_string(fps) + "fps.mp4")).string();
    {
      std::ofstream out(cut_path, std::ios::binary);
      out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(cut_at));
    }
    const auto cut = gc::read_mp4(std::span(bytes.data(), cut_at), error);
    int64_t cut_pictures = -1;
    if (cut) {
      cut_pictures = 0;
      for (const gc::Mp4Sample& s : cut->samples) cut_pictures += s.track == 1 ? 1 : 0;
    }
    const int64_t cut_ffmpeg = ffmpeg ? probe_frames(cut_path) : -1;
    if (!cut || cut_pictures <= 0 || (ffmpeg && cut_ffmpeg < cut_pictures)) bad++;
    table.rows.push_back(
        {num::to_string(fps), std::to_string(t.frames),
         num::to_fixed(static_cast<double>(t.bytes) / 1e3, 1),
         whole ? std::to_string(whole->fragments.size()) : "-", num::to_fixed(worst, 2),
         num::to_fixed(off, 2),
         !ffmpeg                                           ? "-"
         : t.ffmpeg_decodes && t.ffmpeg_frames == t.frames ? "every picture"
                                                           : "NO",
         cut ? std::to_string(cut_pictures) + " of " + std::to_string(t.frames) : "NO",
         ffmpeg ? std::to_string(cut_ffmpeg) : "-"});
    if (whole && !shown) {
      lab.shot({"mp4-cut",
                "THE MP4 CUT SHORT, AS A CRASH WOULD LEAVE IT",
                "The sync fixture's MP4 at the first frame rate across its bytes: the header, "
                "each fragment, the index at the end. Cut halfway through the last fragment, the "
                "engine's reader keeps "
                "every whole fragment; ffmpeg plays those and what it can of the one cut.",
                "The green bar reaching the last whole fragment before the red line, and the "
                "yellow one at least as far.",
                {}},
               file_map(*whole, bytes.size(), cut_at, cut_pictures, cut_ffmpeg, t.frames, fps),
               gl::Det::measured);
      shown = true;
    }
    lab.metric("mp4.fps" + num::to_string(fps) + ".worst-lag-ms", worst, "ms", gl::Det::measured);
    lab.metric("mp4.fps" + num::to_string(fps) + ".cut-pictures", static_cast<double>(cut_pictures),
               "count", gl::Det::measured);
  }
  lab.table(std::move(table), gl::Det::measured);
  if (bad) {
    lab.fail(std::to_string(bad) +
                 " MP4 exports out of sync, not decoded whole by ffmpeg, or not read to their "
                 "last whole fragment once cut short",
             "Read capture/mp4.cpp's fragments and its edit list; run tests/capture_mp4_test.cpp "
             "(skill: replay).");
  }
}

// The compressed export (AV1 and Opus, a .webm): the same sync check through the decoders, and
// what the AV1 encoder costs at 720p and 1080p at three qualities. Everything here is measured
// (an encoder's bytes and timing vary with the machine and its threads), so the digest is the same
// with the encoders or without them.
void coded(gl::Lab& lab, const std::vector<double>& rates, int64_t steps, const fs::path& dir,
           bool ffmpeg, int cost_frames) {
  const std::string shot_id = "sync-av1";
  const std::string title = "THE SAME, AS AV1 AND OPUS IN A .WEBM";
  const std::string about =
      "The sync fixture exported as AV1 and Opus and decoded back by the engine's decoders: each "
      "row is 240 ms round its first click, as above. Opus smears a click's onset by a few "
      "milliseconds, so the wave may start just before the red line.";
  const std::string look =
      "The wave starting within 3 ms of the red line in every row, and the "
      "first white box after it by less than one box's width.";
  if (!gc::encoders_built()) {
    lab.note("this build has no encoders (GE_ENCODERS is OFF): the compressed export is skipped");
    gl::Picture blank = gl::create_picture(480, 60, {250, 250, 252, 255});
    gl::draw_text(blank, 8, 24, "NO ENCODERS IN THIS BUILD", {30, 34, 40, 255});
    lab.shot({shot_id, title, about, look, {}}, blank, gl::Det::measured);
    lab.shot({"mp4-cut", "THE MP4 CUT SHORT, AS A CRASH WOULD LEAVE IT", about, look, {}}, blank,
             gl::Det::measured);  // the same shots with the encoders or without, one digest
    return;
  }
  std::vector<Take> takes;
  gl::LabTable table{
      "AV1 and Opus: every frame rate's pictures, KB, KB a second, worst lag "
      "(picture after its step), sound off its step",
      {"fps", "frames", "KB", "KB/s", "clicks", "worst-lag-ms", "sound-off-ms", "ffmpeg"},
      {}};
  int64_t bad = 0;
  for (const double fps : rates) {
    Take t = take(fps, steps, dir, ffmpeg, "av1");
    if (!t.error.empty()) {
      lab.fail("the AV1 export at " + num::to_string(fps) + " fps failed: " + t.error,
               "Run tests/capture_codecs_test.cpp (skill: replay).");
      continue;
    }
    double worst = 0, off = 0;
    for (const Click& c : t.clicks) {
      off = std::max(off, std::fabs(c.sound_ms - c.due_ms));
      const double lag = c.lit_ms < 0 ? 1e9 : c.lit_ms - c.due_ms;
      worst = std::max(worst, lag);
    }
    if (worst >= 1000.0 / fps + 3 || off > 3 || t.clicks.empty()) bad++;
    const double secs = static_cast<double>(t.frames) / fps;
    table.rows.push_back({num::to_string(fps), std::to_string(t.frames),
                          num::to_fixed(static_cast<double>(t.bytes) / 1e3, 1),
                          num::to_fixed(static_cast<double>(t.bytes) / 1e3 / secs, 1),
                          std::to_string(t.clicks.size()), num::to_fixed(worst, 2),
                          num::to_fixed(off, 2),
                          !ffmpeg                                           ? "-"
                          : t.ffmpeg_decodes && t.ffmpeg_frames == t.frames ? "every picture"
                                                                            : "NO"});
    if (ffmpeg && (!t.ffmpeg_decodes || t.ffmpeg_frames != t.frames)) bad++;
    lab.metric("av1.fps" + num::to_string(fps) + ".worst-lag-ms", worst, "ms", gl::Det::measured);
    takes.push_back(std::move(t));
  }
  lab.table(std::move(table), gl::Det::measured);
  if (bad) {
    lab.fail(std::to_string(bad) +
                 " AV1 exports out of sync (a flash 3 ms or more past a frame "
                 "after its step, a click 3 ms or more off it) or not decoded whole by ffmpeg",
             "Read av_export.cpp's interleave and the CodecDelay it writes; run "
             "tests/capture_codecs_test.cpp (skill: replay).");
  }
  lab.shot({shot_id, title, about, look, {}}, strips(takes, title), gl::Det::measured);
  in_mp4(lab, rates, steps, dir, ffmpeg);

  // What the encoder costs: `cost_frames` pictures of the pattern at each size and quality.
  gl::LabTable cost{
      "AV1 (libaom, realtime, speed 9, 4 threads) on a scrolling pattern of hashed "
      "blocks (much harder than a game's picture): a frame's encode, MB a minute at "
      "30 fps, luma PSNR decoded",
      {"size", "crf", "encode-ms", "MB/min", "psnr-db"},
      {}};
  for (const auto& [w, h] : {std::pair{1280, 720}, std::pair{1920, 1080}}) {
    std::vector<std::vector<uint8_t>> frames;
    for (int k = 0; k < cost_frames; k++) {
      std::vector<uint8_t> yuv;
      gc::to_i420(pattern(w, h, k), yuv);
      frames.push_back(std::move(yuv));
    }
    for (const int crf : {24, 32, 40}) {
      gc::Av1Encoder enc;
      gc::Av1Options o;
      o.width = w;
      o.height = h;
      o.crf = crf;
      std::string error;
      std::vector<gc::Packet> packets;
      if (!enc.open(o, error)) {
        lab.fail("the AV1 encoder did not open: " + error, "Run tests/capture_codecs_test.cpp.");
        return;
      }
      const auto t0 = Clock::now();
      for (size_t k = 0; k < frames.size(); k++) {
        enc.encode(frames[k], gc::frame_ms(static_cast<int64_t>(k), 30), packets, error);
      }
      enc.flush(packets, error);
      const double ms = std::chrono::duration<double, std::milli>(Clock::now() - t0).count() /
                        static_cast<double>(frames.size());
      size_t bytes = 0;
      double psnr = 0;
      gc::Av1Decoder dec;
      dec.open(error);
      for (size_t k = 0; k < packets.size() && k < frames.size(); k++) {
        bytes += packets[k].data.size();
        std::vector<uint8_t> got;
        int gw = 0, gh = 0;
        bool any = false;
        dec.decode(packets[k].data, got, gw, gh, any, error);
        if (any) psnr += luma_psnr(frames[k], got, w, h);
      }
      psnr /= static_cast<double>(frames.size());
      const double mb_min =
          static_cast<double>(bytes) / static_cast<double>(frames.size()) * 30 * 60 / 1e6;
      const std::string key = "av1." + std::to_string(h) + "p.crf" + std::to_string(crf);
      lab.metric(key + ".encode-ms", ms, "ms", gl::Det::measured);
      lab.metric(key + ".mb-per-minute", mb_min, "MB", gl::Det::measured);
      lab.metric(key + ".psnr-db", psnr, "dB", gl::Det::measured);
      cost.rows.push_back({std::to_string(w) + "x" + std::to_string(h), std::to_string(crf),
                           num::to_fixed(ms, 1), num::to_fixed(mb_min, 1), num::to_fixed(psnr, 1)});
    }
  }
  lab.table(std::move(cost), gl::Det::measured);
}

// The platform's own encoders (capture/platform_video.hpp, M23d4), on a machine that has one:
// for H.264 and HEVC, which encoder took the pictures (the media engine or the system's software
// one), the sync fixture exported as an .mp4 at 30 fps and decoded back by the platform's decoder
// (every picture out, each click on its step, ffmpeg playing the same file), and what a frame
// costs at 720p with its size and quality. A machine with none says so. Everything is measured,
// so every machine has the same digest.
void platform(gl::Lab& lab, int64_t steps, const fs::path& dir, bool ffmpeg, int cost_frames) {
  const std::string shot_id = "sync-platform";
  const std::string title = "THE SAME, AS THE PLATFORM'S H.264 AND HEVC IN AN .MP4";
  const std::string about =
      "The sync fixture exported at 30 fps through the platform's encoder (VideoToolbox on a "
      "Mac) and decoded back by its decoder: the first row H.264, the second HEVC, each 240 ms "
      "round its first click.";
  const std::string look =
      "The wave starting within 3 ms of the red line in each row, and the first white box after "
      "it by less than one box's width.";
  if (gc::platform_encoder_name().empty() || !gc::encoders_built()) {
    lab.note("this build or platform has no platform video encoder: its rows are skipped");
    gl::Picture blank = gl::create_picture(480, 60, {250, 250, 252, 255});
    gl::draw_text(blank, 8, 24, "NO PLATFORM ENCODER HERE", {30, 34, 40, 255});
    lab.shot({shot_id, title, about, look, {}}, blank, gl::Det::measured);
    return;
  }
  gl::LabTable table{
      "The platform's encoders: who coded the pictures, the sync fixture at 30 fps as an .mp4 "
      "(pictures decoded back, worst lag, sound off its step, ffmpeg), and a 720p frame's encode "
      "at the default bitrate (MB a minute, luma PSNR decoded)",
      {"codec", "encoder", "frames", "decoded", "worst-lag-ms", "sound-off-ms", "ffmpeg",
       "720p encode-ms", "MB/min", "psnr-db"},
      {}};
  std::vector<Take> takes;
  int64_t bad = 0;
  std::vector<std::vector<uint8_t>> frames;
  for (int k = 0; k < cost_frames; k++) {
    std::vector<uint8_t> yuv;
    gc::to_i420(pattern(1280, 720, k), yuv);
    frames.push_back(std::move(yuv));
  }
  for (const std::string codec : {"h264", "hevc"}) {
    gc::VideoEncoder enc;
    gc::VideoEncoderOptions o;
    o.width = 1280;
    o.height = 720;
    std::string error;
    if (!enc.open(codec, o, error)) {
      std::string why = "the platform's " + codec + " encoder did not open: ";
      why += error;
      lab.fail(why, "Run tests/capture_platform_video_test.cpp on this machine (skill: replay).");
      continue;
    }
    std::vector<gc::Packet> packets;
    const auto t0 = Clock::now();
    for (size_t k = 0; k < frames.size(); k++) {
      if (!enc.encode(frames[k], gc::frame_ms(static_cast<int64_t>(k), 30), packets, error)) break;
    }
    enc.flush(packets, error);
    const double ms = std::chrono::duration<double, std::milli>(Clock::now() - t0).count() /
                      static_cast<double>(frames.size());
    gc::VideoDecoder dec;
    size_t bytes = 0;
    double psnr = 0;
    int decoded = 0;
    if (dec.open(enc.matroska_codec(), enc.codec_private(), o.width, o.height, error)) {
      for (size_t k = 0; k < packets.size() && k < frames.size(); k++) {
        bytes += packets[k].data.size();
        std::vector<uint8_t> got;
        int gw = 0, gh = 0;
        bool any = false;
        if (dec.decode(packets[k].data, got, gw, gh, any, error) && any) {
          psnr += luma_psnr(frames[k], got, gw, gh);
          decoded++;
        }
      }
    }
    psnr /= std::max(1, decoded);
    const double mb_min =
        static_cast<double>(bytes) / static_cast<double>(frames.size()) * 30 * 60 / 1e6;
    Take t = take(30, steps, dir, ffmpeg, codec, true);
    double worst = 0, off = 0;
    for (const Click& c : t.clicks) {
      off = std::max(off, std::fabs(c.sound_ms - c.due_ms));
      worst = std::max(worst, c.lit_ms < 0 ? 1e9 : c.lit_ms - c.due_ms);
    }
    const auto pictures = static_cast<int64_t>(t.pictures.size());
    if (!t.error.empty() || worst >= 1000.0 / 30 + 3 || off > 3 || t.clicks.empty() ||
        pictures != t.frames || decoded != static_cast<int>(frames.size())) {
      bad++;
    }
    if (ffmpeg && (!t.ffmpeg_decodes || t.ffmpeg_frames != t.frames)) bad++;
    table.rows.push_back({codec, enc.encoder(), std::to_string(t.frames), std::to_string(pictures),
                          num::to_fixed(worst, 2), num::to_fixed(off, 2),
                          !ffmpeg                                           ? "-"
                          : t.ffmpeg_decodes && t.ffmpeg_frames == t.frames ? "every picture"
                                                                            : "NO",
                          num::to_fixed(ms, 2), num::to_fixed(mb_min, 1), num::to_fixed(psnr, 1)});
    const std::string key = "platform." + codec;
    lab.metric(key + ".hardware", enc.hardware() ? 1 : 0, "bool", gl::Det::measured);
    lab.metric(key + ".worst-lag-ms", worst, "ms", gl::Det::measured);
    lab.metric(key + ".720p.encode-ms", ms, "ms", gl::Det::measured);
    lab.metric(key + ".720p.mb-per-minute", mb_min, "MB", gl::Det::measured);
    lab.metric(key + ".720p.psnr-db", psnr, "dB", gl::Det::measured);
    if (!t.error.empty()) {
      lab.fail("the platform's " + codec + " export failed: " + t.error,
               "Run tests/capture_platform_video_test.cpp on this machine (skill: replay).");
    }
    t.fps = 30;
    takes.push_back(std::move(t));
  }
  lab.table(std::move(table), gl::Det::measured);
  if (bad) {
    lab.fail(std::to_string(bad) +
                 " platform exports out of sync, not decoded whole by the platform's decoder or "
                 "by ffmpeg",
             "Read capture/videotoolbox.cpp; run tests/capture_platform_video_test.cpp on this "
             "machine (skill: replay).");
  }
  lab.shot({shot_id, title, about, look, {}}, strips(takes, title), gl::Det::measured);
}

void run(gl::Lab& lab) {
  const double seconds = lab.arg_num("seconds");
  const std::vector<double> rates = lab.arg_nums("fps");
  const auto steps = static_cast<int64_t>(std::llround(seconds * gc::kSyncGame.hz));
  const fs::path dir = fs::temp_directory_path() / "ge-capture-lab";
  std::error_code ec;
  fs::create_directories(dir, ec);
  const bool ffmpeg = tool_found("ffmpeg") && tool_found("ffprobe");
  lab.note(ffmpeg ? "ffmpeg found: each file is decoded by it too"
                  : "ffmpeg not found: the files are read by the engine's reader alone");

  std::vector<Take> takes;
  gl::LabTable table{
      "Every frame rate: pictures, samples, MB, clicks, worst lag (picture after "
      "click), sound off its step",
      {"fps", "frames", "samples", "MB", "clicks", "worst-lag-ms", "sound-off-ms"},
      {}};
  double worst_off = 0;
  int64_t late = 0, early = 0, unshown = 0, mismatched = 0;
  for (const double fps : rates) {
    Take t = take(fps, steps, dir, ffmpeg, "");
    if (!t.error.empty()) {
      lab.fail("the export at " + num::to_string(fps) + " fps failed: " + t.error,
               "Run tests/capture_av_export_test.cpp (skill: replay).");
      continue;
    }
    double worst = 0, off = 0;
    for (const Click& c : t.clicks) {
      off = std::max(off, std::fabs(c.sound_ms - c.due_ms));
      if (c.lit_ms < 0) {
        unshown++;
        continue;
      }
      const double lag = c.lit_ms - c.sound_ms;
      worst = std::max(worst, lag);
      if (lag >= 1000.0 / fps) late++;
      if (lag <= -1) early++;
    }
    worst_off = std::max(worst_off, off);
    const int64_t expected = gc::frame_sample(t.frames, fps, ge::audio::kRate);
    if (t.samples != expected) mismatched++;
    table.rows.push_back({num::to_string(fps), std::to_string(t.frames), std::to_string(t.samples),
                          num::to_fixed(static_cast<double>(t.bytes) / 1e6, 2),
                          std::to_string(t.clicks.size()), num::to_fixed(worst, 2),
                          num::to_fixed(off, 3)});
    const std::string key = "fps" + num::to_string(fps);
    lab.metric(key + ".frames", static_cast<double>(t.frames), "count");
    lab.metric(key + ".worst-lag-ms", worst, "ms");
    if (ffmpeg) {
      lab.metric(key + ".ffmpeg-decodes", t.ffmpeg_decodes ? 1 : 0, "bool", gl::Det::measured);
      lab.metric(key + ".ffmpeg-frames", static_cast<double>(t.ffmpeg_frames), "count",
                 gl::Det::measured);
      if (!t.ffmpeg_decodes || t.ffmpeg_frames != t.frames) {
        lab.fail("ffmpeg read " + std::to_string(t.ffmpeg_frames) + " of " +
                     std::to_string(t.frames) + " pictures at " + num::to_string(fps) + " fps",
                 "Read the file with tests/capture_matroska_test.cpp's checks; compare the "
                 "muxer's elements with ffprobe -show_packets (skill: replay).");
      }
    }
    takes.push_back(std::move(t));
  }
  lab.table(std::move(table));
  lab.metric("late", static_cast<double>(late), "count");
  lab.metric("early", static_cast<double>(early), "count");
  lab.metric("unshown", static_cast<double>(unshown), "count");
  lab.metric("sound-off-ms", worst_off, "ms");
  lab.metric("length-mismatches", static_cast<double>(mismatched), "count");
  if (late || early || unshown) {
    lab.fail(std::to_string(late) + " flashes shown a frame or more after their click, " +
                 std::to_string(early) + " before it, " + std::to_string(unshown) + " never",
             "Read av_export.cpp's stepped and frame: the synth is rendered to a step's sample "
             "before its events (skill: replay).");
  }
  if (worst_off > 0.1) {
    lab.fail("a click sounded " + num::to_fixed(worst_off, 3) + " ms off its step's sample",
             "The step's sound must start on the block that starts on its sample: read "
             "av_export.cpp's render_to (skill: replay).");
  }
  if (mismatched) {
    lab.fail("the sound did not run exactly as long as the pictures",
             "Read av_export.cpp's close (skill: replay).");
  }

  // The uncompressed path's cost at 1280x720: the conversion and the muxer, a frame.
  {
    const gl::Picture frame = gl::create_picture(1280, 720, {90, 140, 200, 255});
    gc::MatroskaOptions mo;
    gc::VideoTrack v;
    v.width = 1280;
    v.height = 720;
    mo.video = v;
    gc::MatroskaWriter w;
    std::string error;
    std::vector<uint8_t> yuv;
    w.open("", mo, error);
    constexpr int kFrames = 30;
    const auto t0 = Clock::now();
    for (int k = 0; k < kFrames; k++) {
      gc::to_i420(frame, yuv);
      w.video(gc::frame_ms(k, 30), true, yuv, error);
    }
    const double ms =
        std::chrono::duration<double, std::milli>(Clock::now() - t0).count() / kFrames;
    lab.metric("720p.frame-ms", ms, "ms", gl::Det::measured);
    lab.metric("720p.mb-per-minute", static_cast<double>(gc::i420_bytes(1280, 720)) * 30 * 60 / 1e6,
               "MB");
  }

  lab.shot({"sync",
            "A CLICK AND A FLASH ON ONE STEP, AT " + std::to_string(rates.size()) + " FRAME RATES",
            "The sync fixture exported at each frame rate and read back from the file: each row "
            "is 240 ms round its first click, the pictures as boxes along the time line (white "
            "when the flash shows), the sound's wave under them, the click's step as the red "
            "line, and how long after the click the first white picture comes.",
            "The wave starting on the red line in every row, and the first white box starting "
            "on it or after it by less than one box's width.",
            {{"seconds", num::to_string(seconds)}, {"fps", lab.arg("fps")}}},
           strips(takes, "EACH FRAME RATE'S FIRST CLICK: PICTURES ABOVE, SOUND BELOW"));

  coded(lab, rates, steps, dir, ffmpeg, static_cast<int>(lab.arg_num("cost-frames")));
  platform(lab, steps, dir, ffmpeg, static_cast<int>(lab.arg_num("cost-frames")));
}

const gl::LabSpec kSpec{
    "capture",
    gl::LabKind::scenario,
    "engine",
    "engine/capture (av_export, matroska, mkv_read, mp4, sync_fixture, video, platform_video), "
    "engine/play/record.cpp",
    "replay",
    "Does an export's sound land on its steps, and does a flash on a click's step show less than "
    "a frame after it, at every frame rate?",
    {{"seconds", "30", "how long each export runs"},
     {"fps", "24,25,30,60", "the frame rates exported"},
     {"cost-frames", "60", "the pictures the AV1 encoder's cost is timed over, at each size"}},
    "--seconds=4 --fps=24,30 --cost-frames=4",
    run,
};
GE_LAB(kSpec);

}  // namespace
