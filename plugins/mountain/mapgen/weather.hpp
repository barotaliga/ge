// R19 — THE WEATHER: which sky a map is skied under, how hard it snows, how thick the fog lies,
// how the wind blows, and whether the run is skied in the evening (R15's exception): the
// generator's half. Dealt LAST, off a stream of its
// own, so nothing built moves with it.
// Held by tests/mountain_generate_test.cpp (`world`, and the digest's weather words).
//
// ge:summary R19 the weather: the sky, the fall, the fog, the wind and the evening
// ge:keywords weather sky snows evening with_sky sky-pick weather_named
// ge:skill mountain
// ge:test tests/mountain_generate_test.cpp
#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

#include "mountain/mapgen/rules.hpp"
#include "mountain/mapgen/types.hpp"

namespace ge::mountain::mapgen {

// The weather words, by `WeatherKind` (the digest mixes them, so the spelling is fixed).
const char* weather_name(WeatherKind kind);

// Whether snow falls out of this sky.
bool snows(WeatherKind kind);

// The hour of sunset on a day at a latitude, solar hours (12 when the sun never sets or rises).
double sunset_of(double day_of_year, double latitude);

struct DealtWeather {
  Weather weather;
  double hour;  // the run's start hour: the dealt hour, or past sunset on an evening map
};

// R19 — deal the weather off the attempt's sub-seed `sub` for the day R15 dealt.
DealtWeather deal_weather(uint32_t sub, const Sun& sun);

// The kind a weather word names (`weather_name`'s spelling), or none.
std::optional<WeatherKind> weather_named(std::string_view name);

// A SKY PICKED BY HAND over a built map (a server's settings, a lab): the kind, and the fall, the
// fog, the mean wind (m/s) and the heading it blows from (rad), each kept where negative. A kind
// other than the map's brings its own fall and fog (the middle of its bands, none for a sky that
// does not snow or a clear one) and its wind (the middle of its band, as the deal would draw it),
// unless they are picked too. Nothing the map built moves: only `Level::weather` changes.
struct SkyPick {
  std::optional<WeatherKind> kind;
  double snowfall = -1;
  double fog = -1;
  double wind = -1;
  double wind_from = -1;
};
void with_sky(Level& level, const SkyPick& pick);

}  // namespace ge::mountain::mapgen
