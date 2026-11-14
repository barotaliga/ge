// THE CAMPAIGN IN C++: the single-player game (the single-player form) as a native program
// (progs/native.hpp) on the campaign host, each function the rule it replaces in the campaign's
// QC, statement for statement, so a game plays the same.
//
// It shares the arena's rules (common_*.cpp, compiled a second time against campaign_decl.hpp)
// and adds its own: campaign_glue (the form's ways, the spawn parms, spawn points, the jump),
// campaign_world (worldspawn, the intermission, exits, secrets, runes, keys), campaign_ai (the
// monsters' minds), campaign_monsters and campaign_monsters_big (the roster), campaign_bosses
// (the bosses, the lightning, teleports) and campaign_set_pieces (trains, secret doors, boxes,
// traps, bubbles, ambient sounds).
//
// ge:summary The campaign's single-player rules written in C++ as a native program
// ge:keywords campaign native-campaign native-rules single-player rules-declaration
// ge:keywords native_campaign
// ge:skill progs shooter
// ge:test tests/shooters_edge_campaign_rules_test.cpp
#pragma once

#include "progs/native.hpp"

namespace shooters_edge::server {

// The campaign's declaration: its globals, fields, strings and functions, on the single-player
// form.
const ge::progs::NativeProgram& native_campaign();

}  // namespace shooters_edge::server
