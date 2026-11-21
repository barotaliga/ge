// SHOOTER'S EDGE'S BOTS: the plugin's bots (plugins/shooterbots: the fighter, its mind and its
// map) in view of the game's own bot files by their own names, and the game's arsenals they read:
// Quake's and Quake II's weapons, items, sounds, words and maps (`BotArsenal`).
//
// ge:summary Shooter's Edge's bots: the plugin's in view, and Quake's and Quake II's arsenals
// ge:keywords bot-arsenal first-arsenal q2-arsenal arsenals shooters-edge-bots
// ge:skill bots shooter
// ge:test tests/shooters_edge_q2_nav_test.cpp
#pragma once

#include "shooterbots/bot_arsenal.hpp"

namespace shooters_edge::bots {

using namespace ge::shooterbots;  // NOLINT(google-build-using-namespace): the plugin's bots, by
                                  // their own names, in the game's bot files

// Quake's arsenal (and its maps' book and team words), and Quake II's.
const BotArsenal& first_arsenal();
const BotArsenal& q2_arsenal();

}  // namespace shooters_edge::bots
