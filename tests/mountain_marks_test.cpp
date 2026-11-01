// plugins/mountain/air/marks.hpp: a clear, calm night leaves frost on a car's roof by dawn that the
// morning's sun takes by noon, and an overcast or windy night leaves next to none; freezing rain
// (a snow line picked high over a cold valley) glazes a road within the hour, plain snow and a
// thawing road never do; lake ice grows by the root of the winter's days, thicker higher up, and
// bears a skier after a few cold weeks, and a mild map's lake stays open.
//
// ge:summary Test: the cold's marks on the map: frost by dawn, black ice, lake ice
// ge:keywords test frost frost_on frost_cover glaze_at black-ice freezing-rain lake_ice ice_bears
// ge:skill mountain testing
#include <cmath>

#include "check.hpp"
#include "mountain/air/air.hpp"
#include "mountain/air/marks.hpp"
#include "mountain/mapgen/weather.hpp"
#include "wind/climate.hpp"
#include "wind/cold.hpp"

namespace {

namespace ma = ge::mountain::air;
namespace mg = ge::mountain::mapgen;
namespace gw = ge::wind;

mg::Level day(mg::WeatherKind kind, double mean, double wind = 1) {
  mg::Level l;
  l.seed = 38;
  l.sun = {12, 30, 46, std::nullopt};
  l.mountain.base.y = 1000;
  l.mountain.summit.y = 2400;
  l.weather.kind = kind;
  l.weather.wind = wind;
  l.weather.mean_c = mean;
  return l;
}

// A car's roof in the village at the mountain's foot: bare, open to the sky.
gw::Site roof() {
  gw::Site s;
  s.y = 1000;
  s.above = 1.5;
  return s;
}

}  // namespace

TEST(a_clear_night_frosts_a_roof_by_dawn_and_the_sun_takes_it) {
  const mg::Level clear = day(mg::WeatherKind::clear, -4);
  const double dawn = ma::frost_on(clear, roof(), 7), noon = ma::frost_on(clear, roof(), 13);
  CHECK(dawn > ma::kFrostHides);
  CHECK(ma::frost_cover(dawn) == 1);
  CHECK(noon == 0 && ma::frost_cover(noon) == 0);
  // It grew through the night.
  CHECK(ma::frost_on(clear, roof(), 2) < dawn);
  // Under cloud, or in a wind, the roof stays near the air and gathers far less.
  CHECK(ma::frost_on(day(mg::WeatherKind::overcast, -4), roof(), 7) < 0.3 * dawn);
  CHECK(ma::frost_on(day(mg::WeatherKind::clear, -4, 9), roof(), 7) < 0.5 * dawn);
  // A night too warm to freeze leaves none.
  CHECK(ma::frost_on(day(mg::WeatherKind::clear, 12), roof(), 7) == 0);
  // The same hour, the same frost.
  CHECK_BITS(ma::frost_on(clear, roof(), 7), dawn);
}

TEST(freezing_rain_glazes_a_cold_road_and_snow_does_not) {
  mg::Level rain = day(mg::WeatherKind::snow, -4);
  mg::SkyPick pick;
  pick.snowfall = 0.6;
  pick.snowline = 2;  // a warm layer aloft: rain to the summit and beyond
  mg::with_sky(rain, pick);
  const double t = 3600;
  CHECK(ma::fall_at(rain, t, roof().y, ma::hour_at(rain, t)).rain > 0.99);
  CHECK(ma::glaze_at(rain, roof(), t) > 0.99);
  // Plain snow on the same road: no glaze.
  mg::Level snow = day(mg::WeatherKind::snow, -4);
  snow.weather.snowfall = 0.6;
  CHECK(ma::glaze_at(snow, roof(), t) == 0);
  // The same rain on a thawing road: none.
  mg::Level thaw = rain;
  thaw.weather.mean_c = 8;
  CHECK(ma::glaze_at(thaw, roof(), t) == 0);
  // A dry day: none.
  CHECK(ma::glaze_at(day(mg::WeatherKind::clear, -4), roof(), t) == 0);
}

TEST(lake_ice_grows_through_the_winter_and_bears_a_skier) {
  const mg::Level cold = day(mg::WeatherKind::clear, -8);
  const double month = ma::lake_ice(cold, 1000, 30), four = ma::lake_ice(cold, 1000, 120);
  CHECK(std::fabs(four / month - 2) < 1e-9);
  // Higher is colder: thicker.
  CHECK(ma::lake_ice(cold, 1800, 30) > month);
  // A month at -8 C bears a skier with room to spare; a mild map's lake is open.
  CHECK(gw::ice_bears(month) > 4 * 90);
  CHECK(ma::lake_ice(day(mg::WeatherKind::clear, 6), 1000, 30) == 0);
}
