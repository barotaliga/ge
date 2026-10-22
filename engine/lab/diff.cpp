// The picture diff: the per-region numbers, the bands, the sheet, the distance file and two runs
// compared shot by shot.
//
// ge:summary The picture diff's numbers, bands, sheet, distance file and run comparison
// ge:keywords lab diff psnr ssim band contour heatmap sheet distance-file diff-runs
// ge:skill labs
// ge:test tests/lab_diff_test.cpp
#include "lab/diff.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <regex>

#include "core/libm/libm.hpp"
#include "lab/png.hpp"

namespace ge::lab {

namespace fs = std::filesystem;

namespace {

constexpr size_t kJudgedPixels = 64;
constexpr int kWindow = 8, kStride = 4;
// The SSIM's stabilisers for 8-bit luma: (0.01 * 255)^2 and (0.03 * 255)^2.
constexpr double kC1 = 6.5025, kC2 = 58.5225;
constexpr double kLn10 = 2.302585092994045684;
const char kMagic[8] = {'G', 'E', 'D', 'I', 'S', 'T', '1', '\n'};

std::string fixed(double v, int digits) {
  char buf[48];
  std::snprintf(buf, sizeof buf, "%.*f", digits, v);
  return buf;
}

std::string metres(double m) {
  char buf[32];
  std::snprintf(buf, sizeof buf, "%g", m);
  return buf;
}

std::string band_name(const DiffOptions& o, size_t k) {
  const size_t n = o.edges.size();
  if (k == n + 1) return "sky";
  if (k == n) return metres(n ? o.edges[n - 1] : 0) + "m+";
  return metres(k == 0 ? 0 : o.edges[k - 1]) + "-" + metres(o.edges[k]) + "m";
}

double floor_of(const DiffOptions& o, size_t k) {
  if (o.min_psnr.empty()) return 0;
  return o.min_psnr[std::min(k, o.min_psnr.size() - 1)];
}

// The band of a distance: 0 nearest, edges.size() the farthest finite, edges.size() + 1 the sky.
size_t band_of(const DiffOptions& o, float d) {
  if (!std::isfinite(d)) return o.edges.size() + 1;
  size_t k = 0;
  while (k < o.edges.size() && d >= o.edges[k]) ++k;
  return k;
}

// The distance at a picture's pixel: the map's nearest sample.
float distance_at(const DistanceMap& m, int x, int y, int width, int height) {
  const int mx = std::min(m.width - 1, x * m.width / width);
  const int my = std::min(m.height - 1, y * m.height / height);
  return m.metres[static_cast<size_t>(my) * static_cast<size_t>(m.width) + static_cast<size_t>(mx)];
}

bool usable(const DistanceMap* m) {
  return m && m->width > 0 && m->height > 0 &&
         m->metres.size() == static_cast<size_t>(m->width) * static_cast<size_t>(m->height);
}

double luma(const uint8_t* p) { return 0.299 * p[0] + 0.587 * p[1] + 0.114 * p[2]; }

struct Sums {
  size_t pixels = 0, changed = 0;
  double abs = 0, sq = 0, max = 0, ssim = 0;
  size_t windows = 0;
};

RegionDiff finish(const Sums& s, std::string name, double floor, bool judge) {
  RegionDiff r;
  r.name = std::move(name);
  r.pixels = s.pixels;
  r.min_psnr = floor;
  if (s.pixels == 0) return r;
  const double n = static_cast<double>(s.pixels) * 3;
  r.mean = s.abs / n;
  r.max = s.max;
  r.rmse = std::sqrt(s.sq / n);
  r.psnr = r.rmse > 0 ? std::min(100.0, 20 * core::libm::log(255.0 / r.rmse) / kLn10) : 100;
  r.ssim = s.windows ? s.ssim / static_cast<double>(s.windows) : 1;
  r.changed = static_cast<double>(s.changed) / static_cast<double>(s.pixels);
  r.judged = judge && floor > 0 && s.pixels >= kJudgedPixels;
  r.pass = !r.judged || r.psnr >= floor;
  return r;
}

// The heat ramp: blue at the tolerance, yellow at 32 levels, red from 64.
Rgba heat(double m) {
  const double t = std::clamp(m / 64.0, 0.0, 1.0);
  if (t < 0.5) {
    const double u = t * 2;
    return {static_cast<uint8_t>(40 + 215 * u), static_cast<uint8_t>(90 + 165 * u),
            static_cast<uint8_t>(255 - 215 * u)};
  }
  const double u = (t - 0.5) * 2;
  return {255, static_cast<uint8_t>(255 - 215 * u), 40};
}

const Rgba kBandColours[] = {{214, 72, 60},  {236, 160, 50}, {232, 220, 80},
                             {90, 180, 110}, {70, 140, 210}, {130, 100, 200}};
const Rgba kSkyColour{40, 44, 60};

Rgba band_colour(const DiffOptions& o, size_t k) {
  if (k == o.edges.size() + 1) return kSkyColour;
  return kBandColours[std::min(k, std::size(kBandColours) - 1)];
}

std::string json_str(const std::string& s) {
  std::string out = "\"";
  for (const char c : s) {
    if (c == '"' || c == '\\') out += '\\';
    if (static_cast<unsigned char>(c) < 0x20) {
      out += ' ';
      continue;
    }
    out += c;
  }
  return out + "\"";
}

std::string region_json(const RegionDiff& r) {
  return "{\"name\": " + json_str(r.name) + ", \"pixels\": " + std::to_string(r.pixels) +
         ", \"mean\": " + fixed(r.mean, 3) + ", \"max\": " + fixed(r.max, 0) +
         ", \"rmse\": " + fixed(r.rmse, 3) + ", \"psnr\": " + fixed(r.psnr, 2) +
         ", \"ssim\": " + fixed(r.ssim, 4) + ", \"changed\": " + fixed(r.changed, 4) +
         ", \"min_psnr\": " + fixed(r.min_psnr, 1) +
         ", \"judged\": " + (r.judged ? "true" : "false") +
         ", \"pass\": " + (r.pass ? "true" : "false") + "}";
}

std::optional<std::vector<uint8_t>> read_bytes(const fs::path& path) {
  std::ifstream f(path, std::ios::binary);
  if (!f) return {};
  return std::vector<uint8_t>{std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()};
}

Picture crop(const Picture& p, int width, int height) {
  Picture out{width, height, {}};
  out.rgba.resize(static_cast<size_t>(width) * static_cast<size_t>(height) * 4);
  const auto row = static_cast<size_t>(width) * 4;
  for (int y = 0; y < height; ++y) {
    std::memcpy(&out.rgba[static_cast<size_t>(y) * row],
                &p.rgba[static_cast<size_t>(y) * static_cast<size_t>(p.width) * 4], row);
  }
  return out;
}

// A shot's words back from its <id>.json: the id, the picture's own size, its distance file.
struct ShotFile {
  std::string id, distance;
  int width = 0, height = 0;
};
std::optional<ShotFile> read_shot_json(const fs::path& path) {
  std::ifstream f(path);
  const std::string text{std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()};
  static const std::regex kId("\"id\": \"([^\"]*)\""), kW("\"width\": ([0-9]+)"),
      kH("\"height\": ([0-9]+)"), kDist("\"distance\": \"([^\"]*)\"");
  std::smatch m;
  ShotFile s;
  if (!std::regex_search(text, m, kId)) return {};
  s.id = m[1];
  if (std::regex_search(text, m, kW)) s.width = std::stoi(m[1]);
  if (std::regex_search(text, m, kH)) s.height = std::stoi(m[1]);
  if (std::regex_search(text, m, kDist)) s.distance = m[1];
  return s;
}

bool parse_list(const std::string& text, std::vector<double>& out) {
  out.clear();
  size_t at = 0;
  while (at <= text.size()) {
    const size_t comma = std::min(text.find(',', at), text.size());
    const std::string word = text.substr(at, comma - at);
    char* end = nullptr;
    const double v = std::strtod(word.c_str(), &end);
    if (word.empty() || *end != '\0' || !std::isfinite(v)) return false;
    out.push_back(v);
    at = comma + 1;
  }
  return !out.empty();
}

}  // namespace

bool parse_diff_options(const std::string& edges, const std::string& min_psnr, DiffOptions& options,
                        std::string& error) {
  if (!edges.empty()) {
    if (!parse_list(edges, options.edges) ||
        !std::is_sorted(options.edges.begin(), options.edges.end()) || options.edges.front() <= 0) {
      error = "--diff-bands=" + edges +
              ": want ascending distances in metres over 0, e.g. --diff-bands=25,100,400";
      return false;
    }
  }
  if (!min_psnr.empty() && !parse_list(min_psnr, options.min_psnr)) {
    error = "--diff-psnr=" + min_psnr +
            ": want each band's floor in dB, nearest first and the sky last, e.g. "
            "--diff-psnr=40,36,32,28,28";
    return false;
  }
  return true;
}

ImageDiff diff_pictures(const Picture& before, const Picture& after, const DistanceMap* distance,
                        const DiffOptions& options) {
  ImageDiff d;
  d.width = after.width;
  d.height = after.height;
  d.has_distance = usable(distance);
  const size_t bands = d.has_distance ? options.edges.size() + 2 : 0;
  std::vector<Sums> sums(bands);
  Sums all;
  const int w = after.width, h = after.height;
  std::vector<uint8_t> band(static_cast<size_t>(w) * static_cast<size_t>(h), 0);
  double weighted = 0, weights = 0;
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      const size_t i = static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x);
      const uint8_t* a = &before.rgba[i * 4];
      const uint8_t* b = &after.rgba[i * 4];
      double abs = 0, sq = 0, most = 0;
      for (int c = 0; c < 3; ++c) {
        const double e = std::abs(int{a[c]} - int{b[c]});
        abs += e;
        sq += e * e;
        most = std::max(most, e);
      }
      auto add = [&](Sums& s) {
        ++s.pixels;
        s.abs += abs;
        s.sq += sq;
        s.max = std::max(s.max, most);
        if (most > options.tolerance) ++s.changed;
      };
      add(all);
      double weight = 1;
      if (d.has_distance) {
        const float m = distance_at(*distance, x, y, w, h);
        band[i] = static_cast<uint8_t>(band_of(options, m));
        add(sums[band[i]]);
        // The sky weighs as a surface 2 km off.
        weight = 1 / (1 + (std::isfinite(m) ? m : 2000.0) / options.weight_metres);
      }
      weighted += weight * abs / 3;
      weights += weight;
    }
  }
  // The SSIM of the luma, window by window; a window counts in its centre pixel's band.
  for (int y = 0; y + kWindow <= h; y += kStride) {
    for (int x = 0; x + kWindow <= w; x += kStride) {
      double ma = 0, mb = 0, va = 0, vb = 0, cov = 0;
      for (int v = 0; v < kWindow; ++v) {
        for (int u = 0; u < kWindow; ++u) {
          const size_t i =
              (static_cast<size_t>(y + v) * static_cast<size_t>(w) + static_cast<size_t>(x + u)) *
              4;
          const double la = luma(&before.rgba[i]), lb = luma(&after.rgba[i]);
          ma += la;
          mb += lb;
          va += la * la;
          vb += lb * lb;
          cov += la * lb;
        }
      }
      const double n = kWindow * kWindow;
      ma /= n;
      mb /= n;
      va = va / n - ma * ma;
      vb = vb / n - mb * mb;
      cov = cov / n - ma * mb;
      const double ssim =
          ((2 * ma * mb + kC1) * (2 * cov + kC2)) / ((ma * ma + mb * mb + kC1) * (va + vb + kC2));
      all.ssim += ssim;
      ++all.windows;
      if (d.has_distance) {
        Sums& s = sums[band[static_cast<size_t>(y + kWindow / 2) * static_cast<size_t>(w) +
                            static_cast<size_t>(x + kWindow / 2)]];
        s.ssim += ssim;
        ++s.windows;
      }
    }
  }
  // Without bands the whole picture is held to the nearest band's floor.
  d.all = finish(all, "all", d.has_distance ? 0 : floor_of(options, 0), !d.has_distance);
  for (size_t k = 0; k < bands; ++k) {
    d.bands.push_back(finish(sums[k], band_name(options, k), floor_of(options, k), true));
  }
  d.weighted_mean = weights > 0 ? weighted / weights : 0;
  d.pass = d.all.pass;
  for (const RegionDiff& r : d.bands) d.pass = d.pass && r.pass;
  return d;
}

