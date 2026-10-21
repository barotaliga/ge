// R31– — A RACE COURSE SET OVER A MAP HELD TO ITS DISCIPLINE'S RULES, reading only what the
// `Level` publishes (`Level::race`, the checkpoints, the spawn, the snow, the woods). The map under
// the course was held to its own rules when it was built (`RaceCourse::base`); this answers
// whether the course set over it is the discipline's. `analyze_level` answers it on a map with a
// `race`.
//
// Every message's text is fixed.
//
// ge:summary R31- a race course set over a map held to its discipline's rules (the slalom's)
// ge:keywords analysis analyze-race race-course slalom pole-gate hairpin r31
// ge:skill mountain
// ge:test tests/mountain_slalom_test.cpp
#pragma once

#include "mountain/analysis/index.hpp"

namespace ge::mountain::analysis {

// The course `level.race` names, held to its discipline's rule; one error if it names none.
LevelAnalysis analyze_race(const mapgen::Level& level);

}  // namespace ge::mountain::analysis
