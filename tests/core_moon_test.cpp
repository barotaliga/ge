// engine/core/moon.cpp: its bits against tests/golden/core_moon.txt (the engine's own record:
// `GE_GOLDEN_WRITE=1 build/<preset>/tests/core_moon_test` rewrites it, and the PR says why), and
// its accuracy against a high-precision ephemeris: the moon as the JPL Horizons system gives it
// (DE441) from four places over 2000, 2026 and 2049, and the hours of the total eclipse of
// 3 March 2026, each row the place seen without the air, the lit share, the disc's size, the
// point under the eye, the axis's angle and the phase angle. Held: the place to 0.01 degree, the
// face to 0.02, the lit share to 0.02 per cent, the size to half an arc-second.
//
// ge:summary Golden and accuracy test: core/moon against its golden and a reference ephemeris
// ge:keywords test golden moon ephemeris accuracy reference libration eclipse delta-t
// ge:skill testing determinism
#include "core/moon.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "check.hpp"
#include "core/solar.hpp"
#include "golden.hpp"

namespace c = ge::core;

namespace {

constexpr const char* kGolden = "tests/golden/core_moon.txt";
constexpr double kDeg = c::kPi / 180.0;

bool env_set(const char* name) {
#ifdef _MSC_VER
  char* v = nullptr;
  size_t n = 0;
  const bool set = _dupenv_s(&v, &n, name) == 0 && v != nullptr;
  std::free(v);
  return set;
#else
  return std::getenv(name) != nullptr;
#endif
}

std::string hex(double v) {
  char buf[24];
  std::snprintf(buf, sizeof buf, "%016llx", static_cast<unsigned long long>(ge::test::bits(v)));
  return buf;
}

// One row: the inputs (jd, latitude, longitude), then every output of the module.
std::vector<double> row_of(double jd, double lat, double lon) {
  const c::LunarPosition p = c::lunar_position(jd);
  const c::MoonSky s = c::moon_sky(jd, lat, lon);
  std::vector<double> r = {jd,
                           lat,
                           lon,
                           c::delta_t(jd),
                           c::apparent_sidereal_time(jd),
                           p.longitude,
                           p.latitude,
                           p.distance_km,
                           p.right_ascension,
                           p.declination,
                           p.obliquity,
                           p.nutation,
                           s.elevation,
                           s.apparent_elevation,
                           s.azimuth,
                           s.distance_km,
                           s.angular_radius,
                           s.phase_angle,
                           s.lit,
                           s.age,
                           s.libration_longitude,
                           s.libration_latitude,
                           s.axis_angle,
                           s.umbra,
                           s.penumbra};
  for (const c::Vec3d& v : {s.toward, s.pole, s.prime, s.east, s.sun, s.shadow}) {
    r.push_back(v.x);
    r.push_back(v.y);
    r.push_back(v.z);
  }
  return r;
}

// The reference rows: place (latitude, longitude east), UT date and time, then the moon seen
// without the air: azimuth and elevation (degrees), the lit share (per cent), the disc's
// diameter (arc-seconds), the point under the eye (selenographic longitude east and latitude),
// the axis's position angle and the phase angle (degrees).
struct Reference {
  double lat, lon;
  int year, month, day, hour, minute;
  double az, el, lit, diam, sub_lon, sub_lat, axis, phase;
};
constexpr Reference kReference[] = {
    {46.0, 7.0, 2026, 1, 3, 0, 0, 211.627267, 69.137459, 99.59680, 2016.592, 2.369658, -5.595738,
     2.3338, 7.2765},
    {46.0, 7.0, 2026, 2, 9, 5, 17, 182.453268, 21.520298, 52.87729, 1788.718, 2.457086, 7.142579,
     16.3663, 86.7014},
    {46.0, 7.0, 2026, 3, 18, 10, 34, 168.437881, 39.546616, 0.50690, 1928.309, 354.471751,
     -0.550323, 338.4147, 171.8289},
    {46.0, 7.0, 2026, 4, 24, 15, 51, 118.445378, 48.963215, 56.70615, 1923.601, 6.470845, -2.483292,
     14.9634, 82.2912},
    {46.0, 7.0, 2026, 5, 31, 21, 8, 144.139298, 6.728694, 99.47805, 1767.453, 0.532322, 7.163035,
     6.3676, 8.2901},
    {46.0, 7.0, 2026, 7, 8, 2, 25, 115.429010, 36.281554, 46.74705, 1920.177, 353.306169, -4.254788,
     338.6709, 93.7299},
    {46.0, 7.0, 2026, 8, 14, 7, 42, 94.479445, 11.370474, 3.63434, 1925.601, 6.508565, 1.924129,
     21.0171, 158.0250},
    {46.0, 7.0, 2026, 9, 20, 12, 59, 111.034654, -17.024352, 66.21424, 1770.631, 359.217382,
     5.344558, 353.9307, 71.0760},
    {46.0, 7.0, 2026, 10, 27, 18, 16, 69.371262, 12.757701, 96.24586, 1971.227, 358.630350,
     -5.725144, 346.1213, 22.3493},
    {46.0, 7.0, 2026, 12, 3, 23, 33, 75.067063, -27.559026, 22.43378, 1821.927, 7.262419, 5.481873,
     21.9528, 123.4548},
    {69.6, 19.0, 2000, 1, 2, 3, 0, 118.086569, -4.761195, 17.82758, 1772.224, 4.788730, -5.769485,
     14.3886, 130.0457},
    {69.6, 19.0, 2000, 2, 3, 23, 31, 31.889734, -38.743557, 2.31389, 1766.315, 357.500062,
     -0.104206, 347.3161, 162.4954},
    {69.6, 19.0, 2000, 3, 7, 20, 2, 294.893617, -11.263665, 2.98320, 1872.106, 354.215222, 6.721243,
     336.0454, 160.1133},
    {69.6, 19.0, 2000, 4, 9, 16, 33, 204.934467, 38.596733, 29.12913, 1966.289, 0.045453, 4.478693,
     359.2574, 114.6739},
    {69.6, 19.0, 2000, 5, 12, 13, 4, 95.127669, 11.426498, 69.04846, 1890.866, 6.118278, -4.147819,
     23.8582, 67.6041},
    {69.6, 19.0, 2000, 6, 14, 9, 35, 13.428619, -35.477600, 94.15666, 1773.064, 4.767112, -5.128234,
     12.6237, 27.9725},
    {69.6, 19.0, 2000, 7, 17, 6, 6, 273.027685, -23.708861, 99.63040, 1757.334, 357.768169,
     1.575009, 345.4577, 6.9763},
    {69.6, 19.0, 2000, 8, 19, 2, 37, 194.694732, 18.621358, 85.69712, 1847.658, 353.712486,
     7.356295, 336.6382, 44.4474},
    {69.6, 19.0, 2000, 9, 20, 23, 8, 90.976799, 22.187891, 50.94952, 1942.442, 357.599255, 3.639218,
     0.2576, 88.9120},
    {69.6, 19.0, 2000, 10, 23, 19, 39, 357.301094, -11.997057, 14.24003, 1910.808, 3.795694,
     -4.635683, 23.8317, 135.6555},
    {69.6, 19.0, 2000, 11, 25, 16, 10, 261.699373, -15.535120, 0.15554, 1810.834, 4.626392,
     -3.949435, 10.8965, 175.4750},
    {69.6, 19.0, 2000, 12, 28, 12, 41, 176.337649, -0.860493, 6.87390, 1764.302, 358.661081,
     3.438423, 343.5622, 149.6059},
    {-45.0, 170.5, 2049, 2, 1, 10, 0, 206.866799, -24.117496, 2.10149, 1980.915, 359.918604,
     -5.188554, 350.3348, 163.3240},
    {-45.0, 170.5, 2049, 3, 3, 10, 1, 219.669100, -32.853884, 0.74850, 1936.907, 3.014859,
     -7.282974, 339.0044, 170.0690},
    {-45.0, 170.5, 2049, 4, 2, 10, 2, 233.302872, -42.315195, 0.20152, 1872.117, 4.718935,
     -6.357562, 336.0113, 174.8530},
    {-45.0, 170.5, 2049, 5, 2, 10, 3, 247.262200, -47.042509, 0.20878, 1809.867, 4.327494,
     -3.142895, 341.8799, 174.7672},
    {-45.0, 170.5, 2049, 6, 1, 10, 4, 256.033479, -45.302039, 0.60932, 1765.681, 2.207320, 0.899473,
     354.3091, 171.0516},
    {-45.0, 170.5, 2049, 7, 1, 10, 5, 255.953471, -39.237046, 1.31001, 1746.948, 359.322203,
     4.355316, 8.2142, 166.8609},
    {-45.0, 170.5, 2049, 7, 31, 10, 6, 249.696903, -30.874397, 2.25926, 1756.128, 356.635258,
     6.132442, 19.0120, 162.7156},
    {-45.0, 170.5, 2049, 8, 30, 10, 7, 242.159899, -20.833409, 3.52842, 1792.121, 354.830838,
     5.569518, 24.0853, 158.3516},
    {-45.0, 170.5, 2049, 9, 29, 10, 8, 237.914606, -9.852713, 5.42651, 1848.697, 354.354036,
     2.634696, 21.3449, 153.0636},
    {-45.0, 170.5, 2049, 10, 29, 10, 9, 240.232943, -0.228541, 8.46291, 1911.478, 355.565494,
     -1.800988, 9.7987, 146.1798},
    {-45.0, 170.5, 2049, 11, 28, 10, 10, 249.752716, 4.491501, 12.96743, 1958.462, 358.576460,
     -5.904053, 353.4138, 137.7912},
    {-45.0, 170.5, 2049, 12, 28, 10, 11, 263.438240, 2.683976, 18.54915, 1970.125, 2.605120,
     -7.587000, 340.2246, 128.9813},
    {0.0, -80.0, 2026, 3, 3, 6, 0, 309.130699, 77.310754, 99.92725, 1908.816, 4.511702, 0.060969,
     20.5726, 3.0854},
    {0.0, -80.0, 2026, 3, 3, 7, 0, 287.932067, 64.214739, 99.94051, 1905.617, 4.302413, 0.223608,
     20.6090, 2.7895},
    {0.0, -80.0, 2026, 3, 3, 8, 0, 281.583375, 49.984102, 99.95308, 1900.535, 4.120420, 0.383339,
     20.6488, 2.4767},
    {0.0, -80.0, 2026, 3, 3, 9, 0, 278.778646, 35.532546, 99.96500, 1893.881, 3.977784, 0.533436,
     20.6935, 2.1382},
    {0.0, -80.0, 2026, 3, 3, 10, 0, 277.339237, 21.033736, 99.97598, 1886.071, 3.883664, 0.667993,
     20.7443, 1.7707},
    {0.0, -80.0, 2026, 3, 3, 11, 0, 276.603684, 6.546187, 99.98534, 1877.584, 3.843776, 0.782355,
     20.8016, 1.3825},
    {0.0, -80.0, 2026, 3, 3, 12, 0, 276.334840, -7.904446, 99.99209, 1868.934, 3.860151, 0.873408,
     20.8653, 1.0151},
    {0.0, -80.0, 2026, 3, 3, 13, 0, 276.477989, -22.305542, 99.99501, 1860.621, 3.931175, 0.939734,
     20.9345, 0.8090},
    {0.0, -80.0, 2026, 3, 3, 14, 0, 277.127017, -36.649190, 99.99275, 1853.110, 4.051872, 0.981626,
     21.0080, 0.9791},
    {0.0, -80.0, 2026, 3, 3, 15, 0, 278.648726, -50.922902, 99.98405, 1846.800, 4.214360, 1.000996,
     21.0841, 1.4520},
    {0.0, -80.0, 2026, 3, 3, 16, 0, 282.352193, -65.079312, 99.96791, 1842.008, 4.408423, 1.001189,
     21.1612, 2.0582},
};

double wrapped(double d) { return std::remainder(d, 360.0); }

}  // namespace

