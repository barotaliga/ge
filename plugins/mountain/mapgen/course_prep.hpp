// A RACE COURSE PREPARED: what the organisers do to a built map's piste before a discipline is
// set on it (R31–): the piste's kickers on the course LEVELLED away, a START DROP cut out of the
// start house's door, the snow GROOMED hard from the house to the end of the run-out whatever
// drift lay across it, the relief under the course COMBED smooth of the short lips a steep face
// carries, and the trees CUT for the course and its finish arena. The rest of the mountain is the
// map's own.
//
// Each discipline hands its own numbers (`CoursePrep`, discipline_rules.hpp) and the stretch it
// is set on. Every cut and every comb is a pure function of the map, the stretch and the numbers.
//
// ge:summary A race course prepared: kickers levelled, the start drop, groomed, combed, trees cut
// ge:keywords course-prep start-drop comb groom level-kickers clear-trees finish-arena race-course
// ge:skill mountain
// ge:test tests/mountain_slalom_test.cpp
#pragma once

#include <vector>

#include "mountain/mapgen/discipline_rules.hpp"
#include "mountain/mapgen/types.hpp"

namespace ge::mountain::mapgen {

// The stretch of the piste a course is set on: the start gate's arc (the wand) and the finish
// line's, m.
struct Stretch {
  double from;
  double to;
};

// THE MAP WITH A COURSE PREPARED ON IT: every kicker on the piste within `P.clearance` of the
// stretch levelled (its profile taken back out of the piste's line and the ground under it,
// across the piste and eased out over 6 m), the start drop cut out of the door the same way, the
// snow groomed hard, and the relief under the course combed smooth. Its trees are the map's.
Level prepare_course(const Level& level, const Stretch& stretch, const CoursePrep& P);

// The woods of `level` with the course down `stretch` and its finish arena cut out of them.
std::vector<TreeDef> cleared_trees(const Level& level, const Stretch& stretch, const CoursePrep& P);

}  // namespace ge::mountain::mapgen
