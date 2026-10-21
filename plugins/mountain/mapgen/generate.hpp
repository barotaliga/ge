// THE GENERATOR'S OUTER LOOP: a seed in, a clean map out, or a thrown error that names what
// could not be made clean, and the compile that bakes its grids. Version 1 (the single piste)
// so far.
//
// Each ATTEMPT draws everything off `sub_seed(seed, attempt)` in a fixed order (the mountain,
// the start, the piste, the drops, the kickers, the corridor, the off-piste kickers, the
// cliffs, the drifts, the crust, the forest, the day, the weather, the gates and the start
// line), and is accepted only if `analysis::analyze_level` finds no error. A rejected attempt
// is logged at debug level, its findings' text fixed by the golden.
// Bits: tests/mountain_generate_test.cpp against tests/golden/mountain_generate.txt.
//
// ge:summary The generator's outer loop: generate_level, its attempts and the compile
// ge:keywords generate mapgen level seed attempt
// ge:skill mountain
// ge:test tests/mountain_generate_test.cpp
#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>

#include "mountain/mapgen/types.hpp"

namespace ge::mountain::mapgen {

// The sub-seed of an attempt: `(seed + attempt * 0x9e3779b9) >>> 0`.
uint32_t sub_seed(double seed, int attempt);

// What `generate_level` throws: no attempt made a clean map, or the version asked for builds
// a ski area this tree cannot build yet. `what()` says which and what to do next.
struct GenerateError : std::runtime_error {
  using std::runtime_error::runtime_error;
};

// The map for a seed: the first attempt the analysis passes. Throws GenerateError.
Level generate_level(double seed, const GenerateOptions& opts = {});

}  // namespace ge::mountain::mapgen
