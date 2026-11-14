// ge:summary The brink: the keys that would carry a player on foot into lava or slime let go of
// ge:keywords brink lava-lip lava slime harm pit edge fall-in brink_guard over_harm keep-out
// ge:keywords fall_into_harm stair-fall air-brake
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

constexpr float kNear = 96;          // the guard runs only this near a harm node, units across
constexpr float kLook = 40;          // a step each way round, units (a player's width and a little)
constexpr float kRun_s = 0.25f;      // the run looked along this far ahead, s
constexpr float kRunMost = 80;       // and no further, units (a run's stop from full speed)
constexpr float kRunMin = 60;        // a run under this speed is not looked along, units/s
constexpr float kDeep = 400;         // the floor looked for this far under, units
constexpr float kKey = 320;          // a key pressed full, units/s
constexpr float kGravity = 800;      // sv_gravity's default, units/s^2
constexpr float kFly_dt = 0.05f;     // the flown box's step, s
constexpr int kFlySteps = 20;        // and a second of them at most
constexpr float kLandRun = 32;       // the run on after a landing looked along at least, units
constexpr float kFlyReach_s = 0.6f;  // a flight's reach across: the run this long, units
constexpr float kFlyReachMost = 256;  // and no more
constexpr float kFloorZ = 0.7f;       // a plane this steep or flatter is a floor
constexpr float kStepUp = 18;         // a stair's step the move climbs, units

bool harm_at(const Server& sv, Vec3f origin) {
  const int c = world_contents(sv, {origin.x, origin.y, origin.z + kPlayerMins.z + 2});
  return c == kContentsLava || c == kContentsSlime;
}

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

bool fall_into_harm(const Server& sv, Vec3f origin, Vec3f velocity, float ground, int me) {
  Vec3f p = origin, v = velocity;
  bool flew = false;
  for (int i = 0; i < kFlySteps && ground > 0; i++) {
    // Across: a stair's step (a rise of kStepUp) is climbed; a wall slides it along its face as
    // the move does (a run into dm2's YA pillar slides on off the step), and only a dead stop
    // ends the run.
    const Vec3f start = p;
    for (int tries = 0; tries < 2; tries++) {
      const Vec3f to{p.x + v.x * kFly_dt, p.y + v.y * kFly_dt, p.z};
      HullTrace h = trace_move(sv, p, kPlayerMins, kPlayerMaxs, to, kTraceNoMonsters, me);
      if (h.startsolid) return false;
      bool stepped = false;
      if (h.fraction < 1) {
        const Vec3f up{p.x, p.y, p.z + kStepUp};
        const HullTrace u = trace_move(sv, p, kPlayerMins, kPlayerMaxs, up, kTraceNoMonsters, me);
        const HullTrace s = trace_move(sv, u.endpos, kPlayerMins, kPlayerMaxs,
                                       {to.x, to.y, u.endpos.z}, kTraceNoMonsters, me);
        if (!s.startsolid && s.fraction > h.fraction) {
          h = s;
          stepped = true;
        }
      }
      p = h.endpos;
      if (h.fraction >= 1 || stepped || h.plane.normal.z >= kFloorZ) break;
      const float into = v.x * h.plane.normal.x + v.y * h.plane.normal.y;
      v.x -= h.plane.normal.x * into;
      v.y -= h.plane.normal.y * into;
    }
    const float dx = p.x - start.x, dy = p.y - start.y;
    const float moved = std::sqrt(dx * dx + dy * dy);
    if (moved < 1 && v.z >= 0) return false;  // a dead stop on the floor: the run ends here
    // Down: gravity; a floor lands it (and a step down a stair is a short flight).
    v.z -= kGravity * kFly_dt;
    const HullTrace d = trace_move(sv, p, kPlayerMins, kPlayerMaxs, {p.x, p.y, p.z + v.z * kFly_dt},
                                   kTraceNoMonsters, me);
    p = d.endpos;
    if (harm_at(sv, p)) return true;
    if (d.fraction < 1 && d.plane.normal.z >= kFloorZ) {
      if (flew && ground < kLandRun) ground = kLandRun;
      flew = false;
      v.z = 0;
      ground -= moved;
    } else {
      flew = true;
    }
  }
  return false;
}

BrinkGuard brink_guard(const Server& sv, const NavGraph& g, int me, Vec3f origin, Vec3f velocity,
                       MoveCmd& cmd, bool fall) {
  BrinkGuard out;
  const float speed = std::sqrt(velocity.x * velocity.x + velocity.y * velocity.y);
  const bool near = nav_harm_near(g, origin, kNear);
  const float reach = speed * kFlyReach_s < kFlyReachMost ? speed * kFlyReach_s : kFlyReachMost;
  const bool fly = fall && speed > kRunMin && (near || nav_harm_near(g, origin, kNear + reach));
  if (!near && !fly) return out;
  out.ran = true;
  // The wish on the plan: forward along the view's yaw, the side to its right.
  const double yaw = cmd.angles.y * (core::kPi / 180.0);
  const float fx = static_cast<float>(core::libm::cos(yaw)),
              fy = static_cast<float>(core::libm::sin(yaw));
  const float rx = fy, ry = -fx;
  float wx = cmd.forward * fx + cmd.side * rx, wy = cmd.forward * fy + cmd.side * ry;
  // A step each way round: the wish's share along a way into the pit let go of.
  for (int k = 0; k < 8 && near; k++) {
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
  // The run itself: where it carries him a quarter second on, braked when that is the pit; with
  // `fall`, flown on down a stair's steps or a hop's arc to where it lands and runs on.
  if (speed > kRunMin) {
    const float ahead = speed * kRun_s < kRunMost ? speed * kRun_s : kRunMost;
    const float vx = velocity.x / speed, vy = velocity.y / speed;
    out.run_in =
        near && over_harm(sv, origin, {origin.x + vx * ahead, origin.y + vy * ahead, origin.z}, me);
    if (!out.run_in && fly && fall_into_harm(sv, origin, velocity, ahead, me))
      out.run_in = out.fall_in = true;
    if (out.run_in) {
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
