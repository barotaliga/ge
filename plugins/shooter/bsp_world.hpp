// A BRUSH MAP'S MODELS AS THE SERVER TRACES THEM: a version 29 or BSP2 map read by the brush
// plugin (brush/map.hpp) turned into the clip hulls a trace walks, one `BrushModel` a model.
//
// - **Hull 0** (points) is the map's node tree: each node's plane, and a child that is a leaf
//   becomes that leaf's contents number. A model whose head is a leaf gets one node whose two
//   children are that leaf's contents.
// - **Hulls 1 and 2** (the player's box, the large box) are the map's clip nodes as they stand,
//   each starting at the model's own head for that hull.
// - **Planes** are the map's, each given its type (0, 1, 2 for +x, +y, +z exactly, else 3) as the
//   trace's fast path reads it.
// - Model k is `models[k]`: 0 the world, then the map's inline brush models (`*1`, `*2` ...) in
//   order, with the bounds the map stores; `clip_mins` is the box each hull was grown by.
//
// A leaf whose contents is not one of the format's numbers (-1 to -15) is refused, since in hull 0
// it would read as a node. Only 29 and BSP2 store clip hulls; 38 and 46 sweep through brushes and
// are refused here, naming the family. The views point into `BspWorld`'s arrays: move it, never
// copy it, once a model has been taken. Held by tests/shooter_bsp_world_test.cpp.
//
// ge:summary A brush map's models as the server traces them: hull 0 from the tree, hulls 1 and 2
// ge:keywords bsp map world brush-model clip-hull hull0 hull1 hull2 clipnodes nodes leaf contents
// ge:keywords bsp_world load inline-model planes plane-type
// ge:skill shooter brush
// ge:test tests/shooter_bsp_world_test.cpp
#pragma once

#include <string>
#include <vector>

#include "brush/map.hpp"
#include "shooter/brush_hull.hpp"

namespace ge::shooter {

struct BspWorld {
  std::vector<Plane> planes;
  std::vector<ClipNode> point_nodes;  // hull 0: the node tree, leaves as contents
  std::vector<ClipNode> clip_nodes;   // hulls 1 and 2
  std::vector<BrushModel> models;     // [0] the world, then the inline models
};

// Fills `out` from `map`; false with the reason (and the next step) in `*error`.
bool bsp_world(const brush::Map& map, BspWorld* out, std::string* error);

}  // namespace ge::shooter
