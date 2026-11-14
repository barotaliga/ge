// THE BRINK: a person on foot near a pit of lava or slime sees the lip and keeps off it. Whatever
// pressed the keys (the route's walk round a corner, a lane beside it, a chase with the view on
// an enemy, a side step), the keys that would carry him over the lip are let go of, and a run
// heading into it is braked. A rocket out of the pit, or a jump the nav proved over it, is not
// this guard's: the caller runs it only while the feet are free (walking, strafing, hopping or
// standing), on the ground and in the air alike: a run down a stair is airborne from step to step,
// and a wish against the run brakes in the air at the full acceleration (the air's cap is on the
// speed gained along the wish, not on the speed lost).
//
// The check (`brink_guard`): only near a floor node in lava or slime (`nav_harm_near`), the
// player's box is swept a step each way round (eight ways) and along its own run a quarter second
// ahead, and the floor looked for under where it stops: lava or slime there is a way into the
// pit. The wish (the forward and side keys under the view) loses its share along each such way;
// a run into one is answered with the back of the wish along it, and the jump is let go of (a
// hop off a lip is a fall).
//
// ge:summary The brink: the keys that would carry a player on foot into lava or slime let go of
// ge:keywords brink lava-lip lava slime harm pit edge fall-in brink_guard over_harm keep-out
// ge:keywords fall_into_harm stair-fall air-brake
// ge:skill shooter bots
// ge:test tests/shooter_brink_test.cpp
#pragma once

#include "shooter/nav.hpp"
#include "shooter/pmove.hpp"
#include "shooter/server.hpp"

namespace ge::shooter {

// Whether the player's box swept from `from` toward `to` (stopped by a wall) stands over lava or
// slime: the floor under where it stops, looked for down to 400 units, is in either.
bool over_harm(const Server& sv, Vec3f from, Vec3f to, int me);

// Whether the player's box at `origin` moving `velocity`, flown on under gravity (walls stop it, a
// stair's step is climbed, a floor lands it), comes with its feet into lava or slime within
// `ground` units run on the floor (any flight between not counted; at least `kLandRun` after a
// landing) or a second.
bool fall_into_harm(const Server& sv, Vec3f origin, Vec3f velocity, float ground, int me);

// What the guard found and did on one command.
struct BrinkGuard {
  bool ran = false;      // near a harm node: the guard looked
  int ways = 0;          // the ways round (of eight) into the pit
  bool run_in = false;   // the run itself heads into it within a quarter second
  bool fall_in = false;  // the run's flight (a step's fall, a hop) lands it in (`fall`)
  bool changed = false;  // the keys were changed
};

// Keeps player `me` (at `origin`, moving `velocity`, its feet free) out of the harm `g` knows
// (`NavGraph::harm`): `cmd`'s forward and side keys (under its view's yaw) lose their share along
// every way into a pit, a run into one is braked, and the jump is let go of. With `fall` the run
// is flown (`fall_into_harm`), from as far out as the flight could carry it.
BrinkGuard brink_guard(const Server& sv, const NavGraph& g, int me, Vec3f origin, Vec3f velocity,
                       MoveCmd& cmd, bool fall = false);

}  // namespace ge::shooter
