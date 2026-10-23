// The bot's hands on the paramotor (pilot.hpp).
//
// ge:summary The paramotor's bot: launch, hold a height toward an aim, glide in, flare, pump folds
// ge:keywords paramotor pilot para_pilot launch aim hold-height glide flare pump
// ge:skill paramotor
// ge:test tests/paramotor_scenarios_test.cpp
#include "paramotor/pilot.hpp"

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "core/num.hpp"

namespace ge::paramotor {

namespace {
namespace lm = core::libm;
namespace num = core::num;
namespace mg = mountain::mapgen;

// The heading down the fall line at (x, z), or `fallback` on the flat.
double fall_line(const mg::Level& level, double x, double z, double fallback) {
  mg::Vec3 n{0, 1, 0};
  level.normal_at(x, z, n);
  return core::hypot(n.x, n.z) > 0.02 ? lm::atan2(n.x, n.z) : fallback;
}
}  // namespace

ParaControls para_pilot(const ParaState& p, const skier::SkierState& s, const ParaWorld& world,
                        const ParaAim& aim) {
  const mg::Level& level = *world.level;
  const double heading = p.flying ? p.heading : s.heading;
  const double to_aim = lm::atan2(aim.x - s.x, aim.z - s.z);
  ParaControls c;
  if (!p.flying) {
    if (p.flights > 0) {
      // LANDED: the throttle cut, the brakes on.
      c.brake = 1;
      return c;
    }
    // THE LAUNCH: down the fall line on full throttle, the toggles half toward the aim.
    const double want = aim.has_aim ? to_aim : fall_line(level, s.x, s.z, heading);
    const double down = fall_line(level, s.x, s.z, want);
    c.throttle = 1;
    c.steer = core::clamp(core::angle_diff(heading, down) * pilot::kTurn, -1.0, 1.0) * 0.5;
    return c;
  }
  const double want = aim.has_aim ? to_aim : fall_line(level, s.x, s.z, heading);
  c.steer = core::clamp(core::angle_diff(heading, want) * pilot::kTurn, -1.0, 1.0);
  // THE HEIGHT over the highest snow under him and ahead along his way.
  const double sp = core::hypot(s.vx, s.vz);
  const double ux = sp > 1 ? s.vx / sp : lm::sin(heading);
  const double uz = sp > 1 ? s.vz / sp : lm::cos(heading);
  double ground = level.ground_at(s.x, s.z);
  for (int k = 1; k * pilot::kLookStep <= pilot::kLook; k++) {
    const double d = k * pilot::kLookStep;
    ground = num::max(ground, level.ground_at(s.x + ux * d, s.z + uz * d));
  }
  const double over = s.y - ground;
  // ...and how fast that height is closing: his climb less the snow's under his way.
  const double under = level.ground_at(s.x, s.z);
  const double rel = s.vy - (level.ground_at(s.x + s.vx, s.z + s.vz) - under);
  c.throttle = core::clamp(
      pilot::kCruise + (aim.height - over) * pilot::kPerMetre - rel * pilot::kPerSink, 0.0, 1.0);
  if (aim.land) {
    // Within the glide of the aim: the throttle cut, glide down to it, flare at the snow.
    const double dist = core::hypot(aim.x - s.x, aim.z - s.z);
    if (dist < p.agl * pilot::kGlide + pilot::kMargin) c.throttle = 0;
    if (p.agl < pilot::kFlare) {
      c.brake = core::clamp((pilot::kFlare - p.agl) / (pilot::kFlare - 1), 0.0, 1.0);
      c.steer = 0;
    }
  }
  // A FOLD pumped out on the brakes, never while stalled; the other toggle holds the line.
  if (p.fold > pilot::kPump && !p.stalled && num::mod(world.now * 2, 1.0) < 0.5) {
    c.brake = num::max(c.brake, pilot::kPumpBrake);
  }
  c.steer = core::clamp(c.steer - p.fold_side * p.fold * 0.5, -1.0, 1.0);
  return c;
}

}  // namespace ge::paramotor
