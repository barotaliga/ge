// THE BENCH: what a game's step costs, measured on a run the game's own pilot played. `ge bench`,
// `ge profile` and `ge mem` are three readings of it (engine/cli/play.hpp):
//
//   ge bench     how long a step takes (median, p95, p99, worst) against the step's budget
//                (1/hz), how long the run takes to open, steps a second; --tag and --compare
//   ge profile   where the step's time goes: one row a timing zone (GE_ZONE, prof/zones.hpp),
//                its exclusive time a step and its share, the time no zone covers, the slowest
//                steps and the zone that dominated each
//   ge mem       what the run allocates: opening it, every step, the steps after the first second
//                that still allocate (a hot path that allocates) and the zone they allocate in,
//                and the process's peak resident set
//
// How it measures: the pilot plays the seed once, untimed, and its input log is replayed
// `repeats` times with every zone recording; each step is timed alone on the steady clock and its
// allocations counted (prof/memory.hpp). A step's time is its median over the repeats; the zones
// are read off one more replay, recorded the same way. Every repeat must reach the pilot's
// digest (`same_digest`): a bench that changed the run measured something else.
//
// It writes two files a seed beside the input logs, for `ge graph` and for reading:
//   <game>-<seed>.bench.tsv   one row a step: step, step_us, allocs, then <zone>_us a zone (the
//                             zone's exclusive time in that step); `# key: value` lines first
//   <game>-<seed>.spans.tsv   every zone span of the steps round the slowest one: name, depth,
//                             start_ns, dur_ns (a timeline: who called what, for how long)
//
// ge:summary The bench: a pilot's run replayed and timed per step, per zone, with its allocations
// ge:keywords play bench profile mem benchmark step-cost zones allocations outliers budget
// ge:keywords steps-per-second hotspot series spans
// ge:skill agent-interface perf
// ge:test tests/play_session_test.cpp
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "play/game.hpp"
#include "play/inputs.hpp"

namespace ge::play {

struct BenchOptions {
  int repeats = 5;
  int64_t most_steps = -1;  // only the first this many steps of the log (-1: all)
  int window = 240;         // steps of spans written round the slowest step
};

struct ZoneCost {
  std::string name;
  double calls_per_step = 0;
  double excl_us_per_step = 0;  // its own time, a step on average
  double incl_us_per_step = 0;  // with its children's
  double share = 0;             // of the step's time (exclusive)
  int64_t allocs = 0;           // exclusive, over the run (after the warm-up)
};

struct OpenZone {
  std::string name;
  int64_t calls = 0;
  double excl_ms = 0, incl_ms = 0;
};

struct SlowStep {
  int64_t step = 0;
  double us = 0;
  std::string zone;  // the zone with the most exclusive time in it ("" when none was open)
};

struct BenchRun {
  int64_t steps = 0;
  uint64_t digest = 0;
  bool same_digest = true;
  int64_t dropped_spans = 0;  // zones past the recording's room (24 a step): not in the rows
  double open_ms = 0;         // opening the run (the game's setup), median over the repeats
  int64_t open_allocs = 0;
  std::vector<OpenZone> open_zones;  // the zones the open ran (a map build, a bake)
  // A step's microseconds, median over the repeats, and its allocations.
  std::vector<double> step_us;
  std::vector<int64_t> step_allocs;
  std::vector<ZoneCost> zones;    // by exclusive time, most first
  double untracked_share = 0;     // of the step's time, in no zone
  std::vector<SlowStep> slowest;  // the five slowest steps, slowest first
  int64_t warmup_steps = 0;       // the first second: allocations there are expected
  int64_t steps_allocating = 0;   // after the warm-up
  int64_t first_allocating = -1;
  std::string alloc_zone;  // the zone that allocated most after the warm-up
  // The files' text.
  std::string series;
  std::string spans;
};

std::optional<BenchRun> bench(const GameSpec& spec, const InputLog& log, const BenchOptions& o,
                              std::string& error);

// The value at share q (0..1) of `v` (the nearest rank; `v` is copied and sorted).
double quantile(std::vector<double> v, double q);

}  // namespace ge::play
