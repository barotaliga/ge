// The sky over a run as the server set it: a picked weather brings its own fall, fog and wind and
// keeps everything the map built, each picked number wins, an unknown word keeps the map's sky,
// and the sky's hour runs at sv_skyrate and wraps.
//
// ge:summary Tests the server's sky over a run: with_sky, server_sky and the sky's hour
// ge:keywords test sky weather with_sky server_sky sky_hour sv_weather sv_skyrate
// ge:skill sim testing
#include <cmath>

#include "check.hpp"
#include "console/console.hpp"
#include "fall_line/cfg/cvars.hpp"
#include "fall_line/sim/sky.hpp"
#include "mountain/mapgen/rules.hpp"
#include "mountain/mapgen/weather.hpp"

namespace mg = ge::mountain::mapgen;
using fall_line::cfg::Settings;

namespace {

mg::Level fair_noon() {
  mg::Level l{};
  l.weather = {mg::WeatherKind::fair, 0, 0, 3, 1.0, false};
  l.sun.hour = 12;
  l.sun.day_of_year = 60;
  l.sun.latitude = 46;
  return l;
}

bool in(double x, mg::Band b) { return x >= b.min && x <= b.max; }

}  // namespace

TEST(a_picked_sky_brings_its_own_fall_fog_and_wind) {
  mg::Level l = fair_noon();
  mg::with_sky(l, {.kind = mg::WeatherKind::storm});
  CHECK(l.weather.kind == mg::WeatherKind::storm);
  CHECK(in(l.weather.snowfall, mg::rules::weather::snowfall_storm) && l.weather.fog == 0);
  CHECK(in(l.weather.wind, mg::rules::weather::wind[size_t(mg::WeatherKind::storm)]));
  CHECK(l.weather.wind_from == 1.0 && l.sun.hour == 12);
  mg::with_sky(l, {.kind = mg::WeatherKind::fog});
  CHECK(l.weather.snowfall == 0 && in(l.weather.fog, mg::rules::weather::fog));
  // The same kind as the map's keeps the map's numbers.
  mg::Level same = fair_noon();
  mg::with_sky(same, {.kind = mg::WeatherKind::fair});
  CHECK(same.weather.wind == 3);
}

TEST(each_picked_number_wins) {
  mg::Level l = fair_noon();
  mg::with_sky(
      l, {.kind = mg::WeatherKind::snow, .snowfall = 0.2, .fog = 0.5, .wind = 9, .wind_from = 2});
  CHECK(l.weather.snowfall == 0.2 && l.weather.fog == 0.5);
  CHECK(l.weather.wind == 9 && l.weather.wind_from == 2);
  CHECK(mg::weather_named("storm") == mg::WeatherKind::storm && !mg::weather_named("sunny"));
}

TEST(the_server_cvars_reach_the_map) {
  ge::console::Console c;
  Settings s;
  fall_line::cfg::add_cvars(c, s);
  c.execute(
      "sv_weather snow; sv_wind 12; sv_winddir 90; sv_cover 0.6; sv_thick 0.25; sv_skyrate 60; "
      "sv_cloudheight 300; sv_cloudspeed 1");
  CHECK(c.problems().empty());
  mg::Level l = fair_noon();
  CHECK(fall_line::sim::server_sky(s, l).empty());
  CHECK(l.weather.kind == mg::WeatherKind::snow && l.weather.wind == 12);
  CHECK(std::fabs(l.weather.wind_from - 1.5707963267948966) < 1e-12);
  CHECK(s.cover == 0.6 && s.thick == 0.25 && s.sky_rate == 60);
  CHECK(s.cloud_height == 300 && s.cloud_speed == 1);
  // An unknown word keeps the map's sky and says so; the rest still applies.
  c.execute("sv_weather sunny");
  mg::Level k = fair_noon();
  const std::string problem = fall_line::sim::server_sky(s, k);
  CHECK(problem.find("sv_weather 'sunny'") != std::string::npos);
  CHECK(k.weather.kind == mg::WeatherKind::fair && k.weather.wind == 12);
}

TEST(the_sky_hour_runs_and_wraps) {
  Settings s;
  const mg::Level l = fair_noon();
  CHECK(fall_line::sim::sky_hour(s, l, 600) == 12);  // sv_skyrate 0: the hour stands
  s.sky_rate = 60;                                   // an hour a minute
  CHECK(std::fabs(fall_line::sim::sky_hour(s, l, 90) - 13.5) < 1e-12);
  s.hour = 23;
  CHECK(std::fabs(fall_line::sim::sky_hour(s, l, 120) - 1) < 1e-12);
}

TEST(a_client_cannot_pick_the_sky) {
  ge::console::Console c(false);
  Settings s;
  fall_line::cfg::add_cvars(c, s);
  c.execute("sv_weather storm");
  CHECK(s.sky == "map" && c.problems().size() == 1);
}