Picture diff_sheet(const Picture& before, const Picture& after, const DistanceMap* distance,
                   const DiffOptions& options) {
  const int w = after.width, h = after.height, gap = 4, label = 14;
  Picture sheet = create_picture(2 * w + gap, 2 * (h + label) + gap, {16, 18, 24});
  const bool banded = usable(distance);
  std::vector<uint8_t> band(static_cast<size_t>(w) * static_cast<size_t>(h), 0);
  if (banded) {
    for (int y = 0; y < h; ++y) {
      for (int x = 0; x < w; ++x) {
        band[static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x)] =
            static_cast<uint8_t>(band_of(options, distance_at(*distance, x, y, w, h)));
      }
    }
  }
  auto edge = [&](int x, int y) {
    const size_t i = static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x);
    return (x + 1 < w && band[i] != band[i + 1]) ||
           (y + 1 < h && band[i] != band[i + static_cast<size_t>(w)]);
  };
  Picture change{w, h, after.rgba}, banding{w, h, after.rgba};
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      const size_t i = static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x);
      const uint8_t* a = &before.rgba[i * 4];
      uint8_t* c = &change.rgba[i * 4];
      uint8_t* f = &banding.rgba[i * 4];
      int most = 0;
      for (int k = 0; k < 3; ++k) most = std::max(most, std::abs(int{a[k]} - int{c[k]}));
      const auto dim = static_cast<uint8_t>(luma(c) * 0.3);
      const Rgba hot = heat(most);
      const bool changed = most > options.tolerance;
      const Rgba bc = band_colour(options, band[i]);
      for (int k = 0; k < 3; ++k) {
        const uint8_t hk = k == 0 ? hot.r : k == 1 ? hot.g : hot.b;
        const uint8_t bk = k == 0 ? bc.r : k == 1 ? bc.g : bc.b;
        // The distance panel: the band's colour over the picture's luma.
        f[k] = static_cast<uint8_t>(0.55 * bk + 0.45 * luma(c));
        c[k] = changed ? hk : dim;
      }
      if (banded && edge(x, y)) {
        for (int k = 0; k < 3; ++k) {
          c[k] = changed ? c[k] : 200;
          f[k] = 255;
        }
      }
    }
  }
  const Rgba text{230, 232, 240};
  auto panel = [&](const Picture& p, int col, int row, const std::string& name) {
    const int x = col * (w + gap), y = row * (h + label + gap);
    draw_text(sheet, x + 2, y + 3, name, text);
    blit(sheet, p, x, y + label);
  };
  panel(before, 0, 0, "BEFORE");
  panel(after, 1, 0, "AFTER");
  panel(change, 0, 1,
        "CHANGE: OVER " + std::to_string(options.tolerance) + " LEVELS, BLUE TO RED BY HOW MUCH");
  panel(banding, 1, 1, banded ? "DISTANCE BANDS" : "NO DISTANCE MAP");
  if (banded) {
    // Each band's name at the middle of its pixels.
    const size_t bands = options.edges.size() + 2;
    std::vector<double> sx(bands), sy(bands), n(bands);
    for (int y = 0; y < h; ++y) {
      for (int x = 0; x < w; ++x) {
        const size_t k =
            band[static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x)];
        sx[k] += x;
        sy[k] += y;
        n[k] += 1;
      }
    }
    for (size_t k = 0; k < bands; ++k) {
      if (n[k] < kJudgedPixels) continue;
      const std::string name = band_name(options, k);
      const double cx = sx[k] / n[k], cy = sy[k] / n[k];
      const double tx = (w + gap) + cx - text_width(name) / 2.0;
      const double ty = (h + label + gap) + label + cy - 4;
      fill_rect(sheet, tx - 2, ty - 2, text_width(name) + 4, kGlyphH + 3, {0, 0, 0, 170});
      draw_text(sheet, tx, ty, name, {255, 255, 255});
    }
  }
  return sheet;
}

