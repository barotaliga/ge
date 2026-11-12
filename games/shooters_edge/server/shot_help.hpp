// THE AIM HELP ON A CLASSIC MOD'S SERVER: the glue between a runner whose program shoots by
// tracing lines (the QC server's and the compiled-module server's `traceline`) and the loop's half
// of the aim help (plugins/shooter/aim_help.hpp). A line traced for a player granted the aim help
// (`*aimhelp` in his user info, engine/server/aids.hpp) that is his shot is bent by the server's
// leeway (`sv_aimhelp_x`, `sv_aimhelp_y`) onto the nearest other player in reach, never onto a
// teammate under `teamplay` (the same `team` in the user info), and counted (`aim_helped`, the
// players list's "used"). A missile he launches (the loop's new missile: a rocket, a grenade, a
// nail) is led instead (`aim_help_missile`): its flight and each player's run or fall foreseen,
// it is sent where the player it would just miss will be. Both are held while attack is: each
// shot of a held weapon (each frame's beam) is bent on its own.
//
// THE WEAPONS (`sv_aimhelp_allow`, `sv_aimhelp_deny`: names, spaces or commas between, `*` any
// characters; deny wins, an empty allow list allows the rest) name what the help may bend, by the
// weapon held: the classic game's `axe sg ssg ng sng gl rl lg` (`weapon_name`), the sequel's
// `blaster sg ssg mg cg gl rl hb rg bfg` (sequel_fight.cpp). `sv_aimhelp_deny "rl lg"` keeps the
// help off the rocket launcher and the lightning gun.
//
// The sequel's runner bends its own lines (sequel_fight.cpp) with the plugin's `aim_help`.
//
// ge:summary The aim help on a classic mod's server: a granted player's traced shot bent
// ge:keywords aim-help aimhelp traceline hitscan grant teammates shot_help aim_helped
// ge:skill shooter progs
// ge:test tests/shooters_edge_shot_help_test.cpp
#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "shooter/vec.hpp"
#include "shooters_edge/server/rules_runner.hpp"

namespace shooters_edge::server {

inline constexpr const char* kAimHelpKey = "*aimhelp";  // the grant's user-info key

// The aim help's own cvars (the weapon lists) registered on a runner. (Its missile rule is set on
// the runner's loop when a player is granted it: `s` must not move after.)
void add_aim_help(RulesCommon& s);
// Whether the help may bend a shot of the weapon called `weapon` (the lists above).
bool aim_help_weapon_allowed(const RulesCommon& s, std::string_view weapon);
// The classic game's weapon called by the item bit a mod holds in `weapon` ("" for none known).
const char* weapon_name(float weapon_bit);

// The aim help's grant read from a user-info change (`key`, `value`) onto the loop's slot.
void note_aim_help_info(RulesCommon& s, int slot, const std::string& key, const std::string& value);

// The slots of `slot`'s teammates (a bit each) under `teamplay`; 0 without it.
uint32_t team_friends(const RulesCommon& s, int slot);

// The line `start`..`end` a runner traces with `pass` the passing entity: bent in `end` when
// `pass` is a player granted the aim help and the line is his shot. The target's entity, or -1.
int help_traced_shot(RulesCommon& s, int pass, sh::Vec3f start, sh::Vec3f& end);

}  // namespace shooters_edge::server
