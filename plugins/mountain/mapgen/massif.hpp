// R25 — THE RESORT'S MOUNTAIN: one massif a whole ski area is laid out on, dealt or laid off a
// real face.
//
// The summit ridge rises to a PEAK and falls away to a lower SHOULDER; the face is steepest
// under the peak and gentler under the shoulder; a BENCH part-way down eases the fall where the
// mid-station stands; and the lower slopes run out to the valley floor. So the height is
// `ridge(x) · P(x, u)`: the ridge's share of the vertical at that x times a profile blended
// across x between four tables (steep and gentle, each with and without the bench), every one
// falling from one at the ridge to nothing on the floor. Over it lie R3's folds, drawn over the
// same noise stack as the single piste's mountain (terrain.hpp), the headwalls weighted to the
// steep sector. A pure function of the plan, baked once onto a grid wider than the rule book's.
//
// A REAL FACE (real_face.hpp) is laid 1:1: every draw is made all the same (so the stream the
// rest of the ski area is dealt off runs on as it would), and what the face stands in for is read
// off it: its summit ridge where the real one stands (down to its BROW, where a plateau starts
// to fall), its peak and shoulder on the sides the real ones stand (kept in the dealt bands), its
// valley at the real lowest point, its floor (the map's nought) and its own profile down the
// fall line. The height is then the real one over the floor, fading onto that profile on the
// valley floor so the hub stands level; the side ridges, crests, bowls and headwalls are the
// face's own, and only a share of R3's hills and spurs is laid over it (`massif::real`). The
// real peak, shoulder and valley are kept for the first `real::placed` attempts, the dealt ones
// after.
// Held by tests/mountain_resort_build_test.cpp (the ground).
//
// ge:summary R25 the resort's massif: its plan and the baked mountain a ski area stands on
// ge:keywords massif resort mountain plan-massif bake-massif peak shoulder bench ridge-share
// ge:keywords read-face face-lift brow real-face
// ge:skill mountain
// ge:test tests/mountain_resort_build_test.cpp
#pragma once

#include <array>
#include <memory>

#include "core/heightfield.hpp"
#include "core/prng.hpp"
#include "mountain/mapgen/real_hints.hpp"
#include "mountain/mapgen/regions.hpp"
#include "mountain/mapgen/terrain.hpp"

namespace ge::mountain::mapgen {

// What the massif adds to a plan: where the peak, the shoulder and the bench are, and the
// tables the profile is blended from.
struct Massif {
  double side;  // which hand the peak stands on: +1 east of the middle, −1 west
  double peak_x;
  double peak_spread;  // the peak's spread along the ridge (a bell's sigma), m
  double shoulder_x;
  double shoulder_share;  // the ridge's share of the vertical at the shoulder
  double steep_spread;    // the steep sector: a bell round the peak, its sigma, m
  double bench_u;         // how far down the descent the bench lies
  double bench_width;     // its half-width in u
  double bench_depth;     // how much of the fall it takes out at its middle
  double bench_x;         // where across the face it is strongest (the mid-station's x)
  double bench_spread;    // how far that reaches (sigma, m)
  double village_x;       // the village's x on the valley floor
  // The profile tables: [gentle, steep] × [no bench, bench].
  std::array<std::array<double, kProfileSamples + 1>, 4> tables;
  double ridge_seed;  // a slow noise along the ridge, so a ridge is not two bells
  double wide;        // how much wider than the rule book's square the massif's is
};

// R25 — deal the resort's mountain off the attempt's stream, in `region` (R21), laid off `face`
// where one is given (the attempt's number decides whether the real peak is kept), its woods and
// tree line off the face's `hints`, and the real lifts and pistes handed on to be leant on where
// `lean` and the attempt allow.
TerrainPlan plan_massif(ge::core::Rng& rng, const Region& region,
                        const std::shared_ptr<const RealFace>& face = nullptr, int attempt = 0,
                        const std::shared_ptr<const RealHints>& hints = nullptr, bool lean = true);

// Bake the massif onto the map's grid (R1), row by row.
ge::core::Heightfield bake_massif(const TerrainPlan& plan);

}  // namespace ge::mountain::mapgen
