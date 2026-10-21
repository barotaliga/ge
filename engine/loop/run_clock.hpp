// THE CLOCK AN APP STEPS ITS ENGINE ON. Port of oss-game-framework `src/loop/run-clock.ts`,
// bit for bit. The display hands frames at whatever rate it likes; the simulation takes steps
// of exactly 1/hz. This is the accumulator between the two. The host feeds it each frame's
// elapsed wall time and the window's visibility; it hands back how many steps to take.
//
// THE THREE DECISIONS, each stated once:
// - THE FRAME DELTA IS CLAMPED to `max_frame` (kMaxFrameSeconds by default) and THE TIME BEYOND
//   IT IS DROPPED, never paid down: after a hitch the run resumes where it was, at most a
//   tenth of a second behind, instead of simulating the stall a hundred steps a frame.
// - LOSING FOCUS PAUSES THE RUN (single player): nothing steps until `resume`, and resume
//   empties the accumulator so the first frame back is one frame long.
// - THE SIMULATION NEVER LEARNS THE FRAME RATE. Steps are whole; the leftover fraction carries
//   to the next frame, and `alpha` is for the renderer's interpolation only.
// Parity: tests/loop_run_clock_test.cpp against tests/golden/loop_run_clock.txt.
//
// ge:summary The fixed-step accumulator between display frames and the 120 Hz step
// ge:keywords loop run-clock fixed-step timestep accumulator frame alpha interpolation pause resume
// ge:keywords focus createRunClock MAX_FRAME_SECONDS
// ge:oracle oss-game-framework/src/loop/run-clock.ts
// ge:skill sim
// ge:test tests/loop_run_clock_test.cpp
#pragma once

#include <cmath>

#include "core/js.hpp"

namespace ge::loop {

// The most frame time one frame may hand the accumulator, s: twelve steps at 120 Hz. A frame
// much longer than that is a stall, not a slow frame.
inline constexpr double kMaxFrameSeconds = 0.1;

struct RunClock {
  double hz;           // fixed steps per second
  double dt;           // 1 / hz, s
  double max_frame;    // the clamp, s
  double acc = 0;      // unspent frame time, s
  double dropped = 0;  // frame time the clamp has dropped so far, s (a stutter diagnostic)
  bool paused = false;
};

inline RunClock create_run_clock(double hz, double max_frame = kMaxFrameSeconds) {
  return RunClock{hz, 1.0 / hz, max_frame};
}

// Hand the clock `elapsed` seconds of wall time since the last frame; returns how many fixed
// steps to take now (0 while paused). A NaN `elapsed` poisons the accumulator exactly as in
// the oracle, which then returns NaN steps; here that is 0 steps, the same number taken.
inline int frame(RunClock& c, double elapsed) {
  if (c.paused) return 0;
  const double taken = core::js::min(core::js::max(0.0, elapsed), c.max_frame);
  c.dropped += core::js::max(0.0, elapsed) - taken;
  c.acc += taken;
  const double steps = std::floor(c.acc * c.hz + 1e-9);
  c.acc -= steps * c.dt;
  if (c.acc < 0) c.acc = 0;
  return steps == steps ? static_cast<int>(steps) : 0;
}

// The fraction of a step left after the last frame, 0..1: the renderer's interpolation weight,
// never the engine's.
inline double alpha(const RunClock& c) { return c.acc * c.hz; }

// The window went away: nothing steps until resume.
inline void pause(RunClock& c) { c.paused = true; }

// The window is back: the absence is forgotten and the next frame is one frame long.
inline void resume(RunClock& c) {
  c.paused = false;
  c.acc = 0;
}

}  // namespace ge::loop
