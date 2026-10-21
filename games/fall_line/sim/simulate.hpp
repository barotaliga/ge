// THE HEADLESS RUN: a pilot at the controls of a run from the start to the finish (or the time
// limit), every step's controls snapped and recorded on the control tape, the whole reduced to a
// `RunReport`. It is what the `sim` lab prints a row of, what the tests hold golden, and, with a
// tape handed in, a replay: a run is a pure function of its map and its controls, so the tape
// played back gives the same digest.
//
// ge:summary The headless run: simulate a pilot over a map, the RunReport, the tape, replay
// ge:keywords simulate sim run-report report digest tape replay headless time-limit trace
// ge:keywords simulate_on replay_tape
// ge:skill sim
// ge:test tests/fall_line_sim_test.cpp
#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include "mountain/mapgen/types.hpp"
#include "racing/tape.hpp"

namespace fall_line::sim {

struct SimOptions {
  double max_seconds = 600;    // the run's time limit
  std::optional<int> version;  // the generator's
};

// The player's run, as the sim lab's table prints it.
struct RunReport {
  double seed = 0;
  bool finished = false;
  double time = 0;         // s, the finish's clock (or the limit)
  int32_t gates = 0;       // gates taken, the start gate and the finish among them
  int32_t gate_count = 0;  // gates on the course
  int32_t missed = 0;
  int32_t resets = 0;
  int32_t wipeouts = 0;
  int32_t flights = 0;
  double top_speed = 0;   // m/s
  double mean_speed = 0;  // m/s, the piste's length over the time (finished runs)
  double max_landing_g = 0;
  int64_t steps = 0;
  uint64_t digest = 0;  // FNV-1a over every step's state
  ge::racing::ControlTape tape;
};

// A point of the player's line, every tenth of a second, for a picture.
struct LinePoint {
  double x, z, speed;
  bool on_snow, down;
};

// The bot down the map built for `seed`.
RunReport simulate(double seed, const SimOptions& opts = {});
// The bot down `level`; `line`, when given, gets the player's line.
RunReport simulate_on(std::shared_ptr<const ge::mountain::mapgen::Level> level, double seed,
                      const SimOptions& opts, std::vector<LinePoint>* line = nullptr);
// `tape` played back on `level`: the same run, step for step (its digest is the report's).
RunReport replay_tape(std::shared_ptr<const ge::mountain::mapgen::Level> level, double seed,
                      const ge::racing::ControlTape& tape);

}  // namespace fall_line::sim
