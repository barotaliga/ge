// The bot's hands on the snowmobile (pilot.hpp).
//
// ge:summary sled_pilot: the bearing, the climb, the cruise and the trunks dodged
// ge:keywords snowmobile sled_pilot dodge lane look-ahead climb lean
// ge:skill snowmobile
// ge:test tests/snowmobile_scenarios_test.cpp
#include "snowmobile/pilot.hpp"

#include <cmath>

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "core/num.hpp"

namespace ge::snowmobile {

namespace {

namespace mg = mountain::mapgen;
namespace num = core::num;
namespace lm = core::libm;

// How far ahead the bot looks for trunks, m, and how wide a lane it wants clear of them either
// side of its centre line, m.
constexpr double kLook = 24;
constexpr double kLane = 2.4;

struct Dodge {
  double steer = 0;
  double slow = 0;
};

// The bars away from the trunks ahead and how much to slow for them: each trunk in the lane ahead
// pushes the bars toward its far side, harder the nearer it stands and the squarer in the way.
Dodge dodge(const SledState& s, const SledWorld& world) {
  Dodge out;
  if (!world.trees) return out;
  const mg::Level& level = *world.level;
  const double fx = lm::sin(s.heading);
  const double fz = lm::cos(s.heading);
  // The trees filed round a point half the look ahead: every one within the look is among them.
  skier::for_each_tree_near(*world.trees, s.x + fx * kLook / 2, s.z + fz * kLook / 2,
                            [&](int32_t i) {
                              const mg::TreeDef& t = level.trees[static_cast<size_t>(i)];
                              const double dx = t.x - s.x;
                              const double dz = t.z - s.z;
                              const double along = dx * fx + dz * fz;
                              if (along <= 0 || along > kLook) return;
                              // A trunk to the right is a positive side.
                              const double side = dx * fz - dz * fx;
                              const double room = kLane + t.radius;
                              if (std::fabs(side) >= room) return;
                              const double close = 1 - along / kLook;
                              const double square = 1 - std::fabs(side) / room;
                              const double w = close * (0.4 + 0.6 * square);
                              out.steer -= (side >= 0 ? 1 : -1) * w * 2.5;
                              out.slow = num::max(out.slow, w);
                            });
  return out;
}

}  // namespace

SledControls sled_pilot(const SledState& s, const SledWorld& world, const SledAim& aim) {
  const mg::Level& level = *world.level;
  const double dx = aim.x - s.x;
  const double dz = aim.z - s.z;
  const double d = core::hypot(dx, dz);
  if (d < kAimReach) return SledControls{0, 1, 0, 0};
  const double off = core::angle_diff(s.heading, lm::atan2(dx, dz));
  const Dodge avoid = dodge(s, world);
  const double steer = core::clamp(off * 1.6 + avoid.steer, -1, 1);
  // How steep it climbs: the snow's rise along the nose.
  constexpr double kAhead = 4;
  const double fx = lm::sin(s.heading);
  const double fz = lm::cos(s.heading);
  const double rise =
      (level.ground_at(s.x + fx * kAhead, s.z + fz * kAhead) - level.ground_at(s.x, s.z)) / kAhead;
  const double want =
      num::min(aim.cruise, std::sqrt(num::max(4.0, d)) * 2) * (1 - 0.6 * avoid.slow);
  const double climb = 1 - 0.7 * avoid.slow;
  const double throttle = rise > 0.15 ? climb : core::clamp((want - s.way) * 0.25 + 0.35, 0, 1);
  const double over = avoid.slow > 0.3 ? 2 : 6;
  const double brake = s.way > want + over ? core::clamp((s.way - want - over) * 0.2, 0, 1) : 0;
  const double lean = rise > 0.25 ? -0.7 : rise < -0.2 ? 0.4 : 0;
  return SledControls{throttle, brake, steer, lean};
}

}  // namespace ge::snowmobile
