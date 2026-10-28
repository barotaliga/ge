// SNOW OVER TIME (docs/snow.md § Time). A layer given a `Weather` changes with the clock, and
// almost all of it is read, not stepped: the weather is a few SCHEDULES (piecewise linear over the
// game's seconds: the fall, the air's temperature, the wind's mean at 10 m), and what they do to a
// column between two moments is worked out in closed form from their integrals.
//
// - NEW SNOW: the fall's integral between two moments, laid at `fresh_density`. The untouched
//   cover is read weathered from the layer's start (t = 0); a cell from the minute it last changed
//   (`Cell::minute`), so a groove pressed in a storm holds only what fell since.
// - SETTLING: the old snow's density rises toward `settled_density` (`wet_density` once it has
//   melted) as rho_s - (rho_s - rho) e^(-dt / tau), tau `settle_tau` at -5 C and shorter by
//   e^(-settle_warm (T + 5)) when warmer, T the air's mean over the interval; new snow settles over
//   half the interval (it fell through it). Mass is kept: the column sinks.
// - FILLING: a hollow (a cell under the cover read at the same moment) catches `fill` times its
//   share of the fall more, and whatever the wind drifts across it, until it is level again. A
//   20 cm ski groove in a 5 cm/h storm is level in about two hours.
// - DRIFT: snow lifted where the friction velocity passes `drift_threshold` and the air is below
//   freezing. The saltation flux, Q = 0.68 rho_air u*t (u*^2 - u*t^2) / (g u*) with u* off the
//   log law over the snow's roughness (engine/wind), is what crosses a metre of hollow and lands in
//   it, at `drift_density`.
// - MELT AND REFREEZE: the positive degree-seconds melt `melt_factor` kg/m2 a degree-day. Snow
//   that is melting is spring snow; once the air is below freezing again it is a crust (ice where a
//   press polished it). A night's crust bears what loads it under `crust_load`: crust_strength h^2,
//   h growing with the root of the degree-hours of frost since the thaw (Stefan's law).
// - A FALL OVER WHATEVER LAY THERE: at least `powder_over` m of new snow makes the column powder
//   again; powder that settles past the settled row's loose density is settled.
//
// `bring_to` writes a cell's weathered state into it (press and groom do that before they change
// one); the mass the weather added or took is counted into `Layer::mass_in`, so the mass the tiles
// hold plus what spilled is still what they were given. A layer without a weather is exactly the
// static layer of layer.hpp.
//
// ge:summary Snow over time: new snow, settling, tracks filling, drift, melt, crust, grooming
// ge:keywords snow time weather schedule fall snowfall settling densification filling drift
// ge:keywords melt refreeze crust corn spring ice groom groomer corduroy crust_load bring_to
// ge:keywords degree-day stefan saltation friction-velocity
// ge:skill snow
// ge:test tests/snow_time_test.cpp
#pragma once

#include <cstdint>
#include <vector>

#include "snow/layer.hpp"

