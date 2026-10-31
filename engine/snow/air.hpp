// THE SNOW'S AIR (docs/snow.md § The air): a layer's weather read off the world's air
// (engine/wind/climate.hpp) at the layer's place, and what that air makes of the snow for the
// contact and the sound (engine/wind/cold.hpp).
//
// - THE AIR (`air_schedule`, `air_c`): a run of days, each its own `wind::Climate` (a warm day
//   then a cold front is two of them), asked at one `wind::Site` (its height, the hollow its night
//   pools in, the water near it). The day's curve, the lapse to the site's height and the night's
//   cold pool are `air_at`'s; two days meet over the four hours round midnight. The schedule
//   samples it every `step` seconds, which is what `Weather::air` holds, so the layer's settling,
//   melt and crust follow from the same air the world's fog and frost read.
// - THE NEW SNOW'S DENSITY (`fresh_density_over`): `wind::fresh_density` of the air it fell
//   through, the mean weighted by the fall: a cold day's snow is lighter (about 70 kg/m3 at
//   -12 C) and so deeper for the same water, a mild day's heavier (about 100 at -1 C).
// - THE WEATHER (`weather_of`): the air, the wind (each day's mean) and a fall, with that density.
// - THE FEEL (`feel_at`): the snow's surface (`surface_at`: colder than the air on a clear night,
//   never above 0), the new snow of the last day, whether the snow thawed and froze again, read
//   into `wind::snow_feel` (powder, squeaky, soft, sticky, slush, crust), `glide_of` for the
//   contact's friction and `squeaks` for the sound (below -10 C at the surface).
//
// Pure functions of their arguments: nothing is stepped, no clock and no random number.
//
// ge:summary The snow's air: a layer's weather off the world's climate, new snow's density, the
// feel ge:keywords snow air weather climate air_at fresh_density fresh-snow-density snow-feel feel
// ge:keywords squeak glide thaw frost crust air_schedule weather_of feel_at snow_air
// ge:skill snow water
// ge:test tests/snow_air_test.cpp
#pragma once

#include <vector>

#include "snow/time.hpp"
#include "wind/climate.hpp"
#include "wind/cold.hpp"

namespace ge::snow {

// Where a layer's air comes from: day k's climate from its midnight (the last day held after), at
// one site, the layer's t = 0 falling at `start_hour` of day 0.
struct SnowAir {
  std::vector<wind::Climate> days;
  wind::Site site;
  double start_hour = 0;
};

// The hours over which two days' air meets, centred on midnight.
inline constexpr double kDayBlendHours = 4;

// The air, C, at the layer's place at its time `t`, s (0 with no days).
double air_c(const SnowAir& a, double t);
// The snow's surface, C, there (never above 0).
double snow_c(const SnowAir& a, double t);

// `air_c` from 0 to `seconds`, a point every `step` s: what `Weather::air` holds.
Schedule air_schedule(const SnowAir& a, double seconds, double step = 1800);
// Each day's mean wind, m/s, held through the day.
Schedule wind_schedule(const SnowAir& a, double seconds);

// The density, kg/m3, of the snow that fell between `t0` and `t1`: `wind::fresh_density` of the
// air it fell through, weighted by the fall (the air's mean when nothing fell).
double fresh_density_over(const Schedule& fall, const Schedule& air, double t0, double t1);

// A layer's weather: `fall` with the air and wind off `a` over `seconds`, the new snow laid at
// the density of the air it fell through.
Weather weather_of(const SnowAir& a, const Schedule& fall, double seconds);

// What the snow is like underfoot at `t`.
struct SnowFeel {
  wind::Feel kind = wind::Feel::soft;
  double air_c = 0;   // C
  double snow_c = 0;  // C at the surface
  double fresh = 0;   // m of snow fallen over the last day
  double wet = 0;     // the liquid share, 0 dry .. 1 slush
  double glide = 0;   // a ski's kinetic friction coefficient
  bool thawed = false;
  bool squeak = false;
};
SnowFeel feel_at(const SnowAir& a, const Weather& w, double t);

}  // namespace ge::snow
