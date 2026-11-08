// LAB `se-demo-smooth`: IS A PLAYED DEMO SMOOTH? A recording played the window's way
// (timedemo.hpp's paced `playdemo_step`) at a display's picture rate, twice: each picture showing
// the newest frame read (`demo_lerp 0`), and each drawn between the last two frames read
// (demo_lerp.hpp, `demo_lerp 1`, the default). By default the demo is a multiview match written in
// code (demo_write.hpp: a player walking a circle at a steady 320 units a second, recorded at 77
// frames a second as a modern server writes them, a second one standing); `--file=<path>` reads a
// real one (plain, packed or in an archive), `--track=<slot>` picks the view's player.
//
// What it shows: per picture, how far the view's eye moved on the plan (a steady walk should move
// it the same each picture; a picture that repeats the last frame moves it 0 and the next one
// twice as far), as a chart of a stretch of pictures each way and a table: the mean step, its
// spread, the mean change from one step to the next (the judder), the share of pictures that
// stood still while the player walked (repeats), and the pictures drawn between frames. A third
// chart is the eye's x over the same stretch: a staircase each way. Everything is deterministic.
//
// ge:summary Lab: a demo played at a display's rate with and without drawing between its frames
// ge:keywords lab se-demo-smooth demo smooth lerp interpolation judder stutter jerky repeat
// ge:keywords playdemo demo_lerp multiview mvd picture-rate step-length
// ge:skill shooter labs
// ge:test tests/shooters_edge_demo_lerp_test.cpp
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "core/num_text.hpp"
#include "lab/chart.hpp"
#include "lab/lab.hpp"
#include "shooters_edge/client/demo.hpp"
#include "shooters_edge/client/demo_pack.hpp"
#include "shooters_edge/client/demo_write.hpp"
#include "shooters_edge/client/timedemo.hpp"

#include <fstream>
#include <iterator>

