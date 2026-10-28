// THUNDERSTORMS AS PURE FUNCTIONS OF PLACE AND TIME (docs/storms.md). A `Storm` is data laid once
// (its cells, where and when each rose, how strong, the wind they drift on); what a cell is doing,
// the rain it drops and the outflow it blows at any point and moment are computed from it and
// never stepped, so a replay, a ghost and a server read the same storm with nothing to send.
//
// - A CELL'S LIFE (`cell_at`): towering (the cumulus stage: an updraft and no rain, 15 min),
//   mature (the updraft and the downdraft side by side, the heaviest rain, the most lightning, the
//   gust front out on the ground; 15 to 30 min by its strength) and dissipating (the downdraft
//   alone, the rain thinning to the anvil's light fall, 30 min). Its `peak` sets the numbers: an
//   ordinary cell (0.5) rises to about 10.5 km on a 24 m/s updraft and flashes about 6 times a
//   minute; a severe one (1) to 14 km on 40 m/s and about 40 times a minute.
// - THE LIGHTNING'S PACE: no flash until the tower passes the glaciation level (about 6.5 km over
//   the ground, where graupel and ice crystals collide and charge), the rate rising with the
//   updraft and lagging it on the way down; the first flashes nearly all in the cloud, then about
//   three in four (more in a strong cell), and fewer in the cloud as the cell dies.
// - THE GUST FRONT (`outflow_at`): the downdraft's cold air touches down as the cell matures and
//   spreads over the ground as a ring, its edge running out at 4 to 12 m/s from the cell, the air
//   behind it at 8 to 30 m/s, strongest just behind the edge (the head), shallow (under 1.5 km),
//   lifting the warm air it undercuts (where a shelf cloud forms and the next cell rises), and
//   carried on the cell's drift so its forward side blows hardest.
// - THE RAIN (`rain_at`): each cell's fall on the precipitation dial (`wind/precip`'s
//   intensity, 0 .. 1) at its core, falling off with the distance from it, the cells combined as
//   independent chances (1 - product of 1 - each).
// - A MULTICELL STORM (`lay_storm`): a new cell every 10 to 15 minutes on the forward-right flank
//   of the last, where its gust front lifts the inflow, for as long as the storm is asked to
//   regenerate; a single cell when it is not.
//
// Heights are metres over the ground under the storm (`Storm::ground` is its world y). Nothing
// here draws from a run's generator: every draw is a hash of a cell's seed.
//
// ge:summary Thunderstorm cells' life cycle, gust fronts and rain as pure functions of place, time
// ge:keywords storm thunderstorm cumulonimbus cell multicell life-cycle towering mature
// ge:keywords dissipating updraft downdraft glaciation anvil gust-front outflow cold-pool
// ge:keywords shelf-cloud squall rain-core cell_at outflow_at rain_at lay_storm storm_hash
// ge:skill water
// ge:test tests/wind_storm_test.cpp
#pragma once

#include <cstdint>

#include "wind/field.hpp"

namespace ge::wind {

inline constexpr int kMaxCells = 16;

inline constexpr double kTowering = 900;      // s, the cumulus stage
inline constexpr double kDissipating = 1800;  // s
inline constexpr double kGlaciation = 6500;   // m over the ground, where the charging starts

enum class Stage : uint8_t { none, towering, mature, dissipating };

// The stages' words ("none", "towering", "mature", "dissipating").
const char* stage_name(Stage stage);

struct Cell {
  double x = 0, z = 0;  // where its updraft stands at `born`, m
  double born = 0;      // s
  double peak = 0.5;    // its strength: 0.3 a weak cell, 0.5 an ordinary one, 1 a severe one
  double core = 2500;   // the rain core's radius, m
  uint32_t seed = 0;    // what its lightning is hashed off
};

struct Storm {
  double vx = 0, vz = 0;  // the steering wind every cell drifts on, m/s
  double base = 1200;     // the cloud base over the ground, m
  double ground = 0;      // the world y of the ground under the storm, m
  int cells = 0;
  Cell cell[kMaxCells]{};
};

// What one cell is doing at a moment.
struct CellAt {
  Stage stage = Stage::none;
  double age = 0;        // s since it rose
  double x = 0, z = 0;   // its centre, drifted, m
  double updraft = 0;    // m/s at its core
  double downdraft = 0;  // m/s at its core
  double top = 0;        // the cloud top over the ground, m (the anvil's, once spread)
  double rain = 0;       // the fall at its core on the precipitation dial, 0 .. 1
  double flashes = 0;    // lightning flashes a minute
  double in_cloud = 0;   // the share of them that stay in the cloud
  double front = 0;      // the gust front's distance from the centre, m (0: not down yet)
  double outflow = 0;    // the outflow's speed just behind the front, m/s
};

// How long a cell of strength `peak` stays mature, s.
double mature_of(double peak);
// The whole life of a cell of strength `peak`, s.
double life_of(double peak);

// Cell `k` of `storm` at `t`.
CellAt cell_at(const Storm& storm, int k, double t);

// THE OUTFLOW at a point: every live cell's gust front, as air to add to the wind (field.hpp's
// `wind_at`) over ground of roughness `roughness`. `gust` is how near the strongest front's head
// the point stands, 0 (no front) .. 1 (on it).
Air outflow_at(const Storm& storm, const Where& where, double t,
               double roughness = kRoughness::grass);

// THE RAIN at a point on the precipitation dial, 0 .. 1.
double rain_at(const Storm& storm, double x, double z, double t);

// A multicell storm asked for.
struct StormSpec {
  uint32_t seed = 1;
  double x = 0, z = 0;    // where the first cell rises, m
  double start = 0;       // when, s
  double renew = 0;       // how long new cells keep rising, s (0: one cell)
  double severity = 0.5;  // the cells' mean strength, 0 .. 1
  double vx = 0, vz = 0;  // the steering wind, m/s
  double base = 1200;     // m
  double ground = 0;      // m
};
Storm lay_storm(const StormSpec& spec);

// A uniform number in [0, 1) hashed off a seed, a salt and an index.
double storm_hash(uint32_t seed, uint32_t salt, uint32_t index);

}  // namespace ge::wind
