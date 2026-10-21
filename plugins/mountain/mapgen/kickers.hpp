// R4, R9 — THE KICKERS: crests shaped so a skier at speed leaves the snow (the park's built
// landings, R20, arrive with M4c).
//
// A kicker is a profile along the way a skier crosses it: a RAMP rising `height` over `ramp`
// m as t² (steepest right at the lip, so it hands the skier upward speed at the one moment it
// matters), then a LANDING falling the same height away over `landing` m as (1 − u)². ON THE
// PISTE (R9) the profile is added to the graded line by arc, before the corridor is pressed,
// only where the line comes down to the lip gently and falls away past it at least as steeply.
// OFF THE PISTE (R4) it is stamped into the ground in plan on a roll of the face.
// Held by tests/mountain_generate_test.cpp (`kickers`, `ground`).
//
// ge:summary R4, R9 the kickers: on the piste and off it, their profile and stamp
// ge:keywords kicker kickers
// ge:skill mountain
// ge:test tests/mountain_generate_test.cpp
#pragma once

#include <span>
#include <vector>

#include "core/heightfield.hpp"
#include "core/prng.hpp"
#include "mountain/mapgen/drops.hpp"
#include "mountain/mapgen/terrain.hpp"
#include "mountain/mapgen/track.hpp"
#include "mountain/mapgen/types.hpp"

namespace ge::mountain::mapgen {

// The lift a kicker's profile adds `u` m past its lip (negative on the ramp), m.
double kicker_profile(double height, double ramp, double landing, double u);

// An on-piste kicker as it is laid: its lip's station.
struct TrackKicker {
  size_t index;
  double height;
  double ramp;
  double landing;
};

// R9 — choose the piste's kickers and add them to the graded profile, as many and as tall as
// the grade asks, `drop.kicker_clear` off every drop (R24). In order down the line.
std::vector<TrackKicker> lay_track_kickers(ge::core::Rng& rng, Piste& piste, const GradeRow& grade,
                                           std::span<const TrackDrop> drops);

// Publish the piste's kickers against the finished line (`K1…`).
std::vector<Kicker> publish_track_kickers(const Piste& piste, std::span<const TrackKicker> kickers);

// R4 — stand the natural kickers on the face's rolls clear of the piste, facing down the fall
// line, and stamp them into the ground (`X1…`).
std::vector<Kicker> lay_off_kickers(ge::core::Rng& rng, const TerrainPlan& plan,
                                    ge::core::Heightfield& ground, const Piste& piste);

// Add one kicker's profile to the ground in plan.
void stamp_kicker(ge::core::Heightfield& ground, double x, double z, double heading, double height,
                  double ramp, double landing, double width);

// Whether a plan point stands on a kicker's footprint, with `margin` m to spare.
bool on_kicker(std::span<const Kicker> kickers, double x, double z, double margin);

}  // namespace ge::mountain::mapgen
