// ge:summary The brink: the keys that would carry a player on foot into lava or slime let go of
// ge:keywords brink lava-lip lava slime harm pit edge fall-in brink_guard over_harm keep-out
// ge:skill shooter bots
// ge:test tests/shooter_brink_test.cpp
#include "shooter/brink.hpp"

#include <cmath>

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "shooter/hull.hpp"
#include "shooter/trace.hpp"
#include "shooter/usercmd.hpp"

namespace ge::shooter {

namespace {

constexpr float kNear = 96;      // the guard runs only this near a harm node, units across
constexpr float kLook = 40;      // a step each way round, units (a player's width and a little)
constexpr float kRun_s = 0.25f;  // the run looked along this far ahead, s
constexpr float kRunMost = 80;   // and no further, units (a run's stop from full speed)
constexpr float kRunMin = 60;    // a run under this speed is not looked along, units/s
constexpr float kDeep = 400;     // the floor looked for this far under, units
constexpr float kKey = 320;      // a key pressed full, units/s

}  // namespace

bool over_harm(const Server& sv, Vec3f from, Vec3f to, int me) {
  const HullTrace go = trace_move(sv, from, kPlayerMins, kPlayerMaxs, to, kTraceNoMonsters, me);
  if (go.startsolid) return false;
  const Vec3f end = go.endpos;
  const HullTrace down = trace_move(sv, end, kPlayerMins, kPlayerMaxs,
                                    {end.x, end.y, end.z - kDeep}, kTraceNoMonsters, me);
  if (down.fraction >= 1) return false;  // no floor near: not a pit this guard knows
  const float feet = down.endpos.z + kPlayerMins.z;
  const int c = world_contents(sv, {down.endpos.x, down.endpos.y, feet + 2});
  return c == kContentsLava || c == kContentsSlime;
}

BrinkGuard brink_guard(const Server& sv, const NavGraph& g, int me, Vec3f origin, Vec3f velocity,
                       MoveCmd& cmd) {
  BrinkGuard out;
  if (!nav_harm_near(g, origin, kNear)) return out;
  // The wish on the plan: forward along the view's yaw, the side to its right.
  const double yaw = cmd.angles.y * (core::kPi / 180.0);
  const float fx = static_cast<float>(core::libm::cos(yaw)),
              fy = static_cast<float>(core::libm::sin(yaw));
  const float rx = fy, ry = -fx;
  float wx = cmd.forward * fx + cmd.side * rx, wy = cmd.forward * fy + cmd.side * ry;
  // A step each way round: the wish's share along a way into the pit let go of.
  for (int k = 0; k < 8; k++) {
    const double a = k * (core::kPi / 4);
    const float dx = static_cast<float>(core::libm::cos(a)),
                dy = static_cast<float>(core::libm::sin(a));
    if (!over_harm(sv, origin, {origin.x + dx * kLook, origin.y + dy * kLook, origin.z}, me))
      continue;
    out.ways++;
    const float along = wx * dx + wy * dy;
    if (along > 0) {
      wx -= along * dx;
      wy -= along * dy;
    }
  }
  // The run itself: where it carries him a quarter second on, braked when that is the pit.
  const float speed = std::sqrt(velocity.x * velocity.x + velocity.y * velocity.y);
  if (speed > kRunMin) {
    const float ahead = speed * kRun_s < kRunMost ? speed * kRun_s : kRunMost;
    const float vx = velocity.x / speed, vy = velocity.y / speed;
    if (over_harm(sv, origin, {origin.x + vx * ahead, origin.y + vy * ahead, origin.z}, me)) {
      out.run_in = true;
      const float along = wx * vx + wy * vy;
      wx += (-kKey - along) * vx;
      wy += (-kKey - along) * vy;
    }
  }
  if (out.ways == 0 && !out.run_in) return out;
  const int16_t f = quantize_axis(wx * fx + wy * fy), s = quantize_axis(wx * rx + wy * ry);
  const auto jump_off = static_cast<uint8_t>(cmd.buttons & ~kButtonJump);
  out.changed = f != cmd.forward || s != cmd.side || jump_off != cmd.buttons;
  cmd.forward = f;
  cmd.side = s;
  cmd.buttons = jump_off;
  return out;
}

}  // namespace ge::shooter
