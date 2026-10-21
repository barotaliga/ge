// R19 — THE WEATHER: which sky a map is skied under, how hard it snows, how thick the fog lies,
// how the wind blows, and whether the run is skied in the evening (R15's exception): the
// generator's half. Dealt LAST, off a stream of its
// own, so nothing built moves with it.
// Held by tests/mountain_generate_test.cpp (`world`, and the digest's weather words).
//
// ge:summary R19 the weather: the sky, the fall, the fog, the wind and the evening
// ge:keywords weather sky snows evening
// ge:skill mountain
// ge:test tests/mountain_generate_test.cpp
#pragma once

#include <cstdint>

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

}  // namespace ge::mountain::mapgen
