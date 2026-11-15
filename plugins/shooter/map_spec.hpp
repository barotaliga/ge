// A MAP AS A SHOOTER GAME'S RULES START IT: its brush models (`[0]` the world, each with its clip
// hulls: brush_hull.hpp) and its entity text, or, for rules that trace a version 38 map's brushes
// themselves, the map read whole (plugins/brush). Any game's runner starts from it and the bots
// build their nav off it, so neither names a game.
//
// ge:summary A map as rules start it: brush models with hulls, entity text, or the whole map read
// ge:keywords map-spec map brush-models entity-text clip-hulls runner start bots nav
// ge:skill shooter
// ge:test tests/shooters_edge_rules_runner_test.cpp
#pragma once

#include <memory>
#include <string>
#include <vector>

#include "shooter/brush_hull.hpp"

namespace ge::brush {
struct Map;
}

namespace ge::shooter {

struct MapSpec {
  std::string name = "arena";
  std::vector<BrushModel> models;
  std::string entities;
  std::shared_ptr<const ge::brush::Map> brushes;
};

}  // namespace ge::shooter