std::vector<uint8_t> encode_distance(const DistanceMap& map) {
  std::vector<uint8_t> out(kMagic, kMagic + 8);
  auto put = [&](uint32_t v) {
    for (int k = 0; k < 4; ++k) out.push_back(static_cast<uint8_t>(v >> (8 * k)));
  };
  put(static_cast<uint32_t>(map.width));
  put(static_cast<uint32_t>(map.height));
  for (const float m : map.metres) {
    uint32_t bits;
    std::memcpy(&bits, &m, 4);
    put(bits);
  }
  return out;
}

std::optional<DistanceMap> decode_distance(std::span<const uint8_t> bytes) {
  if (bytes.size() < 16 || !std::equal(kMagic, kMagic + 8, bytes.begin())) return {};
  auto get = [&](size_t at) {
    uint32_t v = 0;
    for (int k = 0; k < 4; ++k) v |= uint32_t{bytes[at + static_cast<size_t>(k)]} << (8 * k);
    return v;
  };
  DistanceMap m;
  m.width = static_cast<int>(get(8));
  m.height = static_cast<int>(get(12));
  const size_t n = static_cast<size_t>(m.width) * static_cast<size_t>(m.height);
  if (m.width <= 0 || m.height <= 0 || m.width > 1 << 15 || m.height > 1 << 15 ||
      bytes.size() != 16 + n * 4) {
    return {};
  }
  m.metres.resize(n);
  for (size_t i = 0; i < n; ++i) {
    const uint32_t bits = get(16 + i * 4);
    std::memcpy(&m.metres[i], &bits, 4);
  }
  return m;
}

