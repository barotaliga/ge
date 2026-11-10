// The easy jump's sent command (speedjump_assist.hpp states the rules).
//
// ge:summary The easy jump's sent command: jump released in the air, side key, the weaving yaw
// ge:keywords assisted_command speedjump easy-jump auto-strafe swing
// ge:skill shooter
// ge:test tests/shooter_speedjump_assist_test.cpp
#include "shooter/speedjump_assist.hpp"

#include "core/libm/libm.hpp"

namespace ge::shooter {

namespace {

constexpr double kDegrees = 180 / 3.14159265358979323846;
constexpr double kAirCap = 30;  // the air's wish cap in the move's gain
constexpr float kEase = 0.25f;  // the shown swing's step toward the sent one, a command

// An angle in degrees into -180..180.
double wrap(double a) {
  while (a > 180) a -= 360;
  while (a < -180) a += 360;
  return a;
}

void ease(SpeedJumpAid& aid, float view) {
  const float v = view < 0 ? 0 : view > 1 ? 1 : view;
  aid.shown = aid.shown + (v * aid.swing - aid.shown) * kEase;
}

}  // namespace

MoveCmd assisted_command(const MoveCmd& held, const MoveState& s, float share, float view,
                         SpeedJumpAid& aid) {
  const bool hands = held.forward > 0 && held.side == 0;
  const bool shot = (held.buttons & kButtonAttack) != 0;
  if (!hands || shot || s.type != MoveType::normal || s.waterlevel >= 2 || share <= 0) {
    aid.flying = false;
    aid.swing = 0;
    MoveCmd out = held;
    // A shot goes where the screen looks while the shown swing eases back.
    if (shot)
      out.angles.y = static_cast<float>(wrap(static_cast<double>(held.angles.y) + aid.shown));
    ease(aid, view);
    return out;
  }
  MoveCmd out = held;
  if (s.ground != -1) {  // standing: the jump (if held) goes out pressed; the swing kept if hopping
    aid.flying = false;
    if ((held.buttons & kButtonJump) == 0) aid.swing = 0;
    ease(aid, view);
    return out;
  }
  out.buttons = static_cast<uint8_t>(out.buttons & ~kButtonJump);  // flying: let go for the next
  const double vx = s.velocity.x, vy = s.velocity.y;
  const double h = core::libm::pow(vx * vx + vy * vy, 0.5);
  if (h < 1) {
    aid.swing = 0;
    ease(aid, view);
    return out;
  }
  const double aim = held.angles.y;
  const double way = core::libm::atan2(vy, vx) * kDegrees;
  const double off = wrap(way - aim);  // + : the way left of the aim
  if (!aid.flying) {                   // the takeoff: turn back across the aim
    aid.turn = off > 0 ? -1 : off < 0 ? 1 : (aid.turn == 0 ? 1 : -aid.turn);
    aid.flying = true;
  }
  if (aid.turn > 0 && off > kSpeedJumpSwing) aid.turn = -1;
  if (aid.turn < 0 && off < -kSpeedJumpSwing) aid.turn = 1;
  const double k = share > 1 ? 1.0 : static_cast<double>(share);
  double c = kAirCap * core::libm::pow(1 - k, 0.5);
  if (c > h) c = h;
  const double across = core::libm::acos(c / h) * kDegrees;
  const double wish = way + aid.turn * across;
  // Turning left: the side key left (negative), the wish 90 degrees left of the yaw; right: the
  // side key right, the wish 90 degrees right of it.
  const double yaw = wrap(aid.turn > 0 ? wish - 90 : wish + 90);
  out.forward = 0;
  out.side = static_cast<int16_t>(aid.turn > 0 ? -held.forward : held.forward);
  out.angles.y = static_cast<float>(yaw);
  aid.swing = static_cast<float>(wrap(yaw - aim));
  ease(aid, view);
  return out;
}

}  // namespace ge::shooter
