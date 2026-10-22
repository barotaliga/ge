// R31 — A SLALOM SET OVER A BUILT MAP. The map is the mountain and stays it: the ground, the
// piste, the woods' outline, the day. What a slalom sets over it is a COURSE (where on the piste
// the start hut stands and where the finish line is, the pole gates down the stretch between, the
// line a racer takes through them) and what the organisers prepare for it (course_prep.hpp).
//
// THE STRETCH is found first and is the same for both runs: the steepest stretch of the piste
// that drops the vertical a slalom asks for, wide enough, clear of the piste's drops (a cliff is
// not levelled), with a gentler run-out past the line for a finish arena. THE GATES are set
// second, off a stream of the run's own (the map's seed, a salt and the run), so the two runs are
// two courses on one hill and neither draws from any other stream.
//
// A COURSE IS SET LIKE A COURSE SETTER SETS ONE: the direction changes counted off the vertical,
// a rhythm of open gates with the turning pole either side of the line in turn, the hairpins,
// verticals and delays spread down the middle with an open gate before each, nothing hard at the
// start or the finish, and the last gate fast and aimed at the middle of the line.
//
// ge:summary R31 a slalom set over a built map: the stretch, the pole gates, the racing line
// ge:keywords slalom set-slalom slalom-stretch pole-gate open-gate closed-gate hairpin
// ge:keywords vertical-combination delayed-gate racing-line two-runs race-course r31
// ge:skill mountain
// ge:test tests/mountain_slalom_test.cpp
#pragma once

#include <optional>

#include "mountain/mapgen/types.hpp"

namespace ge::mountain::mapgen {

// The stretch a slalom is set on (R31): the start gate's arc, the finish line's, and the
// vertical between.
struct SlalomStretch {
  double from;
  double to;
  double vertical;
};

// R31 — THE STRETCH of `level`'s piste a slalom is set on: one that drops one of
// `slalom::drops`, wide enough, clear of its drops, its finish on a gentler run-out; the steepest
// to the top level's gradient, then the longest drop (`slalom::pick`). A piste with none: its
// last 600 m. A pure function of the map.
SlalomStretch slalom_stretch(const Level& level);

// R31 — A SLALOM SET OVER `level`: run `run`'s course (1 or 2) down the slalom stretch, as a map
// whose checkpoints are its gates (index 0 the start wand, the last the finish line), whose spawn
// and one grid slot are the start hut, whose ground, snow and woods are prepared for it, and whose
// `race` names it. Set over a map that already carries a course, it is set over the map under
// that one (`RaceCourse::base`), so a run's other course stands on the same mountain; the course
// keeps the day and the sky of the map it was handed.
Level set_slalom(const Level& level, int run = 1);

// THE LINE A RACER TAKES down a set slalom, `s` m down the piste: how far right of the piste's
// line, m; how sharply it bends there, 1/m; its slope across the piste, m/m; and its bend signed,
// positive to the right: a cosine from each of the line's points to the next, so it rounds every
// turning pole and crosses every closed gate at its middle. Nothing on a map with no course set.
struct LineAt {
  double offset;
  double curvature;
  double slope;
  double bend;
};
std::optional<LineAt> slalom_line_at(const Level& level, double s);

}  // namespace ge::mountain::mapgen
