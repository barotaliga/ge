// ge:summary The dodge in a fight: the skill off the trait, the stretches drawn, the side checked
// ge:keywords dodge dodge_skill dodge_step dodge_clear side-step ledge liquid
// ge:skill shooter bots
// ge:test tests/shooter_dodge_test.cpp
#include "shooter/dodge.hpp"

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "shooter/brush_hull.hpp"
#include "shooter/hull.hpp"
#include "shooter/server.hpp"
#include "shooter/trace.hpp"
#include "shooter/traits.hpp"

namespace ge::shooter {

namespace {

// How far a side step is looked along, units (a tenth of a second at full side speed and a half).
constexpr float kLook = 48;
// The box may stop this short of it (a wall a hand away): the step is not worth taking.
constexpr float kMinGo = 0.5f;
// The floor is looked for this far under the box: a stair's step down and a little more.
constexpr float kFloor = 30;

}  // namespace

DodgeSkill dodge_skill(uint8_t trait) {
  DodgeSkill k;
  k.hold_s = scale(trait, {0.9, 0.6, 0.38, 0.3});
  k.press = static_cast<float>(scale(trait, {0.45, 0.7, 1, 1}));
  k.straight = scale(trait, {0.55, 0.3, 0.12, 0.05});
  if (k.press > 1) k.press = 1;
  if (k.straight < 0) k.straight = 0;
  return k;
}

void dodge_init(Dodge& d, uint32_t seed) {
  d.rng = core::make_rng(seed ^ 0x6d2b79f5u);
  d.until = -1e9;
  d.dir = 0;
}

int dodge_step(Dodge& d, const DodgeSkill& k, double t, bool left_clear, bool right_clear) {
  const bool clear = d.dir < 0 ? left_clear : d.dir > 0 ? right_clear : true;
  if (t < d.until && clear) return d.dir;
  const int32_t was = d.dir;
  if (core::chance(d.rng, k.straight)) {
    d.dir = 0;
  } else if (was == 0) {
    d.dir = core::chance(d.rng, 0.5) ? 1 : -1;
  } else {
    d.dir = -was;
  }
  if (d.dir < 0 && !left_clear) d.dir = right_clear ? 1 : 0;
  if (d.dir > 0 && !right_clear) d.dir = left_clear ? -1 : 0;
  d.until = t + k.hold_s * core::range(d.rng, 0.5, 1.5);
  return d.dir;
}

bool dodge_clear(const Server& sv, Vec3f origin, float yaw_deg, int dir, int me) {
  const double y = yaw_deg * (core::kPi / 180.0);
  // The view's right: (sin yaw, -cos yaw) on the plan.
  const float rx = static_cast<float>(core::libm::sin(y)) * static_cast<float>(dir);
  const float ry = static_cast<float>(-core::libm::cos(y)) * static_cast<float>(dir);
  const Vec3f to{origin.x + rx * kLook, origin.y + ry * kLook, origin.z};
  const HullTrace go = trace_move(sv, origin, kPlayerMins, kPlayerMaxs, to, kTraceNoMonsters, me);
  if (go.startsolid || go.fraction < kMinGo) return false;
  const Vec3f end = go.endpos;
  const HullTrace down = trace_move(sv, end, kPlayerMins, kPlayerMaxs,
                                    {end.x, end.y, end.z - kFloor}, kTraceNoMonsters, me);
  if (down.fraction >= 1) return false;  // a drop
  // The feet where the box lands, and a little above them: no water, slime or lava.
  const float feet = down.endpos.z + kPlayerMins.z;
  if (world_contents(sv, {down.endpos.x, down.endpos.y, feet + 2}) <= kContentsWater) return false;
  if (world_contents(sv, {down.endpos.x, down.endpos.y, feet - 6}) <= kContentsWater) return false;
  return true;
}

}  // namespace ge::shooter
