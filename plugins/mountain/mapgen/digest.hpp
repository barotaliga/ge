// A MAP'S FINGERPRINT: one eight-hex-digit word that moves when the map a seed builds moves.
// FNV-1a over what a skier meets (every 10th station,
// the gates, the start line, the kickers, the cliffs, the drifts, the trunks, the day, the sky,
// the region and the grade; on a ski area, the course raced and the area's runs, lifts and
// courses; on a race course set over a map, its discipline, run, stretch, pole gates, panelled
// gates and their sides, line, jumps, speed trap and nets),
// each value rounded to centimetres and mixed as four bytes. Bits:
// tests/mountain_generate_test.cpp (`digest`, against tests/golden/mountain_generate.txt).
//
// ge:summary A map's fingerprint: level_digest, FNV-1a over what a skier meets
// ge:keywords digest fingerprint fnv pinned-map
// ge:skill mountain
// ge:test tests/mountain_generate_test.cpp
#pragma once

#include <string>

#include "mountain/mapgen/types.hpp"

namespace ge::mountain::mapgen {

// The digest of `level`: the 32-bit hash as eight lowercase hex digits, zero-padded.
std::string level_digest(const Level& level);

}  // namespace ge::mountain::mapgen
