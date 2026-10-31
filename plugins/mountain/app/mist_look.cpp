// The mountain's mists: its day of air, and the fog that air lays in the valley, round the eye and
// on the summit.
//
// ge:summary The mountain's mists: the formulas behind mist_look.hpp
// ge:keywords mist fog valley-fog ground-mist summit-cloud mountain_climate mists_at
// ge:skill mountain render
// ge:test tests/mountain_mist_look_test.cpp
#include "mountain/app/mist_look.hpp"

#include <algorithm>
#include <cmath>

#include "core/libm/libm.hpp"
#include "mountain/mapgen/sun.hpp"

namespace ge::mountain::app {

namespace {

namespace lm = core::libm;
namespace mg = mapgen;

constexpr double kPi = 3.14159265358979323846;
constexpr double kDeg = kPi / 180;

// The sky's kinds as the air's moisture (the relative humidity at the day's mean) and cover.
struct SkyAir {
  double humidity, cloud;
};

SkyAir sky_air(mg::WeatherKind kind) {
  switch (kind) {
    case mg::WeatherKind::clear:
      return {0.8, 0.05};
    case mg::WeatherKind::fair:
      return {0.8, 0.3};
    case mg::WeatherKind::flurries:
      return {0.85, 0.5};
    case mg::WeatherKind::high:
      return {0.75, 0.5};
    case mg::WeatherKind::overcast:
      return {0.88, 0.9};
    case mg::WeatherKind::snow:
      return {0.92, 0.95};
    case mg::WeatherKind::storm:
      return {0.92, 1.0};
    case mg::WeatherKind::fog:
      // The inversion's day: the air damp under a clear sky over the sea.
      return {0.97, 0.15};
  }
  return {0.8, 0.3};
}

// The way the map's wind blows TO, a unit (x, z).
void wind_toward(const mg::Weather& w, double out[2]) {
  out[0] = -lm::sin(w.wind_from);
  out[1] = -lm::cos(w.wind_from);
}

wind::Site open_snow(double y) {
  wind::Site s;
  s.y = y;
  s.above = 2;
  s.snow = 1;
  return s;
}

// A bank of `mist` lying on the ground round (x, z), `reach` m out.
render::FogVolume bank(const wind::Mist& mist, double x, double z, double reach,
                       const double drift[2]) {
  render::FogVolume v;
  v.shape = render::FogVolume::Shape::ellipsoid;
  v.centre[0] = x;
  v.centre[2] = z;
  v.half[0] = v.half[2] = reach;
  v.half[1] = std::max(mist.depth, 4.0) / 2;
  v.hug_ground = true;
  v.density = mist.density;
  v.edge = std::min(v.half[1], 15.0);
  v.phase = 0.5;
  v.noise = mist.noise;
  v.noise_size = 40;
  v.wind[0] = drift[0];
  v.wind[2] = drift[1];
  return v;
}

}  // namespace

wind::Climate mountain_climate(const mg::Level& level) {
  const mg::Mountain& m = level.mountain;
  const SkyAir air = sky_air(level.weather.kind);
  wind::Climate c;
  // A winter's day: half a degree colder at the sea for every degree from the equator, then the
  // lapse up to the valley's floor.
  c.ref_y = m.base.y;
  c.mean_c = 20 - 0.5 * std::fabs(level.sun.latitude) - c.lapse * std::max(m.altitude, 0.0);
  c.range_c = 10;
  c.humidity = air.humidity;
  c.cloud = air.cloud;
  c.wind = level.weather.kind == mg::WeatherKind::fog ? std::min(level.weather.wind, 1.5)
                                                      : level.weather.wind;
  // Sunrise and sunset off the sun's hour angle at the horizon.
  const double phi = level.sun.latitude * kDeg;
  const double decl = mg::declination_of(level.sun.day_of_year) * kDeg;
  const double cos_h = std::clamp(-lm::tan(phi) * lm::tan(decl), -1.0, 1.0);
  const double half_day = lm::acos(cos_h) / kDeg / 15;
  c.sunrise = 12 - half_day;
  c.sunset = 12 + half_day;
  return c;
}

MistLook mists_at(const mg::Level& level, double hour, const double eye[3]) {
  MistLook out;
  const wind::Climate c = mountain_climate(level);
  const mg::Mountain& m = level.mountain;
  double toward[2];
  wind_toward(level.weather, toward);
  // THE VALLEY'S FLOOR, in a hollow as deep as the mountain: the night's cold pool.
  wind::Site floor = open_snow(m.base.y);
  floor.hollow = std::min(m.vertical, 600.0);
  out.floor = wind::mist_at(c, floor, hour);
  if (out.floor.kind == wind::MistKind::valley || out.floor.kind == wind::MistKind::radiation) {
    out.valley.top_m = m.base.y + out.floor.depth;
    out.valley.density = out.floor.density;
    out.valley.drift[0] = kValleyDrift * c.wind * toward[0];
    out.valley.drift[1] = kValleyDrift * c.wind * toward[1];
  }
  const double drift[2] = {kMistDrift * c.wind * toward[0], kMistDrift * c.wind * toward[1]};
  // THE GROUND under the eye, on the open snow.
  out.ground = wind::mist_at(c, open_snow(level.ground_at(eye[0], eye[2])), hour);
  if (out.ground.kind != wind::MistKind::none)
    out.areas.push_back(bank(out.ground, eye[0], eye[2], kMistReach, drift));
  // THE SUMMIT, in its own cloud.
  out.summit = wind::mist_at(c, open_snow(m.summit.y), hour);
  if (out.summit.kind == wind::MistKind::hill)
    out.areas.push_back(bank(out.summit, m.summit.x, m.summit.z, kSummitReach, drift));
  return out;
}

}  // namespace ge::mountain::app
