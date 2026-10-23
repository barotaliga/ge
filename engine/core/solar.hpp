// WHERE THE SUN AND THE MOON ARE: textbook solar geometry (every transcendental through
// core::libm, every power through libm::pow). A world generator asks which hours of a day
// are daylight before it draws one, a run clock asks where the sun and the moon stand at the
// hour a run has reached, and the sky reads the same answers: one implementation of sunset.
// Bits: tests/core_solar_test.cpp against tests/golden/core_solar.txt.
//
// ANGLES: an elevation is radians above the horizon, negative under it. An azimuth is a WORLD
// HEADING in the engine's convention (0 along +z, clockwise toward +x), so +z is north and the
// noon sun north of the tropics stands at kSouth. Latitudes, longitudes (east positive) and
// declinations are degrees.
// TIMES: an `hour` is the place's apparent solar hour, 0..24 (noon = the sun on the meridian).
// A Julian day `jd` counts days from noon UT on 1 January 4713 BC; `jd0` is the one at 0h UT
// of a civil date.
//
// ge:summary Where the sun and the moon are: elevation, azimuth, daylight windows, dates
// ge:keywords solar sun moon elevation azimuth daylight sunrise sunset julian-day refraction
// ge:keywords sidereal-time sidereal_time stars
// ge:skill determinism
// ge:test tests/core_solar_test.cpp
#pragma once

#include <optional>

#include "core/math.hpp"

namespace ge::core {

// The world heading of due south.
inline constexpr double kSouth = kPi;
// The synodic month, days: new moon to new moon.
inline constexpr double kSynodicMonth = 29.530589;
// The TRUE elevation of the sun's centre when its refracted upper limb touches a level horizon:
// sunrise and sunset as an almanac states them, -0.833 degrees, in radians.
inline constexpr double kSunrise = -0.833 * (kPi / 180.0);

struct SunPlace {
  double elevation;  // rad above the horizon
  double azimuth;    // world heading
  bool rising;       // before solar noon
  double hour;       // the hour it was read at
};

struct MoonPlace {
  double elevation;
  double azimuth;
  bool rising;
  double hour;
  double lit;  // share of the disc lit, 0 (new) .. 1 (full)
};

struct HourWindow {
  double min;  // hour
  double max;  // hour
};

// The sun at `hour` at `latitude` with the sun at `declination` (the caller dates its season).
SunPlace sun_at(double hour, double latitude, double declination);

// The hour the sun stands at `elevation` (rad) rising or setting, or nullopt when it never
// reaches it that day. Searched every 1/20 h and closed linearly.
std::optional<double> hour_of_elevation(double elevation, bool rising, double latitude,
                                        double declination);

// The hours the sun stands over `min_elevation` (rad): nullopt when the noon sun never reaches
// it, the whole clock (0..24) when there is no crossing (the midnight sun).
std::optional<HourWindow> daylight_window(double latitude, double min_elevation,
                                          double declination);

// The moon `age` days past new: it rides age / kSynodicMonth of a day behind the sun, its
// declination swinging from the sun's at new to the opposite at full. No orbital tilt.
MoonPlace moon_at(double hour, double latitude, double sun_declination, double age);

// ── On a real date: NOAA's solar series (after Meeus), Bennett's refraction near the horizon.
//    The moon on a real date is core/moon.hpp's. ──

// The Julian day at 0h UT of a Gregorian civil date.
double julian_day(double year, double month, double day);
// The Julian day at 0h UT of day `day_of_year` (1 = 1 January); past the year's end runs on.
double julian_day_of_year(double year, double day_of_year);

struct SolarCoordinates {
  double declination;       // apparent, degrees
  double equation_of_time;  // minutes: apparent solar time less mean
  double longitude;         // apparent ecliptic longitude, degrees
  double obliquity;         // corrected for nutation, degrees
};
SolarCoordinates solar_coordinates(double jd);

// Greenwich mean sidereal time at `jd`, degrees 0..360: how far the stars have turned past the
// meridian at longitude 0 (a place's own is this plus its longitude east).
double sidereal_time(double jd);

// How far the air lifts an image at a true elevation (rad), rad.
double refraction(double elevation);

struct DatedSunPlace {
  double elevation;  // APPARENT (refracted), rad
  double azimuth;
  bool rising;
  double hour;
  double true_elevation;  // geometric, rad
  double declination;     // at that instant, degrees
};

// The UT hour (off jd0's midnight) at which a place `longitude` degrees east stands at apparent
// solar `hour`: the longitude's hours and the equation of time taken off (three passes).
double universal_hour_of(double jd0, double hour, double longitude);

DatedSunPlace sun_on_date(double hour, double latitude, double longitude, double jd0);

// The apparent solar hour the sun's centre crosses the TRUE `elevation` (rad) rising or
// setting on a date, or nullopt. Scanned every 1/20 h, then bisected 16 times.
std::optional<double> hour_of_sun_on_date(double elevation, bool rising, double latitude,
                                          double longitude, double jd0);

// The hours the sun stands over `min_elevation` (true, rad) on a date: nullopt on a day it
// never comes up to it, 0..24 under a sun that never goes down to it, and the open end at 0
// or 24 on the day a midnight sun begins or ends.
std::optional<HourWindow> sun_window_on_date(double latitude, double longitude, double jd0,
                                             double min_elevation = kSunrise);

}  // namespace ge::core
