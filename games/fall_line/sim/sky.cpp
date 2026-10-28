// The definitions behind sky.hpp.
//
// ge:summary The server's sky over a run and the sky's hour
// ge:keywords sky server_sky sky_hour
// ge:skill sim
// ge:test tests/fall_line_sky_test.cpp
#include "fall_line/sim/sky.hpp"

#include "core/math.hpp"
#include "core/num.hpp"
#include "mountain/mapgen/weather.hpp"

namespace fall_line::sim {

namespace mg = ge::mountain::mapgen;

std::string server_sky(const cfg::Settings& s, mg::Level& level) {
  mg::SkyPick pick;
  std::string problem;
  if (s.sky != "map") {
    pick.kind = mg::weather_named(s.sky);
    if (!pick.kind) {
      problem = "sv_weather '" + s.sky +
                "' names no sky; the map's own is kept. next: set sv_weather to clear, fair, "
                "flurries, high, overcast, snow, storm, fog or map.";
    }
  }
  pick.snowfall = s.fall;
  pick.snowline = s.snowline;
  pick.fog = s.fog;
  pick.wind = s.air;
  if (s.air_from >= 0) pick.wind_from = s.air_from * ge::core::kPi / 180;
  mg::with_sky(level, pick);
  return problem;
}

double sky_hour(const cfg::Settings& s, const mg::Level& level, double t) {
  const double start = s.hour < 0 ? level.sun.hour : s.hour;
  const double h = ge::core::num::mod(start + t * s.sky_rate / 3600, 24.0);
  return h < 0 ? h + 24 : h;
}

}  // namespace fall_line::sim
