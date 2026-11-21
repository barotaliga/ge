// AREAS AND THEIR PORTALS (version 38 maps): the compiler splits a map's open space into areas at
// its area-portal brushes (a door's frame), each leaf naming its area (`Leaf::area`; 0 the outside
// and solid, never used). Each area lists the portals out of it (`Map::areas`, runs of
// `Map::area_portals`: the portal's number and the area across it), a portal listed once from
// each side.
//
// A game keeps each portal open or shut (one flag a number: the last set wins) and floods the
// areas over the open ones: `flood_areas` gives each area a group, the areas from 1 up taken in
// order, each new one starting the next group (1, 2, ...); area 0 is never flooded and keeps group
// 0, so it reaches only itself. Two areas are connected when their groups match. A map with no
// area data (another family, or a 38 map without them) connects everything.
//
// ge:summary Version 38 areas: each leaf's area, the portals between areas, flooded over the open
// ge:keywords area area-portal areaportal flood_areas areas_connected area_at doors-block-hearing
// ge:skill brush
// ge:test tests/brush_areas_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "brush/map.hpp"

namespace ge::brush {

// Each area's group over the portals open in `open` (one byte a portal number, nonzero open; a
// number past its end is shut), into `group` (one an area).
void flood_areas(const Map& map, std::span<const uint8_t> open, std::vector<int32_t>* group);
// Whether areas `a` and `b` are joined in `group` (flood_areas'); an area out of its range, or a
// map with no areas, is joined to everything.
bool areas_connected(std::span<const int32_t> group, int32_t a, int32_t b);
// The area of the leaf `p` (file coordinates) falls in; 0 outside or with no area data.
int32_t area_at(const Map& map, const float p[3]);

}  // namespace ge::brush
