// THE QUICK WEAPON IN SHOOTER'S EDGE: the shooter plugin's quick fire (shooter/quick_weapon.hpp)
// wired to the game's keys, cvars, HUD status and hand, in its own match and on a server joined.
//
// - The cvars (`add_quick_cvars`): `cl_quickweapon 0|1` (+attack is a quick fire; off by default,
//   so +attack fires what is in hand), `cl_quickweapon_prio "7 8 5 6 3 2 4 1"` (+attack's list,
//   best first, by weapon key), `cl_quickweapon_prio2 "8 3 2"` (+attack2's: the hitscan guns),
//   `cl_quickweapon_default "2 1"` (the weapon put back), `cl_quickweapon_preview 1` (the weapon
//   +attack would fire drawn see-through in hand) and `cl_quickweapon_hide 0|1` (no weapon drawn
//   instead, with the preview off). +attack2 is bound to no key: `bind mouse2 +attack2`.
// - `quick_moves` turns this frame's +attack, +attack2 and weapon key into the attack and the
//   `impulse` (the weapon's key) the command carries.
// - THE PREVIEW keeps the weapon +attack would fire in the hand through a tap: `quick_show` names
//   it for the hand to hold before the hand steps (so the hand never switches to the kept weapon
//   and back: the ghost turns solid, fires, and fades back), and `quick_hand` lays the look on
//   the frame the hand made: its opacity, at rest while it is not the weapon in hand, or hidden.
// - The weapons by key: 1 the melee weapon .. 8 the beam gun (the HUD's order); one ready is
//   carried with the ammunition for a shot (two for the double shotgun and the heavy nailgun).
//
// ge:summary Shooter's Edge's quick weapon: the plugin's quick fire on the game's keys, cvars
// ge:summary and hand
// ge:keywords quick-weapon quick_fire add_quick_cvars quick_moves quick_hand quick_armed attack2
// ge:keywords cl_quickweapon cl_quickweapon_prio cl_quickweapon_prio2 cl_quickweapon_default
// ge:keywords cl_quickweapon_hide cl_quickweapon_preview weapon-preview
// ge:skill input shooter
// ge:test tests/shooters_edge_quick_fire_test.cpp
#pragma once

#include <array>
#include <string>

#include "console/console.hpp"
#include "shooter/quick_weapon.hpp"
#include "shooters_edge/hud/status.hpp"
#include "shooters_edge/view/hand.hpp"

namespace shooters_edge::client {

struct Moves;

struct QuickFire {
  // The cvars as set.
  bool on = false;
  std::string prio = "7 8 5 6 3 2 4 1", prio2 = "8 3 2", fallback = "2 1";
  bool hide = false, preview = true;
  // Parsed from them (`quick_settings`), and the lists last parsed.
  ge::shooter::QuickSettings settings;
  std::array<std::string, 3> parsed{"\x01", "\x01", "\x01"};
  ge::shooter::QuickState state;
  ge::shooter::QuickLook look;  // `quick_show`'s, for `quick_hand`
};

// The cvars over `q` (`q` must outlive `con`).
void add_quick_cvars(ge::console::Console& con, QuickFire& q);
// The settings the cvars say (the lists parsed again only when changed).
const ge::shooter::QuickSettings& quick_settings(QuickFire& q);
// The player's weapons as the plugin reads them.
ge::shooter::QuickArmed quick_armed(const hud::Status& s);
// `m`'s attack keys and weapon key made into the command's attack and impulse at `t` s.
void quick_moves(QuickFire& q, const hud::Status& s, double t, Moves& m);
// Once a drawn frame at `t` s, before the hand steps: the look worked out, and the weapon the hand
// holds in place of the game's (its `view::Weapon` index; -1 the game's own).
int quick_show(QuickFire& q, const hud::Status& s, double t);
// The look laid on the hand's frame: hidden, or the shown weapon's opacity (at rest when it is
// not the weapon in hand).
void quick_hand(const QuickFire& q, view::HandFrame& hand);

}  // namespace shooters_edge::client
