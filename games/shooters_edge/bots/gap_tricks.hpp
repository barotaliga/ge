// THE MAPS' GAP TRICKS, as data: the plain jumps people make over a pit at a hopper's speed again
// and again (the strafe jump over dm2's lava straight to the red or the yellow armour), read off
// their recorded games by the `se-rj` lab (its gap_tricks.tsv: 51 public dm2 matches, read
// 2026-11-10), each a takeoff and a landing, the takeoff's speed people had (its quarter: most
// came faster), how often they made it, whether the pit holds lava or slime and the least
// jumper's level that makes it (the lowest whose players made a fifth of its jumps or more).
// Only those a run's jump falls short of in most of the jumps (speed jumps) and made ten times
// or more are kept. The nav proves each (shooter/nav_gaps.hpp `nav_add_gap_tricks`) when a map's
// is built (`build_map_nav`): one the move cannot fly from the graph's nodes, or one a run's jump
// makes too, is left out. To add a map's, run `se-rj --dir=<its demos>` and copy its rows.
//
// ge:summary The maps' gap tricks (speed jumps over a pit) read off people's demos, as nav data
// ge:keywords gap-tricks gap-jump speed-jump strafe-jump-over lava-jump map-data dm2
// ge:keywords copy-movement movement-skill nav_add_gap_tricks
// ge:skill bots
// ge:test tests/shooter_nav_gaps_test.cpp
#pragma once

#include <span>
#include <string_view>
#include <vector>

#include "shooter/nav_gaps.hpp"

namespace shooters_edge::bots {

struct GapTrick {
  const char* map;
  ge::shooter::Vec3f from, to;  // people's takeoff and landing (origins)
  float speed;                  // the takeoff's speed across, its quarter, units/s
  int jumps;                    // how often people made it in the demos read
  bool harm;                    // the pit holds lava or slime
  uint8_t level;                // 0 amateur, 1 intermediate, 2 expert
};

// The gap tricks of map `name` (its file's name, "dm2"); none for a map nobody read.
std::span<const GapTrick> gap_tricks(std::string_view name);

// The same as the nav's.
std::vector<ge::shooter::NavGapTrick> nav_gap_tricks(std::span<const GapTrick> tricks);

}  // namespace shooters_edge::bots
