// R42 — WHAT IS LAID DOWN A WILD SKI ROUTE once its line is found (ski_routes.hpp): the trunks it
// threads felled, and its JUMPS.
//
// A wild line goes THROUGH the woods: every trunk whose bark stands within `route::open` m of it
// is felled, and nothing else, so a tree run is a lane a few metres wide with the wood standing at
// its edges.
//
// Its JUMPS are kickers stood on the line's ROLLS (where the ground past the lip falls away
// steeper than it came up to it by `wild::jump::roll`), facing the way the LINE goes there, not
// the fall line: one on a traverse throws a skier across the face, one before a bend throws him
// straight on past it. Each is scored by how much the ground rolls away under it, how hard the
// line turns over its landing and whether that landing is narrow; the best are kept
// `wild::jump::spacing` apart, clear of the line's ends and its drops, up to `wild::jump::count`.
// The lip stands higher the harder the ground rolls, and a GAP is dug past it, longer the higher
// the lip. No lip is laid where it would make the line steeper than a route may be. Nothing draws
// from a stream.
//
// ge:summary R42 a wild route's felled trunks and its jumps cut into the ground
// ge:keywords route-jump route-gap fell-on-routes lay-route-jumps wild-route r42
// ge:skill mountain
// ge:test tests/mountain_ski_routes_test.cpp
#pragma once

#include <span>
#include <vector>

#include "core/heightfield.hpp"
#include "mountain/mapgen/types.hpp"

namespace ge::mountain::mapgen {

// The trunks a wild route fells: every trunk within `route::open` m of its line or on one of its
// jumps' footprints. Returns the trees left, in their order.
std::vector<TreeDef> fell_on_routes(std::span<const TreeDef> trees,
                                    std::span<const SkiRoute> routes,
                                    std::span<const Kicker> kickers);

// The jumps down every wild route, cut into `ground`; each route's stations re-read off the
// ground and its steepest window re-measured after. Ids `O1…` across the map, in the routes'
// order.
std::vector<Kicker> lay_route_jumps(ge::core::Heightfield& ground, std::vector<SkiRoute>& routes);

}  // namespace ge::mountain::mapgen
