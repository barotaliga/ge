// R14 — THE FOREST: snow-loaded trees where a slow noise says woods, glades between, clearings
// cut out of the woods, nothing above the tree line, and nothing in the run's way. The single
// piste's woods (a ski area's, and their options, arrive with M4b).
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

#include <span>
#include <vector>

#include "core/heightfield.hpp"
#include "core/prng.hpp"
#include "mountain/mapgen/terrain.hpp"
#include "mountain/mapgen/types.hpp"

namespace ge::mountain::mapgen {

// R14 — how old a tree is, years: the age the growth curve gives the height it `grew` to,
// spread by a log-normal off hashes of where it stands, now and then a veteran's.
double tree_age(double grew, double x, double z, double seed);

// R14 — the trunk's radius at breast height for a tree of `age` years, m.
double trunk_radius(double age);

// R14 — grow the forest. `line_y` is the tree line as a HEIGHT on this map, m.
std::vector<TreeDef> grow_forest(ge::core::Rng& rng, const TerrainPlan& plan,
                                 const ge::core::Heightfield& ground, const Track& piste,
                                 std::span<const Kicker> kickers, std::span<const Cliff> cliffs,
                                 double line_y);

}  // namespace ge::mountain::mapgen
