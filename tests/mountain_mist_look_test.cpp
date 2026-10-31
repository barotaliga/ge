// plugins/mountain/app/mist_look.hpp: a clear, still winter night fills the valley with a level sea
// of fog that is gone by noon; a wind keeps it out; a fog sky lays it under its clear lid; the
// summit stands in its own cloud under a damp overcast; the day's sunrise and sunset come off the
// latitude and the day of the year; and the answer is the same every time it is asked.
//
// ge:summary Test: the mountain's mists by its sky and hour: the valley's sea, the summit's cloud
// ge:keywords test mist fog valley-fog sea-of-fog burn-off hill-fog mountain_climate mists_at
// ge:skill mountain testing
#include <cmath>

#include "check.hpp"
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
  const ge::wind::Climate c = ma::mountain_climate(level);
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
  const ge::wind::Climate winter = ma::mountain_climate(mountain(mg::WeatherKind::clear, 1));
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
