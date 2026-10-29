// PUSHERS: doors, plats and trains (movetype push, solid bsp) move on their own clock `ltime` and
// are exact: they stop at their nextthink instead of overshooting it, and they carry what stands on
// them and shove what is in their way, or stop and say who blocked them.
//
// A PUSHER'S FRAME (`physics_pusher`):
//  1. movetime = dt, or less when the nextthink falls inside this frame on the pusher's own clock
//     (nextthink < ltime + dt): then max(nextthink - ltime, 0). Its clock is float, as the
//     program's fields are.
//  2. A non-zero movetime: a pusher with no velocity only adds it to ltime; otherwise it pushes by
//     velocity x movetime and adds movetime to ltime only when the push went through (a blocked
//     door's clock stops).
//  3. A nextthink after the old ltime and at or before the new one: cleared, the think runs with
//     the server's time; a think that moved the pusher more than 1/64 unit is put back and pushed
//     by that move instead (a train that teleports still carries its riders).
//
// A PUSH (`push_move`) by `move`:
//  1. the pusher moves and is relinked (no touches);
//  2. every entity in number order, not free and not a push, none or noclip: skipped when it is
//     inside something with the pusher made non-solid (it was already stuck); unless it stands on
//     the pusher (kFlagOnGround on it), skipped when its absolute box misses the pusher's moved box
//     (strictly) or it is not inside the pusher's new position;
//  3. a non-walking entity loses kFlagOnGround; it moves by `move` and, free there, is relinked;
//  4. otherwise it goes back, and free there it is forgotten;
//  5. still stuck: a zero-width entity is left (relinked), a `not` or trigger solid (a corpse) is
//     flattened to a zero box, and anything else BLOCKS: the pusher goes back, its blocked function
//     runs (self the pusher, other the blocker), every entity moved so far goes back, and the push
//     fails.
//
// ge:summary Pushers: doors, plats and trains on their own clock, carrying, shoving and blocked
// ge:keywords pusher push door plat train ltime movetime physics_pusher push_move blocked rider
// ge:keywords carry crush
// ge:skill shooter
// ge:test tests/shooter_push_test.cpp
#pragma once

#include "shooter/server.hpp"

namespace ge::shooter {

// One pusher's physics frame of `dt` seconds.
void physics_pusher(Server& sv, int e, double dt);

// Pushes the pusher by `move`, carrying and shoving; false when something blocked it.
bool push_move(Server& sv, int pusher, Vec3f move);

}  // namespace ge::shooter
