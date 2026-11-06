// The easy jump's command for a server that does not run it (speedjump_assist.hpp states the
// rules).
//
// ge:summary The easy jump's sent command on a stock server: jump released in the air, side + yaw
// ge:keywords assisted_command speedjump easy-jump auto-strafe
// ge:skill shooter
// ge:test tests/shooter_speedjump_assist_test.cpp
#include "shooter/speedjump_assist.hpp"

#include "core/libm/libm.hpp"

namespace ge::shooter {

namespace {

constexpr double kDegrees = 180 / 3.14159265358979323846;
constexpr double kMostOff = 45;  // degrees between the way and the view the air help reaches
constexpr double kAirCap = 30;   // the air's wish cap in the move's gain

// An angle in degrees into -180..180.
double wrap(double a) {
  while (a > 180) a -= 360;
  while (a < -180) a += 360;
  return a;
}

}  // namespace

MoveCmd assisted_command(const MoveCmd& held, const MoveState& s, float share) {
  if (held.forward <= 0 || held.side != 0 || (held.buttons & kButtonAttack) != 0 ||
      s.type != MoveType::normal || s.waterlevel >= 2 || share <= 0)
    return held;
  MoveCmd out = held;
  if (s.ground != -1) return out;  // standing: the jump (if held) goes out pressed
  out.buttons = static_cast<uint8_t>(out.buttons & ~kButtonJump);  // flying: let go for the next
  const double vx = s.velocity.x, vy = s.velocity.y;
  const double h = core::libm::pow(vx * vx + vy * vy, 0.5);
  if (h < 1) return out;
  const double way = core::libm::atan2(vy, vx) * kDegrees;
  const double off = wrap(static_cast<double>(held.angles.y) - way);
  if (off > kMostOff || off < -kMostOff) return out;
  const double k = share > 1 ? 1.0 : static_cast<double>(share);
  double c = kAirCap * core::libm::pow(1 - k, 0.5);
  if (c > h) c = h;
  const double across = core::libm::acos(c / h) * kDegrees;
  const double wish = way + (off >= 0 ? across : -across);
  out.forward = 0;
  out.side = held.forward;
  out.angles.y = static_cast<float>(wrap(wish + 90));
  return out;
}

}  // namespace ge::shooter
