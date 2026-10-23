// READING THE SNOW AHEAD: how a machine the army drives over the mountain picks its way (the
// snowmobiles now, army_sled.hpp; the tracked carriers and tanks later, each with its own
// numbers in a `DriveRead`).
//
// A heading is walked out `look` m, every `step` m, and stopped short where a rise is steeper than
// the machine climbs, a drop steeper than it rides off, a wind tunnel's tube or the map's edge is
// near. It costs for every metre it is stopped short, for every trunk near its line (a trunk
// nearer counting more), for every radian off the way wanted and off the way held. A fan of
// headings either side of the way wanted is weighed, and the cheapest is steered. The hub's WIND
// TUNNELS are a wall to all of them: a tube on the snow no belt goes through.
//
// Pure over the map: nothing here draws a random number or allocates.
//
// ge:summary Reading the snow ahead for the army's machines: way_cost, choose_way, tunnel_within
// ge:keywords army drive machine terrain-read heading fan cost climb cliff tunnel edge trunks
// ge:keywords choose_way way_cost tunnel_within woods
// ge:skill sim
// ge:test tests/fall_line_army_sled_test.cpp
#pragma once

#include <cstdint>

#include "mountain/mapgen/types.hpp"
#include "skier/trees.hpp"

namespace fall_line::sim {

// What a machine reads ahead and takes: how far and how finely, m; the fan of headings it weighs
// (`fan` a side across `spread` rad); the steepest rise it climbs and drop it rides off (slopes);
// the gap it keeps from a tunnel's tube, a trunk off its line and the map's edge, m; and what a
// heading costs: a radian off the way wanted, a metre short of `look`, a trunk near its line, a
// radian off the way held.
struct DriveRead {
  double look = 0;
  double step = 0;
  int32_t fan = 0;
  double spread = 0;
  double rise = 0;
  double drop = 0;
  double tunnel = 0;
  double trunk = 0;
  double edge = 0;
  double turn_cost = 0;
  double blocked_cost = 0;
  double tree_cost = 0;
  double keep_cost = 0;
};

// A heading's cost from (x, z), how many trunks stand in its first three reads (the woods it
// would be driving into), and how far it runs before it is stopped, m (`look` when it is not).
struct WayCost {
  double cost = 0;
  int32_t trees = 0;
  double reach = 0;
};

// The way steered, whether it runs among trees, its cost, and how far it runs, m.
struct WayChoice {
  double way = 0;
  bool woods = false;
  double cost = 0;
  double reach = 0;
};

// Whether (x, z) is within `r` m of a wind tunnel's tube: its line from entrance to exit, half its
// width either side.
bool tunnel_within(const ge::mountain::mapgen::Level& level, double x, double z, double r);

// WHAT A HEADING COSTS from (x, z), wanting `want` and holding `keep` (the header says how).
WayCost way_cost(const ge::mountain::mapgen::Level& level, const ge::skier::TreeIndex& trees,
                 double x, double z, double heading, double want, double keep,
                 const DriveRead& read);

// THE WAY TO STEER from (x, z), holding `held`: the cheapest of the fan either side of `want`, and
// the way held.
WayChoice choose_way(const ge::mountain::mapgen::Level& level, const ge::skier::TreeIndex& trees,
                     double x, double z, double held, double want, const DriveRead& read);

}  // namespace fall_line::sim
