// R21 — THE REGION'S OWN SNOW: the wind crust, laid only where a region's row asks for it, off
// a stream of its own, over broad patches of slow noise and over every crest a few metres
// proud of the ground round it. Port of game5 `engine/mapgen/surface.ts`. It never reaches the
// piste: it is folded into the packed field only past `kClear` m of the centreline, so R10
// holds.
// Parity: through tests/mapgen_generate_test.cpp (`crust`, `packed` on the fell and continental).
//
// ge:summary R21 the wind crust: laid off its own stream, folded into the packed field
// ge:keywords surface crust wind-crust layCrust foldSurface
// ge:oracle game5/engine/mapgen/surface.ts
// ge:skill sim
// ge:test tests/mapgen_generate_test.cpp
#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "core/heightfield.hpp"
#include "mapgen/regions.hpp"

namespace fall_line::mapgen {

// How far from the centreline the region's own snow starts, m: past the widest piste's flat
// shoulder and windrow with three metres to spare.
inline constexpr double kClear =
    rules::track::width.max / 2 + rules::track::shoulder_flat + rules::berm::width + 3;

// R21 — the wind crust's share of the mountain, 0..1 on the ground's grid; none where the
// region lays none.
std::optional<ge::core::Heightfield> lay_crust(uint32_t sub, const Region& region,
                                               const ge::core::Heightfield& ground);

// Fold the crust into the packed field at its support, taken out within `kClear` of the piste
// (`dist`, the corridor's), where the crust's own field is zeroed too.
void fold_surface(ge::core::Heightfield& packed, const std::vector<float>& dist,
                  const Region& region, ge::core::Heightfield& crust);

}  // namespace fall_line::mapgen
