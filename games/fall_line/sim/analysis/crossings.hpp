// R5 — DOES THE PISTE CROSS ITSELF: every pair of non-adjacent segments tested for a proper
// intersection, off a 16 m spatial hash. Port of game5 `engine/analysis/crossings.ts`.
// Parity: through tests/mapgen_generate_test.cpp (`findings`).
//
// ge:summary R5 whether the piste crosses itself
// ge:keywords analysis selfCrossings crossing
// ge:oracle game5/engine/analysis/crossings.ts
// ge:skill sim
// ge:test tests/mapgen_generate_test.cpp
#pragma once

#include <vector>

#include "mapgen/types.hpp"

namespace fall_line::analysis {

// How many times the open polyline of the piste's stations crosses itself.
int self_crossings(const std::vector<mapgen::TrackPoint>& points);

}  // namespace fall_line::analysis
