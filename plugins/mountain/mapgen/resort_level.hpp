// R28 — A SKI AREA RACED ON ONE OF ITS COURSES: a built attempt (resort_build.hpp) stood up as a
// `Level`. The course's line is the map's track; the kickers, drops and drifts it skis are named
// by their arc down it (`K1…`, `D1…` in the order skied) and every other run's are the mountain's,
// named by their run (`3K1`, `3D1`); the gates are set down it (course_gates.hpp), the day is the
// course's own (course_day.hpp), and the area itself is published as `Level::resort`.
//
// THE SHAPE. Every course of one area shares the ground, the packed snow, the crust and the woods
// (tens of megabytes), so they are moved ONCE out of the attempt into a base map
// (`resort_base`), and each course is then stood up on it in place (`set_course`): the generator
// checks every course of an area this way without copying a grid, and races the one it chooses.
//
// ge:summary R28 a ski area's course stood up as a map: the base moved out once, a course set on it
// ge:keywords resort-level resort-base set-course course-map race-map publish-resort
// ge:skill mountain
// ge:test tests/mountain_resort_level_test.cpp
#pragma once

#include <cstddef>
#include <optional>
#include <string>

#include "mountain/mapgen/resort_build.hpp"
#include "mountain/mapgen/types.hpp"

namespace ge::mountain::mapgen {

// The map every course of attempt `a` (built through its courses) shares: the grids and the woods
// moved out of it (it keeps everything else), the area published, `laps` runs. No course is set.
Level resort_base(ResortAttempt& a, int laps);

// Stand course `index` of `a` up on `level` (its base): the track, the gates, the start line, the
// kickers, cliffs and drifts, the day, the mountain's figures, the grade and `resort->course`.
void set_course(Level& level, const ResortAttempt& a, size_t index);

// R20 — course `index` of `a` stood up on `level` (its base) with the TERRAIN PARK laid down it:
// the park planned first, clear of the course's own kickers, drops and drifts, each run-out far
// enough from the next ramp for a gate or two to stand between; then the gates set round it
// (`course_gates` with the park), and any kicker they could not be set round dropped, until they
// stand; then the park stamped into the ground. Why no park stands (the course is then set
// without one), or nothing.
std::optional<std::string> set_parked_course(Level& level, const ResortAttempt& a, size_t index);

}  // namespace ge::mountain::mapgen
