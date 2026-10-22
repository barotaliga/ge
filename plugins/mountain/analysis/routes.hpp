// R42 — THE SKI ROUTES held on the finished map, read off the routes the area publishes
// (`ResortMap::routes`) and the ground, trunks and kickers round them, never off the search that
// found them. A map with no route is no fault.
//
// ge:summary R42 checks: the ski routes' count, ids, ends, length, fall, pitch, trunks and jumps
// ge:keywords analysis check-routes ski-route orange-route route-jump r42
// ge:skill mountain
// ge:test tests/mountain_ski_routes_test.cpp
#pragma once

#include "mountain/analysis/index.hpp"

namespace ge::mountain::analysis {

// R42 — at most `route::count.max` routes, `SR1…`; each off a chair's or a gondola's top and onto
// a run the area has, as long and as deep as a route must be, steeper than a black and no
// steeper than a route over a colour window, never a cliff over its pitch window outside its
// drops, no trunk on its line, and its jumps (`Kicker::route`) its own and no more than a route
// carries.
void check_routes(const mapgen::Level& level, const Add& add);

}  // namespace ge::mountain::analysis
