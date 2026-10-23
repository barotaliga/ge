// Where the army's helicopter puts its men down (army_land.hpp).
//
// ge:summary land_spot, army_floor and rope_hover's bodies: the landing zone search
// ge:keywords army helicopter landing land_spot army_floor rope_hover grid cost
// ge:skill sim
// ge:test tests/fall_line_army_heli_test.cpp
#include "fall_line/sim/army_land.hpp"

#include <cmath>
#include <vector>

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "core/num.hpp"

#include "fall_line/sim/army_heli_rules.hpp"
#include "fall_line/sim/heli_ride.hpp"

namespace fall_line::sim {

namespace mg = ge::mountain::mapgen;
namespace ml = ge::mountain::lifts;
namespace sk = ge::skier;
namespace lm = ge::core::libm;
namespace num = ge::core::num;
using ge::core::hypot;
using ge::core::kPi;
using namespace army_heli;

namespace {

const std::vector<ml::LiftPlan> kNoLifts;

const std::vector<ml::LiftPlan>& lifts_of(const GameState& state) {
  return state.lifts ? *state.lifts : kNoLifts;
}

// Whether no trunk stands within `r` m of (x, z).
bool no_trunk(const GameState& state, double x, double z, double r) {
  bool hit = false;
  sk::for_each_tree_near(state.trees, x, z, [&](int32_t i) {
    const mg::TreeDef& t = state.level->trees[static_cast<size_t>(i)];
    if (hypot(t.x - x, t.z - z) < r + t.radius) hit = true;
  });
  return !hit;
}

}  // namespace

LandRing drop_ring() { return LandRing{kNearM, kFarM, kPreferM}; }

double army_floor(const GameState& state, double x, double z, double floor) {
  const mg::Level& level = *state.level;
  double ground = level.ground_at(x, z);
  for (int32_t i = 0; i < 6; i++) {
    const double a = i / 6.0 * 2 * kPi;
    ground = num::max(ground, level.ground_at(x + lm::sin(a) * kRotorM, z + lm::cos(a) * kRotorM));
  }
  double low = ground + floor;
  sk::for_each_tree_near(state.trees, x, z, [&](int32_t i) {
    const mg::TreeDef& t = level.trees[static_cast<size_t>(i)];
    if (hypot(t.x - x, t.z - z) < kRotorM + t.crown) low = num::max(low, t.y + t.height + kCrownM);
  });
  return low;
}

double rope_hover(const GameState& state, double x, double z, double y) {
  return num::max(y + kRopeHoverM, army_floor(state, x, z, kRopeHoverM));
}

std::optional<LandSpot> land_spot(const GameState& state, double cx, double cy, double cz,
                                  double vx, double vz, std::span<const LandAvoid> avoid,
                                  LandRing ring) {
  const mg::Level& level = *state.level;
  const double room = kRotorM + kRoomM;
  const double edge = room + 40;
  const double run = hypot(vx, vz);
  const double ux = run > 3 ? vx / run : 0;
  const double uz = run > 3 ? vz / run : 0;
  // THE COST of a place: off the preferred distance, less what it lies ahead of him, and what it
  // lies more than 15 m below him (the men would climb back up).
  const auto cost = [&](double x, double z, double y) {
    const double d = hypot(x - cx, z - cz);
    const double ahead = d > 1 ? ((x - cx) * ux + (z - cz) * uz) / d : 0;
    return std::fabs(d - ring.prefer) - kAheadCost * ahead + 2 * num::max(0.0, cy - 15 - y);
  };
  const auto ok = [&](double x, double z) {
    if (x < edge || z < edge || x > level.size - edge || z > level.size - edge) return false;
    const double d = hypot(x - cx, z - cz);
    if (d < ring.near || d > ring.far) return false;
    for (const LandAvoid& a : avoid) {
      if (hypot(a.x - x, a.z - z) < a.r) return false;
    }
    return true;
  };
  bool has_best = false;
  bool has_rope = false;
  double best_c = 0, rope_c = 0;
  LandSpot best{}, rope{};
  const auto n = static_cast<int32_t>(std::floor(ring.far / kGridM));
  for (int32_t iz = -n; iz <= n; iz++) {
    for (int32_t ix = -n; ix <= n; ix++) {
      const double x = cx + ix * kGridM;
      const double z = cz + iz * kGridM;
      if (!ok(x, z)) continue;
      const double y = level.ground_at(x, z);
      const double c = cost(x, z, y);
      if (has_best && c >= best_c) continue;
      if (pad_lean(level, x, z, room) <= kSlope &&
          pad_clear(level, lifts_of(state), state.trees, x, z, room)) {
        has_best = true;
        best_c = c;
        best = LandSpot{x, y, z, 0, false};
        continue;
      }
      // A rope's: clear round a man's drop, under a hover the rope reaches.
      if (has_rope && c >= rope_c) continue;
      if (!no_trunk(state, x, z, kRopeClearM)) continue;
      if (rope_hover(state, x, z, y) - y > kRopeReachM) continue;
      has_rope = true;
      rope_c = c;
      rope = LandSpot{x, y, z, 0, true};
    }
  }
  if (!has_best && !has_rope) return std::nullopt;
  LandSpot at = has_best ? best : rope;
  // Set down side on to him: a door each way, the one toward him first.
  at.heading = lm::atan2(cx - at.x, cz - at.z) + kPi / 2;
  return at;
}

}  // namespace fall_line::sim
