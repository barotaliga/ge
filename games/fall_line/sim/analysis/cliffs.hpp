// R22 — THE CLIFFS, and R24 — THE DROPS across the piste, held on the finished map: read off
// the cliffs the level publishes and the ground under them. Port of the single piste's half of
// game5 `engine/analysis/cliffs.ts`.
// Parity: through tests/mapgen_generate_test.cpp (`findings`).
//
// ge:summary R22 cliffs and R24 drops held on the finished map
// ge:keywords analysis checkCliffs checkDrops cliff drop
// ge:oracle game5/engine/analysis/cliffs.ts
// ge:skill sim
// ge:test tests/mapgen_generate_test.cpp
#pragma once

#include <span>

#include "analysis/index.hpp"

namespace fall_line::analysis {

// R22 — the cliffs off the piste: each a drop on the ground, clear of the piste, enough of them.
void check_cliffs(const mapgen::Level& level, const Add& add);

// R24 — the drops across the piste: as many as the grade deals, each a drop on the line, off
// the gates and the kickers, spaced down the piste.
void check_drops(const mapgen::Level& level, std::span<const mapgen::Kicker* const> kickers,
                 const Add& add);

}  // namespace fall_line::analysis
