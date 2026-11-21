// ge:summary A fighter's footwork: the dodge's side key in a fight, the walk onto an exit node
// ge:keywords footwork fighter_dodge fighter_landing dodge_step dodge_clear landing exit-node
// ge:skill shooterbots bots
// ge:test tests/shooters_edge_footwork_test.cpp
#include "shooterbots/footwork.hpp"

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "core/prng.hpp"
#include "shooterbots/dodge.hpp"

namespace sb = ge::shooterbots;

namespace ge::shooterbots {

namespace {

constexpr double kLandingNear = 12;  // the exit node walked onto until this near it, units
// THE PRESS (`press`): the forward key's draw in a fight, on at him or back off, by whether a
// loaded rocket launcher is in hand (its splash keeps people back). People's e1m2 recordings, an
// enemy faced: across the line 174 units/s, toward him +76 without the launcher and +37 with it,
// the fight's ground keys forward 55 %, back 20 % (se-people `fight.*`, `keys.fight.*`).
constexpr double kPressOn = 0.8, kPressBack = 0.1;
constexpr double kPressOnRl = 0.4, kPressBackRl = 0.3;
// (`presspair`) the odds on and back by (his own launcher, the one faced's): none-none, none-rl,
// rl-none, rl-rl (people's closing 55, 111, 24, 49 units/s)
constexpr double kPairOn[4] = {0.7, 0.95, 0.15, 0.5};
constexpr double kPairBack[4] = {0.1, 0.0, 0.35, 0.2};
constexpr float kPressNear = 96;  // nearer than this, never on at him (the bodies would meet)
// (`fightway`) He is chasing when coming on faster than this, units/s.
constexpr float kStandChasing = 100;
// (`goalfeet`) The route's side key this share of its forward or more: the route runs across the
// line to him (past about 35 degrees off it), and the side key is the route's.
constexpr float kGoalAcross = 0.7f;

}  // namespace

void fighter_dodge(Fighter& f, const ge::shooterbots::BotView& s, int me, double t,
                   ge::ai::Why* why, sh::MoveCmd& cmd) {
  // In a fight, the side key across the line to him (the view is on him, so the side is across):
  // the dodge (shooterbots/dodge.hpp), not on a jump the route proved, a lift, a door, or in water.
  // With `press`, whenever the one aimed at is in sight (a chase that sees him, a roam that has
  // not turned to the fight yet): people's feet are measured with an enemy faced, whatever the
  // mind calls it.
  const bool pressing = f.features[sb::Feature::press];
  f.press_note = "-";
  if (f.bot.aiming) {
    const sb::PilotMode pm = f.bot.pilot.pilot.mode;
    bool seen = false;
    for (const sb::Contact& k : f.bot.senses.contacts)
      if (k.ent == f.bot.target && k.visible) seen = true;
    f.press_note =
        f.unlocking || f.lost_to >= 0 || f.escort.leader >= 0       ? "errand"
        : s.world.entities[static_cast<size_t>(me)].waterlevel >= 2 ? "wet"
        : !seen && !pressing                                        ? "unseen"
        : !(pm == sb::PilotMode::walk || pm == sb::PilotMode::strafe || pm == sb::PilotMode::hop ||
            pm == sb::PilotMode::idle || pm == sb::PilotMode::arrived)
            ? "feet"
        : f.mode == FightMode::retreat ? "retreat"
                                       : "route";
  }
  if (f.bot.aiming && !f.unlocking && f.lost_to < 0 && f.escort.leader < 0 &&
      (pressing || f.mode == FightMode::fight || f.mode == FightMode::retreat) &&
      s.world.entities[static_cast<size_t>(me)].waterlevel < 2) {
    const sb::PilotMode pm = f.bot.pilot.pilot.mode;
    // (`aimstrafe`) In a hop's air the side key is the aim's turn's (shooter/bot.cpp): the dodge
    // never presses the other one, a strafe against the turn.
    const bool hop_air = f.features[sb::Feature::aimstrafe] && pm == sb::PilotMode::hop &&
                         (s.world.entities[static_cast<size_t>(me)].flags & sh::kFlagOnGround) == 0;
    const bool free_feet = !hop_air && (pm == sb::PilotMode::walk || pm == sb::PilotMode::strafe ||
                                        pm == sb::PilotMode::hop || pm == sb::PilotMode::idle ||
                                        pm == sb::PilotMode::arrived);
    bool in_sight = false;
    for (const sb::Contact& k : f.bot.senses.contacts)
      if (k.ent == f.bot.target && k.visible) in_sight = true;
    // With `press` the feet go on while the aim holds where he was (a peek, a corner): people's
    // do not stop for an enemy out of sight a moment.
    if (free_feet && (in_sight || pressing)) {
      // (`goalfeet`) The route's keys under the aimed view, before the dodge has them.
      const int16_t route_f = cmd.forward, route_s = cmd.side;
      const bool routing =
          pm == sb::PilotMode::walk || pm == sb::PilotMode::strafe || pm == sb::PilotMode::hop;
      const sh::Vec3f me_at = s.world.entities[static_cast<size_t>(me)].origin;
      const float yaw = f.bot.yaw;
      const int was = f.dodge.dir;
      const int dir =
          sb::dodge_step(f.dodge, f.dodge_k, t, sb::dodge_clear(s.sv, me_at, yaw, -1, me),
                         sb::dodge_clear(s.sv, me_at, yaw, 1, me));
      if (dir != was && was != 0 && dir != 0) f.stats.dodge_turns++;
      if (dir != 0) {
        cmd.side = sh::quantize_axis(static_cast<float>(dir) * 320 * f.dodge_k.press);
        f.stats.dodging++;
      }
      // The press: the forward key drawn for a stretch with the side (`press`), in a fight only.
      int32_t press = 0;
      if (pressing && f.mode != FightMode::retreat && !f.item_first) {
        if (t >= f.press_until) {
          const Stats st = read_stats(s, me, *f.arsenal);
          const bool rl = st.weapon != 0 && st.weapon == f.arsenal->launcher &&
                          ammo_of(st, f.arsenal->launcher_ammo) > 0;
          const double u = ge::core::next(f.press_rng);
          double on = rl ? kPressOnRl : kPressOn, back = rl ? kPressBackRl : kPressBack;
          // (`presspair`) By both launchers, his believed: people close in hardest on a launcher
          // without one (111 units/s), keep their range with one against none (24).
          if (f.features[sb::Feature::presspair]) {
            const bool his = foe_has_key(f, f.bot.target, t);
            const int k = (rl ? 2 : 0) + (his ? 1 : 0);
            on = kPairOn[k];
            back = kPairBack[k];
          }
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
        if (press != 0 && !sb::dodge_clear(s.sv, me_at, yaw + 90, press, me)) {
          press = 0;
          f.stats.press_blocked++;
          f.press_note = "blocked";
        }
        if (press != 0) cmd.forward = sh::quantize_axis(static_cast<float>(press) * 320);
        if (press != 0) f.press_note = press > 0 ? "on" : "back";
        f.stats.press_on += press > 0;
        f.stats.press_back += press < 0;
      }
      // (`fightway`, M18r.md 0w) A person walks backwards only when chased, firing at the
      // chaser: in a fight with him in sight and not coming on, the route's back key is let go
      // and he stands and dodges (the side key alone) until the fight is over.
      if (f.features[sb::Feature::fightway] && f.mode == FightMode::fight && in_sight &&
          !f.item_first && press >= 0 && cmd.forward < 0) {
        float coming = 0;
        for (const sb::Contact& k : f.bot.senses.contacts) {
          if (k.ent != f.bot.target) continue;
          const float dx = me_at.x - static_cast<float>(k.seen.x);
          const float dy = me_at.y - static_cast<float>(k.seen.y);
          const float d = std::sqrt(dx * dx + dy * dy);
          if (d > 1) coming = (k.course.x * dx + k.course.y * dy) / d;
        }
        if (coming < kStandChasing) {
          cmd.forward = 0;
          f.stats.stood++;
          f.press_note = "stand";
        }
      }
      // (`goalfeet`, M18r.md 6f) THE FIGHT ON THE WAY: the aim on him, the feet still tend along
      // the route to the goal. Across the line to him the side key is the route's and the jinks are
      // the press's (toward and away); along it toward him the forward key is the route's and the
      // jinks the dodge's side; a goal behind the line keeps the fight's feet (no back key unless
      // chased), counted. (Out of sight the bot does not aim, and the route has the feet already.)
      int goal_feet = 0;  // 0 the fight's feet, 1 across, 2 ahead, 3 behind
      if (f.features[sb::Feature::goalfeet] && f.mode != FightMode::retreat && routing &&
          (route_f != 0 || route_s != 0)) {
        const float af = std::fabs(static_cast<float>(route_f));
        const float as = std::fabs(static_cast<float>(route_s));
        if (as >= af * kGoalAcross) {
          goal_feet = 1;
          cmd.side = route_s;
          if (press == 0)
            cmd.forward = f.features[sb::Feature::fightway] && route_f < 0 ? int16_t{0} : route_f;
          f.stats.goal_across++;
          f.press_note = "goal-across";
        } else if (route_f > 0) {
          goal_feet = 2;
          cmd.forward = route_f;
          f.stats.goal_ahead++;
          f.press_note = "goal-ahead";
        } else {
          goal_feet = 3;
          f.stats.goal_behind++;
        }
      }
      if (why) {
        why->decide("dodge");
        why->fact("goal_feet", goal_feet);
        why->fact("dir", dir);
        why->fact("press", press);
        why->choose(dir < 0 ? "left" : dir > 0 ? "right" : "straight", "across his line");
      }
    }
  }
}

bool fighter_landing(Fighter& f, const ge::shooterbots::BotView& s, int me, double t,
                     sh::MoveCmd& cmd) {
  if (f.landing < 0 || t >= f.landing_until ||
      s.world.entities[static_cast<size_t>(me)].waterlevel != 0)
    return false;
  // Just climbed out: straight onto the exit node, the edge of the water behind it, before the
  // route from the nearest node (which may stand in the water) takes over.
  // Done as soon as the route's own leg starts dry, on the ground: he is up already, and the
  // exit node may lie behind him down the bank (base1's river: walked back onto it, he slid off
  // the slope into the water again).
  const sb::NavGraph& g = *f.bot.pilot.pilot.g;
  const sb::Pilot& p = f.bot.pilot.pilot;
  const bool route_dry = p.k < p.route.size() && p.route[p.k] >= 0 &&
                         !g.nodes[static_cast<size_t>(p.route[p.k])].wet &&
                         (s.world.entities[static_cast<size_t>(me)].flags & sh::kFlagOnGround) != 0;
  const sh::Vec3f to = g.nodes[static_cast<size_t>(f.landing)].origin;
  const sh::Vec3f me_at = s.world.entities[static_cast<size_t>(me)].origin;
  const double dx = to.x - me_at.x, dy = to.y - me_at.y;
  if (route_dry || dx * dx + dy * dy < kLandingNear * kLandingNear) {
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
  const sb::PilotMode mode = f.bot.pilot.pilot.mode;
  const bool proven = mode == sb::PilotMode::run_up || mode == sb::PilotMode::jump ||
                      mode == sb::PilotMode::air || mode == sb::PilotMode::grenade;
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

}  // namespace ge::shooterbots
