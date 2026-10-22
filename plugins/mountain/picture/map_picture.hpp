// A GENERATED MOUNTAIN DRAWN FROM ABOVE on the CPU (lab/raster.hpp): the face shaded by height
// and lit from the upper left, the packed snow tinted, the woods, the piste's line, every cliff
// and kicker labelled, every gate numbered (the start green, the finish black) and the spawn. The
// mountain labs draw a whole map with it, and a game's `ge shot` view draws a map or a window of
// one round its player, so every picture of a map from above reads the same.
//
// +x is to the right and +z down the picture. A window is a square of the map, `metres` a side,
// centred on (x, z); the parts of it off the map draw as the map's edge does.
//
// ge:summary A generated mountain drawn from above on the CPU: the whole map or a window of it
// ge:keywords map picture draw_map hillshade gates kickers cliffs trees piste window above raster
// ge:skill mountain
// ge:test tests/mountain_map_picture_test.cpp
#pragma once

#include "lab/raster.hpp"
#include "mountain/mapgen/types.hpp"

namespace ge::mountain::picture {

// A square of the map: its centre and its side, in metres.
struct MapWindow {
  double x = 0;
  double z = 0;
  double metres = 0;
};

// The whole map, `px` pixels a side.
ge::lab::Picture draw_map(const mapgen::Level& level, int px);

// `window` of the map, `px` pixels a side.
ge::lab::Picture draw_map(const mapgen::Level& level, int px, const MapWindow& window);

}  // namespace ge::mountain::picture
