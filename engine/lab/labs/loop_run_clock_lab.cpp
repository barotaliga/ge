// LAB `run-clock`: how the fixed-step clock (engine/loop/run_clock.hpp) turns a display's frames
// into 120 Hz steps, at the frame rates a player's screen runs at. A judder that lives between
// steps shows only at a rate that does not divide the step rate (144 beside 60), so this lab
// is that cadence on its own: how many
// steps each frame takes, how far `alpha` (the renderer's interpolation weight) swings, and
// what a hitch costs. Frame times can be jittered off a seeded stream, so the run is
// deterministic and its digest is a determinism test.
//
// ge:summary Lab: the run clock's cadence (steps a frame, alpha, hitches) at display rates
// ge:keywords lab run-clock cadence fps frame step alpha jitter hitch judder interpolation
// ge:skill labs sim
// ge:test tests/lab_runner_test.cpp
#include <cstdio>
#include <string>

#include "core/prng.hpp"
#include "lab/lab.hpp"
#include "loop/run_clock.hpp"

namespace {

using ge::lab::Det;

std::string fmt(double v, int digits) {
  char b[32];
  std::snprintf(b, sizeof b, "%.*f", digits, v);
  return b;
}

void run(ge::lab::Lab& lab) {
  const double hz = lab.arg_num("hz");
  const double seconds = lab.arg_num("seconds");
  const double jitter = lab.arg_num("jitter");
  const double hitch = lab.arg_num("hitch");
  ge::lab::LabTable t{
      "cadence at " + fmt(hz, 0) + " Hz over " + fmt(seconds, 1) + " s",
      {"fps", "frames", "steps", "0-step", "2+-step", "max", "alpha-min", "alpha-max", "dropped-s"},
      {}};
  for (const double fps : lab.arg_nums("fps")) {
    auto clock = ge::loop::create_run_clock(hz);
    auto rng = ge::core::make_rng(static_cast<uint32_t>(lab.arg_num("seed")));
    const int frames = static_cast<int>(seconds * fps);
    // At a rate that divides the step rate, every frame should take exactly hz / fps steps.
    const double per = hz / fps;
    const bool divides = per >= 1 && per == static_cast<int>(per);
    int steps = 0, zero = 0, many = 0, most = 0, uneven = 0;
    double amin = 1, amax = 0;
    for (int f = 0; f < frames; f++) {
      double elapsed = (1.0 / fps) * (1.0 + jitter * (2 * ge::core::next(rng) - 1));
      if (hitch > 0 && f == frames / 2) elapsed += hitch;
      const int n = ge::loop::frame(clock, elapsed);
      steps += n;
      zero += n == 0;
      many += n >= 2;
      most = n > most ? n : most;
      uneven += divides && n != static_cast<int>(per);
      const double a = ge::loop::alpha(clock);
      amin = a < amin ? a : amin;
      amax = a > amax ? a : amax;
    }
    const std::string key = "fps-" + fmt(fps, fps == static_cast<int>(fps) ? 0 : 2);
    lab.metric(key + ".steps-per-frame", frames ? double(steps) / frames : 0, "steps");
    lab.metric(key + ".zero-step-frames", zero, "count");
    lab.metric(key + ".multi-step-frames", many, "count");
    lab.metric(key + ".dropped", clock.dropped, "s");
    t.rows.push_back({fmt(fps, 2), std::to_string(frames), std::to_string(steps),
                      std::to_string(zero), std::to_string(many), std::to_string(most),
                      fmt(amin, 3), fmt(amax, 3), fmt(clock.dropped, 3)});
    if (hitch == 0 && jitter == 0 && uneven) {
      lab.fail("at " + fmt(fps, 2) + " fps, a rate dividing " + fmt(hz, 0) + " Hz, " +
                   std::to_string(uneven) + " frames did not take exactly " + fmt(per, 0) +
                   " steps",
               "The clock's epsilon or its clamp moved: run tests/loop_run_clock_test.cpp. "
               "Skill: sim.");
    }
  }
  lab.table(std::move(t));
  lab.note(
      "Uneven frames (0 steps beside 1, or 1 beside 2) at a rate that does not divide the "
      "step rate are expected; the renderer hides them with alpha. At a dividing rate "
      "without jitter every frame takes the same number of steps, or the lab fails.");
}

const ge::lab::LabSpec kSpec{
    "run-clock",
    ge::lab::LabKind::sim,
    "engine",
    "engine/loop/run_clock.hpp",
    "sim",
    "How many fixed steps does each display frame take, and how far does alpha swing?",
    {
        {"fps", "60,144,30,165,59.94", "display rates, comma-separated"},
        {"hz", "120", "the step rate, Hz"},
        {"seconds", "10", "how long each rate runs"},
        {"jitter", "0", "frame-time jitter, a fraction of a frame (0.2 = ±20%)"},
        {"seed", "1", "the jitter's stream"},
        {"hitch", "0", "one stall this many seconds long, halfway (the clamp drops the excess)"},
    },
    "--seconds=2 --jitter=0.1",
    run,
};
GE_LAB(kSpec);

}  // namespace
