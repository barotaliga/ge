// A fighter's keyboard (keyboard.hpp): the bindings and times by skill, the wish as a key set, the
// command built from the keys held.
//
// ge:summary A fighter's keyboard: bindings by skill, the key set, the command from the keys
// ge:keywords keyboard keys key-set bindings weapon-key tap fighter_keyboard
// ge:skill bots
// ge:test tests/shooters_edge_footwork_test.cpp
#include "shooters_edge/bots/keyboard.hpp"

#include <cmath>

#include "ai/keys.hpp"
#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "shooter/traits.hpp"
#include "shooter/usercmd.hpp"

namespace shooters_edge::bots {

namespace {

namespace ai = ge::ai;

// The actions: the four move keys, the jump, the weapons 1 to 8 (impulse = action - kWeapon1 + 1).
enum Act : uint8_t { kFwd, kBack, kLeft, kRight, kJump, kWeapon1 };
constexpr float kPressMin = 0.3f;  // in the air, a wish under this share of a run: no key
constexpr float kCreep = 0.03f;    // on the ground, a wish under this share: no key
constexpr double kHold = 4;        // the set held until the wish is this far past its edge, deg
constexpr float kKeySpeed = 400;   // a key's axis, units/s (cl_forwardspeed)
constexpr double kFull = 0.9;      // a wish of this share of a run or more is held, not feathered
constexpr double kBand = 12;       // feathering's band round the speed wished, units/s

uint32_t bit(int a) { return 1u << static_cast<uint32_t>(a); }

ai::KeyLayout layout_of(sh::AimSkill tier) {
  ai::KeyLayout l;
  l.bind[kFwd] = ai::Key::w;
  l.bind[kBack] = ai::Key::s;
  l.bind[kLeft] = ai::Key::a;
  l.bind[kRight] = ai::Key::d;
  l.bind[kJump] = ai::Key::space;
  l.home = {ai::Key::shift, ai::Key::a, ai::Key::w, ai::Key::d, ai::Key::space};
  constexpr ai::Key kRow[8] = {ai::Key::n1, ai::Key::n2, ai::Key::n3, ai::Key::n4,
                               ai::Key::n5, ai::Key::n6, ai::Key::n7, ai::Key::n8};
  for (size_t w = 0; w < 8; w++) l.bind[kWeapon1 + w] = kRow[w];
  if (tier == sh::AimSkill::novice) return l;
  // The nailguns, the launchers and the arc on the keys round the hand: 5 F, 6 R, 7 Q, 8 E.
  l.bind[kWeapon1 + 4] = ai::Key::f;
  l.bind[kWeapon1 + 5] = ai::Key::r;
  l.bind[kWeapon1 + 6] = ai::Key::q;
  l.bind[kWeapon1 + 7] = ai::Key::e;
  if (tier == sh::AimSkill::average) return l;
  l.bind[kJump] = ai::Key::mouse2;
  l.bind[kWeapon1 + 2] = ai::Key::space;
  return l;
}

ai::KeyTiming timing_of(double moves) {
  ai::KeyTiming t;
  t.press_lo = sh::scale(moves, {0.025, 0.015, 0.01, 0.008});
  t.press_hi = sh::scale(moves, {0.07, 0.045, 0.03, 0.025});
  t.release_lo = sh::scale(moves, {0.02, 0.012, 0.01, 0.008});
  t.release_hi = sh::scale(moves, {0.06, 0.04, 0.03, 0.025});
  t.long_p = sh::scale(moves, {0.05, 0.03, 0.02, 0.015});
  t.travel_s = sh::scale(moves, {0.08, 0.06, 0.05, 0.045});
  t.tap_s = sh::scale(moves, {0.09, 0.07, 0.06, 0.055});
  t.shift_s = sh::scale(moves, {0.25, 0.18, 0.15, 0.13});
  return t;
}

double wrap(double deg) {
  while (deg > 180) deg -= 360;
  while (deg < -180) deg += 360;
  return deg;
}

}  // namespace

void fighter_keyboard_init(Fighter& f, uint8_t moves, uint32_t seed) {
  f.keys.board =
      ai::Keyboard(layout_of(sh::tier_of(moves)), timing_of(moves), seed * 2654435789u + 4242u);
  f.keys.set = -1;
}

void fighter_keyboard(Fighter& f, sh::MoveCmd& cmd, sh::Vec3f velocity, bool onground) {
  // The wish's key set: 0 forward, clockwise by 45 degrees (side positive is right).
  const double fw = cmd.forward, sd = cmd.side;
  const double m = std::sqrt(fw * fw + sd * sd) / 320.0;
  int set = -1;
  if (m >= (onground ? kCreep : kPressMin)) {
    double ang = ge::core::libm::atan2(sd, fw) * (180.0 / ge::core::kPi);
    if (ang < 0) ang += 360;
    set = static_cast<int>(std::floor(ang / 45 + 0.5)) % 8;
    if (f.keys.set >= 0 && std::fabs(wrap(ang - f.keys.set * 45.0)) <= 22.5 + kHold)
      set = f.keys.set;
  }
  f.keys.set = set;
  // Feathering a wish short of a run on the ground: on under the speed wished, off over it.
  if (set >= 0 && onground && m < kFull) {
    const double yaw = cmd.angles.y * (ge::core::kPi / 180.0);
    const double cy = ge::core::libm::cos(yaw), sy = ge::core::libm::sin(yaw);
    // The wish in the world: forward along the yaw, right a quarter turn clockwise of it.
    const double wx = fw * cy + sd * sy, wy = fw * sy - sd * cy;
    const double along = (velocity.x * wx + velocity.y * wy) / (m * 320.0);
    const double wished = m * 320.0;
    if (along < wished - kBand) f.keys.feather = true;
    if (along > wished + kBand) f.keys.feather = false;
    if (!f.keys.feather) set = -1;
  } else {
    f.keys.feather = true;
  }
  uint32_t want = 0;
  if (set == 7 || set == 0 || set == 1) want |= bit(kFwd);
  if (set >= 1 && set <= 3) want |= bit(kRight);
  if (set >= 3 && set <= 5) want |= bit(kBack);
  if (set >= 5 && set <= 7) want |= bit(kLeft);
  if (cmd.buttons & sh::kButtonJump) want |= bit(kJump);
  uint32_t tap = 0;
  if (cmd.impulse >= 1 && cmd.impulse <= 8) tap = bit(kWeapon1 + cmd.impulse - 1);
  uint32_t tapped = 0;
  const uint32_t held = f.keys.board.step(cmd.msec / 1000.0, want, tap, &tapped);
  const auto axis = [&](int plus, int minus) {
    return sh::quantize_axis(
        kKeySpeed * static_cast<float>(((held & bit(plus)) != 0) - ((held & bit(minus)) != 0)));
  };
  cmd.forward = axis(kFwd, kBack);
  cmd.side = axis(kRight, kLeft);
  cmd.buttons = static_cast<uint8_t>((cmd.buttons & ~sh::kButtonJump) |
                                     ((held & bit(kJump)) ? sh::kButtonJump : 0));
  cmd.impulse = 0;
  for (int w = 0; w < 8; w++)
    if (tapped & bit(kWeapon1 + w)) cmd.impulse = static_cast<uint8_t>(w + 1);
}

}  // namespace shooters_edge::bots
