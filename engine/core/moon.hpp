// THE MOON ON A DATE, as an almanac states it: where it stands from a place on the surface,
// how far, how much of it is lit, which way its face is turned to the eye, and where the Earth's
// shadow falls at its distance. Every transcendental through core::libm. Bits:
// tests/core_moon_test.cpp against tests/golden/core_moon.txt; accuracy against a high-precision
// ephemeris's own rows (tests/core_moon_test.cpp's reference table).
//
// - THE ORBIT. The lunar theory's sixty largest periodic terms in longitude and distance and
//   sixty in latitude (the truncated ELP-2000/82 series as Meeus tabulates it, chapter 47), with
//   the additive terms for Venus, Jupiter and the Earth's flattening: some ten arc-seconds in
//   longitude, four in latitude, a few kilometres in distance. Nutation in longitude and
//   obliquity by its four largest terms, so the place is APPARENT, of date.
// - TIME. A date is given in Universal Time (a Julian day, `jd`); the theory runs on dynamical
//   time, `jd + delta_t(jd) / 86400`, delta T from its polynomial fits (some 70 s in the 2020s).
// - THE PLACE SEEN. The geocentric place less the eye's own place on a round Earth (the
//   topocentric parallax: up to a degree near the horizon), so the moon rides lower and nearer
//   in the sky than from the Earth's centre would make it. Airless and refracted elevations both.
// - THE PHASE. The phase angle (sun, moon, eye) from the three bodies' places, and the lit share
//   (1 + cos i) / 2.
// - THE FACE. The moon's north pole and prime meridian by the IAU's rotation model for it (the
//   pole's wobble and the physical librations in its periodic terms), precessed to the date and
//   turned onto the sky with everything else, so the near side shows its seas where they are:
//   the optical libration (some 8 degrees in longitude and 7 in latitude) and the face's tilt in
//   the sky (the position angle of its axis, and how the place's latitude and the hour turn it).
// - THE EARTH'S SHADOW. Its axis away from the sun, its umbra's and penumbra's radii at the
//   moon's distance (the Earth's parallax and the sun's, its semidiameter, enlarged a fiftieth
//   for the air as eclipse predictions do), as seen from the eye.
//
// FRAMES: a WORLD vector is the engine's x east, y up, z north (an azimuth is a world heading,
// 0 along +z, clockwise toward +x). Latitudes, longitudes (east positive) and declinations are
// degrees; elevations and the angular radii radians.
//
// ge:summary The moon on a date: its place seen from the surface, its phase, its face, the shadow
// ge:keywords moon lunar ephemeris elp meeus delta-t nutation parallax topocentric libration
// ge:keywords phase-angle illuminated-fraction position-angle selenographic eclipse umbra
// ge:keywords penumbra earth-shadow moon_sky lunar_position delta_t
// ge:skill determinism
// ge:test tests/core_moon_test.cpp
#pragma once

namespace ge::core {

// The moon's mean radius and the Earth's equatorial radius, km; the astronomical unit, km.
inline constexpr double kMoonRadiusKm = 1737.4;
inline constexpr double kEarthRadiusKm = 6378.14;
inline constexpr double kAuKm = 149597870.7;

// Delta T (dynamical time less Universal Time), seconds, at Julian day `jd` (UT).
double delta_t(double jd);

// The moon's apparent geocentric place at `jd` (UT).
struct LunarPosition {
  double longitude;        // ecliptic, of date, nutation applied, degrees
  double latitude;         // ecliptic, degrees
  double distance_km;      // between the centres
  double right_ascension;  // degrees 0..360
  double declination;      // degrees
  double obliquity;        // true, degrees
  double nutation;         // in longitude, degrees
};
LunarPosition lunar_position(double jd);

// Greenwich APPARENT sidereal time at `jd` (UT), degrees 0..360: the mean (core/solar.hpp's
// sidereal_time) and the equation of the equinoxes.
double apparent_sidereal_time(double jd);

struct Vec3d {
  double x, y, z;
};

// The moon seen from latitude `latitude`, longitude `longitude` (east positive) at `jd` (UT).
struct MoonSky {
  double elevation;            // airless, rad
  double apparent_elevation;   // refracted (core/solar.hpp's refraction), rad
  double azimuth;              // world heading, 0 .. 2 pi
  double distance_km;          // from the eye
  double angular_radius;       // rad
  double phase_angle;          // sun, moon, eye: rad, 0 full .. pi new
  double lit;                  // lit share of the disc, 0 .. 1
  double age;                  // days past new, off the elongation in longitude
  double libration_longitude;  // the point under the eye on the moon, selenographic, degrees
  double libration_latitude;
  double axis_angle;  // its north pole's position angle from celestial north through
                      // east, degrees
  Vec3d toward;       // WORLD unit vectors: toward the moon from the eye,
  Vec3d pole;         //   its north pole,
  Vec3d prime;        //   from its centre to 0 N 0 E (the mean near side's middle),
  Vec3d east;         //   to 0 N 90 E,
  Vec3d sun;          //   toward the sun from the Earth's centre,
  Vec3d shadow;       //   toward the Earth's shadow's axis at the moon's distance;
  double umbra;       // the umbra's angular radius there as seen, rad
  double penumbra;    // the penumbra's, rad
};
MoonSky moon_sky(double jd, double latitude, double longitude);

// The moon at apparent solar `hour` at a place on the date whose 0h UT is `jd0` (the hour as
// core/solar.hpp's sun_on_date reads it).
MoonSky moon_on_date(double hour, double latitude, double longitude, double jd0);

// A WORLD direction from an equatorial one of date (x toward the equinox, z toward the north
// celestial pole) at local apparent sidereal angle `sidereal` (degrees) and `latitude`.
Vec3d equatorial_to_world(const Vec3d& e, double sidereal, double latitude);

}  // namespace ge::core
