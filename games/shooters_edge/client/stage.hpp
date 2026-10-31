// PACKS A LAB STAGES OVER A SEARCH PATH, BY NAME (the labs runner's `--stage-pak=<name>`, read
// through `Lab::stage_pak()`), so the demo and picture labs draw what a game folder would hold
// without the classic pack in the tree. A staged pack fills only what the search path lacks: a real
// game folder's own models and palette always win.
//
// - `fixture`: stand-in players and what a match throws (`progs/player.mdl`, `h_player.mdl`,
//   `missile.mdl`, `grenade.mdl`, the three gibs), each the morph plugin's fixture body
//   (morph/write.hpp) scaled to the thing it stands in for: a player the player's hull, a head a
//   third of it, a rocket a small spike. No palette: the halls' grey stays the halls' grey, and
//   the stand-ins are drawn in it.
//
// ge:summary Packs a lab stages over the search path by name: the fixture's stand-in players
// ge:keywords stage-pak fixture stand-in player model missile gib pack lab demo highlights
// ge:skill shooter labs
// ge:test tests/shooters_edge_stage_test.cpp
#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "shooters_edge/client/files.hpp"

namespace shooters_edge::client {

// The names `stage_pak` knows.
std::vector<std::string> stage_pak_names();

// Stages the pack `name` over `path`, the files it adds named in `staged` (none when the path
// already holds every one). False with the reason and the names it knows when `name` is none.
bool stage_pak(SearchPath& path, std::string_view name, std::vector<std::string>* staged,
               std::string& error);

}  // namespace shooters_edge::client
