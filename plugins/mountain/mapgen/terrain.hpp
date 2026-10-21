// R2, R3 — THE MOUNTAIN BEFORE ANYBODY SKIS IT: one flank falling from a summit ridge along
// the top edge to a valley floor along the bottom, side ridges either hand, and a face folded
// into spurs and gullies, bowls, headwalls and rollers. Port of game5
// `engine/mapgen/terrain.ts` (the single piste's half: `planTerrain`, `bakeCountry`,
// `flankAt`, `descentAt`, the noise stack). The ski area's massif, a real face and the sea
// arrive with M4b–M4d.
//
// Everything is a pure function of a PLAN (numbers drawn off the attempt's stream once) and a
// plan point, so the search asks "how high is the untouched ground here?" of the arithmetic
// the bake writes into the grid. The bake runs once an attempt; the piste's grading and the
// kickers are then stamped into the grid it made, and the noise is never evaluated again.
//
// THE PROFILE is the mountain's shape down the fall line, integrated once into a table and
// scaled to fall the dealt vertical; the headwalls are steps taken out of that vertical and
// put back as short bands. The ground falls toward +z: heading 0 is the fall line.
// Parity: through tests/mountain_generate_test.cpp (`ground`).
//
// ge:summary R2, R3 the mountain: planTerrain and the baked country
// ge:keywords terrain mountain heightfield planTerrain bakeCountry descentAt flankAt profile
// ge:oracle game5/engine/mapgen/terrain.ts
// ge:skill mountain
// ge:test tests/mountain_generate_test.cpp
#pragma once

#include <array>
#include <vector>

#include "core/heightfield.hpp"
#include "core/noise.hpp"
#include "core/prng.hpp"
#include "mountain/mapgen/grades.hpp"
#include "mountain/mapgen/regions.hpp"

namespace ge::mountain::mapgen {

// A bowl: a round hollow in the high face (R3).
struct Bowl {
  double x, z, r, depth;
};

// A headwall: a band across the face where the profile drops `drop` m more over `run` m,
// centred `z` down the map (R3).
struct Headwall {
  double z, drop, run;
};

// How finely the profile is tabulated over the descent.
inline constexpr int kProfileSamples = 1024;

// Everything the mountain is drawn from, dealt once an attempt.
struct TerrainPlan {
  double vertical;   // valley floor to summit ridge, m (R2)
  double altitude;   // the valley floor over the sea, m
  double tree_line;  // m over the sea
  double size;       // R1, the map's side, m
  double summit_z;   // where the summit ridge's crest runs, m down the map
  double base_z;     // where the valley floor begins
  double flank;      // the side ridges' height over the face, m
  double crests;     // the summit's crests at full height, m
  double hills;
  double ridges;
  double rollers;  // crest over trough, m
  std::vector<Bowl> bowls;
  std::vector<Headwall> headwalls;
  // The share of the vertical still to fall at u (0 summit … 1 base), every 1/1024 of the
  // descent, the headwalls' drops taken out. Shared by every plan of one profile shape.
  const std::array<double, kProfileSamples + 1>* profile;
  const Region* region;
  const GradeRow* grade;
  struct {
    double warp, flank, hills, ridges, rollers, crests, headwalls;
  } seeds;  // one noise seed per layer, so the layers do not echo each other
};

// Deal the mountain's plan off the attempt's stream, in `region` (R21), for a piste of `grade`.
TerrainPlan plan_terrain(ge::core::Rng& rng, const Region& region, const GradeRow& grade);

// Bake the untouched mountain onto the map's grid (R1), row by row.
ge::core::Heightfield bake_country(const TerrainPlan& plan);

// The share of the way down the descent a point stands (0 summit ridge … 1 valley floor).
inline double descent_at(const TerrainPlan& plan, double z) {
  return (z - plan.summit_z) / (plan.base_z - plan.summit_z);
}

// R2 — how far up a side ridge a plan point stands, 0 on the face … 1 at the flank's height.
double flank_at(const TerrainPlan& plan, double x, double z);

}  // namespace ge::mountain::mapgen
