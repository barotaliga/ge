// THE MOUNTAIN'S NIGHT: the moon where it stands on the map's own date at the frame's hour, and
// the stars' sphere turned to it, laid into a frame (render/night.hpp does the rest, and makes
// the moon the key light once it outshines the twilight).
//
// - THE DATE. A map's sky is its day of the year in kSkyYear, so a day of the year is always the
//   same sky: the same declination, the same phase of the moon. 2026 is a year near the moon's
//   major standstill, when its path swings furthest north and south, so a winter's full moon
//   rides highest.
// - THE MOON is the dated one (core/moon.hpp's moon_on_date: its real orbit, its place, distance
//   and face seen from the surface, the Earth's shadow), at longitude 0 (a generated map has no
//   longitude).
//
// ge:summary The map's moon and stars on its date at an hour, laid into a frame's night
// ge:keywords night moon stars date sky-year night_look moonlight phase sidereal
// ge:skill mountain render
// ge:test tests/mountain_night_look_test.cpp
#pragma once

#include "core/moon.hpp"
#include "mountain/mapgen/types.hpp"
#include "render/view.hpp"

namespace ge::mountain::app {

// The year a map's day of the year is read in.
inline constexpr double kSkyYear = 2026;

// The moon on `level`'s date at solar `hour`, as render::set_night takes it.
struct MapMoon {
  double elevation, azimuth, lit, distance_km;
  double sidereal_deg;  // the local sidereal angle, degrees
  core::MoonSky sky;    // all of it
};
MapMoon moon_of(const mapgen::Level& level, double hour);

// Lay `level`'s night at `hour` into `view` (its sun already placed).
void night_look(const mapgen::Level& level, double hour, render::FrameView& view);

}  // namespace ge::mountain::app
