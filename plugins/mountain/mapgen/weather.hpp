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
// fog, the mean wind (m/s), the heading it blows from (rad) and the snow line, each kept where
// negative. A kind
// other than the map's brings its own fall and fog (the middle of its bands, none for a sky that
// does not snow or a clear one) and its wind (the middle of its band, as the deal would draw it),
// unless they are picked too. Nothing the map built moves: only `Level::weather` changes.
struct SkyPick {
  std::optional<WeatherKind> kind;
  double snowfall = -1;
  double fog = -1;
  double wind = -1;
  double wind_from = -1;
  // The snow line as a share of the mountain's height, from its base (0) to its summit (1); over
  // 1 rain falls on the summit too. Kept where negative.
  double snowline = -1;
  // A thunderstorm's strength, 0 none .. 1 severe (`Weather::thunder`), kept where negative. A
  // storm is a summer sky's: with no snow line picked it brings the freezing level of a summer
  // afternoon over the summit (`kSummerFreezing`), so its rain is rain to the top.
  double thunder = -1;
  // The day's mean air at the mountain's base, C (`Weather::mean_c`); kept when empty.
  std::optional<double> mean_c = std::nullopt;
};
inline constexpr double kSummerFreezing = 600;  // m over the summit
void with_sky(Level& level, const SkyPick& pick);

}  // namespace ge::mountain::mapgen
