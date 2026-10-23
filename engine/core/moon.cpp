// The moon on a date; see moon.hpp. Statement order and operand order are the ones
// tests/golden/core_moon.txt was cut under: a reordered sum is a different double.
//
// ge:summary The lunar theory's terms, the place seen, the face and the shadow behind moon.hpp
// ge:keywords moon lunar ephemeris periodic-terms nutation precession iau-rotation libration
// ge:keywords topocentric shadow
// ge:skill determinism
// ge:test tests/core_moon_test.cpp
#include "core/moon.hpp"

#include <cmath>

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "core/num.hpp"
#include "core/solar.hpp"

namespace ge::core {
namespace {

constexpr double kDeg = kPi / 180.0;
constexpr double kArcsec = kDeg / 3600.0;

double wrap360(double x) { return num::mod(num::mod(x, 360.0) + 360.0, 360.0); }
double sin_d(double x) { return libm::sin(wrap360(x) * kDeg); }
double cos_d(double x) { return libm::cos(wrap360(x) * kDeg); }
double clamp_unit(double v) { return v < -1.0 ? -1.0 : v > 1.0 ? 1.0 : v; }

Vec3d add(const Vec3d& a, const Vec3d& b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
Vec3d sub(const Vec3d& a, const Vec3d& b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
Vec3d scale(const Vec3d& a, double s) { return {a.x * s, a.y * s, a.z * s}; }
double dot(const Vec3d& a, const Vec3d& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
double length(const Vec3d& a) { return std::sqrt(dot(a, a)); }
Vec3d unit(const Vec3d& a) { return scale(a, 1.0 / length(a)); }
Vec3d cross(const Vec3d& a, const Vec3d& b) {
  return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
// An equatorial unit vector at right ascension `ra` and declination `dec` (degrees).
Vec3d polar(double ra, double dec) {
  return {cos_d(dec) * cos_d(ra), cos_d(dec) * sin_d(ra), sin_d(dec)};
}

// One periodic term: the multiples of the elongation D, the sun's anomaly M, the moon's M' and
// its argument of latitude F, and its amplitude (millionths of a degree; distance: metres).
struct LonTerm {
  signed char d, m, mm, f;
  int lon, dist;
};
struct LatTerm {
  signed char d, m, mm, f;
  int lat;
};

constexpr LonTerm kLonTerms[] = {
    {0, 0, 1, 0, 6288774, -20905355},
    {2, 0, -1, 0, 1274027, -3699111},
    {2, 0, 0, 0, 658314, -2955968},
    {0, 0, 2, 0, 213618, -569925},
    {0, 1, 0, 0, -185116, 48888},
    {0, 0, 0, 2, -114332, -3149},
    {2, 0, -2, 0, 58793, 246158},
    {2, -1, -1, 0, 57066, -152138},
    {2, 0, 1, 0, 53322, -170733},
    {2, -1, 0, 0, 45758, -204586},
    {0, 1, -1, 0, -40923, -129620},
    {1, 0, 0, 0, -34720, 108743},
    {0, 1, 1, 0, -30383, 104755},
    {2, 0, 0, -2, 15327, 10321},
    {0, 0, 1, 2, -12528, 0},
    {0, 0, 1, -2, 10980, 79661},
    {4, 0, -1, 0, 10675, -34782},
    {0, 0, 3, 0, 10034, -23210},
    {4, 0, -2, 0, 8548, -21636},
    {2, 1, -1, 0, -7888, 24208},
    {2, 1, 0, 0, -6766, 30824},
    {1, 0, -1, 0, -5163, -8379},
    {1, 1, 0, 0, 4987, -16675},
    {2, -1, 1, 0, 4036, -12831},
    {2, 0, 2, 0, 3994, -10445},
    {4, 0, 0, 0, 3861, -11650},
    {2, 0, -3, 0, 3665, 14403},
    {0, 1, -2, 0, -2689, -7003},
    {2, 0, -1, 2, -2602, 0},
    {2, -1, -2, 0, 2390, 10056},
    {1, 0, 1, 0, -2348, 6322},
    {2, -2, 0, 0, 2236, -9884},
    {0, 1, 2, 0, -2120, 5751},
    {0, 2, 0, 0, -2069, 0},
    {2, -2, -1, 0, 2048, -4950},
    {2, 0, 1, -2, -1773, 4130},
    {2, 0, 0, 2, -1595, 0},
    {4, -1, -1, 0, 1215, -3958},
    {0, 0, 2, 2, -1110, 0},
    {3, 0, -1, 0, -892, 3258},
    {2, 1, 1, 0, -810, 2616},
    {4, -1, -2, 0, 759, -1897},
    {0, 2, -1, 0, -713, -2117},
    {2, 2, -1, 0, -700, 2354},
    {2, 1, -2, 0, 691, 0},
    {2, -1, 0, -2, 596, 0},
    {4, 0, 1, 0, 549, -1423},
    {0, 0, 4, 0, 537, -1117},
    {4, -1, 0, 0, 520, -1571},
    {1, 0, -2, 0, -487, -1739},
    {2, 1, 0, -2, -399, 0},
    {0, 0, 2, -2, -381, -4421},
    {1, 1, 1, 0, 351, 0},
    {3, 0, -2, 0, -340, 0},
    {4, 0, -3, 0, 330, 0},
    {2, -1, 2, 0, 327, 0},
    {0, 2, 1, 0, -323, 1165},
    {1, 1, -1, 0, 299, 0},
    {2, 0, 3, 0, 294, 0},
    {2, 0, -1, -2, 0, 8752},
};

constexpr LatTerm kLatTerms[] = {
    {0, 0, 0, 1, 5128122}, {0, 0, 1, 1, 280602},  {0, 0, 1, -1, 277693}, {2, 0, 0, -1, 173237},
    {2, 0, -1, 1, 55413},  {2, 0, -1, -1, 46271}, {2, 0, 0, 1, 32573},   {0, 0, 2, 1, 17198},
    {2, 0, 1, -1, 9266},   {0, 0, 2, -1, 8822},   {2, -1, 0, -1, 8216},  {2, 0, -2, -1, 4324},
    {2, 0, 1, 1, 4200},    {2, 1, 0, -1, -3359},  {2, -1, -1, 1, 2463},  {2, -1, 0, 1, 2211},
    {2, -1, -1, -1, 2065}, {0, 1, -1, -1, -1870}, {4, 0, -1, -1, 1828},  {0, 1, 0, 1, -1794},
    {0, 0, 0, 3, -1749},   {0, 1, -1, 1, -1565},  {1, 0, 0, 1, -1491},   {0, 1, 1, 1, -1475},
    {0, 1, 1, -1, -1410},  {0, 1, 0, -1, -1344},  {1, 0, 0, -1, -1335},  {0, 0, 3, 1, 1107},
    {4, 0, 0, -1, 1021},   {4, 0, -1, 1, 833},    {0, 0, 1, -3, 777},    {4, 0, -2, 1, 671},
    {2, 0, 0, -3, 607},    {2, 0, 2, -1, 596},    {2, -1, 1, -1, 491},   {2, 0, -2, 1, -451},
    {0, 0, 3, -1, 439},    {2, 0, 2, 1, 422},     {2, 0, -3, -1, 421},   {2, 1, -1, 1, -366},
    {2, 1, 0, 1, -351},    {4, 0, 0, 1, 331},     {2, -1, 1, 1, 315},    {2, -2, 0, -1, 302},
    {0, 0, 1, 3, -283},    {2, 1, 1, -1, -229},   {1, 1, 0, -1, 223},    {1, 1, 0, 1, 223},
    {0, 1, -2, -1, -220},  {2, 1, -1, -1, -220},  {1, 0, 1, 1, -185},    {2, -1, -2, -1, 181},
    {0, 1, 2, 1, -177},    {4, 0, -2, -1, 176},   {4, -1, -1, -1, 166},  {1, 0, 1, -1, -164},
    {4, 0, 1, -1, 132},    {1, 0, -1, -1, -119},  {4, -1, 0, -1, 115},   {2, -2, 0, 1, 107},
};

// A polynomial in t, constant first.
double poly(double t, double c0, double c1, double c2, double c3, double c4) {
  return c0 + t * (c1 + t * (c2 + t * (c3 + t * c4)));
}

// The nutation in longitude and obliquity (degrees) and the mean obliquity, at T centuries of
// dynamical time from J2000.
struct Nutation {
  double longitude, obliquity, mean_obliquity;
};
Nutation nutation_at(double t) {
  const double omega = 125.04452 - 1934.136261 * t;
  const double ls = 280.4665 + 36000.7698 * t;
  const double lm = 218.3165 + 481267.8813 * t;
  const double dpsi = -17.20 * sin_d(omega) - 1.32 * sin_d(2.0 * ls) - 0.23 * sin_d(2.0 * lm) +
                      0.21 * sin_d(2.0 * omega);
  const double deps = 9.20 * cos_d(omega) + 0.57 * cos_d(2.0 * ls) + 0.10 * cos_d(2.0 * lm) -
                      0.09 * cos_d(2.0 * omega);
  const double eps0 =
      23.0 + 26.0 / 60.0 + (21.448 - t * (46.8150 + t * (0.00059 - t * 0.001813))) / 3600.0;
  return {dpsi / 3600.0, deps / 3600.0, eps0};
}

// An ecliptic place of date (degrees) as an equatorial unit vector under obliquity `eps`.
Vec3d ecliptic_to_equatorial(double lon, double lat, double eps) {
  const double x = cos_d(lat) * cos_d(lon);
  const double y = cos_d(lat) * sin_d(lon);
  const double z = sin_d(lat);
  return {x, cos_d(eps) * y - sin_d(eps) * z, sin_d(eps) * y + cos_d(eps) * z};
}

// A J2000 equatorial vector precessed to the equator and equinox T centuries later.
Vec3d precess(const Vec3d& v, double t) {
  const double zeta = (2306.2181 + (0.30188 + 0.017998 * t) * t) * t * kArcsec;
  const double z = (2306.2181 + (1.09468 + 0.018203 * t) * t) * t * kArcsec;
  const double theta = (2004.3109 - (0.42665 + 0.041833 * t) * t) * t * kArcsec;
  const double cz = libm::cos(zeta), sz = libm::sin(zeta);
  const double cZ = libm::cos(z), sZ = libm::sin(z);
  const double ct = libm::cos(theta), st = libm::sin(theta);
  return {(cz * ct * cZ - sz * sZ) * v.x + (-sz * ct * cZ - cz * sZ) * v.y + (-st * cZ) * v.z,
          (cz * ct * sZ + sz * cZ) * v.x + (-sz * ct * sZ + cz * cZ) * v.y + (-st * sZ) * v.z,
          (cz * st) * v.x + (-sz * st) * v.y + ct * v.z};
}

}  // namespace

double delta_t(double jd) {
  const double y = 2000.0 + (jd - 2451545.0) / 365.25;
  const double t = y - 2000.0;
  if (y < 1986.0 || y >= 2150.0) {
    const double u = (y - 1820.0) / 100.0;
    return -20.0 + 32.0 * u * u;
  }
  if (y < 2005.0) {
    return 63.86 +
           t * (0.3345 + t * (-0.060374 + t * (0.0017275 + t * (0.000651814 + t * 0.00002373599))));
  }
  if (y < 2050.0) return 62.92 + t * (0.32217 + t * 0.005589);
  const double u = (y - 1820.0) / 100.0;
  return -20.0 + 32.0 * u * u - 0.5628 * (2150.0 - y);
}

LunarPosition lunar_position(double jd) {
  const double jde = jd + delta_t(jd) / 86400.0;
  const double t = (jde - 2451545.0) / 36525.0;
  // The mean longitude and the four fundamental arguments, degrees.
  const double lp =
      wrap360(poly(t, 218.3164477, 481267.88123421, -0.0015786, 1.0 / 538841.0, -1.0 / 65194000.0));
  const double d =
      wrap360(poly(t, 297.8501921, 445267.1114034, -0.0018819, 1.0 / 545868.0, -1.0 / 113065000.0));
  const double m = wrap360(poly(t, 357.5291092, 35999.0502909, -0.0001535, 1.0 / 24490000.0, 0));
  const double mm =
      wrap360(poly(t, 134.9633964, 477198.8675055, 0.0087414, 1.0 / 69699.0, -1.0 / 14712000.0));
  const double f =
      wrap360(poly(t, 93.2720950, 483202.0175233, -0.0036539, -1.0 / 3526000.0, 1.0 / 863310000.0));
  const double a1 = 119.75 + 131.849 * t;
  const double a2 = 53.09 + 479264.290 * t;
  const double a3 = 313.45 + 481266.484 * t;
  // The Earth's orbit's eccentricity shrinking: a term in M is scaled by E a multiple.
  const double e = 1.0 - t * (0.002516 + t * 0.0000074);
  double sl = 0, sr = 0, sb = 0;
  for (const LonTerm& k : kLonTerms) {
    const double arg = k.d * d + k.m * m + k.mm * mm + k.f * f;
    const double ek = k.m == 0 ? 1.0 : (k.m == 1 || k.m == -1) ? e : e * e;
    sl += k.lon * ek * sin_d(arg);
    sr += k.dist * ek * cos_d(arg);
  }
  for (const LatTerm& k : kLatTerms) {
    const double arg = k.d * d + k.m * m + k.mm * mm + k.f * f;
    const double ek = k.m == 0 ? 1.0 : (k.m == 1 || k.m == -1) ? e : e * e;
    sb += k.lat * ek * sin_d(arg);
  }
  // Venus's, Jupiter's and the flattening's additive terms.
  sl += 3958.0 * sin_d(a1) + 1962.0 * sin_d(lp - f) + 318.0 * sin_d(a2);
  sb += -2235.0 * sin_d(lp) + 382.0 * sin_d(a3) + 175.0 * sin_d(a1 - f) + 175.0 * sin_d(a1 + f) +
        127.0 * sin_d(lp - mm) - 115.0 * sin_d(lp + mm);
  const Nutation n = nutation_at(t);
  const double lon = wrap360(lp + sl / 1e6 + n.longitude);
  const double lat = sb / 1e6;
  const double eps = n.mean_obliquity + n.obliquity;
  const Vec3d q = ecliptic_to_equatorial(lon, lat, eps);
  return LunarPosition{lon,
                       lat,
                       385000.56 + sr / 1000.0,
                       wrap360(libm::atan2(q.y, q.x) / kDeg),
                       libm::asin(clamp_unit(q.z)) / kDeg,
                       eps,
                       n.longitude};
}

double apparent_sidereal_time(double jd) {
  const double jde = jd + delta_t(jd) / 86400.0;
  const Nutation n = nutation_at((jde - 2451545.0) / 36525.0);
  return wrap360(sidereal_time(jd) + n.longitude * cos_d(n.mean_obliquity + n.obliquity));
}

Vec3d equatorial_to_world(const Vec3d& e, double sidereal, double latitude) {
  // The hour-angle frame: x toward the meridian's equator point, y toward the east point.
  const double ct = cos_d(sidereal), st = sin_d(sidereal);
  const double hx = e.x * ct + e.y * st;  // cos dec cos H
  const double hy = e.x * st - e.y * ct;  // cos dec sin H (H west of the meridian)
  const double sp = sin_d(latitude), cp = cos_d(latitude);
  return {-hy, sp * e.z + cp * hx, cp * e.z - sp * hx};
}

MoonSky moon_sky(double jd, double latitude, double longitude) {
  const LunarPosition lp = lunar_position(jd);
  const double jde = jd + delta_t(jd) / 86400.0;
  const double t = (jde - 2451545.0) / 36525.0;
  const double lst = wrap360(apparent_sidereal_time(jd) + longitude);
  // Geocentric places, equatorial of date, km: the moon, the sun, the eye on a round Earth.
  const Vec3d moon = scale(polar(lp.right_ascension, lp.declination), lp.distance_km);
  const SolarCoordinates sc = solar_coordinates(jd);
  const double ms = 357.52911 + 35999.05029 * t;
  const double au = 1.00014 - 0.01671 * cos_d(ms) - 0.00014 * cos_d(2.0 * ms);
  const Vec3d sun_dir = ecliptic_to_equatorial(sc.longitude, 0.0, lp.obliquity);
  const Vec3d sun = scale(sun_dir, au * kAuKm);
  const Vec3d eye = scale(polar(lst, latitude), kEarthRadiusKm);
  const Vec3d seen = sub(moon, eye);
  const double dist = length(seen);
  const Vec3d toward = scale(seen, 1.0 / dist);
  // The phase: the angle at the moon between the sun and the eye.
  const Vec3d to_sun = unit(sub(sun, moon));
  const double cos_i = clamp_unit(dot(to_sun, scale(toward, -1.0)));
  const double phase = libm::acos(cos_i);
  // The face: the IAU's pole and prime meridian (J2000), precessed to the date.
  const double dd = jde - 2451545.0;
  const double e1 = 125.045 - 0.0529921 * dd, e2 = 250.089 - 0.1059842 * dd;
  const double e3 = 260.008 + 13.0120009 * dd, e4 = 176.625 + 13.3407154 * dd;
  const double e5 = 357.529 + 0.9856003 * dd, e6 = 311.589 + 26.4057084 * dd;
  const double e7 = 134.963 + 13.0649930 * dd, e8 = 276.617 + 0.3287146 * dd;
  const double e9 = 34.226 + 1.7484877 * dd, e10 = 15.134 - 0.1589763 * dd;
  const double e11 = 119.743 + 0.0036096 * dd, e12 = 239.961 + 0.1643573 * dd;
  const double e13 = 25.053 + 12.9590088 * dd;
  const double tc = dd / 36525.0;
  const double a0 = 269.9949 + 0.0031 * tc - 3.8787 * sin_d(e1) - 0.1204 * sin_d(e2) +
                    0.0700 * sin_d(e3) - 0.0172 * sin_d(e4) + 0.0072 * sin_d(e6) -
                    0.0052 * sin_d(e10) + 0.0043 * sin_d(e13);
  const double d0 = 66.5392 + 0.0130 * tc + 1.5419 * cos_d(e1) + 0.0239 * cos_d(e2) -
                    0.0278 * cos_d(e3) + 0.0068 * cos_d(e4) - 0.0029 * cos_d(e6) +
                    0.0009 * cos_d(e7) + 0.0008 * cos_d(e10) - 0.0009 * cos_d(e13);
  const double w = 38.3213 + 13.17635815 * dd - 1.4e-12 * dd * dd + 3.5610 * sin_d(e1) +
                   0.1208 * sin_d(e2) - 0.0642 * sin_d(e3) + 0.0158 * sin_d(e4) +
                   0.0252 * sin_d(e5) - 0.0066 * sin_d(e6) - 0.0047 * sin_d(e7) -
                   0.0046 * sin_d(e8) + 0.0028 * sin_d(e9) + 0.0052 * sin_d(e10) +
                   0.0040 * sin_d(e11) + 0.0019 * sin_d(e12) - 0.0044 * sin_d(e13);
  const Vec3d pole0 = polar(a0, d0);
  const Vec3d node = {-sin_d(a0), cos_d(a0), 0.0};  // its equator's ascending node
  const Vec3d across = cross(pole0, node);
  const Vec3d prime0 = add(scale(node, cos_d(w)), scale(across, sin_d(w)));
  const Vec3d pole = precess(pole0, tc);
  const Vec3d prime = precess(prime0, tc);
  const Vec3d east = cross(pole, prime);
  // The point under the eye, and the axis's tilt from celestial north (east positive).
  const Vec3d back = scale(toward, -1.0);
  const double lib_lon = libm::atan2(dot(back, east), dot(back, prime)) / kDeg;
  const double lib_lat = libm::asin(clamp_unit(dot(back, pole))) / kDeg;
  const Vec3d ncp = {0.0, 0.0, 1.0};
  const Vec3d sky_east = unit(cross(ncp, toward));
  const Vec3d sky_north = cross(toward, sky_east);
  const double axis = wrap360(libm::atan2(dot(pole, sky_east), dot(pole, sky_north)) / kDeg);
  // The Earth's shadow: its axis away from the sun, through the moon's distance along it.
  const Vec3d anti = scale(sun_dir, -1.0);
  const double along = dot(moon, anti);
  const Vec3d centre = scale(anti, along);
  const Vec3d to_shadow = sub(centre, eye);
  const double shadow_dist = length(to_shadow);
  const double parallax = libm::asin(kEarthRadiusKm / lp.distance_km);
  const double sun_semi = 959.63 * kArcsec / au;
  const double sun_parallax = 8.794 * kArcsec / au;
  const double umbra = 1.02 * (parallax - sun_semi + sun_parallax);
  const double penumbra = 1.02 * (parallax + sun_semi + sun_parallax);
  // The radii at the moon's distance, km, as angles from the eye.
  // On the sun's side of the Earth (a moon near new) no shadow reaches it.
  const double umbra_km = along > 0 ? along * libm::tan(umbra) : 0.0;
  const double penumbra_km = along > 0 ? along * libm::tan(penumbra) : 0.0;
  const Vec3d world = equatorial_to_world(toward, lst, latitude);
  const double elevation = libm::asin(clamp_unit(world.y));
  double azimuth = libm::atan2(world.x, world.z);
  if (azimuth < 0) azimuth += kTau;
  const double elong = wrap360(lp.longitude - sc.longitude);
  return MoonSky{elevation,
                 elevation + refraction(elevation),
                 azimuth,
                 dist,
                 libm::asin(kMoonRadiusKm / dist),
                 phase,
                 (1.0 + cos_i) / 2.0,
                 elong / 360.0 * kSynodicMonth,
                 lib_lon,
                 lib_lat,
                 axis,
                 world,
                 equatorial_to_world(pole, lst, latitude),
                 equatorial_to_world(prime, lst, latitude),
                 equatorial_to_world(east, lst, latitude),
                 equatorial_to_world(sun_dir, lst, latitude),
                 equatorial_to_world(scale(to_shadow, 1.0 / shadow_dist), lst, latitude),
                 umbra_km / shadow_dist,
                 penumbra_km / shadow_dist};
}

MoonSky moon_on_date(double hour, double latitude, double longitude, double jd0) {
  return moon_sky(jd0 + universal_hour_of(jd0, hour, longitude) / 24.0, latitude, longitude);
}

}  // namespace ge::core