TEST(moon_matches_golden_bit_for_bit) {
  const bool write = env_set("GE_GOLDEN_WRITE");
  std::vector<std::string> got;
  for (int i = 0; i < 240; i++) {
    // From 1985 to 2065, every latitude, every longitude: no draw, a fixed walk.
    const double jd = 2446066.5 + i * 122.0371;
    const double lat = -88.0 + std::fmod(i * 37.7, 176.0);
    const double lon = -180.0 + std::fmod(i * 71.3, 360.0);
    std::string line;
    for (double v : row_of(jd, lat, lon)) line += (line.empty() ? "" : " ") + hex(v);
    got.push_back(line);
  }
  if (write) {
    std::ofstream out(ge::test::root_path(kGolden));
    out << "# core/moon.hpp's own record (tests/core_moon_test.cpp says the columns)\n";
    for (const auto& l : got) out << l << "\n";
    std::printf("wrote %s (%zu rows)\n", kGolden, got.size());
    return;
  }
  const auto rows = ge::test::golden_rows(kGolden);
  CHECK_MSG(rows.size() == got.size(), std::string(kGolden) +
                                           " has the wrong row count: rewrite it with "
                                           "GE_GOLDEN_WRITE=1 if the module moved on purpose");
  for (size_t i = 0; i < rows.size() && i < got.size(); i++) {
    const std::vector<double> want_inputs = {ge::test::as_double(rows[i][0]),
                                             ge::test::as_double(rows[i][1]),
                                             ge::test::as_double(rows[i][2])};
    const std::vector<double> r = row_of(want_inputs[0], want_inputs[1], want_inputs[2]);
    CHECK(rows[i].size() == r.size());
    for (size_t k = 0; k < r.size() && k < rows[i].size(); k++) {
      CHECK_BITS(r[k], ge::test::as_double(rows[i][k]));
    }
  }
}

