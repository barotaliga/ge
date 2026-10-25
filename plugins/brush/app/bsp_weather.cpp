// The weather over a BSP map: its cvars, the wet and the snow stepped, the fall's frame.
//
// ge:summary The weather over a BSP map: cvars bound, stepped by the clock, applied to a frame
// ge:keywords weather bsp cvars step_weather apply_weather wet cover lightning gusts
// ge:skill brush render
// ge:test tests/brush_weather_test.cpp
#include "brush/app/bsp_weather.hpp"

#include <algorithm>
#include <cmath>
#include <string>

#include "core/math.hpp"
#include "core/num_text.hpp"
#include "wind/field.hpp"

namespace ge::brush::app {

namespace con = ge::console;

void add_weather_cvars(con::Cvars& cvars, WeatherCvars& out) {
  struct Number {
    std::string_view name;
    double fallback, min, max;
    uint32_t flags;
    std::string_view help;
    double* field;
  };
  const uint32_t server = con::kServer | con::kServerInfo;
  const uint32_t player = con::kArchive;
  const Number numbers[] = {
      {"sv_precip", out.precip, 0, 1, server,
       "how hard it falls, 0 .. 1 (light, moderate, heavy, a storm past 0.85)", &out.precip},
      {"sv_rain", out.rain, 0, 1, server, "the share that is rain: 0 dry snow, 0.5 sleet, 1 rain",
       &out.rain},
      {"sv_wind", out.wind, 0, 60, server, "the mean wind, m/s", &out.wind},
      {"sv_winddir", out.wind_from, 0, 360, server,
       "the bearing the wind blows from, deg (0 the map's +z, clockwise from above)",
       &out.wind_from},
      {"sv_gust", out.gust, 0, 1, server, "the gusts' strength, the speed's deviation over its mean",
       &out.gust},
      {"r_precip_density", out.density, 0.25, 2, player,
       "the particles a full fall draws, a share of the default", &out.density},
  };
  for (const Number& n : numbers) {
    const std::string fallback = core::num::to_string(n.fallback);
    cvars.bind({.name = n.name,
                .fallback = fallback,
                .flags = n.flags,
                .help = n.help,
                .kind = con::CvarKind::number,
                .min = n.min,
                .max = n.max},
               n.field);
  }
  struct Switch {
    std::string_view name;
    bool fallback;
    uint32_t flags;
    std::string_view help;
    bool* field;
  };
  const Switch switches[] = {
      {"sv_weather", out.on, server, "the weather on the map: rain, snow, wind and storms", &out.on},
      {"r_precip", out.draw, player, "the falling rain and snow drawn", &out.draw},
      {"r_splashes", out.splashes, player, "the drops' splashes where the rain lands",
       &out.splashes},
      {"r_wet", out.wet, player, "the wet faces and the puddles where the rain reaches", &out.wet},
      {"r_snowcover", out.snowcover, player, "the snow settling on what faces up and is open",
       &out.snowcover},
      {"r_lightning", out.lightning, player, "a storm's lightning flashes", &out.lightning},
  };
  for (const Switch& s : switches) {
    cvars.bind({.name = s.name,
                .fallback = s.fallback ? "1" : "0",
                .flags = s.flags,
                .help = s.help,
                .kind = con::CvarKind::boolean},
               s.field);
  }
}

namespace {

constexpr double kDeg = core::kPi / 180;
constexpr uint32_t kGustSeed = 0x77e47e5u;

// The air at `t`: the mean from the bearing, the gusts and the veer laid by the strength.
wind::Air air_at(const WeatherCvars& c, double t) {
  wind::Field f;
  f.mean = c.wind;
  f.from = c.wind_from * kDeg;
  wind::lay_gusts(f, kGustSeed, c.gust);
  return wind::open_at(f, t);
}

}  // namespace

WeatherFrame step_weather(const WeatherCvars& c, WeatherState& s, const double eye[3], double t,
                          double sun) {
  WeatherFrame w;
  w.on = c.on;
  if (!c.on) {
    s.placed = false;
    return w;
  }
  w.fall = {std::clamp(c.precip, 0.0, 1.0), std::clamp(c.rain, 0.0, 1.0)};
  const wind::Air air = air_at(c, t);
  w.air[0] = air.x;
  w.air[1] = air.y;
  w.air[2] = air.z;
  w.gust = air.gust;
  w.tier = wind::tier_of(w.fall, air.speed);
  w.rate = wind::rate_of(w.fall);
  w.visibility = wind::visibility_of(w.fall);
  const double pace = wind::fall_speed(w.fall);
  const double v[3] = {air.x, -pace, air.z};
  const double l = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
  for (int k = 0; k < 3; ++k) w.way[k] = v[k] / l;
  if (c.lightning) w.flash = wind::flash_at(t, w.fall, air.speed);

  // The wet ground and the snow's cover over the whole map.
  const double rain_rate = w.rate * w.fall.rain;
  const double dry = wind::dry_rate(air.speed, sun);
  const double since = t - s.t;
  if (!s.placed || since < 0 || since > 600) {
    s.wet = wind::wet_steady(rain_rate, dry);
    s.cover = wind::step_cover({}, w.fall, sun, 3600);
    s.placed = true;
  } else if (since > 0) {
    s.wet = wind::step_wet(s.wet, rain_rate, dry, since);
    s.cover = wind::step_cover(s.cover, w.fall, sun, since);
  }
  s.t = t;
  w.wet = s.wet;
  w.cover = s.cover;

  // The fall's frame: the dial past the light end, the drops' pace, the eddies by the wind.
  render::PrecipFrame& d = w.draw;
  for (int k = 0; k < 3; ++k) {
    d.eye[k] = eye[k];
    d.wind[k] = w.air[k];
  }
  d.fall = c.draw && w.fall.intensity >= wind::kNoFall ? std::sqrt(w.fall.intensity) : 0;
  d.rain = w.fall.rain;
  d.drop_speed = wind::fall_speed({w.fall.intensity, 1});
  d.swirl = wind::swirl_of(air.speed, wind::turbulence_for(wind::kRoughness::town));
  d.splash = c.splashes ? w.fall.rain * std::sqrt(w.fall.intensity) : 0;
  d.time = t;
  return w;
}

void apply_weather(const WeatherFrame& w, const WeatherCvars& c, render::FrameView& view,
                   Options& options, bool fog) {
  options.weather = w.on;
  options.raining = w.on ? std::sqrt(w.fall.intensity) * w.fall.rain : 0;
  for (int k = 0; k < 3; ++k) options.fall[k] = w.way[k];
  render::Weather& v = view.weather;
  v.wet = w.on && c.wet ? w.wet.wet : 0;
  v.puddles = w.on && c.wet ? w.wet.puddles : 0;
  v.snow = w.on && c.snowcover ? w.cover.cover : 0;
  v.flash = w.on ? w.flash : 0;
  if (w.on && fog && w.fall.intensity >= wind::kNoFall) {
    v.visibility_m = v.visibility_m > 0 ? std::min(v.visibility_m, w.visibility) : w.visibility;
    // A storm's deck: the sun mostly hidden, the dome grey.
    const double heavy = std::clamp((w.fall.intensity - 0.2) / 0.6, 0.0, 1.0);
    v.cover = std::max(v.cover, 0.5 + 0.5 * heavy);
    v.sun_pass = std::min(v.sun_pass, 1 - 0.85 * heavy);
  }
}

uint32_t weather_particles(const render::PrecipSettings& base, double density) {
  return static_cast<uint32_t>(std::lround(base.particles * std::clamp(density, 0.25, 2.0)));
}

}  // namespace ge::brush::app
