// Port of game5 `engine/mapgen/weather.ts` (`dealWeather`, `windIn`, `sunsetOf`).
//
// ge:summary The weather's own stream and its draws, in the oracle's order
// ge:keywords weather dealWeather windIn
// ge:oracle game5/engine/mapgen/weather.ts
// ge:skill mountain
// ge:test tests/mountain_generate_test.cpp
#include "mountain/mapgen/weather.hpp"

#include <array>

#include "core/js.hpp"
#include "core/math.hpp"
#include "core/prng.hpp"
#include "core/solar.hpp"
#include "mountain/mapgen/sun.hpp"

namespace ge::mountain::mapgen {

namespace js = ge::core::js;
namespace W = rules::weather;

namespace {

// What sets the weather's stream apart from the attempt's own.
constexpr uint32_t kWeatherSalt = 0x3ea7e418;

Band snowfall_band(WeatherKind kind) {
  if (kind == WeatherKind::flurries) return W::snowfall_flurries;
  if (kind == WeatherKind::snow) return W::snowfall_snow;
  return W::snowfall_storm;
}

// The mean wind of a sky at a draw `u` (0..1) of its band, heavier with the fall.
double wind_in(WeatherKind kind, double u, double snowfall) {
  const Band band = W::wind[size_t(kind)];
  double at = u;
  if (kind == WeatherKind::snow) {
    at = 0.3 * u + 0.7 * snowfall;
  } else if (kind == WeatherKind::storm) {
    const Band heavy = W::snowfall_storm;
    at = 0.4 * u + (0.6 * (snowfall - heavy.min)) / (heavy.max - heavy.min);
  }
  return band.min + (band.max - band.min) * js::min(1.0, js::max(0.0, at));
}

}  // namespace

const char* weather_name(WeatherKind kind) {
  static constexpr std::array<const char*, kWeatherKinds> kNames{
      "clear", "fair", "flurries", "high", "overcast", "snow", "storm", "fog"};
  return kNames[size_t(kind)];
}

bool snows(WeatherKind kind) {
  return kind == WeatherKind::flurries || kind == WeatherKind::snow || kind == WeatherKind::storm;
}

double sunset_of(double day_of_year, double latitude) {
  const auto w = ge::core::daylight_window(latitude, 0, declination_of(day_of_year));
  return w ? w->max : 12;
}

DealtWeather deal_weather(uint32_t sub, const Sun& sun) {
  ge::core::Rng rng = ge::core::make_rng(sub ^ kWeatherSalt);
  double roll = ge::core::next(rng);
  WeatherKind kind = WeatherKind::clear;
  for (int k = 0; k < kWeatherKinds; k++) {
    const double odds = W::odds[size_t(k)];
    if (odds <= 0) continue;
    roll -= odds;
    if (roll < 0) {
      kind = WeatherKind(k);
      break;
    }
  }
  const double snowfall = snows(kind) ? in_band(rng, snowfall_band(kind)) : 0;
  const double fog = kind == WeatherKind::fog ? in_band(rng, W::fog) : 0;
  const double wind = wind_in(kind, ge::core::next(rng), snowfall);
  const double wind_from = ge::core::range(rng, 0, ge::core::kPi * 2);
  const bool evening = ge::core::chance(rng, W::evening);
  const double late = in_band(rng, rules::sun::evening);
  const double hour = evening ? sunset_of(sun.day_of_year, sun.latitude) + late : sun.hour;
  return {{kind, snowfall, fog, wind, wind_from, evening}, hour};
}

}  // namespace ge::mountain::mapgen
