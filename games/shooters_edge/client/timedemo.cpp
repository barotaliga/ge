// The demo benchmark (timedemo.hpp): the report's arithmetic, the demo found and stepped, the
// eye and the HUD from the demo's state.
//
// ge:summary The demo benchmark's report, its demo lookup and steps, the eye and HUD off a demo
// ge:keywords bench_stats bench_report find_demo timedemo_start timedemo_step timedemo_mark
// ge:keywords demo_view demo_status one-percent-low span_add span_report prices price_report
// ge:keywords price_noise
// ge:skill shooter perf
// ge:test tests/shooters_edge_demo_test.cpp
#include "shooters_edge/client/timedemo.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>

#include "shooters_edge/client/view.hpp"

namespace shooters_edge::client {

namespace {

namespace fs = std::filesystem;

// The item bits a demo's stats carry: the weapons in the HUD's order, the blade, the armours,
// the keys.
constexpr int kWeaponBits[hud::kGuns] = {1, 2, 4, 8, 16, 32, 64};
constexpr int kBlade = 4096;
constexpr int kArmour1 = 8192, kArmour2 = 16384, kArmour3 = 32768;
constexpr int kKeySilver = 1 << 17, kKeyGold = 1 << 18;

std::string fixed(double v, int places) {
  char b[48];
  std::snprintf(b, sizeof b, "%.*f", places, v);
  return b;
}

// The slowest `share` of the frames (at least one) as a frame rate over their own time.
double low_fps(const std::vector<double>& sorted_desc, double share) {
  const size_t n =
      std::max<size_t>(1, static_cast<size_t>(static_cast<double>(sorted_desc.size()) * share));
  double sum = 0;
  for (size_t i = 0; i < n; i++) sum += sorted_desc[i];
  return sum > 0 ? static_cast<double>(n) / sum : 0;
}

std::string stem_of(const std::string& path) {
  const size_t slash = path.find_last_of('/');
  std::string name = slash == std::string::npos ? path : path.substr(slash + 1);
  const size_t dot = name.find_last_of('.');
  return dot == std::string::npos ? name : name.substr(0, dot);
}

bool read_disk(const std::string& path, std::vector<uint8_t>& out) {
  std::error_code ec;
  if (!fs::is_regular_file(path, ec)) return false;
  std::ifstream f(path, std::ios::binary);
  if (!f) return false;
  out.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
  return true;
}

}  // namespace

BenchStats bench_stats(std::span<const double> frame_s) {
  BenchStats s;
  if (frame_s.empty()) return s;
  std::vector<double> sorted(frame_s.begin(), frame_s.end());
  std::sort(sorted.begin(), sorted.end(), std::greater<>());
  s.frames = static_cast<int64_t>(sorted.size());
  for (const double f : sorted) s.seconds += f;
  s.avg_fps = s.seconds > 0 ? static_cast<double>(s.frames) / s.seconds : 0;
  s.max_fps = sorted.back() > 0 ? 1 / sorted.back() : 0;
  s.min_fps = sorted.front() > 0 ? 1 / sorted.front() : 0;
  s.low1_fps = low_fps(sorted, 0.01);
  s.low01_fps = low_fps(sorted, 0.001);
  const auto pct = [&](double q) {  // the frame that q of the frames are no longer than
    const size_t k =
        static_cast<size_t>(std::lround((1 - q) * static_cast<double>(sorted.size() - 1)));
    return sorted[std::min(k, sorted.size() - 1)] * 1e3;
  };
  s.p50_ms = pct(0.5);
  s.p99_ms = pct(0.99);
  return s;
}

std::vector<std::string> bench_report(const std::string& demo, const std::string& setup,
                                      const BenchStats& s) {
  return {
      "benchmark " + demo + ": " + std::to_string(s.frames) + " frames in " + fixed(s.seconds, 2) +
          " s (" + setup + ")",
      "  average " + fixed(s.avg_fps, 1) + " fps, max " + fixed(s.max_fps, 1) + " fps, min " +
          fixed(s.min_fps, 1) + " fps",
      "  1% low " + fixed(s.low1_fps, 1) + " fps, 0.1% low " + fixed(s.low01_fps, 1) +
          " fps; median frame " + fixed(s.p50_ms, 2) + " ms, 99th percentile " +
          fixed(s.p99_ms, 2) + " ms",
  };
}

void span_add(PassTimes& p, double ms) {
  if (!std::isnan(ms)) p.span_ms.push_back(ms);
}

double median_of(std::vector<double> v) {
  if (v.empty()) return 0;
  const auto mid = v.begin() + static_cast<std::ptrdiff_t>(v.size() / 2);
  std::nth_element(v.begin(), mid, v.end());
  return *mid;
}

std::string span_report(const PassTimes& p) {
  if (p.span_ms.empty()) return "  GPU frame: not measured (this GPU has no timestamp queries)";
  return "  GPU frame span, median " + fixed(median_of(p.span_ms), 2) +
         " ms (what each thing costs: the se-timedemo lab's --price)";
}

std::vector<Price> prices(std::span<const PriceRun> bases, std::span<const PriceRun> off) {
  std::vector<Price> out;
  for (size_t i = 0; i < off.size() && i + 1 < bases.size(); i++) {
    const double frame = (bases[i].frame_p50_ms + bases[i + 1].frame_p50_ms) / 2;
    const double span = (bases[i].span_p50_ms + bases[i + 1].span_p50_ms) / 2;
    out.push_back({off[i].name, frame - off[i].frame_p50_ms, span - off[i].span_p50_ms});
  }
  std::stable_sort(out.begin(), out.end(),
                   [](const Price& a, const Price& b) { return a.frame_ms > b.frame_ms; });
  return out;
}

double price_noise(std::span<const PriceRun> bases) {
  double most = 0;
  for (size_t i = 1; i < bases.size(); i++)
    most = std::max(most, std::abs(bases[i].frame_p50_ms - bases[i - 1].frame_p50_ms));
  return most;
}

std::vector<std::string> price_report(std::span<const PriceRun> bases,
                                      std::span<const Price> saved) {
  std::vector<std::string> out;
  if (bases.empty()) return out;
  double lo = bases[0].frame_p50_ms, hi = lo;
  for (const PriceRun& b : bases) {
    lo = std::min(lo, b.frame_p50_ms);
    hi = std::max(hi, b.frame_p50_ms);
  }
  out.push_back("  price: everything on, median frame " + fixed(lo, 2) + " to " + fixed(hi, 2) +
                " ms over " + std::to_string(bases.size()) + " runs; a saving under " +
                fixed(price_noise(bases), 2) + " ms is noise");
  for (const Price& p : saved)
    out.push_back("    " + p.name + " off saves " + fixed(p.frame_ms, 2) + " ms a frame (span " +
                  fixed(p.span_ms, 2) + ")");
  return out;
}

bool find_demo(const SearchPath& files, const std::string& name, std::vector<uint8_t>& out,
               std::string& found, std::string& error) {
  // A file's path (absolute, or from the working folder) is read from disk and nowhere else: the
  // game's folder joined with an absolute path is that path again, which is not the game's file.
  const bool is_path = fs::path(name).is_absolute() || name.starts_with("./") ||
                       name.starts_with("../") || name.starts_with(".\\") ||
                       name.starts_with("..\\");
  if (is_path) {
    if (read_disk(name, out)) {
      found = name;
      return true;
    }
    error = "no demo file at " + name + "; next: check the path, or name a demo the packs hold " +
            "(demo1, demo1.dem)";
    return false;
  }
  std::vector<uint8_t> store;
  const bool has_ext = stem_of(name) != name.substr(name.find_last_of('/') + 1);
  std::vector<std::string> tries = {name};
  if (!has_ext) tries.push_back(name + ".dem");
  for (const std::string& path : tries) {
    const std::span<const uint8_t> b = find_file(files, path, store);
    if (!b.empty()) {
      out.assign(b.begin(), b.end());
      found = path + " in the game's files";
      return true;
    }
  }
  if (!has_ext) {  // a file of that name with any extension, in the folder's top level
    std::string folded = name;
    for (char& c : folded) c = c >= 'A' && c <= 'Z' ? char(c - 'A' + 'a') : c;
    for (const ge::vfs::Listed& l : ge::vfs::list(files.vfs)) {
      if (l.name.find('/') != std::string_view::npos || stem_of(std::string(l.name)) != folded)
        continue;
      const std::span<const uint8_t> b = find_file(files, l.name, store);
      out.assign(b.begin(), b.end());
      found = std::string(ge::vfs::stored_name(files.vfs, l.ref)) + " in " +
              files.vfs.sources[l.ref.source].path;
      return true;
    }
  }
  for (const std::string& path : tries) {
    if (read_disk(path, out)) {
      found = path;
      return true;
    }
  }
  error = "no demo named " + name +
          " in the game's packs or folder, nor on disk; next: name a "
          "demo the packs hold (demo1, demo1.dem) or a file's path";
  return false;
}

bool timedemo_start(const SearchPath& files, const std::string& name, Timedemo& t,
                    std::string& error) {
  t = Timedemo{};
  t.name = name;
  std::vector<uint8_t> bytes;
  if (!find_demo(files, name, bytes, t.found, error)) return false;
  if (!open_demo(std::move(bytes), t.demo, error)) {
    error = t.found + ": " + error;
    return false;
  }
  t.start = t.demo;
  t.active = true;
  return true;
}

void DemoTimeline::frame(const DemoState& s, bool new_level) {
  if (new_level) last = -1;
  if (last >= 0) t += std::clamp(s.time - last, 0.0, kTimelineGap);
  last = s.time;
}

TimedemoStep timedemo_step(Timedemo& t, std::string& error) {
  for (;;) {
    const DemoRead r = next_frame(t.demo, error);
    if (r == DemoRead::error) return TimedemoStep::error;
    if (r == DemoRead::end) return TimedemoStep::end;
    const DemoState& s = t.demo.state;
    if (s.map.empty()) continue;  // the signon before the level's map is named: nothing to draw
    if (s.levels != t.levels) {
      t.levels = s.levels;
      t.skip = kSkipAfterLevel;
      t.last_ns = 0;
      t.timeline.frame(s, true);
      return TimedemoStep::new_level;
    }
    t.timeline.frame(s, false);
    return TimedemoStep::frame;
  }
}

TimedemoStep playdemo_step(Timedemo& t, double wall_dt_s, double speed, std::string& error) {
  if (t.clock < 0) {  // the first frame with a map starts the clock at its time
    const TimedemoStep first = timedemo_step(t, error);
    if (first == TimedemoStep::frame || first == TimedemoStep::new_level)
      t.clock = t.demo.state.time;
    return first;
  }
  // A stalled frame (a level's load, a window dragged) moves the demo on by at most a tenth of a
  // second, so the run slows over the stall rather than skipping what it would have shown.
  t.clock += std::min(wall_dt_s, kMostPlayStep) * std::clamp(speed, 0.0, 20.0);
  TimedemoStep out = TimedemoStep::frame;
  // The next demo frame is read once the clock reaches the time it will carry; its time is known
  // only after reading it, so the run reads while the last frame read is behind the clock.
  while (t.demo.state.time < t.clock) {
    const TimedemoStep s = timedemo_step(t, error);
    if (s == TimedemoStep::end || s == TimedemoStep::error) return s;
    if (s == TimedemoStep::new_level) {  // a level's clock starts over: the run's with it
      t.clock = t.demo.state.time;
      out = s;
      break;
    }
  }
  return out;
}

TimedemoStep demo_seek(Timedemo& t, double timeline_s, std::string& error) {
  const int level_before = t.levels;
  const std::string map_before = t.demo.state.map;
  if (timeline_s < t.timeline.t) {  // back: from the start again, the tracked player kept
    const int track = t.demo.track;
    t.demo = t.start;
    t.demo.track = track;
    t.timeline = {};
    t.levels = 0;
  }
  TimedemoStep out = TimedemoStep::frame;
  while (t.timeline.t < timeline_s || t.demo.state.map.empty()) {
    const TimedemoStep s = timedemo_step(t, error);
    if (s == TimedemoStep::error) return s;
    if (s == TimedemoStep::end) {
      out = TimedemoStep::end;
      break;
    }
  }
  t.clock = t.demo.state.time;
  t.skip = 0;
  if (out == TimedemoStep::end) return out;
  return t.levels != level_before || t.demo.state.map != map_before ? TimedemoStep::new_level
                                                                    : TimedemoStep::frame;
}

void timedemo_mark(Timedemo& t, uint64_t now_ns) {
  if (t.skip > 0) {
    t.skip--;
  } else if (t.last_ns != 0) {
    t.frame_s.push_back(static_cast<double>(now_ns - t.last_ns) * 1e-9);
  }
  t.last_ns = now_ns;
}

ge::render::FrameView demo_view(const DemoState& s, double fov_y, double time_s) {
  ge::render::FrameView v;
  const ge::camera::Vec3 eye = engine_point(s.eye);
  v.eye[0] = eye.x;
  v.eye[1] = eye.y;
  v.eye[2] = eye.z;
  v.heading = engine_heading(s.angles.y);
  v.pitch = engine_pitch(s.angles.x);
  v.fov_y = fov_y;
  v.time_s = time_s;
  return v;
}

hud::Status demo_status(const DemoState& s, double t) {
  hud::Status h;
  h.t = t;
  h.health = s.stats[kStatHealth];
  h.dead = h.health <= 0;
  h.armour = s.stats[kStatArmour];
  const int items = s.stats[kStatItems];
  h.armour_class = (items & kArmour3) != 0   ? hud::ArmourClass::heavy
                   : (items & kArmour2) != 0 ? hud::ArmourClass::medium
                   : (items & kArmour1) != 0 ? hud::ArmourClass::light
                                             : hud::ArmourClass::none;
  h.ammo = {s.stats[kStatShells], s.stats[kStatNails], s.stats[kStatRockets], s.stats[kStatCells]};
  h.owned = 0;
  for (int k = 0; k < hud::kGuns; k++)
    if ((items & kWeaponBits[k]) != 0) h.owned = static_cast<uint8_t>(h.owned | (1u << k));
  const int held = s.stats[kStatActiveWeapon];
  h.weapon = hud::Weapon::melee;
  if (held != kBlade)
    for (int k = 0; k < hud::kGuns; k++)
      if (held == kWeaponBits[k]) h.weapon = static_cast<hud::Weapon>(k + 1);
  h.key_silver = (items & kKeySilver) != 0;
  h.key_gold = (items & kKeyGold) != 0;
  return h;
}

}  // namespace shooters_edge::client
