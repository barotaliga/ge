// WEAPON GROUPS IN SHOOTER'S EDGE: the shooter plugin's weapon groups (shooter/weapon_groups.hpp)
// on the game's number keys, cvars and HUD, in its own match and on a server joined.
//
// - The cvars (`add_group_cvars`): `cl_weapongroups 0|1` (off by default: each number key picks
//   its own weapon) and `cl_weapongroup1` .. `cl_weapongroup9`, each a group's weapon keys best
//   first ("7 6"); empty, the game's own group for that key (its profile's `weapon_groups`: the
//   classic game's 1 rockets and grenades, 2 the beam gun, 3 the nailguns, 4 the shotguns, 5 the
//   melee weapon); "0", no group (the key picks its own weapon).
// - `group_moves`: with cl_weapongroups 1 a number key's `impulse` that names a group (the default
//   binds' `impulse 1` .. `impulse 8`) becomes the weapon the group picks, and the attack folds the
//   group. A key with no group passes as it is, so keys 6..8 still pick their own weapon. Runs
//   before the quick weapon (quick_fire.hpp), which then reads the pick as the player's own.
// - `group_status` lays the group's look on the HUD's status (status.hpp's GroupShow): the
//   `wgroup` element opens, folds and marks a second weapon (hud/layout.hpp).
//
// ge:summary Shooter's Edge's weapon groups: number keys pick a group's best weapon, again the
// ge:summary next, and the HUD's group column
// ge:keywords weapon-groups cl_weapongroups cl_weapongroup add_group_cvars group_moves
// ge:keywords group_status group-key number-keys cs-style
// ge:skill input shooter
// ge:test tests/shooters_edge_weapon_groups_test.cpp
#pragma once

#include <array>
#include <span>
#include <string>
#include <string_view>

#include "console/console.hpp"
#include "shooter/weapon_groups.hpp"
#include "shooters_edge/hud/status.hpp"

namespace shooters_edge::client {

struct Moves;

struct WeaponGroups {
  bool on = false;                                        // cl_weapongroups
  std::array<std::string, ge::shooter::kMaxGroups> text;  // cl_weapongroup<n>
  std::span<const std::string_view> game;                 // the game's own groups (its profile's)
  // Parsed from them (`group_settings`), and what they were parsed from.
  ge::shooter::WeaponGroups groups;
  std::array<std::string, ge::shooter::kMaxGroups> parsed;
  const void* parsed_game = nullptr;
  bool parsed_once = false;
  ge::shooter::GroupState state;
};

// The cvars over `g` (`g` must outlive `con`).
void add_group_cvars(ge::console::Console& con, WeaponGroups& g);
// The groups the cvars and the game say (parsed again only when changed).
const ge::shooter::WeaponGroups& group_settings(WeaponGroups& g);
// The player's weapons as the plugin reads them.
ge::shooter::GroupArmed group_armed(const hud::Status& s);
// `m`'s number key made into the weapon its group picks, the attack folding the group, at `t` s.
void group_moves(WeaponGroups& g, const hud::Status& s, double t, Moves& m);
// The group's look at `t` s laid on `s.group` (off with cl_weapongroups 0).
void group_status(WeaponGroups& g, hud::Status& s, double t);

}  // namespace shooters_edge::client
