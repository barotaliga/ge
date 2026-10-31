// TRACES THROUGH THE SERVER'S WORLD: a box swept from one point to another through the world and
// every solid entity the area tree holds near the sweep, the way thrown things, falling things,
// pushers and the program's own traces move.
//
// AGAINST A BRUSH MODEL (solid bsp: the world, a door) the hull is chosen by the moving box's width
// (x size): under 3 units hull 0, up to 32 hull 1, wider hull 2; the hull's own `clip_mins` minus
// the box's mins plus the model's origin is the offset the sweep is taken in. AGAINST A BOX the
// entity's box grown by the moving box (its mins minus the mover's maxs, its maxs minus the
// mover's mins) is a six-plane hull at the entity's origin. Each hull is walked the networked way,
// or the original way (hull.hpp) on the single-player clock and during an original move's own
// traces (`Server::original_walk`).
//
// THE WALK (`trace_move`): the world first; a sweep that is all solid in the world looks no
// further. Then every solid entity linked in the area tree whose absolute box meets the sweep's
// bounds (each axis from the lower end plus the box's mins minus 1 to the upper end plus its maxs
// plus 1, the larger of the two boxes for a missile), in the tree's order, skipping: the passing
// entity; anything but a brush model for `kTraceNoMonsters`; a point-sized entity when the mover
// has a size (points never interact); entities the mover owns, and the mover's owner. A MISSILE
// sweep (`kTraceMissile`) meets a monster (kFlagMonster) as a 30-unit cube (+-15) instead of its
// own box. A trace that is all solid, or reaches a smaller fraction than the best so far, takes its
// place with that entity as `ent`; "started in solid" is kept from any solid met; an all-solid best
// ends the walk.
//
// A trace's `ent` is the entity hit (0 the world), or -1 when nothing was (the fraction is 1 and it
// did not start in a solid). Its end point is where the box stops: the end itself after a full
// run, the start when all solid. Pure: reads the server, writes nothing.
//
// ge:summary Traces through the server's world: hull choice, box hulls, the area tree's solids
// ge:keywords trace trace_move clip_to_entity test_entity_position hull-choice hull0 hull1 hull2
// ge:keywords missile nomonsters passent owner sweep move-bounds point-size
// ge:skill shooter
// ge:test tests/shooter_trace_test.cpp
#pragma once

#include "shooter/server.hpp"

namespace ge::shooter {

constexpr int kTraceNormal = 0;
constexpr int kTraceNoMonsters = 1;  // only brush models block
constexpr int kTraceMissile = 2;     // monsters met as a 30-unit cube

// A sweep through one entity alone (a brush model's hull, or its box grown by the mover's).
HullTrace clip_to_entity(const Server& sv, int ent, Vec3f start, Vec3f mins, Vec3f maxs, Vec3f end);

// A sweep through the world and every solid entity near it; `passent` -1 for none.
HullTrace trace_move(const Server& sv, Vec3f start, Vec3f mins, Vec3f maxs, Vec3f end, int kind,
                     int passent);

// Whether the entity's box at its origin is inside something (a zero-length sweep that starts in
// solid; `kTraceNoMonsters` for triggers and `not` solids).
bool test_entity_position(const Server& sv, int e);

}  // namespace ge::shooter