namespace ge::snow {

// A value over the game's seconds: straight lines between points (`t` ascending), held flat
// before the first and after the last; no points reads 0.
struct Schedule {
  std::vector<double> t;  // s
  std::vector<double> v;
};

double value_at(const Schedule& s, double t);
// The integral of the value from `t0` to `t1` (value-seconds).
double integral(const Schedule& s, double t0, double t1);
// The integral of how far the value stands above `level` (0 where it is under).
double integral_above(const Schedule& s, double level, double t0, double t1);
// The integral of how far the value stands below `level`.
double integral_below(const Schedule& s, double level, double t0, double t1);
// The latest moment at or before `t` the value was above `level`; -1e300 for never.
double last_above(const Schedule& s, double level, double t);

struct Weather {
  Schedule fall;                  // kg/m2/s of new snow (its water equivalent: 1 mm/h is 1/3600)
  Schedule air;                   // C at the snow
  Schedule wind;                  // m/s, the wind's mean 10 m over the snow
  double fresh_density = 100;     // kg/m3 new snow lies at
  double settled_density = 300;   // kg/m3 dry snow settles toward
  double wet_density = 450;       // kg/m3 snow that has melted settles toward
  double settle_tau = 86400;      // s, the settling's time constant at -5 C
  double settle_warm = 0.08;      // 1/C: how much faster it settles a degree warmer
  double fill = 2;                // a hollow catches this many times its share of the fall more
  double melt_factor = 4;         // kg/m2 a degree-day above freezing
  double crust_per_root = 0.006;  // m of crust a root degree-hour of frost
  double crust_strength = 6e5;    // N/m2: the crust breaks under crust_strength h^2 N
  double drift_threshold = 0.2;   // m/s, the friction velocity that lifts dry snow
  double drift_density = 250;     // kg/m3 drifted snow is laid at
  double roughness = 0.003;       // m, the snow's z0 under the wind (wind::kRoughness::snow)
  double powder_over = 0.05;      // m of new snow that makes the column powder again
};

// What the weather does over an interval, the integrals worked out once.
struct Spell {
  double t0 = 0, t1 = 0;
  double fallen = 0;     // kg/m2 of new snow
  double thaw = 0;       // C s above freezing
  double mean_air = -5;  // C
  double air_end = -5;   // C at t1
  double drifted = 0;    // kg/m of snow carried across a metre of hollow
};

Spell spell(const Weather& w, double t0, double t1);

// A column in doubles while the weather works on it.
struct Snowpack {
  double depth = 0;  // m
  double mass = 0;   // kg/m2
  Kind kind = Kind::powder;
  bool pressed = false;  // under a contact's floor (polishes a refrozen crust to ice)
};

// What `spell` does to `pack`: the fall, settling, melt, the kind; no filling.
Snowpack weathered(const Weather& w, const Spell& sp, Snowpack pack);
// The hollow `drop` m under the cover (read at the spell's end) filled by the fall and the drift.
Snowpack filled(const Weather& w, const Spell& sp, Snowpack pack, double drop);

// The minute a moment falls in (what `Cell::minute` stores).
uint16_t minute_of(double t);

// A layer read at one moment, the spells worked out once: the cover's from the layer's start, and
// the last cell minute's (a press or a groom reads many cells changed in the same minute).
struct Moment {
  const Layer* layer = nullptr;
  double t = 0;
  Spell since_start;
  uint16_t minute = 0xffff;  // the minute `since_minute` starts at
  Spell since_minute;
};
Moment moment(const Layer& layer, double t);

// The untouched cover at sample (c, r) at the moment.
Snowpack cover_now(Moment& m, int c, int r);
// The snow at sample (c, r) at the moment: the cell weathered from its minute and filled, or the
// cover where no press has changed it.
Snowpack column_now(Moment& m, int c, int r);
// Writes `column_now` into the cell at (c, r), stamped with the moment's minute (the moment must
// fall on one: `moment(layer, minute_of(t) * 60.0)`); the mass it gained or lost is counted into
// the layer's `mass_in`. A layer with no weather leaves the cell as it is.
void bring_to(Layer& layer, Moment& m, Cell& cell, int c, int r);

// The load, N, a crust left by the last thaw and the frost since bears at `t` before it breaks;
// infinite where the weather has never thawed (a game's own crust never breaks).
double crust_load(const Weather& w, double t);

// THE COLUMN at a point at `t`: `sample` with the weather read in (the static `sample` where the
// layer has none).
Column sample(const Layer& layer, double x, double z, double t);

// A GROOMER'S SWATH: every sample under `swath` levelled to the same mass, laid at the groomed
// kind's packed density (denser snow keeps its density), the corduroy flag set. The mass is kept
// to the unit.
struct Groomed {
  double mass = 0;  // kg moved through the swath
  int samples = 0;
  int refused = 0;  // samples no tile was left for
};
Groomed groom(Layer& layer, const Shape& swath, double t);

}  // namespace ge::snow
