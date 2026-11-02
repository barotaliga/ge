// LAB `screenshot`: what a player's screenshot costs in each format and what each loses
// (engine/capture/screenshot.hpp, capture/still.hpp). Two frames a game might show (a SMOOTH one:
// a sky's gradient over rolling ground with a HUD bar and its yellow words, as a calm view draws;
// a DETAILED one: fine grain over every pixel, as a busy scene and its noise draw, the hardest case
// for every packer) and, with `--file`, a real frame (a PNG a game wrote), each packed as PNG, JPEG
// with full and with halved chroma, AVIF and HEIC at `--quality`, read back by `decode_still`, and
// measured: the bytes against the PNG's, the PSNR and SSIM against the frame, the time to pack on
// the writer's thread. A format this build or machine cannot write is a row that says so. Then a
// sweep of the quality for the JPEG and the AVIF on the hardest frame, and the writer itself:
// a frame handed to `Screenshots` at the defaults, timed to the file on disk and read back. The
// pictures put a part of each frame (the HUD's words on the made ones, the middle of a file's) at
// four times its size in every format, in a grid.
//
// PNG's and JPEG's bytes are the same on every platform (integers throughout) and go in the
// digest; AV1's and the platform's HEVC differ by machine, so their numbers and pictures are
// measured.
//
// ge:summary Lab: screenshots in PNG, JPEG, AVIF and HEIC: bytes, PSNR, SSIM, time, quality sweep
// ge:keywords lab screenshot png jpg jpeg avif heic quality chroma psnr ssim bytes encode-ms sweep
// ge:keywords read-back writer f12 scr_format scr_quality
// ge:skill replay labs
// ge:test tests/capture_still_test.cpp
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <vector>

#include "capture/screenshot.hpp"
#include "capture/still.hpp"
#include "core/noise.hpp"
#include "core/num_text.hpp"
#include "lab/diff.hpp"
#include "lab/lab.hpp"
#include "lab/png.hpp"

