// engine/wind/climate.hpp: the day's curve is coldest at sunrise and warmest after noon, its swing
// shrinks under cloud, in wind and indoors; the air cools with height; a clear, still night pools
// cold in a valley and a windy one does not; the dew point holds while the humidity swings and
// never passes the air; water and wet ground moisten the air; Magnus and the wet bulb meet their
// table values; the phase turns from snow to rain round a 1 C wet bulb, so dry air brings snow
// down further; the snow level lies under the freezing level; and every number keeps its bits in
// tests/golden/wind_climate.txt.
//
// A deliberate change moves the golden: rewrite it with
// `GE_GOLDEN_WRITE=1 build/release/tests/wind_climate_test`, and put the `climate` lab's charts
// before and after in the PR.
//
// ge:summary Test: the air's temperature, humidity, dew point, wet bulb and snow level; golden
// ge:keywords test climate temperature humidity dew-point wet-bulb cold-pool snow-level golden
// ge:skill water testing
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>

#include "check.hpp"
#include "golden.hpp"
#include "wind/climate.hpp"

namespace {

namespace gw = ge::wind;

constexpr const char* kGolden = "tests/golden/wind_climate.txt";

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

gw::Climate clear_calm() {
  gw::Climate c;
  c.mean_c = 1;
  c.range_c = 10;
  c.humidity = 0.8;
  c.cloud = 0;
  c.wind = 0.5;
  return c;
}

double temp(const gw::Climate& c, const gw::Site& s, double h) {
  return gw::air_at(c, s, h).temp_c;
}

}  // namespace

TEST(the_day_is_coldest_at_sunrise_and_warmest_two_hours_past_noon) {
  const gw::Climate c = clear_calm();
  const gw::Site open;
  double low = 1e9, high = -1e9, at_low = -1, at_high = -1;
  for (int k = 0; k < 240; ++k) {
    const double h = k / 10.0, t = temp(c, open, h);
    if (t < low) low = t, at_low = h;
    if (t > high) high = t, at_high = h;
  }
  CHECK(std::fabs(at_low - c.sunrise) < 0.15);
  CHECK(std::fabs(at_high - 14) < 0.15);
  // The midpoint of the low and the high is the mean; their gap the damped swing.
  CHECK(std::fabs(0.5 * (low + high) - c.mean_c) < 0.2);
  CHECK(std::fabs((high - low) - c.range_c * gw::swing_of(c, open)) < 0.05);
  // The curve is continuous at sunset and round midnight.
  CHECK(std::fabs(temp(c, open, 17.999) - temp(c, open, 18.001)) < 0.01);
  CHECK(std::fabs(temp(c, open, 23.999) - temp(c, open, 24.001)) < 0.01);
}

TEST(cloud_wind_and_a_roof_damp_the_swing) {
  gw::Climate c = clear_calm();
  const gw::Site open;
  gw::Site indoors;
  indoors.sky = 0;
  const double clear = gw::swing_of(c, open);
  c.cloud = 1;
  CHECK(gw::swing_of(c, open) < 0.35 * clear);
  c.cloud = 0;
  c.wind = 8.5;
  CHECK(gw::swing_of(c, open) < 0.55 * clear);
  c.wind = 0.5;
  CHECK(gw::swing_of(c, indoors) < 0.2 * clear);
}

TEST(the_air_cools_upward_and_a_still_night_pools_cold_in_the_valley) {
  gw::Climate c = clear_calm();
  gw::Site low, high, valley;
  high.y = 1000;
  valley.hollow = 300;
  CHECK(std::fabs((temp(c, low, 14) - temp(c, high, 14)) - 6.5) < 1e-9);
  // Before dawn the valley floor is the coldest place; after noon the pool has gone.
  CHECK(temp(c, valley, 5) < temp(c, low, 5) - 4);
  CHECK(std::fabs(temp(c, valley, 14) - temp(c, low, 14)) < 1e-9);
  CHECK(gw::night_chill(c, 3) > 0.9);
  CHECK(gw::night_chill(c, 12) == 0);
  c.wind = 8;
  CHECK(gw::night_chill(c, 3) < 0.1);
  c.wind = 0.5;
  c.cloud = 1;
  CHECK(gw::night_chill(c, 3) == 0);
}

TEST(the_dew_point_holds_while_the_humidity_swings_and_never_passes_the_air) {
  const gw::Climate c = clear_calm();
  const gw::Site open;
  const gw::Thermal noon = gw::air_at(c, open, 14), dawn = gw::air_at(c, open, 6);
  CHECK(dawn.rh > noon.rh + 0.2);
  CHECK(dawn.dew_c <= dawn.temp_c);
  CHECK(std::fabs(noon.dew_c - gw::dew_point_of(c.mean_c, c.humidity)) < 1e-9);
  for (int k = 0; k < 48; ++k) {
    const gw::Thermal a = gw::air_at(c, open, k / 2.0);
    CHECK(a.dew_c <= a.temp_c);
    CHECK(a.rh > 0 && a.rh <= 1);
    CHECK(a.wet_bulb_c <= a.temp_c + 1e-9 && a.wet_bulb_c >= a.dew_c - 1e-9);
  }
}

