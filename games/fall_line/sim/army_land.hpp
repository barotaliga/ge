// WHERE THE ARMY'S HELICOPTER PUTS ITS MEN DOWN: a landing zone near the man it hunts, read off the
// map alone (army_heli.hpp asks for it and flies it). Its numbers are army_heli_rules.hpp.
//
// A helicopter is set down on ground that leans no more than its flight manual allows
// (`army_heli::kSlope` over the disc's circle) with the disc and a margin clear of every trunk,
// every lift's line and bottom station and every tunnel's lane (heli_ride.hpp's `pad_lean` and
// `pad_clear`, the free ride's own pad search). Of the places that will do in the ring round him
// (`kNearM` to `kFarM`) it takes the one nearest `kPreferM` off him, AHEAD of him where he is
// moving (the section put down in his way rather than behind him) and not below him, where the men
// would have to climb back up. Where nothing in the ring will do (a forest, a steep face) it takes
// a place clear of trunks round a man's drop (`kRopeClearM`) under a hover the rope reaches: the
// men come down a FAST ROPE.
//
// Pure: a function of the map, the lifts, the trunks and where he is; it draws no random number
// and allocates nothing.
//
// ge:summary Where the army's helicopter sets down: a landing zone, else a fast rope's spot
// ge:keywords army helicopter landing landing-zone land_spot fast-rope rope_hover flat clear
// ge:keywords ahead ring army_floor
// ge:skill sim
// ge:test tests/fall_line_army_heli_test.cpp
#pragma once

#include <optional>
#include <span>

#include "fall_line/sim/state.hpp"

namespace fall_line::sim {

// A place to put men down: the spot on the snow (m), the heading the machine is set down facing
// (rad: side on to him, a door each way), and whether it is a fast rope's.
struct LandSpot {
  double x = 0, y = 0, z = 0;
  double heading = 0;
  bool rope = false;
};

// A circle a landing keeps out of: another machine's spot, the free ride's helicopter.
struct LandAvoid {
  double x = 0, z = 0, r = 0;
};

// The ring a place is looked for in, m: nearer than `near` or further than `far` will not do,
// `prefer` is best.
struct LandRing {
  double near = 0, far = 0, prefer = 0;
};

// The ring a drop near him is looked for in.
LandRing drop_ring();

// THE LOWEST A MACHINE MAY BE at (x, z): its skid datum `floor` m over the highest snow under its
// disc, and clear over every crown the disc sweeps.
double army_floor(const GameState& state, double x, double z, double floor);

// THE HOVER a fast rope is slid from over the snow at (x, y, z): `kRopeHoverM` over it, never
// under the flight's floor over the woods.
double rope_hover(const GameState& state, double x, double z, double y);

// A PLACE TO PUT MEN DOWN round (cx, cz), him stood at height `cy` and moving at (vx, vz) m/s,
// outside every circle of `avoid`; none where neither a landing nor a rope will do.
std::optional<LandSpot> land_spot(const GameState& state, double cx, double cy, double cz,
                                  double vx, double vz, std::span<const LandAvoid> avoid,
                                  LandRing ring);

}  // namespace fall_line::sim