namespace {

namespace gc = ge::capture;
namespace gl = ge::lab;
namespace fs = std::filesystem;

std::string fixed(double v, int places) { return ge::core::num::to_fixed(v, places); }

uint8_t clamp8(double v) { return static_cast<uint8_t>(v < 0 ? 0 : v > 255 ? 255 : v); }

// A frame of `w` x `h`: the sky's gradient, a ridge line off the value noise, the ground shaded by
// its height and, when `grain` > 0, that much fine noise over every pixel; a HUD bar along the
// bottom.
gl::Picture frame(int w, int h, double grain) {
  gl::Picture p = gl::create_picture(w, h, {0, 0, 0});
  for (int x = 0; x < w; ++x) {
    const double ridge = h * (0.45 + 0.12 * ge::core::value_noise(x * 640.0 / w, 0, 160, 7) +
                              0.04 * ge::core::value_noise(x * 640.0 / w, 0, 23, 9));
    for (int y = 0; y < h; ++y) {
      const double n = grain > 0 ? ge::core::value_noise(x, y, 1.7, 3) - 0.5 : 0;
      gl::Rgba c;
      if (y < ridge) {
        const double t = static_cast<double>(y) / h;
        c = {clamp8(70 + 120 * t + grain * n), clamp8(120 + 90 * t + grain * n),
             clamp8(220 + 20 * t + grain * n)};
      } else {
        const double depth = (y - ridge) / (h - ridge + 1);
        const double snow = ge::core::value_noise(x * 2.0, y * 2.0, 31, 5);
        c = {clamp8(150 + 80 * snow - 60 * depth + grain * n),
             clamp8(160 + 70 * snow - 50 * depth + grain * n),
             clamp8(175 + 60 * snow - 30 * depth + grain * n)};
      }
      gl::set_pixel(p, x, y, c);
    }
  }
  const int bar = h / 10;
  gl::fill_rect(p, 0, h - bar, w, bar, {20, 24, 30, 200});
  gl::draw_text(p, 8, h - bar + 4, "TIME 1:23.45  SPEED 87 KM/H", {255, 220, 120},
                bar >= 24 ? 2 : 1);
  return p;
}

double ms_since(std::chrono::steady_clock::time_point t0) {
  return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
}

struct Format {
  const char* name;
  gc::StillOptions options;
};

// A frame read back from a still: its PSNR and SSIM against the frame (the part both cover).
struct Packed {
  bool made = false;
  std::string note;
  std::vector<uint8_t> bytes;
  double ms = 0, psnr = 0, ssim = 0;
  std::optional<gl::Picture> back;
};

Packed pack(const gl::Picture& p, const gc::StillOptions& o) {
  Packed k;
  gc::StillFormat used{};
  const auto t0 = std::chrono::steady_clock::now();
  gc::encode_still(o, p.width, p.height, p.rgba, k.bytes, used, k.note);
  k.ms = ms_since(t0);
  if (used != o.format) return k;  // fell back: the row says so
  std::string error;
  k.back = gc::decode_still(k.bytes, error);
  if (!k.back) {
    k.note = "did not read back: " + error;
    return k;
  }
  // AVIF and HEIC cut an odd side even: compare the part both hold.
  gl::Picture ref = gl::create_picture(k.back->width, k.back->height, {0, 0, 0});
  for (int y = 0; y < ref.height; ++y)
    std::copy_n(&p.rgba[static_cast<size_t>(y) * p.width * 4], static_cast<size_t>(ref.width) * 4,
                &ref.rgba[static_cast<size_t>(y) * ref.width * 4]);
  const gl::ImageDiff d = gl::diff_pictures(ref, *k.back, nullptr, gl::DiffOptions{});
  k.psnr = d.all.psnr;
  k.ssim = d.all.ssim;
  k.made = true;
  return k;
}

constexpr int kZoom = 4;

// `cw` x `ch` of the frame from (x0, y0), each pixel drawn kZoom by kZoom.
gl::Picture zoom(const gl::Picture& p, int x0, int y0, int cw, int ch) {
  gl::Picture out = gl::create_picture(cw * kZoom, ch * kZoom, {0, 0, 0});
  for (int y = 0; y < ch * kZoom; ++y)
    for (int x = 0; x < cw * kZoom; ++x) {
      const int sx = std::min(x0 + x / kZoom, p.width - 1),
                sy = std::min(y0 + y / kZoom, p.height - 1);
      const uint8_t* s = &p.rgba[(static_cast<size_t>(sy) * p.width + sx) * 4];
      gl::set_pixel(out, x, y, {s[0], s[1], s[2]});
    }
  return out;
}

bool machine_bound(gc::StillFormat f) {
  return f == gc::StillFormat::kAvif || f == gc::StillFormat::kHeic;
}

void run(gl::Lab& lab) {
  const int w = static_cast<int>(lab.arg_num("width"));
  const int h = static_cast<int>(lab.arg_num("height"));
  const int quality = static_cast<int>(lab.arg_num("quality"));
  struct Scene {
    std::string id;
    gl::Picture picture;
    std::string what;
  };
  std::vector<Scene> scenes;
  scenes.push_back({"smooth", frame(w, h, 0), "a sky's gradient over rolling ground, a HUD bar"});
  scenes.push_back({"detailed", frame(w, h, 48), "the same with fine grain over every pixel"});
  if (!lab.arg("file").empty()) {
    auto f = gl::read_png(lab.arg("file"));
    if (!f) {
      lab.fail("--file=" + lab.arg("file") + " did not read as a PNG",
               "Pass a PNG a game wrote (scr_format png). Skill: replay.");
      return;
    }
    // By its name alone: a path under a home folder never goes in a picture or a report.
    scenes.push_back(
        {"file", std::move(*f), "the frame in " + fs::path(lab.arg("file")).filename().string()});
  }
  const std::vector<Format> formats = {
      {"png", {gc::StillFormat::kPng, quality, false}},
      {"jpg 4:4:4", {gc::StillFormat::kJpg, quality, false}},
      {"jpg 4:2:0", {gc::StillFormat::kJpg, quality, true}},
      {"avif", {gc::StillFormat::kAvif, quality, true}},
      {"heic", {gc::StillFormat::kHeic, quality, true}},
  };

  gl::LabTable table{"EACH FORMAT AT QUALITY " + std::to_string(quality),
                     {"frame", "format", "KB", "of the PNG", "PSNR dB", "SSIM", "pack ms"},
                     {}};
  for (const Scene& sc : scenes) {
    const gl::Picture& p = sc.picture;
    size_t png_bytes = 0;
    // The part pictured: the HUD's words on a made frame, the middle of a file's.
    const int cw = std::min(p.width, 160), ch = std::min(p.height, 100);
    const bool made = sc.id != "file";
    const int x0 = made ? 0 : (p.width - cw) / 2;
    const int y0 = std::clamp(made ? p.height - p.height / 10 - 60 : p.height / 2 + p.height / 8, 0,
                              p.height - ch);
    constexpr int kColumns = 3, kLabel = 14;
    const int pw = cw * kZoom, ph = ch * kZoom + kLabel;
    const int rows = (static_cast<int>(formats.size()) + kColumns) / kColumns;
    gl::Picture sheet = gl::create_picture(pw * kColumns, ph * rows, {16, 16, 20});
    const auto place = [&](int panel, const gl::Picture& from, const std::string& label) {
      const int px = panel % kColumns * pw, py = panel / kColumns * ph;
      gl::blit(sheet, zoom(from, x0, y0, cw, ch), px, py + kLabel);
      gl::draw_text(sheet, px + 4, py + 3, label, {255, 255, 255});
    };
    place(0, p, "THE FRAME");
    bool sheet_measured = false;
    int panel = 1;
    for (const Format& f : formats) {
      const Packed k = pack(p, f.options);
      const std::string key =
          sc.id + "." + std::string(gc::still_extension(f.options.format)) +
          (f.options.format == gc::StillFormat::kJpg ? (f.options.subsample ? "420" : "444") : "");
      if (!k.made || !k.back) {
        table.rows.push_back({sc.id, f.name, "-", "-", "-", "-", k.note});
        lab.metric(key + ".made", 0, "bool", gl::Det::measured);
        ++panel;
        continue;
      }
      const gl::Picture& back = *k.back;
      if (f.options.format == gc::StillFormat::kPng) {
        png_bytes = k.bytes.size();
        if (k.psnr < 100)
          lab.fail("the " + sc.id + " frame's PNG did not read back to its pixels",
                   "Run tests/capture_screenshot_test.cpp; the packer is lab/png.cpp's "
                   "encode_png_rgb. Skill: replay.");
      } else if (k.psnr < 30) {
        lab.fail(std::string(f.name) + " of the " + sc.id + " frame reads back at " +
                     fixed(k.psnr, 1) + " dB at quality " + std::to_string(quality),
                 "Look at previews/screenshot/" + sc.id +
                     ".png; the encoder is "
                     "capture/still.cpp's for the format. Skill: replay.");
      }
      const gl::Det det =
          machine_bound(f.options.format) ? gl::Det::measured : gl::Det::deterministic;
      sheet_measured = sheet_measured || machine_bound(f.options.format);
      lab.metric(key + ".bytes", static_cast<double>(k.bytes.size()), "bytes", det);
      lab.metric(key + ".psnr", k.psnr, "dB", det);
      lab.metric(key + ".ms", k.ms, "ms", gl::Det::measured);
      const double share =
          png_bytes ? static_cast<double>(k.bytes.size()) / static_cast<double>(png_bytes) : 1;
      table.rows.push_back({sc.id, f.name, fixed(static_cast<double>(k.bytes.size()) / 1024, 0),
                            fixed(share * 100, 1) + "%", k.psnr >= 100 ? "exact" : fixed(k.psnr, 2),
                            fixed(k.ssim, 4), fixed(k.ms, 1)});
      std::string label = f.name;
      for (char& c : label) c = static_cast<char>(c >= 'a' && c <= 'z' ? c - 32 : c);
      place(panel, back,
            label + "  " + fixed(static_cast<double>(k.bytes.size()) / 1024, 0) + " KB  " +
                (k.psnr >= 100 ? std::string("EXACT") : fixed(k.psnr, 1) + " DB"));
      ++panel;
    }
    lab.shot({sc.id,
              "THE " + sc.id + " FRAME IN EACH FORMAT, FOUR TIMES ITS SIZE",
              sc.what + ": " + std::to_string(cw) + "x" + std::to_string(ch) + " px from (" +
                  std::to_string(x0) + ", " + std::to_string(y0) +
                  ") as it reads back from each format at quality " + std::to_string(quality) +
                  ", the frame itself first; each with its size and PSNR over the whole frame.",
              made ? "the HUD's yellow words keep their edges in the 4:4:4 JPEG; the 4:2:0 forms "
                     "blur their colour; no blocks or banding in the sky at this quality"
                   : "edges between two colours stay clean in the 4:4:4 JPEG, the AVIF and the "
                     "HEIC; the 4:2:0 JPEG bleeds colour across them; no blocks at this quality",
              {{"size", std::to_string(p.width) + "x" + std::to_string(p.height)},
               {"quality", std::to_string(quality)}}},
             sheet, sheet_measured ? gl::Det::measured : gl::Det::deterministic);
  }
  lab.table(table, gl::Det::measured);  // its times differ run to run

  // The quality sweep on the hardest frame: what each step of scr_quality buys.
  const gl::Picture& hard = scenes.back().id == "file" ? scenes.back().picture : scenes[1].picture;
  gl::LabTable sweep{"SCR_QUALITY ON THE " +
                         std::string(scenes.back().id == "file" ? "FILE'S" : "DETAILED") + " FRAME",
                     {"quality", "jpg 4:4:4 KB", "dB", "jpg 4:2:0 KB", "dB", "avif KB", "dB"},
                     {}};
  for (const int q : {50, 75, 85, 90, 95}) {
    std::vector<std::string> row = {std::to_string(q)};
    for (const gc::StillOptions o : {gc::StillOptions{gc::StillFormat::kJpg, q, false},
                                     gc::StillOptions{gc::StillFormat::kJpg, q, true},
                                     gc::StillOptions{gc::StillFormat::kAvif, q, true}}) {
      const Packed k = pack(hard, o);
      row.push_back(k.made ? fixed(static_cast<double>(k.bytes.size()) / 1024, 0) : "-");
      row.push_back(k.made ? fixed(k.psnr, 2) : "-");
    }
    sweep.rows.push_back(row);
  }
  lab.table(sweep, gl::Det::measured);

  // The writer itself at the defaults (scr_format jpg, scr_quality 90, scr_chroma 444), as the
  // key's frame goes: handed in, packed and written off-thread.
  const fs::path dir = fs::temp_directory_path() / "ge_screenshot_lab";
  std::error_code ec;
  fs::remove_all(dir, ec);
  {
    gc::Screenshots shots(dir.string(), "lab");
    const auto t0 = std::chrono::steady_clock::now();
    shots.write(static_cast<uint32_t>(hard.width), static_cast<uint32_t>(hard.height), hard.rgba);
    shots.flush();
    lab.metric("writer.default.ms", ms_since(t0), "ms", gl::Det::measured);
    const std::vector<std::string> lines = shots.poll();
    std::string file;
    for (const auto& e : fs::directory_iterator(dir, ec)) file = e.path().string();
    std::ifstream in(file, std::ios::binary);
    const std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(in)), {});
    std::string error;
    if (fs::path(file).extension() != ".jpg" || !gc::decode_still(bytes, error))
      lab.fail("the writer at the defaults did not leave a JPEG that reads back (" +
                   (lines.empty() ? std::string("no line") : lines[0]) + ")",
               "Run tests/capture_screenshot_test.cpp. Skill: replay.");
  }
  fs::remove_all(dir, ec);
}

const gl::LabSpec kSpec{
    "screenshot",
    gl::LabKind::bench,
    "engine",
    "engine/capture/screenshot.hpp, still.hpp, jpeg.hpp, heif.hpp; lab/png.cpp (encode_png_rgb)",
    "replay",
    "How small is a screenshot in each format at a quality, how much does it lose, and how long "
    "does it take to pack?",
    {{"width", "1920", "the frames' width, px"},
     {"height", "1080", "the frames' height, px"},
     {"quality", "90", "scr_quality for the lossy formats, 1..100"},
     {"file", "", "a PNG a game wrote, measured beside the two frames"}},
    "--width=320 --height=180",
    run,
};
GE_LAB(kSpec);

}  // namespace
