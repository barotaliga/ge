// THE BASELINE MOD AS THE SERVER SEES IT: the deathmatch's rules written in C++
// (games/shooters_edge/rules/arena*.cpp: weapons, ammunition, armour, items, deaths, respawn,
// frags, doors, plats, triggers) laid out as a native program (plugins/progs/native.hpp), so it
// runs on the mod server exactly where a compiled progs would; and the test arena's entity text
// for it: spawn points, every weapon, ammunition of each kind,
// the three armours, health of each size, the DOOR in the doorway (brush model "*1") and the PLAT
// in the east room (brush model "*2"), so a match exercises every part of the mod.
//
// ge:summary The baseline deathmatch mod (the arena's rules in C++), and its arena's entity text
// ge:keywords baseline-mod baseline_mod kbaselinearenaentities deathmatch native-arena
// ge:keywords func_door func_plat weapons items armour arena campaign_mod native-campaign own_mod
// ge:skill progs shooter
// ge:test tests/shooters_edge_baseline_mod_test.cpp
#pragma once

#include <string>

#include "progs/file.hpp"

namespace shooters_edge::server {

// The test arena's entities for the baseline mod (the arena's door and plat as its models).
extern const char* const kBaselineArenaEntities;

// The baseline mod built as a progs (its functions C++, bound by any VM made over it). False with
// `error` only when the program's declaration is wrong, which is the game built wrong.
bool baseline_mod(ge::progs::Progs& out, std::string& error);

// The campaign (the single-player game's rules in C++, games/shooters_edge/rules/campaign*.cpp)
// built as a progs on the single-player form, the same way.
bool campaign_mod(ge::progs::Progs& out, std::string& error);

// One of our own programs by name, as a lab's `--progs` names it: "arena" (the baseline mod) or
// "campaign". False with `error` (naming both) for any other name.
bool own_mod(const std::string& name, ge::progs::Progs& out, std::string& error);

}  // namespace shooters_edge::server