TEST(moon_matches_the_reference_ephemeris) {
  double worst_place = 0, worst_face = 0, worst_lit = 0, worst_size = 0;
  for (const Reference& r : kReference) {
    const double jd = c::julian_day(r.year, r.month, r.day) + (r.hour + r.minute / 60.0) / 24.0;
    const c::MoonSky s = c::moon_sky(jd, r.lat, r.lon);
    const double az = wrapped(s.azimuth / kDeg - r.az) * std::cos(r.el * kDeg);
    const double el = s.elevation / kDeg - r.el;
    worst_place = std::fmax(worst_place, std::fmax(std::fabs(az), std::fabs(el)));
    worst_face = std::fmax(worst_face, std::fabs(wrapped(s.libration_longitude - r.sub_lon)));
    worst_face = std::fmax(worst_face, std::fabs(s.libration_latitude - r.sub_lat));
    worst_face = std::fmax(worst_face, std::fabs(wrapped(s.axis_angle - r.axis)));
    worst_face = std::fmax(worst_face, std::fabs(s.phase_angle / kDeg - r.phase));
    worst_lit = std::fmax(worst_lit, std::fabs(s.lit * 100 - r.lit));
    worst_size = std::fmax(worst_size, std::fabs(2 * s.angular_radius / kDeg * 3600 - r.diam));
  }
  std::printf("worst: place %.4f deg, face %.4f deg, lit %.4f %%, size %.3f arcsec\n", worst_place,
              worst_face, worst_lit, worst_size);
  CHECK(worst_place < 0.01);
  CHECK(worst_face < 0.02);
  CHECK(worst_lit < 0.02);
  CHECK(worst_size < 0.5);
}

