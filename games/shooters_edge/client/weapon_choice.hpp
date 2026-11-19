// WEAPON CHOICE: the classic configs' weapon keys, `weapon <n> [n ...]`, preselection and hiding
// (porting/shooters-edge/weapon-choice.md holds the facts).
//
// - `weapon <n> [n ...]` remembers its list (at most 10, best first) and asks for the first weapon
//   of it the player can fire: carried, with a shot's ammo (the super shotgun and the super
//   nailgun two), the melee weapon always. None ready: the list's first number as typed (the
//   server says why not). `weapon 10` and `weapon 12` step to the next ready weapon up or down
//   from the one chosen, wrapping. `impulse <n> <m> ...` with several numbers chooses the same way
//   at once, whatever the preselection.
// - `cl_weaponpreselect`: 0 the choice is asked for at once; 1 only remembered, asked for with
//   the next attack press (in the same command as the attack); 2 like 1, but at once while the
//   attack is held; 3 and 4 are 1 and 2 in deathmatch mode 1 and 0 otherwise. With any of 1 to 4
//   every attack press asks again for the list's best, so an empty weapon gives way to the next.
// - `cl_weaponhide`: 1 releasing the attack asks for the shotgun (the melee weapon without one or
//   its shells; always the melee weapon with `cl_weaponhide_axe 1`); 2 only in deathmatch mode 1.
// - `cl_weaponforgetorder 1`: each choice rewrites the list to the weapon chosen, the shotgun, the
//   melee weapon (the shotgun left out when it was the one chosen, or with `cl_weaponhide_axe 1`).
// - The list starts as the shotgun then the melee weapon.
//
// The classic layer's: both classic games read their weapon keys the same way (each game's own
// numbers through the HUD's status, `quick_armed`).
//
// ge:summary The weapon command, preselection on the attack press and hiding on its release
// ge:keywords weapon-choice weapon-command cl_weaponpreselect cl_weaponhide cl_weaponhide_axe
// ge:keywords cl_weaponforgetorder preselect weapon-hide impulse-list choice_moves
// ge:skill input shooter
// ge:test tests/shooters_edge_weapon_choice_test.cpp
#pragma once

#include <array>
#include <functional>

#include "console/console.hpp"
#include "shooter/controls.hpp"
#include "shooters_edge/client/demo.hpp"
#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/hud/status.hpp"

namespace shooters_edge::client {

inline constexpr int kChoiceList = 10;  // numbers a weapon list keeps

struct WeaponChoice {
  // The cvars.
  int preselect = 0;  // cl_weaponpreselect 0..4
  int hide = 0;       // cl_weaponhide 0..2
  bool hide_axe = false, forget = false;
  // The remembered list, best first (0 ends it).
  std::array<int, kChoiceList> list{2, 1};
  int last = 2;  // the weapon last chosen: where `weapon 10` and `weapon 12` step from
  // A command this frame: `weapon` (preselection applies) or a list `impulse` (at once).
  bool asked = false, asked_now = false;
  bool was_attack = false;
  // The server's deathmatch mode (modes 3 and 4, hide 2); null reads 0.
  std::function<int()> deathmatch;
};

// The cvars and the `weapon` command over `w` (`w` must outlive `con`), in the classic layer.
void add_choice_cvars(ge::console::Console& con, WeaponChoice& w);
// `impulse <n> <m> ...`: the list remembered and chosen at once (the next `choice_moves`).
void choose_list(WeaponChoice& w, const ge::console::Args& a);
// The first ready weapon of the list from `s` (the list's first number when none is ready).
int best_of(const WeaponChoice& w, const hud::Status& s);
// This frame's weapon command, attack press and release made into `m`'s impulse.
void choice_moves(WeaponChoice& w, const hud::Status& s, ge::shooter::Moves& m);
// The deathmatch mode of the server joined (`joined`, its server info), else of the listen server.
int deathmatch_now(const DemoState* joined, const Listen& l);
// The list back to the shotgun and the melee weapon (a new map or server).
void reset_choice(WeaponChoice& w);

}  // namespace shooters_edge::client
