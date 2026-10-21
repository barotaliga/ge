// R22 — THE CLIFFS: an edge a skier drops off into the lower ground below.
//
// A cliff is a profile along the way a skier goes over it, like a kicker's but taking the
// ground away: a SHELF climbing out of the country (smoothstepped, so the skier leaves the edge
// flying flat), a FACE falling the whole drop over a couple of metres, and an APRON below
// falling away the way he is going, so a drop is landed onto a slope. Each faces down the fall
// line. Dealt off a stream of their own (the attempt's sub-seed, salted): they move the ground
// the forest then grows on and nothing else the map draws.
// Held by tests/mountain_generate_test.cpp (`cliffs`, `ground`).
//
// ge:summary R22 cliffs off the piste: their profile, footprint, laying and stamp
// ge:keywords cliff cliffs
// ge:skill mountain
// ge:test tests/mountain_generate_test.cpp
#pragma once

#include <functional>
#include <span>
#include <vector>

#include "core/heightfield.hpp"
#include "mountain/mapgen/terrain.hpp"
#include "mountain/mapgen/types.hpp"

namespace ge::mountain::mapgen {

// The lift a cliff's profile adds `u` m past its edge (negative on the shelf), m. Reads only
// drop, shelf, face, apron and landing, so a drop across the piste (drops.hpp) shares it.
double cliff_profile(double drop, double shelf, double face, double apron, double landing,
                     double u);

// Plan points over a cliff's footprint (shelf, face, landing, run-out) every `step` m: what
// the piste is kept clear of.
std::vector<PlanPoint> cliff_footprint(const Cliff& c, double step = 12);

// Whether a plan point stands on a cliff or the landing below it, with `margin` m to spare.
bool on_cliff(std::span<const Cliff> cliffs, double x, double z, double margin);

// R22 — stand the cliffs on the face clear of the piste, the kickers and the drops, and cut
// them into the ground: first those the grade stands BESIDE the piste (R23), then the rest. A
// ski area hands its own `distance_to` (how far off its runs, pads and ramps), read in place of
// the piste's line.
std::vector<Cliff> lay_cliffs(uint32_t sub, const TerrainPlan& plan, ge::core::Heightfield& ground,
                              const Track& piste, std::span<const Kicker> kickers,
                              std::span<const Cliff> drops, const DistanceTo& distance_to = {});

// Add one cliff's profile to the ground in plan.
void stamp_cliff(ge::core::Heightfield& ground, const Cliff& c);

}  // namespace ge::mountain::mapgen
