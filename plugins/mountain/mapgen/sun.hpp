// R15 — THE DAY THE RUN IS SKIED ON: a latitude, a winter's day and a solar hour at which the
// sun is well up in a clear sky: the generator's half (choosing a day or a place by hand
// arrives with the app). The sun's arithmetic is
// `core/solar`'s; what is this game's is which day and which hours count.
// Held by tests/mountain_generate_test.cpp (`world`).
//
// ge:summary R15 the day: declination, the sun's window, deal_sun and the face's bearing
// ge:keywords sun day solar latitude face-the-sun facing bearing
// ge:skill mountain
// ge:test tests/mountain_generate_test.cpp
#pragma once

#include <cstdint>
#include <optional>

#include "core/prng.hpp"
#include "core/solar.hpp"
#include "mountain/mapgen/rules.hpp"

namespace ge::mountain::mapgen {

// The sun's declination on a day of the year, degrees.
double declination_of(double day_of_year);

// The hours of the rule's band in which the sun stands over its floor on that day at that
// latitude, or nullopt when there are none.
std::optional<ge::core::HourWindow> sun_window(double latitude, double day_of_year);

struct DealtSun {
  double hour, day_of_year, latitude;
};

// R15 — deal the day off the region's latitudes and days (R21). Up to 64 draws; a draw with no
// hour over the floor is drawn again.
DealtSun deal_sun(ge::core::Rng& rng, Band latitude, Band day_of_year);

// R15 — THE FACE TURNED TO THE SUN: the compass bearing a ski area's fall line is given, within
// `sun.facing` of the sun's own bearing at `sun`'s hour, off a stream of its own (`sub`, salted).
// The mountain shades itself, so the face a run goes down is turned to the light. On [0, 2π).
double face_the_sun(uint32_t sub, const DealtSun& sun);

}  // namespace ge::mountain::mapgen
