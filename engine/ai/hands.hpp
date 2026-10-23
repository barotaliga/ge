// THE BOT'S HANDS: what stands between what a bot WANTS on its controls and what it presses. A
// bot decides perfectly and instantly; a person reacts late, cannot move a stick across its
// travel in one step, wanders a little off what they meant and overshoots a quick move. `Hands`
// is that, per axis, as a filter the bot's wanted controls pass through every step:
//
// 1. DELAY: the wanted value `delay_s` ago (a ring of past steps; a reaction time).
// 2. WANDER: plus a slow noise of `wander` amplitude, off the hands' own stream (`core::Rng`,
//    seeded by the caller per bot, never the game's), so a field of bots wanders apart. It fades
//    out near the axis's ends and its `rest`: a trigger pressed to its stop or let go, and a
//    stick let back to its centre, sit still.
// 3. THE HAND: a spring toward that target that settles in about `settle_s`, damped by
//    `damping` (1 settles without passing; under 1 overshoots a quick move and comes back), its
//    speed held under `slew` units a second. `settle_s` of 0 is no spring: the target, slewed.
// 4. REACH: clamped to the axis's travel, `lo`..`hi`.
//
// An axis with `pass` set (a button, a flag) goes through untouched. Hands with every axis at
// its default pass everything through unchanged, so a game can hold one for every bot and
// switch them on by profile. No transcendental is called: the bits are the same everywhere.
//
// Where the hands change what is pressed, the bot's INTENT is worth keeping beside it: a pilot
// records a decision called "hands" whose facts are each axis's wanted value (`record_intent`),
// and the humanness metrics (human.hpp) measure the delay between what was wanted and what was
// pressed off those facts.
//
// ge:summary The bot's hands: a reaction delay, a slow wander, a damped slewed hand, per axis
// ge:keywords ai bot hands humanize reaction delay slew wander noise overshoot spring damping
// ge:keywords profile intent human-like controls filter
// ge:skill bots
// ge:test tests/ai_hands_test.cpp
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include "ai/why.hpp"
#include "core/prng.hpp"

namespace ge::ai {

struct AxisHands {
  bool pass = false;       // a button or a flag: passed through untouched
  double delay_s = 0;      // reaction delay, s
  double wander = 0;       // the slow noise's amplitude, axis units
  double wander_hz = 0.5;  // about how often the wander turns, Hz
  double settle_s = 0;     // the hand's spring settles in about this long, s (0: no spring)
  double damping = 1;      // the spring's damping ratio: under 1 overshoots
  double slew = 0;         // the most the hand moves a second, axis units (0: no limit)
  double lo = -1, hi = 1;  // the axis's travel
  double rest = 0;         // where it sits let go
};

// The most axes hands hold, and the longest delay in steps (0.8 s at 120 Hz). Hands keep no heap
// storage, so a copy is cheap: a pilot asked what it would press without stepping (the play
// protocol's `why` op) works on a copy and keeps it only when the step is taken.
inline constexpr size_t kMostHandAxes = 6;
inline constexpr size_t kMostDelaySteps = 96;

class Hands {
 public:
  Hands() = default;
  // Hands for `axes.size()` axes (at most kMostHandAxes) at `hz` steps a second, their wander off
  // `seed`; a delay is held to kMostDelaySteps.
  Hands(std::span<const AxisHands> axes, double hz, uint32_t seed);

  size_t axes() const { return count_; }
  // `wanted` (one value an axis) through the hands, into `out`; one call a step.
  void step(std::span<const double> wanted, std::span<double> out);
  // Back to rest: the hands at `at` (one value an axis), nothing remembered.
  void reset(std::span<const double> at);

 private:
  struct Axis {
    AxisHands spec;
    std::array<double, kMostDelaySteps + 1> ring{};  // the last `len` wanted values
    size_t len = 1, head = 0;
    double wander = 0, wander_to = 0;
    double pos = 0, vel = 0;
  };
  double dt_ = 0;
  core::Rng rng_{0};
  std::array<Axis, kMostHandAxes> axes_{};
  size_t count_ = 0;
};

// The intent before the hands, as a decision called "hands" with one fact an axis (`names`, each
// a string literal): a pilot that holds hands records it so `ge why` and the humanness metrics see
// what was meant beside what was pressed.
void record_intent(Why* why, std::span<const char* const> names, std::span<const double> wanted);

}  // namespace ge::ai
