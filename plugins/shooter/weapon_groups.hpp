// WEAPON GROUPS, FOR ANY SHOOTER: the number keys pick a group of weapons instead of one weapon,
// as the tactical shooters do. Each group is a short list of the game's weapon numbers (1..31, its
// weapon keys), best first. It runs on the client, out of what a HUD already knows, and asks the
// server only for a weapon by its number, so it works in a game's own match and on any server.
//
// - A GROUP KEY PRESSED (`group_press`): the best weapon of the group that is ready (carried, with
//   a shot's ammunition) is asked for. Pressed again while the group is open, the next ready one
//   after the last asked for (round to the first again). A group with no ready weapon asks for
//   nothing but still opens, so the player sees what it holds.
// - THE GROUP STAYS OPEN `kOpenS` after the last press, or until the attack (`group_fire`): then it
//   closes over `kCloseS` into the weapon in hand.
// - THE LOOK (`group_look`), for a HUD: open, the group's carried weapons best first, the one
//   asked for lit; closing, the others fold away (`open` falls from 1 to 0); closed, the weapon in
//   hand alone. `more` marks a weapon whose group holds another weapon carried, so a player knows a
//   second press has somewhere to go.
//
// A game's default groups are text, one list a group ("7 6": weapons 7 then 6), parsed by
// `weapon_list` (quick_weapon.hpp). Pure and allocation-free per frame (the groups are parsed when
// a setting changes). Held by tests/shooter_weapon_groups_test.cpp and the `se-weapongroups` lab.
//
// ge:summary Weapon groups for any shooter: a number key picks the best weapon of its group, again
// ge:summary the next; the HUD's open group and its fold
// ge:keywords weapon-groups weapon-group group-key slot cl_weapongroups cl_weapongroup group_press
// ge:keywords group_fire group_look weapon-slot cs-style tactical-shooter second-weapon
// ge:skill shooter input
// ge:test tests/shooter_weapon_groups_test.cpp
#pragma once

#include <array>
#include <cstdint>
#include <vector>

namespace ge::shooter {

inline constexpr double kOpenS = 2.0;   // s a group stays open after its key
inline constexpr double kCloseS = 0.2;  // s it takes to fold into the weapon in hand
inline constexpr int kMaxGroups = 9;    // groups 1..9 (the number keys)
inline constexpr int kGroupMost = 4;    // weapons a group shows at most

// What the client knows of the player's weapons (bit n: weapon n).
struct GroupArmed {
  uint32_t carried = 0;
  uint32_t ready = 0;  // carried, with a shot's ammunition
  int in_hand = 0;     // the weapon in hand (0 none)
};

// The groups, by key: groups[0] is key 1's. An empty group is no group (its key is the game's own).
using WeaponGroups = std::array<std::vector<int>, kMaxGroups>;

struct GroupState {
  int group = -1;  // the group open or closing (-1 none)
  int asked = 0;   // the weapon last asked for from it
  double pressed_at = -1e9;
  double closed_at = -1e9;  // when it began to close (the attack, or kOpenS after the press)
};

// The group of `key` (1..kMaxGroups) is a group: its key picks from it.
bool is_group(const WeaponGroups& g, int key);
// The group holding `weapon` (0-based; -1 none).
int group_of(const WeaponGroups& g, int weapon);

// Key `key` (1..kMaxGroups, a group) pressed at `t` s: the weapon to ask for (0 none ready).
int group_press(GroupState& s, const WeaponGroups& g, const GroupArmed& a, int key, double t);
// The attack at `t` s: an open group starts to close.
void group_fire(GroupState& s, double t);

// How the group shows this frame.
struct GroupLook {
  int group = -1;  // the group shown (0-based; -1: the weapon in hand alone, no group)
  std::array<int, kGroupMost> weapons{};  // best first
  std::array<bool, kGroupMost> ready{};
  int count = 0;
  int lit = -1;       // the index of the weapon asked for (open) or in hand
  float open = 0;     // 1 open .. 0 folded into `weapons[lit]`
  bool more = false;  // the lit weapon's group holds another weapon carried
};
GroupLook group_look(const GroupState& s, const WeaponGroups& g, const GroupArmed& a, double t);

}  // namespace ge::shooter
