// A MAP'S FINGERPRINT: one eight-hex-digit word that moves when the map a seed builds moves.
// Port of game5 `engine/mapgen/digest.ts`: FNV-1a over what a skier meets (every 10th station,
// the gates, the start line, the kickers, the cliffs, the drifts, the trunks, the day, the sky,
// the region and the grade), each value rounded to centimetres and mixed as four bytes.
// Parity: tests/mapgen_generate_test.cpp (`digest`, against tests/golden/mapgen_generate.txt).
//
// ge:summary A map's fingerprint: levelDigest, FNV-1a over what a skier meets
// ge:keywords digest fingerprint fnv levelDigest pinned-map
// ge:oracle game5/engine/mapgen/digest.ts
// ge:skill sim
// ge:test tests/mapgen_generate_test.cpp
#pragma once

#include <string>

#include "mapgen/types.hpp"

namespace fall_line::mapgen {

// The digest of `level`, as game5 prints it: `hash.toString(16).padStart(8, "0")`.
std::string level_digest(const Level& level);

}  // namespace fall_line::mapgen
