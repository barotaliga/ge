// A SKIER'S BREATH IN THE COLD, as arithmetic: each frame, the puffs he breathes out, read off his
// state and how thick the air shows a breath (engine/wind/cold.hpp's `breath_of`: 0 unseen, 1 a
// -10 C winter's plume, more when colder), for a game to fly and draw like the spray (Fall Line
// hands them to the particle pool, render/particles.hpp).
//
// He breathes every `kBreathPeriod` s (faster the harder he skis: a racer at speed every 2 s), out
// through the first `kExhaleShare` of it: a puff at his mouth (1.5 m over the skis standing, a
// metre in a tuck, swung out with his lean), carried on with his own speed and blown half a
// metre a second ahead of his face, dragged back to the air within a second so a fast skier's
// breath streams behind him. The puffs are big, thin and short-lived: the colder the air, the
// thicker and the longer they hang. Nothing shows where the air shows no breath. Every number is
// drawn from the pen's own generator (core/prng.hpp), so a run breathed again breathes the same.
//
// ge:summary A skier's breath in the cold: puffs at the mouth by the air's breath_of
// ge:keywords skier breath breathing exhale plume cold condensation puff breath_puffs breath-pen
// ge:skill skier render
// ge:test tests/skier_breath_test.cpp
#pragma once

#include <cstdint>
#include <vector>

#include "core/prng.hpp"
#include "skier/app/spray.hpp"
#include "skier/skier.hpp"

namespace ge::skier {

inline constexpr double kBreathPeriod = 3.2;   // s between breaths, standing
inline constexpr double kBreathFastest = 2;    // s between breaths, racing at kBreathHard
inline constexpr double kBreathHard = 25;      // m/s
inline constexpr double kExhaleShare = 0.4;    // of a breath, breathing out
inline constexpr double kPuffRate = 30;        // puffs a second while breathing out
inline constexpr double kMouthStanding = 1.5;  // m over the skis' base
inline constexpr double kMouthTucked = 1.0;    // m, in a full tuck

// What a skier's breath remembers between frames: where in a breath he is, the puffs owed, and
// his generator.
struct BreathPen {
  core::Rng rng{1};
  double phase = 0;  // 0..1 through the current breath
  double owed = 0;   // puffs owed
};

// A pen for racer `k`: its own stream, and a breath started somewhere of its own.
BreathPen breath_pen(uint32_t k);

// Where the mouth is, world m.
void mouth_of(const SkierState& s, double out[3]);

// The puffs `s` breathes out over `dt` s, appended to `out`, when the air shows a breath as thick
// as `show` (wind::breath_of); the count.
int breath_puffs(const SkierState& s, double show, double dt, BreathPen& pen,
                 std::vector<SprayGrain>& out);

}  // namespace ge::skier
