// A TARN IN A CIRQUE: a small round lake a game lays on a generated map (mapgen/types.hpp's
// `MapLake`, `Level::lakes`), and the ice on it on the map's day. The generator itself lays no
// water, so no dealt map moves; Fall Line's free ride lays one (fall_line/sim/free.hpp's
// `free_map`).
//
// - THE SITE (`find_tarn`): the flattest open ground `radius` m round in the upper part of the
//   mountain, between `kTarnLow` and `kTarnHigh` of the way from its base to its summit (where a
//   glacier's cirque leaves its basin), clear of the runs and lifts (THE SHORE). None where the
//   flattest such site's ground spreads
//   over `kTarnRelief` of its radius. The water's surface is the rim's lowest ground, so on a
//   slope the ground above is cut back into a headwall, as a glacier left a cirque.
// - THE SHORE (`reach_of`, `shore_of`): no circle but `radius` swelled and pinched by two
//   lobes and three (`kTarnLobe2`, `kTarnLobe3` of it) turned by phases off the site, so each
//   tarn has its own shape; the ground it reshapes reaches twice the shore out, and that whole
//   reach must lie `kTarnClear` m clear of every run, ski route, piste and lift line.
// - THE ICE (`tarn_ice`): `air::lake_ice` at its surface over the winter so far (`winter_days`:
//   the days since the freeze-up, mid-November in the north and mid-May in the south, none in the
//   summer half), the map's day's mean taken for the whole winter.
// - THE BASIN (`lay_tarn`): the ground inside the shore dug into a bowl `kTarnDepth` m under the
//   rim's lowest point (its floor flat in the middle, shelving up over the outer 0.7 of the
//   radius), the ground outside raised to just over the water out to the corners of its box and
//   blended back by twice the radius, the slope above eased down to the shore over the same reach
//   (the headwall), the trees inside the shore felled. Ice that BEARS A SKIER
//   (`kSkierKg` by Gold's formula) is filled flat at the surface instead: to a skier a frozen tarn
//   is a level field of snow. Ice too thin to bear him lies over open water; through it, he sinks.
//
// ge:summary A tarn in a cirque laid on a map: its site, its basin, the ice on the map's day
// ge:keywords tarn lake cirque find_tarn lay_tarn tarn_ice winter_days lake-ice thin-ice bears
// ge:keywords map-lake basin dig freeze-up shore reach_of lobes
// ge:skill mountain
// ge:test tests/mountain_tarn_test.cpp
#pragma once

#include <optional>
#include <vector>

#include "mountain/mapgen/types.hpp"
#include "water/fetch.hpp"

namespace ge::mountain::tarn {

inline constexpr double kTarnLow = 0.45;    // of the base-to-summit height, the lowest site
inline constexpr double kTarnHigh = 0.85;   // the highest
inline constexpr double kTarnClear = 30;    // m from every run's and lift's line
inline constexpr double kTarnRelief = 0.8;  // of the radius: the most the site's ground may spread
inline constexpr double kTarnDepth = 6;     // m: the bowl's deepest water
inline constexpr double kSkierKg = 90;      // a skier and his gear
inline constexpr double kTarnWater = 4;     // C: the water under a tarn's ice, or just thawed
inline constexpr double kTarnLobe2 = 0.12;  // of the radius: the shore's two lobes
inline constexpr double kTarnLobe3 = 0.07;  // its three
inline constexpr double kTarnBulge = 1 + kTarnLobe2 + kTarnLobe3;  // the shore's farthest reach

// The site for a tarn `radius` m round, its surface the rim's lowest ground; none if there is no
// open, flat enough ground for it.
std::optional<mapgen::MapLake> find_tarn(const mapgen::Level& level, double radius);
// The days of winter so far on the map's day of the year: since the freeze-up, 0 in summer.
double winter_days(const mapgen::Level& level);
// The ice on `lake` on the map's day, m.
double tarn_ice(const mapgen::Level& level, const mapgen::MapLake& lake);
// Whether ice `ice` m thick bears `kg`.
bool bears(double ice, double kg = kSkierKg);
// Dig `lake` into the map (or fill it flat where its ice bears a skier), fell the trees in it and
// add it to `level.lakes`; its `depth` set to the bowl's deepest water.
void lay_tarn(mapgen::Level& level, mapgen::MapLake lake);
// THE BASIN alone: the ground dug `depth` m into a bowl under `lake`'s rim (its `level` set to the
// rim's lowest ground), or with `flat` filled level at it; the trees in it felled and the rest set
// on the new ground; `lake.depth` the deepest water dug.
void dig_basin(mapgen::Level& level, mapgen::MapLake& lake, double depth, bool flat);
// How far `lake`'s shore lies from its middle at `angle` (radians from +x toward +z), m.
double reach_of(const mapgen::MapLake& lake, double angle);
// The lake's shore, `points` round.
std::vector<water::Point> shore_of(const mapgen::MapLake& lake, int points = 64);
// Whether (x, z) is inside the shore.
bool on_lake(const mapgen::MapLake& lake, double x, double z);

}  // namespace ge::mountain::tarn
