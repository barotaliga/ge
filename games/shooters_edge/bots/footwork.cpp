// ge:summary A fighter's footwork: the dodge's side key in a fight, the walk onto an exit node
// ge:keywords footwork fighter_dodge fighter_landing dodge_step dodge_clear landing exit-node
// ge:skill bots
// ge:test tests/shooters_edge_footwork_test.cpp
#include "shooters_edge/bots/footwork.hpp"

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "core/prng.hpp"
#include "shooter/dodge.hpp"

namespace shooters_edge::bots {

namespace {

constexpr double kLandingNear = 12;  // the exit node walked onto until this near it, units
// THE PRESS (`press`): the forward key's draw in a fight, on at him or back off, by whether a
// loaded rocket launcher is in hand (its splash keeps people back). People's e1m2 recordings, an
// enemy faced: across the line 174 units/s, toward him +76 without the launcher and +37 with it,
// the fight's ground keys forward 55 %, back 20 % (se-people `fight.*`, `keys.fight.*`).
constexpr double kPressOn = 0.8, kPressBack = 0.1;
constexpr double kPressOnRl = 0.4, kPressBackRl = 0.3;
constexpr float kPressNear = 96;   // nearer than this, never on at him (the bodies would meet)

}  // namespace

void fighter_dodge(Fighter& f, const server::RulesCommon& s, int me, double t, ge::ai::Why* why,
                   sh::MoveCmd& cmd) {
  // In a fight, the side key across the line to him (the view is on him, so the side is across):
  // the dodge (shooter/dodge.hpp), not on a jump the route proved, a lift, a door, or in water.
  // With `press`, whenever the one aimed at is in sight (a chase that sees him, a roam that has
  // not turned to the fight yet): people's feet are measured with an enemy faced, whatever the
  // mind calls it.
  const bool pressing = f.features[sh::Feature::press];
  f.press_note = "-";
  if (f.bot.aiming) {
    const sh::PilotMode pm = f.bot.pilot.pilot.mode;
    bool seen = false;
    for (const sh::Contact& k : f.bot.senses.contacts)
      if (k.ent == f.bot.target && k.visible) seen = true;
    f.press_note =
        f.unlocking || f.lost_to >= 0 || f.escort.leader >= 0       ? "errand"
        : s.world.entities[static_cast<size_t>(me)].waterlevel >= 2 ? "wet"
        : !seen && !pressing                                        ? "unseen"
        : !(pm == sh::PilotMode::walk || pm == sh::PilotMode::strafe || pm == sh::PilotMode::hop ||
            pm == sh::PilotMode::idle || pm == sh::PilotMode::arrived)
            ? "feet"
        : f.mode == FightMode::retreat ? "retreat"
                                       : "route";
  }
  if (f.bot.aiming && !f.unlocking && f.lost_to < 0 && f.escort.leader < 0 &&
      (pressing || f.mode == FightMode::fight || f.mode == FightMode::retreat) &&
      s.world.entities[static_cast<size_t>(me)].waterlevel < 2) {
    const sh::PilotMode pm = f.bot.pilot.pilot.mode;
    const bool free_feet = pm == sh::PilotMode::walk || pm == sh::PilotMode::strafe ||
                           pm == sh::PilotMode::hop || pm == sh::PilotMode::idle ||
                           pm == sh::PilotMode::arrived;
    bool in_sight = false;
    for (const sh::Contact& k : f.bot.senses.contacts)
      if (k.ent == f.bot.target && k.visible) in_sight = true;
    // With `press` the feet go on while the aim holds where he was (a peek, a corner): people's
    // do not stop for an enemy out of sight a moment.
    if (free_feet && (in_sight || pressing)) {
      const sh::Vec3f me_at = s.world.entities[static_cast<size_t>(me)].origin;
      const float yaw = f.bot.yaw;
      const int was = f.dodge.dir;
      const int dir =
          sh::dodge_step(f.dodge, f.dodge_k, t, sh::dodge_clear(s.sv, me_at, yaw, -1, me),
                         sh::dodge_clear(s.sv, me_at, yaw, 1, me));
      if (dir != was && was != 0 && dir != 0) f.stats.dodge_turns++;
      if (dir != 0) {
        cmd.side = sh::quantize_axis(static_cast<float>(dir) * 320 * f.dodge_k.press);
        f.stats.dodging++;
      }
      // The press: the forward key drawn for a stretch with the side (`press`), in a fight only.
      int32_t press = 0;
      if (pressing && f.mode != FightMode::retreat) {
        if (t >= f.press_until) {
          const Stats st = read_stats(s, me, *f.arsenal);
          const bool rl = st.weapon != 0 && st.weapon == f.arsenal->launcher &&
                          ammo_of(st, f.arsenal->launcher_ammo) > 0;
          const double u = ge::core::next(f.press_rng);
          const double on = rl ? kPressOnRl : kPressOn, back = rl ? kPressBackRl : kPressBack;
          f.press = u < on ? 1 : u < on + back ? -1 : 0;
          f.press_until = t + f.dodge_k.hold_s * ge::core::range(f.press_rng, 0.5, 1.5);
        }
        press = f.press;
        f.stats.press_weighed++;
        if (press > 0 && f.bot.aim_dist < kPressNear) {
          press = 0;
          f.stats.press_near++;
          f.press_note = "near";
        }
        // Forward is the right of a view a quarter turn left of it: the dodge's own check.
        if (press != 0 && !sh::dodge_clear(s.sv, me_at, yaw + 90, press, me)) {
          press = 0;
          f.stats.press_blocked++;
          f.press_note = "blocked";
        }
        if (press != 0) cmd.forward = sh::quantize_axis(static_cast<float>(press) * 320);
        if (press != 0) f.press_note = press > 0 ? "on" : "back";
        f.stats.press_on += press > 0;
        f.stats.press_back += press < 0;
      }
      if (why) {
        why->decide("dodge");
        why->fact("dir", dir);
        why->fact("press", press);
        why->choose(dir < 0 ? "left" : dir > 0 ? "right" : "straight", "across his line");
      }
    }
  }
}

bool fighter_landing(Fighter& f, const server::RulesCommon& s, int me, double t, sh::MoveCmd& cmd) {
  if (f.landing < 0 || t >= f.landing_until ||
      s.world.entities[static_cast<size_t>(me)].waterlevel != 0)
    return false;
  // Just climbed out: straight onto the exit node, the edge of the water behind it, before the
  // route from the nearest node (which may stand in the water) takes over.
  const sh::Vec3f to = f.bot.pilot.pilot.g->nodes[static_cast<size_t>(f.landing)].origin;
  const sh::Vec3f me_at = s.world.entities[static_cast<size_t>(me)].origin;
  const double dx = to.x - me_at.x, dy = to.y - me_at.y;
  if (dx * dx + dy * dy < kLandingNear * kLandingNear) {
    f.landing = -1;
  } else {
    const double rel = ge::core::libm::atan2(dy, dx) - cmd.angles.y * (ge::core::kPi / 180.0);
    cmd.forward = sh::quantize_axis(static_cast<float>(ge::core::libm::cos(rel) * 320));
    cmd.side = sh::quantize_axis(static_cast<float>(-ge::core::libm::sin(rel) * 320));
    cmd.buttons = static_cast<uint8_t>(cmd.buttons & ~sh::kButtonJump);
    f.stats.landings++;
  }
  return true;
}

void fighter_keys(Fighter& f, sh::MoveCmd& cmd) {
  const int8_t sign = static_cast<int8_t>((cmd.side > 0) - (cmd.side < 0));
  const sh::PilotMode mode = f.bot.pilot.pilot.mode;
  const bool proven = mode == sh::PilotMode::run_up || mode == sh::PilotMode::jump ||
                      mode == sh::PilotMode::air || mode == sh::PilotMode::grenade;
  const bool turned = !proven && sign != 0 && f.side_was == -sign;
  if (turned) {
    // Turned round this command: the old key let go, the other pressed 1 to 3 commands later,
    // unless this change is a clean one.
    const bool clean = static_cast<float>(ge::core::next(f.keys_rng)) < f.clean_keys;
    f.side_gap =
        clean ? 0 : static_cast<int8_t>(1 + static_cast<int>(ge::core::next(f.keys_rng) * 3));
  } else if (proven || sign != f.side_was) {
    f.side_gap = 0;  // a proven jump's keys, or one let go or pressed from none: no change
  }
  f.side_was = sign;
  if (f.side_gap > 0) {
    f.side_gap--;
    cmd.side = 0;
  }
}

}  // namespace shooters_edge::bots
