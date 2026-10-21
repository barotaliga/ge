// R25–R30 — A SKI AREA'S MAP HELD TO ITS RULES, reading only what the `Level` publishes: the
// course `track` is (R28), held as a piste a race is run down (`analyze_course`), and the area
// round it, its runs, lifts, woods, hub and tunnels (`analyze_resort`). `analyze_level` answers
// both on a map with a `resort`. The generator reads the area once an attempt, on its first
// course's map, and every course on its own, so a course that will not stand is not offered.
//
// Every message's text is fixed: `generate_level` writes the refused attempts' and the left-out
// courses' findings into its debug lines.
//
// ge:summary R25-R30 a ski area's map held to its rules: the course raced, and the area round it
// ge:keywords analysis analyze-course analyze-resort course resort network access gates
// ge:skill mountain
// ge:test tests/mountain_resort_level_test.cpp
#pragma once

#include "mountain/analysis/index.hpp"

namespace ge::mountain::analysis {

// R1, R5, R7, R8, R10–R13, R15–R17, R19, R28 — the course `level.track` is, as a race runs it.
LevelAnalysis analyze_course(const mapgen::Level& level);

// R14, R25–R30 — the area round the course; one error if the level publishes none.
LevelAnalysis analyze_resort(const mapgen::Level& level);

}  // namespace ge::mountain::analysis
