// The hand-built maps' builds (synthetic.hpp): the face, its snow, its piste and its one tree.
//
// ge:summary Builds the synthetic slope and the drag strip
// ge:keywords synthetic slope flat strip build
// ge:skill skier
// ge:test tests/skier_scenarios_test.cpp
#include "skier/synthetic.hpp"

#include "core/heightfield.hpp"
#include "core/libm/libm.hpp"
#include "core/math.hpp"

namespace ge::skier {

namespace {

namespace mg = mountain::mapgen;

// A straight piste down x = `x` from z = `from` to `to`, a point every 2 m, with its start gate
// a few metres down and its finish line at its end.
void lay_piste(mg::Level& level, double x, double from, double to, double width) {
  const auto n = static_cast<int>((to - from) / 2);
  for (int i = 0; i <= n; i++) {
    const double z = from + 2.0 * i;
    level.track.points.push_back(mg::TrackPoint{x, z, level.ground_at(x, z), z - from, 0, width});
  }
  level.track.length = level.track.points.back().s;
  const auto gate = [&](double z, mg::GateColour c) {
    level.checkpoints.push_back(mg::Checkpoint{x, z, level.ground_at(x, z), 0, width, z - from, c});
  };
  gate(from + 8, mg::GateColour::red);
  gate(to, mg::GateColour::blue);
  level.spawn = mg::Spawn{x, from, 0};
  level.grid = {level.spawn};
}

}  // namespace

mg::Level synthetic_slope() {
  namespace k = slope;
  mg::Level level;
  level.size = k::kSize;
  level.cell = 2;
  const int n = static_cast<int>(k::kSize / level.cell) + 1;
  level.ground = core::create_heightfield(0, 0, level.cell, n, n);
  level.packed = core::create_heightfield(0, 0, level.cell, n, n);
  const double grade = core::libm::tan(k::kPitch);
  core::fill_field(level.ground, [&](double x, double z) {
    double h = 400 - z * grade;
    if (z > k::kRollersFrom && z < k::kRollersTo) {
      // Rollers: whole waves of 25 m, 0.8 m high, across the whole face.
      const double phase = (z - k::kRollersFrom) / 25 * core::kTau;
      h += 0.4 * (1 - core::libm::cos(phase));
    }
    const double foot = k::kKickerLip - k::kKickerRamp;
    if (z > foot && z <= k::kKickerLip && x > k::kCentre - 6 && x < k::kCentre + 6) {
      h += k::kKickerHeight * (z - foot) / k::kKickerRamp;
    }
    return h;
  });
  core::fill_field(level.packed, [&](double x, double) {
    const double off = x < k::kCentre ? k::kCentre - x : x - k::kCentre;
    return 1 - core::smoothstep(20, 30, off);
  });
  lay_piste(level, k::kCentre, 10, k::kSize - 20, 40);
  level.trees.push_back(mg::TreeDef{k::kTreeX, k::kTreeZ, level.ground_at(k::kTreeX, k::kTreeZ), 18,
                                    0.2, 60, 2.5, mg::TreeKind::spruce, -1});
  level.kickers.push_back(
      mg::Kicker{"K1", k::kCentre, k::kKickerLip, level.ground_at(k::kCentre, k::kKickerLip), 0,
                 k::kKickerHeight, k::kKickerRamp, 20, 12, true, k::kKickerLip - 10});
  return level;
}

mg::Level flat_strip(double pitch, double packed) {
  mg::Level level;
  level.size = kStripSize;
  level.cell = 16;
  const int n = static_cast<int>(kStripSize / level.cell) + 1;
  level.ground = core::create_heightfield(0, 0, level.cell, n, n);
  level.packed = core::create_heightfield(0, 0, level.cell, n, n);
  const double grade = core::libm::tan(pitch);
  core::fill_field(level.ground, [&](double, double z) { return 2000 - z * grade; });
  core::fill_field(level.packed, [&](double, double) { return packed; });
  lay_piste(level, kStripSize / 2, 10, kStripSize - 20, 60);
  return level;
}

}  // namespace ge::skier
