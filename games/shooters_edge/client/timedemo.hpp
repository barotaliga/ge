// THE DEMO BENCHMARK: a recorded game (demo.hpp) played back as fast as the machine draws it, at
// the settings the window has, and the frame rate it reached. The window runs it from the console
// (`benchmark <demo>`) or the command line (`-benchmark <demo>`, which quits with the report);
// the `se-timedemo` lab runs the same loop into an offscreen target.
//
// THE RULE: ONE DEMO FRAME A DRAWN FRAME, whatever the demo's own clock says (a single-player
// block, or a client recording's packet). So every machine draws the same frames and the time it
// took is the measure; the demo's clock is not waited on. The window draws every frame but shows
// only some (main.cpp: a window's present waits for the display on some platforms whatever its
// mode), and has no frame cap. A level start loads its map from the search path
// and rebuilds the world's frame, and the frames until `kSkipAfterLevel` draws later are not
// timed (the load, the pipelines' first use).
//
// THE REPORT (`bench_stats`): the frames timed and the seconds they took; the AVERAGE frame rate
// (frames over seconds), the MAX (one over the shortest frame) and the MIN (one over the
// longest); the 1% LOW and the 0.1% LOW: the slowest 1% (0.1%) of frames, at least one, as a
// frame rate over their own time; the median and 99th-percentile frame in milliseconds.
//
// THE GPU'S FRAME (`PassTimes`): each timed frame's GPU span (its first pass's start to its last
// pass's end), a frame or more late (the timer's); a GPU without timestamp queries gives none and
// the report says so. A pass's own timestamps do not say what it costs: a tiled GPU overlaps the
// passes, and each one's reading stretches over its neighbours' work.
//
// THE PRICE (`prices`): what each thing the look draws costs, found the one honest way: the demo
// played again with that thing switched off, its median frame and span against the base run's.
// The base is played first and last, so the two give the noise a saving has to clear.
//
// FINDING THE DEMO (`find_demo`): the name as given through the search path (a pack's
// `demo1.dem`, a loose file in the game folder), then with `.dem` added, then the first file in
// the search path whose name before its extension is the name; then the name as a path on disk.
//
// ge:summary The demo benchmark: a recording played flat out, one demo frame a drawn frame
// ge:keywords timedemo benchmark demo fps frame-rate average max min one-percent-low low
// ge:keywords percentile frame-time find_demo bench_stats bench_report timedemo_start
// ge:keywords timedemo_step timedemo_mark demo_view demo_status unlocked-fps vsync-off
// ge:keywords gpu-span span_add span_report price prices price_report switch-off-pricing
// ge:keywords playdemo playdemo_step cl_demospeed demo-pace
// ge:skill shooter perf
// ge:test tests/shooters_edge_demo_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "render/view.hpp"
#include "shooters_edge/client/demo.hpp"
#include "shooters_edge/client/files.hpp"
#include "shooters_edge/hud/status.hpp"

namespace shooters_edge::client {

inline constexpr int kSkipAfterLevel = 3;     // frames not timed after a level starts
inline constexpr double kMostPlayStep = 0.1;  // playback: the most wall time one frame counts, s

struct BenchStats {
  int64_t frames = 0;
  double seconds = 0;
  double avg_fps = 0, max_fps = 0, min_fps = 0, low1_fps = 0, low01_fps = 0;
  double p50_ms = 0, p99_ms = 0;
};

// The report over the timed frames' lengths, s each.
BenchStats bench_stats(std::span<const double> frame_s);
// The report as lines for the console and the terminal: what was played and at what.
std::vector<std::string> bench_report(const std::string& demo, const std::string& setup,
                                      const BenchStats& s);

// The GPU's span of each timed frame over a run, ms.
struct PassTimes {
  std::vector<double> span_ms;
};
// A frame's GPU span (NaN, not measured, left out).
void span_add(PassTimes& p, double ms);
// The report's GPU line: the median span, or that this GPU has no timestamp queries.
std::string span_report(const PassTimes& p);

// One run of a price: what was switched off (empty for the base) and its medians, ms (span NaN
// when the GPU has no timestamp queries).
struct PriceRun {
  std::string name;
  double frame_p50_ms = 0;
  double span_p50_ms = 0;
};
// What switching a thing off saved against the base: the frame's and the span's, ms.
struct Price {
  std::string name;
  double frame_ms = 0;
  double span_ms = 0;
};
// Each run's saving against `base`, the most saved first.
std::vector<Price> prices(const PriceRun& base, std::span<const PriceRun> off);
// The price as lines: the base's two runs (their gap is the noise) and each saving, most first.
std::vector<std::string> price_report(const PriceRun& base, const PriceRun& again,
                                      std::span<const Price> saved);
// The median of `v` (0 when empty).
double median_of(std::vector<double> v);

// The demo `name` (see above) as bytes; `found` says where it came from. False with the places
// tried and the next step.
bool find_demo(const SearchPath& files, const std::string& name, std::vector<uint8_t>& out,
               std::string& found, std::string& error);

struct Timedemo {
  std::string name;   // as asked
  std::string found;  // where it was read from
  Demo demo;
  std::vector<double> frame_s;  // the timed frames, s
  PassTimes passes;             // the timed frames' GPU spans
  int skip = 0;                 // frames still to go untimed
  int levels = 0;               // the level starts the window has loaded
  uint64_t last_ns = 0;         // the last mark (0: none yet)
  bool active = false;
  bool quit_after = false;  // the command line's run: quit with the report
  bool paced = false;       // played at the demo's pace (playdemo), not flat out
  double clock = -1;        // the paced run's demo clock, s (-1: not started)
};

// `name` found and opened; the run begins at the next step.
bool timedemo_start(const SearchPath& files, const std::string& name, Timedemo& t,
                    std::string& error);

enum class TimedemoStep : uint8_t {
  frame,      // draw this frame
  new_level,  // load `t.demo.state.map`, then draw this frame
  end,        // the demo is over: report
  error,      // a frame did not read: report what was timed and the error
};
// The next demo frame.
TimedemoStep timedemo_step(Timedemo& t, std::string& error);

// PLAYBACK AT THE DEMO'S OWN PACE (`playdemo <demo>`): the same run, but each drawn frame reads
// the demo frames up to the demo's clock moved on by the frame's wall time (at most
// `kMostPlayStep`) times `speed` (`cl_demospeed`, 0..20; 0 pauses), none when the clock has not
// reached the next one. A level start stops the step there as `new_level` and sets the clock to
// that level's time; the frames are not timed.
TimedemoStep playdemo_step(Timedemo& t, double wall_dt_s, double speed, std::string& error);

// A frame presented at `now_ns`: its length is timed unless it is one of the untimed frames.
void timedemo_mark(Timedemo& t, uint64_t now_ns);

// The view of the demo's eye: its look, the window's field of view `fov_y` (rad).
ge::render::FrameView demo_view(const DemoState& s, double fov_y, double time_s);
// The HUD's snapshot from the demo's stats.
hud::Status demo_status(const DemoState& s, double t);

}  // namespace shooters_edge::client
