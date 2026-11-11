// Shooter's Edge's quick weapon (quick_fire.hpp).
//
// ge:summary Shooter's Edge's quick weapon: the plugin's quick fire on the game's keys, cvars
// ge:summary and hand
// ge:keywords quick-weapon quick_moves quick_hand quick_armed
// ge:skill input shooter
// ge:test tests/shooters_edge_quick_fire_test.cpp
#include "shooters_edge/client/quick_fire.hpp"

#include "console/cvar.hpp"
#include "shooters_edge/client/controls.hpp"

namespace shooters_edge::client {

namespace sh = ge::shooter;

namespace {

// The ammunition one shot of `w` takes.
int shot_cost(hud::Weapon w) {
  switch (w) {
    case hud::Weapon::melee:
      return 0;
    case hud::Weapon::double_shotgun:
    case hud::Weapon::heavy_nailgun:
      return 2;
    default:
      return 1;
  }
}

int key_of(hud::Weapon w) { return static_cast<int>(w) + 1; }

}  // namespace

void add_quick_cvars(ge::console::Console& con, QuickFire& q) {
  using ge::console::CvarKind;
  using ge::console::kArchive;
  con.cvars().bind({.name = "cl_quickweapon",
                    .fallback = "0",
                    .flags = kArchive,
                    .help = "1: +attack fires the best weapon of cl_quickweapon_prio you carry, "
                            "then switches back to cl_quickweapon_default",
                    .kind = CvarKind::boolean},
                   &q.on);
  con.cvars().bind({.name = "cl_quickweapon_prio",
                    .fallback = "7 8 5 6 3 2 4 1",
                    .flags = kArchive,
                    .help = "the weapons +attack fires with cl_quickweapon 1, best first, by "
                            "weapon key (1 melee .. 8 beam gun)",
                    .kind = CvarKind::text},
                   &q.prio);
  con.cvars().bind({.name = "cl_quickweapon_prio2",
                    .fallback = "8 3 2",
                    .flags = kArchive,
                    .help = "the weapons +attack2 fires, best first, by weapon key (empty: "
                            "+attack2 fires what is in hand)",
                    .kind = CvarKind::text},
                   &q.prio2);
  con.cvars().bind({.name = "cl_quickweapon_default",
                    .fallback = "2 1",
                    .flags = kArchive,
                    .help = "the weapon a quick fire switches back to: the first of these you "
                            "carry with ammunition",
                    .kind = CvarKind::text},
                   &q.fallback);
  con.cvars().bind({.name = "cl_quickweapon_preview",
                    .fallback = "1",
                    .flags = kArchive,
                    .help = "1: with cl_quickweapon 1, the weapon +attack would fire is drawn "
                            "see-through in hand, turning solid as it fires",
                    .kind = CvarKind::boolean},
                   &q.preview);
  con.cvars().bind(
      {.name = "cl_quickweapon_hide",
       .fallback = "0",
       .flags = kArchive,
       .help = "1: with cl_quickweapon 1 and the preview off, no weapon is drawn while "
               "+attack would switch to another",
       .kind = CvarKind::boolean},
      &q.hide);
}

const sh::QuickSettings& quick_settings(QuickFire& q) {
  sh::QuickSettings& s = q.settings;
  s.on = {q.on, true};
  s.hide = q.hide;
  s.preview = q.preview;
  if (q.parsed[0] != q.prio) s.prio[0] = sh::weapon_list(q.parsed[0] = q.prio);
  if (q.parsed[1] != q.prio2) s.prio[1] = sh::weapon_list(q.parsed[1] = q.prio2);
  if (q.parsed[2] != q.fallback) s.fallback = sh::weapon_list(q.parsed[2] = q.fallback);
  return s;
}

sh::QuickArmed quick_armed(const hud::Status& s) {
  sh::QuickArmed a;
  for (int k = 0; k <= hud::kGuns; ++k) {
    const auto w = static_cast<hud::Weapon>(k);
    if (s.has(w) && s.ammo_for(w) >= shot_cost(w)) a.ready |= 1u << key_of(w);
    a.ammo[static_cast<size_t>(key_of(w))] = w == hud::Weapon::melee ? -1 : s.ammo_for(w);
  }
  a.in_hand = key_of(s.weapon);
  a.alive = !s.dead && s.health > 0;
  return a;
}

void quick_moves(QuickFire& q, const hud::Status& s, double t, Moves& m) {
  const sh::QuickKeys keys{.fire = {m.attack, m.attack2},
                           .own_select = m.impulse};  // a weapon key, or the next one
  const sh::QuickCommand c = sh::quick_step(q.state, quick_settings(q), quick_armed(s), keys, t);
  m.attack = c.attack;
  m.attack2 = false;
  if (m.impulse == 0 && c.select != 0) m.impulse = c.select;
}

int quick_show(QuickFire& q, const hud::Status& s, double t) {
  q.look = sh::quick_look(q.state, quick_settings(q), quick_armed(s), t);
  return q.look.ghost ? q.look.weapon - 1 : -1;
}

void quick_hand(const QuickFire& q, view::HandFrame& hand) {
  if (q.look.hide) hand.drawn = false;
  if (!q.look.ghost) return;
  hand.opacity = q.look.opacity;
  if (!q.look.rest) return;
  hand.frame_a = hand.frame_b = 0;  // not the weapon in hand: at rest, it has not fired
  hand.frame_t = 0;
  hand.flashing = hand.fired = hand.arc = false;
}

}  // namespace shooters_edge::client
