// WHAT A BOT SEES OF A RUNNING MATCH: the loop's state (the entities, their boxes, the clock, the
// settings), the rules' world (rules_world.hpp: the clients' numbers and an entity's row) and the
// lists the rules made at the map's start, as references into whatever runs the rules. Every
// shooter game's bots read the match through it, never through a game's server, so the bots can
// live in this plugin and play any game whose rules fill the world.
//
// A view is five references, made where a bot is called (any rules type with members of these
// names converts to it), and holds nothing: it lives no longer than the call.
//
// ge:summary What a bot sees of a match: the loop, the rules' world, the lists, by reference
// ge:keywords bot-view bots rules-world loop-state models sounds map-name seams
// ge:skill shooterbots bots
// ge:test tests/shooters_edge_fight_test.cpp
#pragma once

#include <string>
#include <vector>

#include "shooter/rules_world.hpp"
#include "shooter/server.hpp"

#include "shooterbots/kit.hpp"

namespace ge::shooterbots {

struct BotView {
  const Server& sv;                        // the loop: entities, boxes, the clock, the settings
  const RulesWorld& world;                 // what the rules show everyone
  const std::vector<std::string>& models;  // the model list; [0] ""
  const std::vector<std::string>& sounds;  // the sound list; [0] ""
  const std::string& map_name;             // the map playing

  // Any rules' state that keeps these by these names (a game's server) is seen through it.
  template <typename Rules>
  BotView(const Rules& r)  // NOLINT(google-explicit-constructor): the call sites pass the rules
      : sv(r.sv), world(r.world), models(r.models), sounds(r.sounds), map_name(r.map_name) {}
};

}  // namespace ge::shooterbots
