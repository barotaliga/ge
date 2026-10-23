// A SOLDIER ON HIS SKIS (army.hpp moves every one through this): how a winter infantryman gets
// from where he is to where he is going over the mountain's own snow.
//
// ROUGH AND CHEAP: a point on the heightfield with a heading and a speed, not a skier on the
// snow's physics. Down a slope the hill carries him (gravity along his way, the snow's drag and
// the air's), checked to the pace he holds by a skid; he turns no tighter than his skis carve
// (`army::kTurnM`). Where the hill does not carry him he WORKS: skates the flats, climbs a short
// rise in a herringbone at a pace that falls with its steepness, and a long steep one across it,
// zig-zagging as a ski-tourer on skins does.
//
// He keeps clear of the trunks (steered off those ahead of him, pushed out of one he touches), of
// the other soldiers, and inside the map. A man who barely moves in a window (`watch_progress`)
// steps off to one side for a while, so nothing holds him for good.
//
// Nothing here draws a random number or allocates: his side-step is the parity of his id.
//
// ge:summary A soldier on his skis: go_toward, stop_here, the slope along his way, the stuck watch
// ge:keywords army troop soldier go_toward stop_here watch_progress slope_along traverse
// ge:keywords herringbone skate zig-zag keep-clear trunks stuck detour kinematic
// ge:skill sim
// ge:test tests/fall_line_army_test.cpp
#pragma once

#include <cstdint>
#include <span>

#include "fall_line/sim/army_state.hpp"
#include "mountain/mapgen/types.hpp"
#include "skier/trees.hpp"

namespace fall_line::sim {

// What a soldier's step reads: the map, its trunks, every soldier (to keep apart), the clock, and
// the step's grid of them by place (`near_reach`; null: every soldier is weighed).
struct TroopWorld {
  const ge::mountain::mapgen::Level* level = nullptr;
  const ge::skier::TreeIndex* trees = nullptr;
  std::span<const Soldier> all;
  double t = 0;   // s, the clock at the step's start
  double dt = 0;  // s
  const ge::ai::Neighbours* near = nullptr;
};

// THE STEP'S GRID of the soldiers by place, built before any of them moves: its cells their reach
// to keep apart plus how far a man moves in a step at most (his pace, and a trunk pushing him out
// of it); a man carried or put down by a machine may be anywhere, so every query visits him.
void build_near(ge::ai::Neighbours& near, std::span<const Soldier> all);

// The slope against a man going along `heading` at (x, z), as a tangent: positive up it.
double slope_along(const ge::mountain::mapgen::Level& level, double x, double z, double heading);

// ONE STEP OF A MAN GOING TOWARD (tx, tz): the way he takes, the hill's pull and the drags, his
// pace checked to `pace` m/s (and to a stop `stop` m short of it, when `stop` >= 0), his working
// pace where the hill does not carry him, the trunks he touches and the map's edge.
void go_toward(const TroopWorld& w, Soldier& s, double tx, double tz, double pace, double stop);

// ONE STEP OF A MAN STOPPING where he is: a hockey stop.
void stop_here(const TroopWorld& w, Soldier& s);

// THE WATCH ON HIS PROGRESS: every `army::kWindowS`, a man who should be moving and moved under
// `kLeastM` steps off to a side for `kDetourS`, the other side each time he is caught again.
// Returns how many windows in a row he has barely moved.
int32_t watch_progress(Soldier& s, double t, bool moving);

}  // namespace fall_line::sim
