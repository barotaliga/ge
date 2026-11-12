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

// The jump asked for: the jump button, or the up move a sequel's jump is.
bool jumps(const MoveCmd& c) { return (c.buttons & kButtonJump) != 0 || c.up >= 10; }

// The velocity's share along the wish that gains `share` of the best gain (the header's THE GAIN):
// `wish` the wish speed sent, `seconds` the command's (0, not yet known: taken as long enough that
// the cap binds, as it does at any rate a classic client sends).
double along(const SpeedJumpRules& r, double wish, double seconds, double share) {
  if (wish > r.max_wish) wish = r.max_wish;
  const double cap = r.cap > 0 && wish > r.cap ? double(r.cap) : wish;
  const double most = double(r.accelerate) * wish * seconds;                      // A
  if (seconds <= 0 || most >= cap) return cap * core::libm::pow(1 - share, 0.5);  // c* = 0
  const double best = cap - most;                                                 // c*
  return core::libm::pow(cap * cap - share * (cap * cap - best * best), 0.5);
}

// A slow air: a command's most along the wish (A) falls short of the air's cap, so a hop turns
// little at speed (the sequel's default air; never the classic move at a client's rates).
bool slow_air(const SpeedJumpRules& r, const MoveCmd& held) {
  double wish = held.forward;
  if (wish > r.max_wish) wish = r.max_wish;
  const double cap = r.cap > 0 && wish > r.cap ? double(r.cap) : wish;
  return held.msec > 0 && double(r.accelerate) * wish * (held.msec * 0.001) < cap;
}

}  // namespace

MoveCmd assisted_command(const MoveCmd& held, const MoveState& s, float share, float view,
                         SpeedJumpAid& aid, const SpeedJumpRules& rules) {
  const bool hands = held.forward > 0 && held.side == 0;
  const bool shot = (held.buttons & kButtonAttack) != 0;
  if (!hands || shot || s.type != MoveType::normal || s.waterlevel >= 2 || share <= 0) {
    aid.flying = false;
    aid.steering = false;
    aid.swing = 0;
    aid.sweep = 0;
    MoveCmd out = held;
    // A shot goes where the screen looks while the shown swing eases back.
    if (shot)
      out.angles.y = static_cast<float>(wrap(static_cast<double>(held.angles.y) + aid.shown));
    ease(aid, view);
    return out;
  }
  MoveCmd out = held;
  if (s.ground != -1) {  // standing: the jump (if held) goes out pressed; the swing kept if hopping
    if (aid.flying) aid.sweep = aid.turned;
    aid.flying = false;
    if (!jumps(held)) aid.swing = 0;
    // A slow air (`slow_air`: its hops turn little at speed) cannot follow a far-off aim: on the
    // ground the jump is held back while the way is more than kSpeedJumpSteer off the aim (and
    // until it is back within the swing), so the walk turns him there as a player lands and turns.
    const bool slow = slow_air(rules, held);
    const double vx = s.velocity.x, vy = s.velocity.y;
    if (slow && jumps(held) && vx * vx + vy * vy >= 1) {
      const double off = wrap(core::libm::atan2(vy, vx) * kDegrees - held.angles.y);
      const double far = off < 0 ? -off : off;
      aid.steering = far > (aid.steering ? kSpeedJumpSwing : kSpeedJumpSteer);
    } else {
      aid.steering = false;
    }
    if (aid.steering) {
      out.buttons = static_cast<uint8_t>(out.buttons & ~kButtonJump);
      if (out.up > 0) out.up = 0;
      aid.swing = 0;
    }
    ease(aid, view);
    return out;
  }
  aid.steering = false;
  out.buttons = static_cast<uint8_t>(out.buttons & ~kButtonJump);  // flying: let go for the next
  if (out.up > 0) out.up = 0;  // (the sequel's jump; the classic air move reads no up move)
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
    // A slow air (`slow_air`, the sequel's default) turns a flight less than the swing and leaves
    // the way near the aim after a hop back to it: from there the hop goes on to the side the last
    // takeoff turned to, so the hops weave to both sides of the aim, not to one alone.
    const bool slow = slow_air(rules, held) && aid.sweep > 0;
    if (slow && (off < 0 ? -off : off) < aid.sweep / 2 && aid.side != 0)
      aid.turn = aid.side;
    else
      aid.turn = off > 0 ? -1 : off < 0 ? 1 : (aid.turn == 0 ? 1 : -aid.turn);
    aid.side = aid.turn;
    aid.turned = 0;
    aid.flying = true;
  } else {
    const double d = wrap(way - aid.last_way);
    aid.turned += static_cast<float>(d < 0 ? -d : d);
  }
  aid.last_way = static_cast<float>(way);
  if (aid.turn > 0 && off > kSpeedJumpSwing) aid.turn = -1;
  if (aid.turn < 0 && off < -kSpeedJumpSwing) aid.turn = 1;
  const double k = share > 1 ? 1.0 : static_cast<double>(share);
  double c = along(rules, held.forward, held.msec * 0.001, k);
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