RunDiff diff_runs(const std::string& before_dir, const std::string& after_dir,
                  const std::string& out_dir, const DiffOptions& options) {
  RunDiff run{before_dir, after_dir, out_dir, {}, true, {}};
  std::error_code ec;
  if (!fs::is_directory(before_dir, ec)) {
    run.error = before_dir + " does not exist: keep a run with --tag first";
    run.pass = false;
    return run;
  }
  // The run's own shots, as its report lists them (a directory may hold an older run's too).
  std::vector<fs::path> sidecars;
  {
    std::ifstream f(fs::path(after_dir) / "report.json");
    const std::string report{std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()};
    static const std::regex kFile("\"file\": \"([^\"]+)\\.png\"");
    for (auto it = std::sregex_iterator(report.begin(), report.end(), kFile);
         it != std::sregex_iterator(); ++it) {
      sidecars.push_back(fs::path(after_dir) / ((*it)[1].str() + ".json"));
    }
  }
  std::sort(sidecars.begin(), sidecars.end());
  if (sidecars.empty()) {
    run.error = after_dir + " holds no shots to compare";
    run.pass = false;
    return run;
  }
  fs::create_directories(out_dir, ec);
  for (const fs::path& sidecar : sidecars) {
    const auto words = read_shot_json(sidecar);
    if (!words) continue;
    ShotDiff shot;
    shot.id = words->id;
    const auto a = read_png((fs::path(before_dir) / (shot.id + ".png")).string());
    const auto b = read_png((fs::path(after_dir) / (shot.id + ".png")).string());
    const auto before_words = read_shot_json(fs::path(before_dir) / (shot.id + ".json"));
    if (!a || !before_words) {
      shot.problem = "not in the kept run";
    } else if (!b) {
      shot.problem = "its picture could not be read";
    } else if (words->width == 0 || before_words->width != words->width ||
               before_words->height != words->height || a->width < words->width ||
               b->width < words->width || a->height < words->height || b->height < words->height) {
      shot.problem = "a different size from the kept run's (or a report older than the diff)";
    }
    if (!shot.problem.empty()) {
      run.shots.push_back(std::move(shot));
      continue;
    }
    const Picture pa = crop(*a, words->width, words->height);
    const Picture pb = crop(*b, words->width, words->height);
    std::optional<DistanceMap> distance;
    if (!words->distance.empty()) {
      if (const auto bytes = read_bytes(fs::path(after_dir) / words->distance)) {
        distance = decode_distance(*bytes);
      }
    }
    const DistanceMap* dm = distance ? &*distance : nullptr;
    shot.diff = diff_pictures(pa, pb, dm, options);
    Shot words_out;
    words_out.id = shot.id;
    words_out.title = "DIFF " + shot.id + (shot.diff.pass ? "" : " (OVER A FLOOR)");
    std::string caption = "all: psnr " + fixed(shot.diff.all.psnr, 1) + " dB, ssim " +
                          fixed(shot.diff.all.ssim, 4) + ", mean " + fixed(shot.diff.all.mean, 2) +
                          ", max " + fixed(shot.diff.all.max, 0) + ", changed " +
                          fixed(100 * shot.diff.all.changed, 2) + "%, near-weighted mean " +
                          fixed(shot.diff.weighted_mean, 2) + ".";
    for (const RegionDiff& r : shot.diff.bands) {
      caption += " " + r.name + ": " + fixed(r.psnr, 1) + " dB (floor " + fixed(r.min_psnr, 0) +
                 (r.judged ? (r.pass ? ", pass)" : ", OVER)") : ", not judged)") + ", ssim " +
                 fixed(r.ssim, 3) + ", changed " + fixed(100 * r.changed, 2) + "%.";
    }
    words_out.caption = caption;
    words_out.look_for =
        "changes in the near bands; a far band may change more before it fails its floor";
    words_out.inputs = {{"before", before_dir}, {"after", after_dir}};
    const fs::path sheet = fs::path(out_dir) / (shot.id + ".png");
    if (write_png(sheet.string(), framed(words_out, diff_sheet(pa, pb, dm, options)))) {
      shot.sheet = sheet.string();
    }
    run.pass = run.pass && shot.diff.pass;
    run.shots.push_back(std::move(shot));
  }
  std::ofstream json(fs::path(out_dir) / "diff.json");
  json << run_diff_json(run) << "\n";
  return run;
}

