// THE RULES' OWN SETTINGS FOR THE REFERENCE MANUAL: the settings a game's rules register when they
// are made (`deathmatch`, `coop`, `skill`, `fraglimit` and the rest), which live in the rules'
// own cvars, not on the window's or the server's console (a line naming one reaches the rules
// through the console's unknown-word hook). Made here with no map, so a program describes them
// for its manual (console/manual.hpp's `ManualInfo::more`): Quake's and QuakeWorld's from the
// settings every Quake program reads (`add_mod_settings`), Quake II's from its rules
// (`make_q2_server`), each in the game's own layer.
//
// ge:summary The rules' own settings (deathmatch, coop, skill...) made with no map, for the manual
// ge:keywords rules-settings manual reference-manual deathmatch coop skill rules_manual
// ge:skill console shooter
// ge:test tests/shooters_edge_manual_test.cpp
#pragma once

#include <string_view>
#include <vector>

#include "console/manual.hpp"

namespace shooters_edge::server {

// The rules' settings of the pass of `games` (ids split by commas: "q1,qw", "qw" or "q2") as
// entries in the first game's own layer (`game:<id>`), appended to `out`; nothing for a game
// with no rules of these.
void rules_manual(std::string_view games, std::vector<ge::console::ManualEntry>& out);

}  // namespace shooters_edge::server
