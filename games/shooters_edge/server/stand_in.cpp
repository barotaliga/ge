// The stand-in bot's command (stand_in.hpp says what it does).
//
// ge:summary The stand-in bot's command: wander, turn at walls, jump, face and fire
// ge:keywords stand-in wander turn jump fire aim
// ge:skill shooter
// ge:test tests/shooters_edge_host_test.cpp
#include "shooters_edge/server/stand_in.hpp"

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "shooter/server.hpp"

namespace shooters_edge::server {

namespace {

namespace sh = ge::shooter;
namespace libm = ge::core::libm;

constexpr double kDegPerRad = 57.29577951308232;
constexpr float kSlow = 60;          // units/s: slower than this while running is a wall
constexpr int kStuckMs = 250;        // ms held slow before it turns away
constexpr double kSightRange = 900;  // units
constexpr float kSightCone = 25;     // degrees either side of its heading

float wrap(float deg) {
  while (deg > 180) deg -= 360;
  while (deg <= -180) deg += 360;
  return deg;
}

int ms_between(ge::core::Rng& r, int lo, int hi) { return ge::core::int_in(r, lo, hi); }

sh::MoveCmd next(void* ctx, const sh::Server& sv, int slot, int msec) {
  StandIn& s = *static_cast<StandIn*>(ctx);
  const sh::Edict& me = sv.edicts[static_cast<size_t>(slot) + 1];
  if (!s.started) {
    s.yaw = me.v_angle.y;
    s.turn_in_ms = ms_between(s.rng, 600, 2400);
    s.started = true;
  }
  sh::MoveCmd c;
  c.forward = 400;

  // A wall: held forward and barely moving.
  const double speed = ge::core::hypot(me.velocity.x, me.velocity.y);
  s.stuck_ms = speed < kSlow ? s.stuck_ms + msec : 0;
  s.turn_in_ms -= msec;
  if (s.stuck_ms >= kStuckMs) {
    const float turn = static_cast<float>(ge::core::range(s.rng, 90, 180));
    s.yaw = wrap(s.yaw + (ge::core::chance(s.rng, 0.5) ? turn : -turn));
    s.stuck_ms = 0;
  } else if (s.turn_in_ms <= 0) {
    s.yaw = wrap(s.yaw + static_cast<float>(ge::core::range(s.rng, -75, 75)));
    s.turn_in_ms = ms_between(s.rng, 600, 2400);
  }

  // Now and then a jump, strafing a moment while it does.
  if (ge::core::chance(s.rng, 1.0 / 200)) {
    c.buttons |= sh::kButtonJump;
    s.strafe_side = ge::core::chance(s.rng, 0.5) ? 1 : -1;
    s.strafe_ms = 300;
  }
  if (s.strafe_ms > 0) {
    c.side = static_cast<int16_t>(400 * s.strafe_side);
    s.strafe_ms -= msec;
  }

  // Another living player ahead and near: face him and fire.
  float view_yaw = s.yaw, view_pitch = 0;
  double nearest = kSightRange;
  for (int o = 1; o <= sv.set.maxclients; o++) {
    if (o == slot + 1) continue;
    const sh::Edict& other = sv.edicts[static_cast<size_t>(o)];
    if (other.free || other.solid == sh::kSolidNot || other.health <= 0) continue;
    const double dx = other.origin.x - me.origin.x, dy = other.origin.y - me.origin.y;
    const double dz = other.origin.z - me.origin.z;
    const double flat = ge::core::hypot(dx, dy);
    if (flat >= nearest || flat < 1) continue;
    const auto bearing = static_cast<float>(libm::atan2(dy, dx) * kDegPerRad);
    if (wrap(bearing - s.yaw) > kSightCone || wrap(bearing - s.yaw) < -kSightCone) continue;
    nearest = flat;
    view_yaw = bearing;
    view_pitch = static_cast<float>(-libm::atan2(dz, flat) * kDegPerRad);
  }
  if (nearest < kSightRange) c.buttons |= sh::kButtonAttack;
  c.angles = {view_pitch, view_yaw, 0};
  return c;
}

}  // namespace

StandIn make_stand_in(uint32_t seed, int slot) {
  StandIn s;
  // The slot's own stream: the run's seed mixed with the slot (a different stream per bot).
  s.rng = ge::core::make_rng(seed * 0x9E3779B1u + static_cast<uint32_t>(slot) * 0x85EBCA77u + 1);
  return s;
}

sh::CmdSource stand_in_source(StandIn& s) { return {&s, next}; }

}  // namespace shooters_edge::server
