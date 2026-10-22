// R35 — A SKI-CROSS COURSE BUILT OVER A MAP. A ski cross is not raced on a piste as it lies: it is
// BUILT out of a gentle stretch of one, a fenced lane fourteen metres wide weaving down it, its
// turns banked into BERMS, its straights carrying ROLLERS, JUMPS and STEP-DOWNS, a start gate of
// four doors over a steep start ramp at its top and a finish area at its foot. Four racers ski it
// at once.
//
// THE STRETCH is the piste's gentlest long enough for a course: the one whose course comes nearest
// the rule's mean gradient, with no drop across it where one can be found, and then the lowest
// (a course finishes on the valley floor). THE LINE is a course of its own: a start straight down
// the piste's line, legs swung either side of it in turn, each corner rounded into a turn of a
// radius dealt off the map's seed, and a finish straight back onto it. The line is the map's
// `track` from then on, so everything that asks the piste asks the course. THE PROFILE is the
// ground under the course combed along it, never rising, its crests and knees rounded; the
// features are built on top of it and the ground is graded to it across the course, the outside of
// every berm banked up into a wall. Everything is a pure function of the map, the set dealt off
// the map's seed on a stream of its own.
//
// ge:summary R35 a ski-cross course built over a map: the stretch, the line, berms and features
// ge:keywords ski-cross set-ski-cross berm rollers jump step-down start-ramp start-lanes
// ge:keywords corridor-gate turning-gate fence finish-area stretch r35
// ge:skill mountain
// ge:test tests/mountain_ski_cross_test.cpp
#pragma once

#include <optional>
#include <string>

#include "mountain/mapgen/types.hpp"

namespace ge::mountain::mapgen {

// R35 — THE COURSE OF A SKI AREA a ski cross is built on (R28): one at least
// `ski_cross::course.length` m long with a stretch's vertical under it, whose mean gradient comes
// nearest `ski_cross::course.grade`; nothing on a map that is not a ski area.
std::optional<std::string> ski_cross_course_of(const Level& level);

// R35 — A SKI-CROSS COURSE BUILT OVER `level`: a map whose `track` is the course, whose ground is
// graded to it, whose checkpoints are its start gate, its flags and its finish line, and whose
// grid is the start gate's four lanes, the inside of the first berm first. A map already carrying
// one is that map; one carrying any other course is built over the map under it. The course
// keeps the day and the sky of the map it was handed.
Level set_ski_cross(const Level& level);

}  // namespace ge::mountain::mapgen
