// A BOX SWEPT THROUGH A MAP'S BRUSHES (38): where a box (mins, maxs about a point) moving from a
// start to an end first touches a brush whose contents share a bit with a mask, and the contents
// at a point. The trace a version 38 map's player move, shots and monsters collide through; every
// float rule below is fixed so the bits are the same everywhere (tests/golden/brush_trace.txt).
//
// - **The position test** (start equals end): every leaf the box grown by 1 touches, front child
//   first; the first brush the box is inside of (its corner for each side, behind or on the side's
//   plane) makes the trace all solid and start solid at fraction 0, with that brush's contents.
// - **The sweep** walks the node tree from the model's head node with the piece's fractions and
//   points. A node's offset is the box's reach across its plane: the box's extent on an axial
//   plane's axis (type 0..2: only that coordinate is tested), else the extents times the normal's
//   magnitudes, each product in float and their sum in double. A piece wholly in front of the
//   offset goes to the front child, wholly behind −offset to the back; otherwise it is split at
//   the near and far fractions, each (t1 ∓ offset ± 1/32) × (1 / (t1 − t2)), the sum and the
//   reciprocal rounded to float, the product in double, held to 0..1. A leaf whose contents miss
//   the mask is skipped; each of its brushes is clipped once a trace.
// - **Clipping to a brush** uses the whole trace's start and end: each side's plane is pushed out
//   by the box's corner; the entering fraction is the largest (d1 − 1/32) / (d1 − d2), the leaving
//   the smallest (d1 + 1/32) / (d1 − d2), each in double rounded to float. A box that never starts
//   out is start solid (and all solid when it never gets out); otherwise an entering fraction
//   below the leaving one and the trace's fraction becomes the trace's, with the plane, the side's
//   surface and the brush's contents. So a trace stops 1/32 short of the plane it hits.
// - The end point is the end at fraction 1, else start + fraction × (end − start) in float.
//
// A trace through a model other than the world reads that model's head node: the caller moves the
// points into the model's frame (entities, rotation and the area tree are the game's). A map
// whose tree shares a node between parents still ends: the walk stops after visiting as many
// nodes and leaves as the map has.
//
// ge:summary A box swept through a version 38 map's brushes, and the contents at a point
// ge:keywords trace sweep box brush clip position-test start-solid all-solid fraction epsilon
// ge:keywords point-contents mask contents surface plane head-node leaf-brushes collision
// ge:skill brush
// ge:test tests/brush_trace_test.cpp
#pragma once

#include <cstdint>
#include <vector>

#include "brush/map.hpp"

namespace ge::brush {

// The distance a trace stops short of the plane it hits.
inline constexpr double kTraceEpsilon = 0.03125;

struct Trace {
  bool all_solid = false;    // the box never left a solid
  bool start_solid = false;  // the box started inside a solid
  float fraction = 1;        // how far it went, 0..1 (1: it hit nothing)
  float end[3] = {};
  Plane plane;  // the plane hit (its normal faces the box), when fraction < 1
  // The hit side's surface: its texture (−1: none), flags and value.
  int32_t texture = -1;
  int32_t flags = 0, value = 0;
  int32_t contents = 0;  // the hit brush's contents
};

// Traces a map, keeping which brushes a trace has clipped. Build one for a map and reuse it: a
// trace allocates nothing once built. The map must outlive it.
class BrushTracer {
 public:
  explicit BrushTracer(const Map& map);

  // The box (mins, maxs) swept from `start` to `end` through model `model`'s brushes whose
  // contents share a bit with `mask`.
  Trace trace(const float start[3], const float end[3], const float mins[3], const float maxs[3],
              int32_t mask, uint32_t model = 0);
  // The contents of the leaf `p` falls in, through model `model`'s tree (0 for a map with no
  // brushes in that leaf: the leaf's contents are the OR of the brushes that fill it).
  int32_t contents(const float p[3], uint32_t model = 0) const;

  const Map& map() const { return *map_; }

 private:
  struct Sweep;
  const Map* map_;
  std::vector<uint32_t> stamp_;  // a brush's last trace
  uint32_t pass_ = 0;
  std::vector<int32_t> stack_;  // the position test's nodes
};

}  // namespace ge::brush
