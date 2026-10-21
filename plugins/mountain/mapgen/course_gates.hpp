// R11, R24, R28 — THE GATES DOWN A SKI AREA'S COURSE, and which course a map is raced on.
//
// The start gate stands across the piste and the finish line across the arena, as on any piste
// (R11). Between them stand SLALOM GATES about every `course::gates::spacing_target` m, the room
// left shared out evenly and every gap in `spacing` but the last (to the finish, which may run
// short, so no gate stands on the finish line), each as wide as its colour's row asks and set
// alternately left and right of the line by a share of the room the piste leaves, never further
// than the turn a skier makes between two of them at that pitch can carry, and on the line over a
// kicker or a drop, where a skier goes straight. A gate the spacing would stand on a drop moves off
// it (R24), inside the spacing's band where it can. Which side the first gate stands and how much
// of its room each takes come off the course's own hash: nothing is drawn from any stream.
//
// ge:summary R11 R24 R28 the slalom gates down a course, and choosing the course a map races
// ge:keywords course-gates slalom-gate gate-offset gate-span weave hash-pick choose-course
// ge:skill mountain
// ge:test tests/mountain_resort_level_test.cpp
#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "mountain/mapgen/types.hpp"

namespace ge::mountain::mapgen {

// R11, R24, R28 — the gates down `track`, a course of colour `grade` carrying `kickers` and
// `drops` (each with its arc down it), off the hash `salt`.
std::vector<Checkpoint> course_gates(const Track& track, std::span<const Cliff> drops,
                                     std::span<const Kicker> kickers, PisteGrade grade,
                                     uint32_t salt);

// A share in [0, 1) off a seed and an index, for a pick that draws nothing from any stream.
double hash_pick(uint32_t seed, int i);

// R28 — which of `courses` a map is raced on: the one named `course`, else the nearest colour to
// `grade` (the harder on a tie, then the seed's own pick), else to `dealt`.
size_t choose_course(std::span<const Course> courses, double seed,
                     const std::optional<std::string>& course, std::optional<PisteGrade> grade,
                     PisteGrade dealt);

}  // namespace ge::mountain::mapgen
