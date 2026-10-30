// THE BASELINE MOD AS THE SERVER SEES IT: the deathmatch mod written in QC in
// games/shooters_edge/qc/ (weapons, ammunition, armour, items, deaths, respawn, frags, doors,
// plats, triggers), compiled from its sources by the QC compiler (plugins/progs/qc_compile.hpp),
// and the test arena's entity text for it: spawn points, every weapon, ammunition of each kind,
// the three armours, health of each size, the DOOR in the doorway (brush model "*1") and the PLAT
// in the east room (brush model "*2"), so a match exercises every part of the mod.
//
// `kBaselineSource` is the project file's path from the repository's root; a caller outside the
// root (a test) passes its root to `compile_baseline_mod`.
//
// ge:summary The baseline QC deathmatch mod compiled from source, and its arena's entity text
// ge:keywords baseline-mod compile_baseline_mod kbaselinearenaentities deathmatch qc-source
// ge:keywords func_door func_plat weapons items armour arena
// ge:skill progs shooter
// ge:test tests/shooters_edge_baseline_mod_test.cpp
#pragma once

#include <filesystem>
#include <string>

#include "progs/file.hpp"

namespace shooters_edge::server {

constexpr const char* kBaselineSource = "games/shooters_edge/qc/progs.src";

// The test arena's entities for the baseline mod (the arena's door and plat as its models).
extern const char* const kBaselineArenaEntities;

// The baseline mod compiled from `<root>/games/shooters_edge/qc/progs.src`. False with `error`
// (every diagnostic's file and line, and the next step).
bool compile_baseline_mod(const std::filesystem::path& root, ge::progs::Progs& out,
                          std::string& error);

}  // namespace shooters_edge::server
