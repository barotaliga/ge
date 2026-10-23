// The mountain's skies as the renderer's weather: one row of numbers a kind, then the fall and
// the fog folded in (weather_look.hpp says what each number means).
//
// ge:summary The weather kinds' rows and the fall and fog folded into the renderer's Weather
// ge:keywords weather look overcast deck fog visibility sun-pass weather_look weather_at
// ge:skill mountain render
// ge:test tests/mountain_weather_look_test.cpp
#include "mountain/app/weather_look.hpp"

#include <algorithm>
#include <cmath>

namespace ge::mountain::app {

namespace {

using mapgen::WeatherKind;

// One kind's look: the dome's share the deck covers, the sun's pass, the deck's light, its grey
// overhead over the horizon's, its tint (luminance 1), and the visual range of its own damp air
// (0: the clear air's alone).
struct Kind {
  double cover, pass, light, zenith;
  double tint[3];
  double range_m;
};

Kind kind_of(WeatherKind k) {
  switch (k) {
    case WeatherKind::clear:
      return {0.0, 1.0, 1.0, 1.0, {1.0, 1.0, 1.0}, 0};
    case WeatherKind::fair:
      return {0.05, 0.8, 1.0, 0.85, {0.97, 1.0, 1.04}, 0};
    case WeatherKind::flurries:
      return {0.03, 0.8, 1.0, 0.8, {0.96, 1.0, 1.06}, 0};
    case WeatherKind::high:
      return {0.21, 0.6, 1.0, 0.85, {0.97, 1.0, 1.04}, 0};
    case WeatherKind::overcast:
      return {1.0, 0.12, 1.15, 0.62, {0.95, 1.0, 1.1}, 3260};
    case WeatherKind::snow:
      return {1.0, 0.08, 1.05, 0.54, {0.96, 1.0, 1.09}, 2350};
    case WeatherKind::storm:
      return {1.0, 0.04, 0.55, 0.27, {0.93, 1.0, 1.12}, 2350};
    case WeatherKind::fog:
      return {1.0, 0.3, 1.35, 1.0, {0.97, 1.0, 1.03}, 1174};
  }
  return kind_of(WeatherKind::clear);
}

// How far above the eye a deck's base hangs, m: the air between that the snow can hide it behind.
constexpr double kDeckOver = 600;
// A valley fog's visual range at its thinnest and the share its thickness closes it by: 1174 m
// at a fog of 0, 130 m at 1 (the damp air's extinction, 5 + 40 fog times a lid's).
constexpr double kFogThin = 1174, kFogThick = 8;

}  // namespace

render::Weather weather_look(const mapgen::Weather& weather, const air::Fall& fall) {
  const Kind k = kind_of(weather.kind);
  const double f = std::clamp(fall.fall, 0.0, 1.0);
  const double fog = weather.kind == WeatherKind::fog ? std::clamp(weather.fog, 0.0, 1.0) : 0.0;
  render::Weather w;
  w.cover = k.cover;
  w.sun_pass = k.pass * (1 - 0.6 * f) * (1 - 0.6 * fog);
  w.deck_light = k.light;
  w.deck_zenith = k.zenith;
  for (int i = 0; i < 3; ++i) w.deck_tint[i] = k.tint[i];
  double range = k.range_m;
  if (weather.kind == WeatherKind::fog) range = kFogThin / (1 + kFogThick * fog);
  if (f > 0) {
    range = range > 0 ? std::min(range, fall.visibility) : fall.visibility;
    w.hidden = 1 - std::exp(-kDeckOver * 3.912 / fall.visibility);
  }
  w.visibility_m = range;
  return w;
}

render::Weather weather_at(const mapgen::Level& level, double t) {
  return weather_look(level.weather, air::fall_at(level, t));
}

}  // namespace ge::mountain::app