namespace {

namespace gl = ge::lab;
namespace cl = shooters_edge::client;
using ge::core::num::to_fixed;

// One play of the demo: per picture, the clock, the eye on the plan, and whether it was between.
struct Play {
  std::vector<double> clock, x, y, step;
  int64_t between = 0;
  int64_t snapped = 0;  // players and entities a rule drew at the newer frame, summed
};

struct Numbers {
  double mean = 0, spread = 0, judder = 0, repeats = 0;
};

// A steady walk round a circle of `radius` units at `speed` units a second, `fps` frames a
// second for `seconds`, as a match: the walker in slot 0, a second player standing in slot 1.
cl::MatchScript walk(double fps, double seconds) {
  cl::MatchScript m;
  m.frame_s = static_cast<float>(1.0 / fps);
  m.frames = static_cast<int>(seconds * fps);
  m.players.resize(2);
  m.players[0].name = "walker";
  m.players[1].name = "stander";
  constexpr double kRadius = 384, kSpeed = 320;
  for (int f = 0; f < m.frames; f++) {
    // The milliseconds byte is whole: the clock a frame is f * whole-ms, as the reader sums it.
    const double t = f * std::floor(1000.0 / fps) / 1000.0;
    const double a = t * kSpeed / kRadius;
    // Whole eighths of a unit, as the wire carries them.
    const auto eighth = [](double v) { return static_cast<float>(std::round(v * 8) / 8); };
    m.players[0].origins.push_back({eighth(512 + kRadius * ge::core::libm::cos(a)),
                                    eighth(512 + kRadius * ge::core::libm::sin(a)), 24});
    m.players[1].origins.push_back({512, 512, 24});
  }
  return m;
}

bool play(std::vector<uint8_t> bytes, int track, bool smooth, double fps, Play& out,
          std::string& error) {
  cl::Timedemo t;
  if (!cl::open_demo(std::move(bytes), t.demo, error)) return false;
  t.start = t.demo;
  t.active = true;
  t.paced = true;
  t.smooth = smooth;
  t.demo.track = track;
  double px = 0, py = 0;
  bool have = false;
  for (int64_t i = 0; i < 4'000'000; i++) {
    const cl::TimedemoStep st = cl::playdemo_step(t, 1.0 / fps, 1, error);
    if (st == cl::TimedemoStep::error) return false;
    if (st == cl::TimedemoStep::end) break;
    if (st == cl::TimedemoStep::new_level) have = false;
    const cl::DemoState& d = cl::demo_drawn(t);
    if (d.map.empty() || d.intermission) continue;
    const double x = d.eye.x, y = d.eye.y;
    const double step = have ? ge::core::hypot(x - px, y - py) : 0;
    px = x;
    py = y;
    if (!have || step > 200) {  // a level's first picture, a respawn, a teleport: no step
      have = true;
      continue;
    }
    out.clock.push_back(static_cast<double>(i + 1) / fps);  // the picture's own time
    out.x.push_back(x);
    out.y.push_back(y);
    out.step.push_back(step);
    if (smooth && t.lerp.f < 1) out.between++;
    if (smooth) out.snapped += t.lerp.snapped;
  }
  return true;
}

// The steps of the pictures where the player was walking (the step or its neighbours moved).
Numbers numbers(const Play& p) {
  Numbers n;
  std::vector<double> walking;
  int64_t repeats = 0, judder_n = 0;
  double judder = 0;
  for (size_t i = 1; i + 1 < p.step.size(); i++) {
    const bool moving = p.step[i - 1] > 0.5 || p.step[i + 1] > 0.5;
    if (!moving) continue;
    walking.push_back(p.step[i]);
    if (p.step[i] < 1e-6) repeats++;
    judder += std::fabs(p.step[i] - p.step[i - 1]);
    judder_n++;
  }
  if (walking.empty()) return n;
  for (double s : walking) n.mean += s;
  n.mean /= static_cast<double>(walking.size());
  for (double s : walking) n.spread += (s - n.mean) * (s - n.mean);
  n.spread = std::sqrt(n.spread / static_cast<double>(walking.size()));
  n.judder = judder_n > 0 ? judder / static_cast<double>(judder_n) : 0;
  n.repeats = static_cast<double>(repeats) / static_cast<double>(walking.size());
  return n;
}

void run(gl::Lab& lab) {
  const std::string file = lab.arg("file");
  const double fps = std::max(1.0, lab.arg_num("fps"));
  const double rec_fps = std::max(1.0, lab.arg_num("record-fps"));
  const int track = static_cast<int>(lab.arg_num("track"));
  const double from = lab.arg_num("from"), span = std::max(0.1, lab.arg_num("seconds"));
  std::vector<uint8_t> bytes;
  if (file.empty()) {
    bytes = cl::write_multiview_match(walk(rec_fps, 6));
  } else {
    std::ifstream in(file, std::ios::binary);
    if (!in) {
      lab.fail("se-demo-smooth: " + file + " does not open", "next: pass --file=<a demo's path>");
      return;
    }
    bytes.assign(std::istreambuf_iterator<char>(in), {});
  }
  const std::vector<std::pair<std::string, std::string>> in = {
      {"file", file.empty() ? "(a walk written in code)" : file},
      {"fps", to_fixed(fps, 0)},
      {"track", std::to_string(track)}};

  Play raw, smooth;
  std::string error;
  if (!play(bytes, track, false, fps, raw, error) ||
      !play(bytes, track, true, fps, smooth, error)) {
    lab.fail("se-demo-smooth: " + error, "next: check the file with the se-demo lab");
    return;
  }
  const Numbers a = numbers(raw), b = numbers(smooth);
  gl::LabTable t{"the view's eye per picture at " + to_fixed(fps, 0) + " pictures a second",
                 {"drawn", "pictures", "mean step u", "spread u", "judder u", "repeats", "between"},
                 {}};
  const auto row = [&](const char* name, const Play& p, const Numbers& n) {
    t.rows.push_back(
        {name, std::to_string(p.step.size()), to_fixed(n.mean, 2), to_fixed(n.spread, 2),
         to_fixed(n.judder, 2), to_fixed(n.repeats * 100, 1) + "%",
         to_fixed(p.step.empty()
                      ? 0.0
                      : 100.0 * static_cast<double>(p.between) / static_cast<double>(p.step.size()),
                  1) +
             "%"});
  };
  row("each frame as read (demo_lerp 0)", raw, a);
  row("between frames (demo_lerp 1)", smooth, b);
  lab.table(t);
  for (const auto& [name, n] : {std::pair{"raw", a}, std::pair{"smooth", b}}) {
    lab.metric(std::string(name) + ".mean-step", n.mean, "u");
    lab.metric(std::string(name) + ".spread", n.spread, "u");
    lab.metric(std::string(name) + ".judder", n.judder, "u");
    lab.metric(std::string(name) + ".repeats", n.repeats, "share");
  }
  lab.metric("smooth.between", static_cast<double>(smooth.between), "count");
  lab.metric("smooth.snapped", static_cast<double>(smooth.snapped), "count");
  lab.metric("pictures", static_cast<double>(smooth.step.size()), "count");

  // The stretch charted: from `from` s of pictures (the middle when negative), `span` long.
  const double end = raw.clock.empty() ? 0 : raw.clock.back();
  const double t0 = from >= 0 ? from : std::max(0.0, end / 2 - span / 2);
  const auto cut = [&](const Play& p, const std::vector<double>& v, const std::string& label) {
    gl::Trace tr{label, {}, {}};
    for (size_t i = 0; i < p.clock.size(); i++)
      if (p.clock[i] >= t0 && p.clock[i] <= t0 + span) {
        tr.x.push_back(p.clock[i]);
        tr.y.push_back(v[i]);
      }
    return tr;
  };
  gl::LineChart steps;
  steps.spikes = false;
  steps.points = true;
  steps.frame = {"HOW FAR THE VIEW MOVED EACH PICTURE",
                 (file.empty() ? std::string("a steady 320 u/s walk recorded at ") +
                                     to_fixed(rec_fps, 0) + " frames a second"
                               : file) +
                     ", drawn at " + to_fixed(fps, 0) + " pictures a second",
                 "t (s)",
                 "eye step on the plan (units)",
                 960,
                 420};
  steps.traces = {cut(raw, raw.step, "each frame as read"),
                  cut(smooth, smooth.step, "between frames")};
  const gl::Chart c = gl::line_chart(steps);
  lab.shot(
      {"steps", c.title,
       "The distance the view's eye moved on the plan from one picture to the next. Read as is, "
       "a picture shows the newest frame: some pictures repeat the last (0) and the next "
       "jumps two frames; drawn between the last two frames, each picture moves by the time "
       "that passed.",
       "between frames, the dots lie on one band at the walk's speed over the picture rate; "
       "as read, they split between 0, one frame's step and two",
       in},
      gl::to_picture(c.plot));
  gl::LineChart xs;
  xs.spikes = false;
  xs.points = true;
  xs.frame = {"THE VIEW'S EYE ALONG X", steps.frame.subtitle, "t (s)", "eye x (units)", 960, 420};
  const double xspan = std::min(span, 0.25);
  const auto cut_x = [&](const Play& p, const std::string& label) {
    gl::Trace tr{label, {}, {}};
    for (size_t i = 0; i < p.clock.size(); i++)
      if (p.clock[i] >= t0 && p.clock[i] <= t0 + xspan) {
        tr.x.push_back(p.clock[i]);
        tr.y.push_back(p.x[i]);
      }
    return tr;
  };
  xs.traces = {cut_x(raw, "each frame as read"), cut_x(smooth, "between frames")};
  const gl::Chart cx = gl::line_chart(xs);
  lab.shot(
      {"eye-x", cx.title,
       "The eye's x picture by picture over a quarter of a second: as read it holds, then jumps "
       "when a frame arrives; between frames it runs on a line through the frames' places, a "
       "frame behind.",
       "as read, pairs of dots at one height (a repeat) or a wide gap; between frames, evenly "
       "spaced",
       in},
      gl::to_picture(cx.plot));
  if (b.judder > a.judder + 1e-9)
    lab.fail("se-demo-smooth: drawn between frames juddered more (" + to_fixed(b.judder, 2) +
                 " u) than as read (" + to_fixed(a.judder, 2) + " u)",
             "next: read client/demo_lerp.cpp's lerp_at; skill: shooter");
}

const gl::LabSpec kSpec{
    "se-demo-smooth",
    gl::LabKind::sim,
    "game:shooters_edge",
    "games/shooters_edge/client: a demo drawn between its frames (demo_lerp.hpp) in the paced "
    "playback (timedemo.hpp)",
    "shooter",
    "Does a played demo move smoothly at a display's picture rate, and by how much more than "
    "drawing each frame as read?",
    {
        {"file", "",
         "a demo's path (plain, packed or in an archive); empty: a walk written in code"},
        {"track", "-1",
         "the slot the view rides in a multiview recording (-1: the lowest playing)"},
        {"fps", "60", "pictures a second the demo is drawn at"},
        {"record-fps", "77", "the written walk's frames a second (a modern server's default)"},
        {"from", "-1", "where the charts start, s of play (-1: the middle)"},
        {"seconds", "2", "how long the charts run, s"},
    },
    "",
    run,
};
GE_LAB(kSpec);

}  // namespace
