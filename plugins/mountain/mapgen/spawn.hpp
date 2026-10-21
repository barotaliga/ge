// R11–R13 — WHERE THE RUN STARTS: the start under the summit ridge, the start line the
// skiers stand on, the start gate ahead of them, and the gates down to the finish.
//
// The start is the piste's ORIGIN (the walk begins there). What is searched for is the
// HEADING out of the gate: the fall line first, then wider either side, and the first the
// untouched ground makes a fair start along (no steeper than a skier holds from standing,
// never really climbing).
// Held by tests/mountain_generate_test.cpp (`start`, `gates`).
//
// ge:summary R11-R13 the start: the start hut, the gates and the start line's grid
// ge:keywords spawn start gates
// ge:skill mountain
// ge:test tests/mountain_generate_test.cpp
#pragma once

#include <string>
#include <variant>
#include <vector>

#include "core/heightfield.hpp"
#include "core/prng.hpp"
#include "mountain/mapgen/grades.hpp"
#include "mountain/mapgen/types.hpp"

namespace ge::mountain::mapgen {

struct Fall {
  double steepest;
  double gentlest;
};

// The untouched ground's fall along a heading from a point, read over `run` m as the steepest
// and gentlest `track.grade_window` of it, m per m.
Fall fall_along(const ge::core::Heightfield& ground, double x, double z, double heading,
                double run);

// R12 — the start, or the reason no heading off the summit makes a fair one.
std::variant<Spawn, std::string> choose_start(ge::core::Rng& rng,
                                              const ge::core::Heightfield& ground,
                                              const GradeRow& grade);

// R13 — the start line: the skiers abreast on the first station, the player's slot first.
void grid_on_track(const Track& track, Spawn& spawn, std::vector<Spawn>& grid);

// R11 — where the start gate stands: `grid.back` and one station down from the start line.
inline double start_gate_arc() { return rules::grid::back + rules::track::step; }

// R11 — the gates' arcs on a piste `length` m long, start gate first, finish line last.
std::vector<double> gate_arcs(double length);

// R11 — the gates themselves.
std::vector<Checkpoint> lay_checkpoints(const Track& track);

}  // namespace ge::mountain::mapgen
