// ge:summary A fighter's footwork: the dodge's side key in a fight, the walk onto an exit node
// ge:keywords footwork fighter_dodge fighter_landing dodge_step dodge_clear landing exit-node
// ge:skill bots
// ge:test tests/shooters_edge_fight_test.cpp
#include "shooters_edge/bots/footwork.hpp"

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "shooter/dodge.hpp"

namespace shooters_edge::bots {

namespace {

constexpr double kLandingNear = 12;  // the exit node walked onto until this near it, units

}  // namespace

void fighter_dodge(Fighter& f, const server::RulesCommon& s, int me, double t, ge::ai::Why* why,
                   sh::MoveCmd& cmd) {
  // In a fight, the side key across the line to him (the view is on him, so the side is across):
  // the dodge (shooter/dodge.hpp), not on a jump the route proved, a lift, a door, or in water.
  if (f.bot.aiming && !f.unlocking && f.lost_to < 0 && f.escort.leader < 0 &&
      (f.mode == FightMode::fight || f.mode == FightMode::retreat) &&
      s.world.entities[static_cast<size_t>(me)].waterlevel < 2) {
    const sh::PilotMode pm = f.bot.pilot.pilot.mode;
    const bool free_feet = pm == sh::PilotMode::walk || pm == sh::PilotMode::strafe ||
                           pm == sh::PilotMode::hop || pm == sh::PilotMode::idle ||
                           pm == sh::PilotMode::arrived;
    bool in_sight = false;
    for (const sh::Contact& k : f.bot.senses.contacts)
      if (k.ent == f.bot.target && k.visible) in_sight = true;
    if (free_feet && in_sight) {
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
      if (why) {
        why->decide("dodge");
        why->fact("dir", dir);
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

}  // namespace shooters_edge::bots
