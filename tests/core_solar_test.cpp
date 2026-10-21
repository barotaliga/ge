// engine/core/solar.cpp against the oracle's runtime (tests/golden/core_solar.txt, written by
// tools/parity/golden-core-libm.mjs under game5's pinned Node: make golden-libm). The row
// layout is stated in that script; the column index of each output is spelled out below.
#include "core/solar.hpp"

#include <optional>

#include "golden.hpp"

using ge::test::as_double;

namespace {

using Row = std::vector<uint64_t>;

void check_bool(bool got, const Row& row, size_t at) { CHECK_BITS(got ? 1.0 : 0.0, as_double(row[at])); }

void check_opt(const std::optional<double>& got, const Row& row, size_t at) {
  check_bool(got.has_value(), row, at);
  CHECK_BITS(got.value_or(0.0), as_double(row[at + 1]));
}

void check_window(const std::optional<ge::core::HourWindow>& got, const Row& row, size_t at) {
  check_bool(got.has_value(), row, at);
  CHECK_BITS(got ? got->min : 0.0, as_double(row[at + 1]));
  CHECK_BITS(got ? got->max : 0.0, as_double(row[at + 2]));
}

}  // namespace

TEST(solar_matches_oracle_bit_for_bit) {
  namespace c = ge::core;
  const auto rows = ge::test::golden_rows("tests/golden/core_solar.txt");
  CHECK(rows.size() == 600);
  for (const auto& row : rows) {
    const double hour = as_double(row[0]), lat = as_double(row[1]), dec = as_double(row[2]);
    const double age = as_double(row[3]), year = as_double(row[4]), doy = as_double(row[5]);
    const double lon = as_double(row[6]), min_el = as_double(row[7]), el = as_double(row[8]);
    // The oracle script's civil date off the day of the year.
    const double month = 1.0 + static_cast<double>(static_cast<int>(doy) % 12);
    const double day = 1.0 + static_cast<double>(static_cast<int>(doy) % 28);

    const c::SunPlace at = c::sun_at(hour, lat, dec);
    CHECK_BITS(at.elevation, as_double(row[9]));
    CHECK_BITS(at.azimuth, as_double(row[10]));
    check_bool(at.rising, row, 11);
    check_opt(c::hour_of_elevation(min_el, true, lat, dec), row, 12);
    check_opt(c::hour_of_elevation(min_el, false, lat, dec), row, 14);
    check_window(c::daylight_window(lat, min_el, dec), row, 16);

    const c::MoonPlace moon = c::moon_at(hour, lat, dec, age);
    CHECK_BITS(moon.elevation, as_double(row[19]));
    CHECK_BITS(moon.azimuth, as_double(row[20]));
    check_bool(moon.rising, row, 21);
    CHECK_BITS(moon.lit, as_double(row[22]));

    const double jd0 = c::julian_day_of_year(year, doy);
    CHECK_BITS(jd0, as_double(row[23]));
    CHECK_BITS(c::julian_day(year, month, day), as_double(row[24]));
    const double jd = jd0 + hour / 24.0;
    const c::SolarCoordinates sc = c::solar_coordinates(jd);
    CHECK_BITS(sc.declination, as_double(row[25]));
    CHECK_BITS(sc.equation_of_time, as_double(row[26]));
    CHECK_BITS(sc.longitude, as_double(row[27]));
    CHECK_BITS(sc.obliquity, as_double(row[28]));
    CHECK_BITS(c::refraction(el), as_double(row[29]));
    CHECK_BITS(c::universal_hour_of(jd0, hour, lon), as_double(row[30]));

    const c::DatedSunPlace dated = c::sun_on_date(hour, lat, lon, jd0);
    CHECK_BITS(dated.elevation, as_double(row[31]));
    CHECK_BITS(dated.azimuth, as_double(row[32]));
    check_bool(dated.rising, row, 33);
    CHECK_BITS(dated.true_elevation, as_double(row[34]));
    CHECK_BITS(dated.declination, as_double(row[35]));
    check_window(c::sun_window_on_date(lat, lon, jd0), row, 36);
    check_window(c::sun_window_on_date(lat, lon, jd0, min_el), row, 39);

    const c::MoonOrbit lc = c::lunar_coordinates(jd);
    CHECK_BITS(lc.longitude, as_double(row[42]));
    CHECK_BITS(lc.latitude, as_double(row[43]));
    CHECK_BITS(lc.distance, as_double(row[44]));
    CHECK_BITS(lc.right_ascension, as_double(row[45]));
    CHECK_BITS(lc.declination, as_double(row[46]));

    const c::DatedMoonPlace dm = c::moon_on_date(hour, lat, lon, jd0);
    CHECK_BITS(dm.elevation, as_double(row[47]));
    CHECK_BITS(dm.azimuth, as_double(row[48]));
    check_bool(dm.rising, row, 49);
    CHECK_BITS(dm.lit, as_double(row[50]));
    CHECK_BITS(dm.age, as_double(row[51]));
    CHECK_BITS(dm.declination, as_double(row[52]));
    CHECK(row.size() == 53);
  }
}
