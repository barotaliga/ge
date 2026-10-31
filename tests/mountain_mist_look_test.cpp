// plugins/mountain/app/mist_look.hpp: a clear, still winter night fills the valley with a level sea
// of fog that is gone by noon; a wind keeps it out; a fog sky lays it under its clear lid; the
// summit stands in its own cloud under a damp overcast; the day's sunrise and sunset come off the
// latitude and the day of the year; and the answer is the same every time it is asked. Over a
// lake on a spring morning a still, clear dawn lays a bank on the water and a windy one nothing
// that veils it; in the winter's cold the open lake steams in any wind.
//
// ge:summary Test: the mountain's mists by its sky and hour: the valley's sea, the summit's cloud
// ge:keywords test mist fog valley-fog sea-of-fog burn-off hill-fog climate_of mists_at
// ge:keywords water_mist lake-fog steam-fog
// ge:skill mountain testing
#include <cmath>

#include "check.hpp"
#include "mountain/air/air.hpp"
#include "mountain/app/mist_look.hpp"

namespace {

namespace mg = ge::mountain::mapgen;
namespace ma = ge::mountain::app;

// A mountain 1000 m over its valley at 900 m, at 46 degrees north in early January, its ground
// level with the valley's floor.
mg::Level mountain(mg::WeatherKind kind, double wind) {
  mg::Level level{};
  level.sun.hour = 10;
  level.sun.day_of_year = 10;
  level.sun.latitude = 46;
  level.mountain.base = {500, 0, 500};
  level.mountain.summit = {500, 1000, -500};
  level.mountain.vertical = 1000;
  level.mountain.altitude = 900;
  level.ground = ge::core::create_heightfield(0, -1000, 500, 4, 4);
  level.weather = {kind, 0, kind == mg::WeatherKind::fog ? 0.7 : 0, wind, 0, false};
  return level;
}

const double kEye[3] = {500, 2, 500};

}  // namespace

TEST(a_clear_still_morning_fills_the_valley_and_clears_by_noon) {
  const mg::Level level = mountain(mg::WeatherKind::clear, 1);
  const ma::MistLook dawn = ma::mists_at(level, 7, kEye);
  CHECK(dawn.valley.density > 0);
  CHECK(dawn.valley.top_m > level.mountain.base.y + 20);
  CHECK(dawn.valley.top_m < level.mountain.summit.y);
  const ma::MistLook noon = ma::mists_at(level, 12, kEye);
  CHECK(noon.valley.density == 0);
}

TEST(a_wind_keeps_the_valley_clear) {
  const ma::MistLook calm = ma::mists_at(mountain(mg::WeatherKind::clear, 1), 7, kEye);
  const ma::MistLook windy = ma::mists_at(mountain(mg::WeatherKind::clear, 12), 7, kEye);
  CHECK(windy.valley.density < 0.2 * calm.valley.density);
}

TEST(a_fog_sky_lays_the_sea_under_a_clear_lid) {
  const mg::Level level = mountain(mg::WeatherKind::fog, 6);
  const ge::wind::Climate c = ge::mountain::air::climate_of(level);
  CHECK(c.cloud < 0.3 && c.wind <= 1.5 && c.humidity > 0.9);
  CHECK(ma::mists_at(level, 7, kEye).valley.density > 0);
}

TEST(the_summit_stands_in_its_cloud_under_a_damp_overcast) {
  const ma::MistLook day = ma::mists_at(mountain(mg::WeatherKind::snow, 3), 13, kEye);
  CHECK(day.summit.kind == ge::wind::MistKind::hill);
  bool round_summit = false;
  for (const ge::render::FogVolume& v : day.areas)
    round_summit = round_summit || (v.centre[0] == 500 && v.centre[2] == -500 && v.hug_ground);
  CHECK(round_summit);
}

TEST(the_day_runs_sunrise_to_sunset_off_the_latitude) {
  const ge::wind::Climate winter =
      ge::mountain::air::climate_of(mountain(mg::WeatherKind::clear, 1));
  CHECK(winter.sunrise > 7 && winter.sunrise < 8.5);
  CHECK(std::fabs(winter.sunrise + winter.sunset - 24) < 1e-9);
}

TEST(the_same_question_gets_the_same_answer) {
  const mg::Level level = mountain(mg::WeatherKind::clear, 1);
  const ma::MistLook a = ma::mists_at(level, 7.5, kEye);
  const ma::MistLook b = ma::mists_at(level, 7.5, kEye);
  CHECK_BITS(a.valley.top_m, b.valley.top_m);
  CHECK_BITS(a.valley.density, b.valley.density);
  CHECK(a.areas.size() == b.areas.size());
}

TEST(a_still_clear_dawn_fogs_the_lake_and_a_windy_one_does_not) {
  const mg::Level level = mountain(mg::WeatherKind::clear, 1);
  ge::wind::Climate spring = ge::mountain::air::climate_of(level);
  spring.mean_c = 3;
  ma::MapWater lake;
  lake.centre[0] = 500, lake.centre[1] = 400;
  lake.half[0] = lake.half[1] = 90;
  lake.level = 0;
  lake.water_c = 4;
  spring.wind = 0.3;
  const ma::WaterMist still = ma::water_mist(level, spring, 7.6, lake);
  CHECK(still.mist.kind == ge::wind::MistKind::radiation);
  CHECK(still.area.density > 0.01 && still.area.centre[1] - still.area.half[1] == 0);
  CHECK(still.area.shape == ge::render::FogVolume::Shape::box && still.area.edge <= 1);
  spring.wind = 8;
  const ma::WaterMist windy = ma::water_mist(level, spring, 7.6, lake);
  // 180 m across the lake through it veils under a tenth of the view.
  CHECK(windy.area.density * 180 < 0.1);
  // A midwinter dawn far colder than the open water: sea smoke, in the calm and in the wind.
  ge::wind::Climate winter = ge::mountain::air::climate_of(level);
  winter.wind = 8;
  const ma::WaterMist smoke = ma::water_mist(level, winter, 7.6, lake);
  CHECK(smoke.mist.kind == ge::wind::MistKind::steam && smoke.area.wind[1] > 0);
}
