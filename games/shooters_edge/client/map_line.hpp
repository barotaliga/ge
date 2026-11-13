// WHERE A LINE THROUGH A MAP STOPS: the first solid a straight line from one point to another
// meets in a map's world (its model 0), for what the player looks at (`$loc_there`). A map of
// the first game's family is walked through its point hull; one of the sequel's family (it
// carries brushes) is swept through its solid and window brushes as a point.
//
// Not for a hot path: the sequel's tracer is built on each call.
//
// ge:summary The point a line through a map's world stops at, for either classic family
// ge:keywords map-line line-of-sight look-trace loc-there sight-line point-hull brush-tracer
// ge:skill shooter
// ge:test tests/shooters_edge_map_line_test.cpp
#pragma once

#include "shooter/vec.hpp"
#include "shooters_edge/server/bsp_map.hpp"

namespace shooters_edge::client {

// The point the line from `from` to `to` stops at in `map`'s world: `to` when nothing is in the
// way, `from` when it starts in solid or the map has no world.
ge::shooter::Vec3f line_end(const server::BspMap& map, ge::shooter::Vec3f from,
                            ge::shooter::Vec3f to);

}  // namespace shooters_edge::client
