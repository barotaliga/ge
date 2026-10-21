// R14 — THE FOREST: snow-loaded trees where a slow noise says woods, glades between, clearings
// cut out of the woods, nothing above the tree line, and nothing in the run's way. A ski area
// hands its own options: its runs, lifts and hub kept clear, and its woods thinning with height.
//
// CLUMPS are grown first (a few trunks round a centre, off hashes of the forest's seed), then
// the SCAN stands one candidate a 6 m cell, jittered inside it, kept with the probability the
// woods noise gives its spot and refused by rule: above the tree line, too steep, on the
// piste's corridor, on a kicker or a cliff, on a winding lane, or within `forest.gap` of a
// tree already standing — which is what makes a wood SKIABLE. Every cell takes four draws off
// the stream whatever happens to it, so a refusal never shifts the trees after it. Ages and
// kinds come off hashes of where a trunk stands, never the stream.
// Held by tests/mountain_generate_test.cpp (`trees`).
//
// ge:summary R14 the forest: clumps, the scan, lanes, gaps, ages and trunks
// ge:keywords forest trees woods clump krummholz
// ge:skill mountain
// ge:test tests/mountain_generate_test.cpp
#pragma once

#include <functional>
#include <span>
#include <vector>

#include "core/heightfield.hpp"
#include "core/prng.hpp"
#include "mountain/mapgen/terrain.hpp"
#include "mountain/mapgen/types.hpp"

namespace ge::mountain::mapgen {

// How much of a spot's woods stand (`keep`) and how far toward the thick of a wood its glades
// close (`close`), both 0..1.
struct WoodsCover {
  double keep;
  double close;
};

// R14 — how old a tree is, years: the age the growth curve gives the height it `grew` to,
// spread by a log-normal off hashes of where it stands, now and then a veteran's.
double tree_age(double grew, double x, double z, double seed);

// R14 — the trunk's radius at breast height for a tree of `age` years, m.
double trunk_radius(double age);

// How a ski area grows its woods (R14 on a resort): what keeps a trunk off a spot, and the woods
// by height (`resort_woods.hpp`).
struct ForestOptions {
  // Whether no trunk may stand here (a run's corridor, a lift's line, the hub).
  std::function<bool(double x, double z)> clear;
  // How much of a spot's woods stand at height `y` (`keep`), and how much nearer the thick of a
  // wood the spot reads (`close`), both 0..1.
  std::function<WoodsCover(double y)> cover;
  // The tallest a tree at `y` grows, as a share of the height band.
  std::function<double(double y)> tall;
};

// R14 — grow the forest. `line_y` is the tree line as a HEIGHT on this map, m. With `resort`, the
// piste and the lanes through the woods are not read: the area's own `clear` keeps the trunks
// off, and its woods thin with height.
std::vector<TreeDef> grow_forest(ge::core::Rng& rng, const TerrainPlan& plan,
                                 const ge::core::Heightfield& ground, const Track& piste,
                                 std::span<const Kicker> kickers, std::span<const Cliff> cliffs,
                                 double line_y, const ForestOptions* resort = nullptr);

}  // namespace ge::mountain::mapgen
