// THE GENERATOR'S OUTER LOOP: a seed in, a clean map out, or a thrown error that names what
// could not be made clean. Port of the single piste's half of game5
// `engine/mapgen/generate.ts` (`generateLevel` at version 1) and of `compile.ts`.
//
// Each ATTEMPT draws everything off `sub_seed(seed, attempt)` in a fixed order (the mountain,
// the start, the piste, the drops, the kickers, the corridor, the off-piste kickers, the
// cliffs, the drifts, the crust, the forest, the day, the weather, the gates and the start
// line), and is accepted only if `analysis::analyze_level` finds no error. A rejected attempt
// is logged at debug level in the oracle's words.
// Parity: tests/mapgen_generate_test.cpp against tests/golden/mapgen_generate.txt.
//
// ge:summary The generator's outer loop: generateLevel, its attempts and the compile
// ge:keywords generate generateLevel mapgen level seed subSeed attempt compileLevel
// ge:oracle game5/engine/mapgen/generate.ts
// ge:skill sim
// ge:test tests/mapgen_generate_test.cpp
#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>

#include "mapgen/types.hpp"

namespace fall_line::mapgen {

// The sub-seed of an attempt: `(seed + attempt * 0x9e3779b9) >>> 0`.
uint32_t sub_seed(double seed, int attempt);

// What `generate_level` throws: no attempt made a clean map, or the version asked for builds
// a ski area this tree cannot build yet. `what()` says which and what to do next.
struct GenerateError : std::runtime_error {
  using std::runtime_error::runtime_error;
};

// The map for a seed: the first attempt the analysis passes. Throws GenerateError.
Level generate_level(double seed, const GenerateOptions& opts = {});

}  // namespace fall_line::mapgen
