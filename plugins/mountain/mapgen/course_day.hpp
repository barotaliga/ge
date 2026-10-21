// R15, R19 — A COURSE'S DAY: the day of the year, the hour and the sky a ski area's course is
// raced under, dealt off a stream of the course's own (the attempt's sub-seed salted by the
// course's number), so every course's map of one area shares every metre of its snow and only its
// sky differs. The hour is held to the face: the sun stands within `sun::facing` of the bearing
// the face was turned to (`ResortAttempt::facing`).
//
// ge:summary R15 R19 a course's day: its own day, hour and sky, turned to the face
// ge:keywords course-day day hour sky weather facing sun-off-face
// ge:skill mountain
// ge:test tests/mountain_resort_level_test.cpp
#pragma once

#include <cstdint>
#include <string>

#include "mountain/mapgen/regions.hpp"
#include "mountain/mapgen/types.hpp"

namespace ge::mountain::mapgen {

struct CourseDay {
  Sun sun;
  Weather weather;
};

// R15, R19 — the day course `id` (a run's number) is raced under, on an area dealt at `latitude`
// whose face is turned to `facing`, rad. Up to 48 draws; past them a noon in the region's days,
// which always faces within the bearing the face was turned for, and never an evening.
CourseDay course_day(uint32_t sub, const Region& region, double latitude, double facing,
                     const std::string& id);

}  // namespace ge::mountain::mapgen
