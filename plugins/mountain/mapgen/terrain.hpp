// R2, R3 — THE MOUNTAIN BEFORE ANYBODY SKIS IT: one flank falling from a summit ridge along
// the top edge to a valley floor along the bottom, side ridges either hand, and a face folded
// into spurs and gullies, bowls, headwalls and rollers: the single piste's mountain (the plan,
// the bake, the flank, the descent, the noise stack). The ski area's massif, a real face and
// the sea arrive with M4b–M4d.
//
// Everything is a pure function of a PLAN (numbers drawn off the attempt's stream once) and a
// plan point, so the search asks "how high is the untouched ground here?" of the arithmetic
// the bake writes into the grid. The bake runs once an attempt; the piste's grading and the
// kickers are then stamped into the grid it made, and the noise is never evaluated again.
//
// THE PROFILE is the mountain's shape down the fall line, integrated once into a table and
// scaled to fall the dealt vertical; the headwalls are steps taken out of that vertical and
// put back as short bands. The ground falls toward +z: heading 0 is the fall line.
// Held by tests/mountain_generate_test.cpp (`ground`).
//
// ge:summary R2, R3 the mountain: plan_terrain and the baked country
// ge:keywords terrain mountain heightfield profile
// ge:skill mountain
// ge:test tests/mountain_generate_test.cpp
#pragma once

#include <array>
#include <memory>
#include <optional>
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
// centred `z` down the map (R3). On a resort's massif (R25) it is a band of the face: centred
// on `x` and reaching a bell's sigma `spread` across it (both absent on the single piste's).
struct Headwall {
  double z, drop, run;
  std::optional<double> x;
  std::optional<double> spread;
};

struct Massif;  // massif.hpp

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
  // R25 — how far over the sea the map's lowest ground stands, m (from v8; absent before, where
  // the floor stands at `altitude`).
  std::optional<double> sea;
  // R25 — how many times R3's wavelengths the hills and the spurs and gullies are drawn at (from
  // v8, `massif.relief.scale`); one before. Multiplying by one is exact, so it is always applied.
  double fold = 1;
  // R25 — THE RESORT'S MOUNTAIN (massif.hpp): the height is the massif's where set.
  std::shared_ptr<const Massif> massif;
  // R2 — where the side ridges start and reach their height, m across from the map's middle:
  // the rule book's `mountain.flank` but on a resort's wider face (R25).
  struct {
    double inner, outer;
  } flank_band{rules::mountain::flank::inner, rules::mountain::flank::outer};
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

// R25 — where the sea stands in the map's frame, m: from v8 the lowest ground's height less the
// plan's `sea`; before, under the village by the floor's altitude.
double sea_level_of(const TerrainPlan& plan, const ge::core::Heightfield& ground,
                    const Vec3& village);

// THE NOISE STACK the mountain is read off (the country's fields, fractal and ridged),
// shared with the massif's bake (massif.hpp). Each field keeps the lattice square it last read,
// and a bake reads its grid in order, so nearly every read reuses its corners.
struct RidgedFields {
  ge::core::NoiseField coarse;
  ge::core::NoiseField fine;
};
struct CountryFields {
  ge::core::NoiseField warp_x;
  ge::core::NoiseField warp_z;
  ge::core::NoiseField flank;
  std::array<ge::core::NoiseField, 4> hills;
  RidgedFields ridges;
  RidgedFields rollers;
  RidgedFields crests;
  RidgedFields crests_fine;
  std::vector<ge::core::NoiseField> headwalls;
};
CountryFields country_fields(const TerrainPlan& plan);
// Fractal value noise centred on zero, roughly −1..1, over the four hill octaves.
double fbm(std::array<ge::core::NoiseField, 4>& fields, double x, double z);
// Ridged noise, 0..1 with sharp crests at 1.
double ridged(RidgedFields& f, double x, double z);
// How open the side ridges are at z, and the flank across x with that already read.
double flank_open(const TerrainPlan& plan, double z);
double flank_across(const TerrainPlan& plan, double noise, double x, double open);

}  // namespace ge::mountain::mapgen
