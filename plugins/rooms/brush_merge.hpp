// A LEVEL'S BRUSHES MERGED: two brushes of one contents and group that touch face to face become
// one when their union is convex, so a level carries fewer, larger brushes and draws and clips
// the same space. Every corner of each must lie behind every plane of the other but the plane
// they touch on (else the union has a dent), and the planes left must close exactly the two
// volumes. A plane both brushes have must look the same on both (material, shading, a projection
// equal to a ten-thousandth), so no face changes its texture; a face with no surface on it
// (hidden) takes the other's look.
//
// Greedy and in order: each brush in turn takes in every neighbour it can, again until no pair
// merges, so the same level merges to the same brushes. A pairwise merge cannot join three pieces
// whose pairs each have a dent; the cloner's cells are mostly of that kind, so a cloned map loses
// about a tenth of its brushes.
//
// ge:summary Merges a level's touching brushes whose union is convex into one brush
// ge:keywords brush brushes merge union convex volume coplanar fewer-brushes clone optimise
// ge:skill rooms
// ge:test tests/rooms_brush_test.cpp
#pragma once

#include <cstddef>

#include "rooms/level.hpp"

namespace ge::rooms {

// `*level`'s brushes merged in place: a merged brush takes the place of the first of the pair, its
// faces the planes that bound the union, and the projections are pooled again in first-use order.
// Returns the number of merges. Call `link` afterwards.
size_t merge_brushes(Level* level);

}  // namespace ge::rooms
