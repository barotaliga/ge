// THE ARENA IN C++: the deathmatch rules (the multiplayer form) as a native program
// (progs/native.hpp) on the classic mod server, each function the rule it replaces in the arena's
// QC, statement for statement, so a match plays the same (tests/shooters_edge_arena_rules_test.cpp
// holds it side by side with the compiled QC).
//
// The program's whole state is its globals and fields, declared below by name as the QC declared
// them (the server, the bots and the saves find `attack_finished`, `maxspeed`, `team` and the rest
// by name); its strings are the values it stores or passes; its functions are the hooks, the spawn
// functions by class name and every think, touch and use a field holds. A rule calls another rule
// as C++; it calls a field's function (`self.th_die()`) through the VM, by its number.
//
// The files follow the rules' subjects: arena_util (helpers, movers, targets, effects, numbers),
// arena_combat (damage, blasts, telefrags, scoring), arena_weapons (the arsenal, firing),
// arena_items (pickups, packs, powers), arena_dmm4 (deathmatch 4), arena_player (the player's
// frame), arena_map (doors, plats, buttons, triggers), arena_glue (the multiplayer form's ways,
// the loaded game's words) and arena_world (worldspawn, the frame, the match's end).
//
// ge:summary The arena's deathmatch rules written in C++ as a native program
// ge:keywords arena native-arena native-rules deathmatch rules-declaration native_arena
// ge:skill progs shooter
// ge:test tests/shooters_edge_arena_rules_test.cpp
#pragma once

#include <cstdint>

#include "progs/native.hpp"
#include "shooters_edge/rules/calls.hpp"

namespace shooters_edge::server {

// The arena's declaration: its globals, fields, strings and functions.
const ge::progs::NativeProgram& native_arena();

}  // namespace shooters_edge::server