TEST(water_and_wet_ground_moisten_the_air) {
  gw::Climate c = clear_calm();
  c.mean_c = 10;
  c.humidity = 0.5;
  c.water_c = 14;
  gw::Site dry, shore, puddles;
  shore.water_m = 10;
  puddles.wet = 1;
  CHECK(gw::air_at(c, shore, 14).dew_c > gw::air_at(c, dry, 14).dew_c + 3);
  CHECK(gw::air_at(c, puddles, 14).dew_c > gw::air_at(c, dry, 14).dew_c + 3);
  // A lake far off moistens nothing.
  gw::Site far;
  far.water_m = 5000;
  CHECK(std::fabs(gw::air_at(c, far, 14).dew_c - gw::air_at(c, dry, 14).dew_c) < 1e-6);
}

TEST(magnus_and_the_wet_bulb_meet_their_table_values) {
  CHECK(std::fabs(gw::saturation_hpa(0) - 6.11) < 0.01);
  CHECK(std::fabs(gw::saturation_hpa(20) - 23.37) < 0.1);
  CHECK(std::fabs(gw::saturation_ice_hpa(-10) - 2.60) < 0.02);
  CHECK(std::fabs(gw::dew_point_of(20, 0.5) - 9.3) < 0.1);
  CHECK(std::fabs(gw::dew_point_of(7, 1) - 7) < 1e-9);
  // Psychrometric tables at sea level: 20 C at 50 % is a 13.8 C wet bulb, 30 C at 30 % 18.0.
  CHECK(std::fabs(gw::wet_bulb_of(20, 0.5) - 13.8) < 0.2);
  CHECK(std::fabs(gw::wet_bulb_of(30, 0.3) - 18.0) < 0.3);
  CHECK(std::fabs(gw::wet_bulb_of(5, 1) - 5) < 1e-6);
  CHECK(gw::pressure_at(2000) < 800 && gw::pressure_at(2000) > 780);
}

TEST(the_fall_turns_to_rain_round_a_one_degree_wet_bulb_and_dry_air_brings_snow_lower) {
  gw::Thermal a;
  a.wet_bulb_c = -1;
  CHECK(gw::rain_share_of(a) == 0);
  a.wet_bulb_c = 1;
  CHECK(std::fabs(gw::rain_share_of(a) - 0.5) < 1e-12);
  a.wet_bulb_c = 3;
  CHECK(gw::rain_share_of(a) == 1);
  // At 3 C a dry air still snows; a saturated one rains.
  gw::Thermal dry, damp;
  dry.temp_c = damp.temp_c = 3;
  dry.wet_bulb_c = gw::wet_bulb_of(3, 0.4);
  damp.wet_bulb_c = gw::wet_bulb_of(3, 1);
  CHECK(gw::rain_share_of(dry) < 0.2);
  CHECK(gw::rain_share_of(damp) > 0.8);
}

TEST(the_snow_level_lies_under_the_freezing_level) {
  gw::Climate c = clear_calm();
  c.mean_c = 6;
  for (const double rh : {0.5, 0.8, 0.98}) {
    c.humidity = rh;
    const double freezing = gw::freezing_level(c, 14), snow = gw::snow_level(c, 14);
    CHECK(snow < freezing);
    CHECK(snow > freezing - 1200);
  }
  // The freezing level is where the open air's temperature crosses 0.
  gw::Site at;
  at.y = gw::freezing_level(c, 9);
  CHECK(std::fabs(temp(c, at, 9)) < 1e-9);
  gw::Climate warm = c;
  warm.lapse = 0;
  CHECK(gw::freezing_level(warm, 9) == 1e9);
}

TEST(every_number_keeps_its_golden) {
  const bool write = env_set("GE_GOLDEN_WRITE");
  const auto want = ge::test::golden_rows(kGolden);
  if (!write) CHECK_MSG(!want.empty(), std::string(kGolden) + " is empty; see the header");
  std::vector<std::vector<uint64_t>> got;
  gw::Climate c = clear_calm();
  c.water_c = 5;
  gw::Site sites[4];
  sites[1].y = 900;
  sites[2].hollow = 120;
  sites[2].water_m = 60;
  sites[3].sky = 0.2;
  sites[3].wet = 0.7;
  sites[3].snow = 1;
  for (const gw::Site& s : sites) {
    for (const double h : {0.0, 5.5, 9.0, 14.0, 19.5}) {
      const gw::Thermal a = gw::air_at(c, s, h);
      got.push_back({ge::test::bits(a.temp_c), ge::test::bits(a.dew_c), ge::test::bits(a.rh),
                     ge::test::bits(a.wet_bulb_c), ge::test::bits(a.frost_c),
                     ge::test::bits(gw::surface_at(c, s, h))});
    }
  }
  for (const double h : {3.0, 14.0}) {
    got.push_back({ge::test::bits(gw::freezing_level(c, h)), ge::test::bits(gw::snow_level(c, h))});
  }
  if (write) {
    std::ofstream out(ge::test::root_path(kGolden));
    out << "# temp, dew, rh, wet bulb, frost point, surface per site and hour; freezing and snow "
           "levels per hour (wind_climate_test)\n";
    for (const auto& row : got) {
      std::string line;
      for (const uint64_t word : row) {
        char b[24];
        std::snprintf(b, sizeof b, "%llx", static_cast<unsigned long long>(word));
        line += (line.empty() ? "" : " ") + std::string(b);
      }
      out << line << "\n";
    }
    return;
  }
  CHECK_MSG(got == want, "the air's temperature moved; if the move is meant, rewrite " +
                             std::string(kGolden) +
                             " with GE_GOLDEN_WRITE=1 on the release build and put the climate "
                             "lab's charts before and after in the PR (skill: water)");
}
