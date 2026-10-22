// WHICH GENERATOR BUILT A MAP: every version the generator can still be asked for, oldest
// first, each with the ways it differs from the current rules (this table carries only the
// traits). v1 is the single piste (`single_piste`), v8 the ski area, v9 the ski area with its
// ski routes (R42) the shortest line down (`straight_routes`), v10 with them wild. Bits:
// tests/mountain_generate_test.cpp builds the golden maps at version 1,
// tests/mountain_resort_level_test.cpp at version 8, tests/mountain_ski_routes_test.cpp at 9
// and 10.
//
// ge:summary Which generator built a map: the versions and their traits
// ge:keywords versions
// ge:skill mountain
// ge:test tests/mountain_generate_test.cpp
#pragma once

#include <array>
#include <optional>

namespace ge::mountain::mapgen {

struct GeneratorTraits {
  int version;
  bool single_piste;       // v1: one ungraded piste down a face due north (no ski area)
  bool stepped_junctions;  // v1–v7: a run meeting another graded under its surface
  bool no_routes;          // v1–v8: no ski routes (R42)
  bool straight_routes;    // v9: the routes the shortest line down
};

inline constexpr std::array<GeneratorTraits, 4> kGeneratorVersions{{
    {1, true, true, true, false},
    {8, false, false, true, false},
    {9, false, false, false, true},
    {10, false, false, false, false},
}};

// The version everything but a pinned map is built by.
inline constexpr int kCurrentGeneratorVersion = kGeneratorVersions.back().version;

// The row for `version`; the current one when absent or unknown.
inline const GeneratorTraits& generator_traits(std::optional<int> version) {
  for (const GeneratorTraits& row : kGeneratorVersions) {
    if (version && row.version == *version) return row;
  }
  return kGeneratorVersions.back();
}

}  // namespace ge::mountain::mapgen
