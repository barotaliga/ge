// THE BAKED LIGHT UNDER A POINT: what a map's lightmaps say the floor straight below a point is
// lit by, for whatever stands there and is not part of the map (a held weapon, a model, an item)
// to be lit as the room round it is. The classic way: a ray straight down to the nearest upward
// face of the world model, and that face's lightmap read where the ray met it.
//
// - THE PROBE (`make_light_probe`): the world model's upward faces that carry a lightmap (no sky,
//   no liquid), binned once on a grid seen from above, `kProbeCell` units a cell, so a ray tests
//   only its cell's triangles.
// - THE READ (`light_below`): the highest of those faces under the point within `reach` units,
//   its lightmap coordinates by the triangle's barycentric weights, the first style's luxels
//   blended bilinearly; 0..1 a channel (a luxel's byte over 255).
//
// ge:summary The baked light on the floor under a point, off the map's lightmaps
// ge:keywords light-probe lightmap light_below make_light_probe floor-light luxel ray-down
// ge:keywords lightpoint held-weapon lighting grid bin
// ge:skill brush
// ge:test tests/brush_light_probe_test.cpp
#pragma once

#include <cstdint>
#include <vector>

#include "brush/map.hpp"

namespace ge::brush {

inline constexpr float kProbeCell = 128;  // units across a cell of the grid

struct LightProbe {
  float lo[2] = {0, 0};  // the grid's corner, file units (x, y)
  int nx = 0, ny = 0;
  std::vector<uint32_t> start;    // a cell's first entry in `tris`; nx * ny + 1
  std::vector<uint32_t> tris;     // a triangle's first index into Map::indices
  std::vector<int32_t> lightmap;  // its surface's lightmap, one a `tris` entry
};

// The probe over `map`'s world model (an empty one when the map has no lightmaps).
LightProbe make_light_probe(const Map& map);

// The first style's light on the highest upward face under `p` (file units, z up) within `reach`
// units, into `rgb` (0..1 a channel). False when there is none.
bool light_below(const LightProbe& probe, const Map& map, const float p[3], float rgb[3],
                 float reach = 4096);

}  // namespace ge::brush