// The worked example of the lunar theory: 1992 April 12 at 0h dynamical time, the apparent
// longitude 133.167265, latitude -3.229126, distance 368409.7 km, right ascension 134.688470
// and declination 13.768368 degrees. Read at the UT instant delta T earlier.
TEST(moon_matches_the_worked_example) {
  const double jde = 2448724.5;
  const double jd = jde - c::delta_t(jde) / 86400.0;
  const c::LunarPosition p = c::lunar_position(jd);
  CHECK(std::fabs(p.longitude - 133.167265) < 1e-4);
  CHECK(std::fabs(p.latitude + 3.229126) < 1e-5);
  CHECK(std::fabs(p.distance_km - 368409.7) < 0.1);
  CHECK(std::fabs(p.right_ascension - 134.688470) < 1e-4);
  CHECK(std::fabs(p.declination - 13.768368) < 1e-4);
}

// The total eclipse of 3 March 2026: the umbra takes the whole disc from 11:04 to 12:03 UT, its
// middle at 11:33; the moon is out of it by 9:50 and after 13:17 (seen from the Earth's centre;
// a place on it sees the moon shifted by its parallax, the shadow with it).
TEST(the_earths_shadow_takes_the_moon_in_the_2026_total_eclipse) {
  auto state = [](double hour) {
    const c::MoonSky s = c::moon_sky(c::julian_day(2026, 3, 3) + hour / 24.0, 0.0, -150.0);
    const double sep =
        std::acos(s.toward.x * s.shadow.x + s.toward.y * s.shadow.y + s.toward.z * s.shadow.z);
    if (sep + s.angular_radius < s.umbra) return 2;  // total
    if (sep - s.angular_radius < s.umbra) return 1;  // partial
    return 0;
  };
  CHECK(state(9.6) == 0);
  CHECK(state(10.5) == 1);
  CHECK(state(11.55) == 2);
  CHECK(state(12.6) == 1);
  CHECK(state(13.5) == 0);
}

// The sky's own frame: the celestial pole stands due north at the latitude's height, the
// equator's point on the meridian due south at 90 degrees less it, the east point due east.
TEST(equatorial_directions_land_where_the_sky_puts_them) {
  const double lat = 46.0, lst = 123.0;
  const c::Vec3d pole = c::equatorial_to_world({0, 0, 1}, lst, lat);
  CHECK(std::fabs(std::asin(pole.y) / kDeg - lat) < 1e-9);
  CHECK(std::fabs(pole.x) < 1e-12 && pole.z > 0);
  const c::Vec3d meridian =
      c::equatorial_to_world({std::cos(lst * kDeg), std::sin(lst * kDeg), 0}, lst, lat);
  CHECK(std::fabs(std::asin(meridian.y) / kDeg - (90 - lat)) < 1e-9);
  CHECK(std::fabs(meridian.x) < 1e-12 && meridian.z < 0);
  const double east = (lst + 90) * kDeg;
  const c::Vec3d e = c::equatorial_to_world({std::cos(east), std::sin(east), 0}, lst, lat);
  CHECK(std::fabs(e.x - 1) < 1e-12);
}
