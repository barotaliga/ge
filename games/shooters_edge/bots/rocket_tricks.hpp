// THE MAPS' ROCKET-JUMP TRICKS, as data: the rocket jumps people make on a map again and again,
// read off their recorded games by the `se-rj` lab (its tricks.tsv: 51 public dm2 matches, 51 of
// dm3, 51 of e1m2, read 2026-11-07), each a takeoff and a landing, the way it is flown (the flip
// or the shot at the feet), its pitch and run and the least jumper's level that makes it (the
// lowest whose players made a fifth of its jumps or more). The nav proves each one by the move
// (shooter/nav.hpp `nav_add_tricks`) when a map's is built (`build_map_nav`), so a trick the move
// cannot fly from the graph's nodes is left out, and one it can is a rocket link a bot of its
// level takes like any other. To add a map's, run `se-rj --dir=<its demos>` and copy its rows.
//
// ge:summary The maps' rocket-jump tricks read off people's demos, as data for the nav
// ge:keywords rocket-tricks tricks rocket-jump flip trick-spots map-data dm2 dm3 e1m2
// ge:keywords copy-movement movement-skill rocket-jump-level nav_add_tricks
// ge:skill bots
// ge:test tests/shooters_edge_team_test.cpp
#pragma once

#include <span>
#include <string_view>
#include <vector>

#include "shooter/nav.hpp"

namespace shooters_edge::bots {

struct RocketTrick {
  const char* map;
  ge::shooter::Vec3f from, to;  // people's takeoff and landing (origins)
  float pitch;                  // the shot, degrees down
  float run;                    // the run at the shot, units/s (0: standing)
  int jumps;                    // how often people made it in the demos read
  bool flip;                    // flown as the flip (the shot behind)
  uint8_t level;                // 0 amateur, 1 intermediate, 2 expert
};

// The tricks of map `name` (its file's name, "dm2"); none for a map nobody read.
std::span<const RocketTrick> rocket_tricks(std::string_view name);

// The same as the nav's tricks.
std::vector<ge::shooter::NavTrick> nav_tricks(std::span<const RocketTrick> tricks);

}  // namespace shooters_edge::bots
