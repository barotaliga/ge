// R5 — DOES THE PISTE CROSS ITSELF: every pair of non-adjacent segments tested for a proper
// intersection, off a 16 m spatial hash. Port of game5 `engine/analysis/crossings.ts`.
// Parity: through tests/mountain_generate_test.cpp (`findings`).
//
// ge:summary R5 whether the piste crosses itself
// ge:keywords analysis selfCrossings crossing
// ge:oracle game5/engine/analysis/crossings.ts
// ge:skill mountain
// ge:test tests/mountain_generate_test.cpp
#pragma once

#include <vector>

#include "mountain/mapgen/types.hpp"

namespace ge::mountain::analysis {

// How many times the open polyline of the piste's stations crosses itself.
int self_crossings(const std::vector<mapgen::TrackPoint>& points);

}  // namespace ge::mountain::analysis