std::string format_run_diff(const RunDiff& run) {
  std::string out = "diff: " + run.before_dir + " -> " + run.after_dir + "\n";
  if (!run.error.empty()) return out + "  " + run.error + "\n";
  char line[256];
  std::snprintf(line, sizeof line, "  %-28s %-10s %9s %7s %7s %5s %8s %s\n", "shot", "band",
                "psnr dB", "ssim", "mean", "max", "changed", "verdict");
  out += line;
  for (const ShotDiff& s : run.shots) {
    if (!s.problem.empty()) {
      out += "  " + s.id + ": " + s.problem + "\n";
      continue;
    }
    auto row = [&](const RegionDiff& r) {
      const std::string verdict =
          !r.judged ? "-" : (r.pass ? "pass" : "OVER (floor " + fixed(r.min_psnr, 0) + ")");
      std::snprintf(line, sizeof line, "  %-28s %-10s %9.2f %7.4f %7.2f %5.0f %7.2f%% %s\n",
                    s.id.c_str(), r.name.c_str(), r.psnr, r.ssim, r.mean, r.max, 100 * r.changed,
                    verdict.c_str());
      out += line;
    };
    row(s.diff.all);
    for (const RegionDiff& r : s.diff.bands) {
      if (r.pixels > 0) row(r);
    }
    out += "  " + std::string(28, ' ') + " near-weighted mean " + fixed(s.diff.weighted_mean, 3) +
           "; sheet " + s.sheet + "\n";
  }
  for (const ShotDiff& s : run.shots) {
    for (const RegionDiff& r : s.diff.bands) {
      if (!r.pass) {
        out += "DIFF " + s.id + " " + r.name + ": psnr " + fixed(r.psnr, 2) +
               " dB is under its floor " + fixed(r.min_psnr, 0) + " dB; look at " + s.sheet + "\n";
      }
    }
    if (!s.diff.all.pass) {
      out += "DIFF " + s.id + ": psnr " + fixed(s.diff.all.psnr, 2) + " dB is under its floor " +
             fixed(s.diff.all.min_psnr, 0) + " dB; look at " + s.sheet + "\n";
    }
  }
  return out;
}

std::string run_diff_json(const RunDiff& run) {
  std::string j = "{\"before\": " + json_str(run.before_dir) +
                  ", \"after\": " + json_str(run.after_dir) +
                  ", \"pass\": " + (run.pass ? "true" : "false") +
                  ", \"error\": " + json_str(run.error) + ", \"shots\": [";
  for (size_t i = 0; i < run.shots.size(); ++i) {
    const ShotDiff& s = run.shots[i];
    j += std::string(i ? ", " : "") + "{\"id\": " + json_str(s.id) +
         ", \"sheet\": " + json_str(s.sheet) + ", \"problem\": " + json_str(s.problem) +
         ", \"pass\": " + (s.diff.pass ? "true" : "false") +
         ", \"distance\": " + (s.diff.has_distance ? "true" : "false") +
         ", \"weighted_mean\": " + fixed(s.diff.weighted_mean, 3) +
         ", \"all\": " + region_json(s.diff.all) + ", \"bands\": [";
    for (size_t k = 0; k < s.diff.bands.size(); ++k) {
      j += std::string(k ? ", " : "") + region_json(s.diff.bands[k]);
    }
    j += "]}";
  }
  return j + "]}";
}

}  // namespace ge::lab
