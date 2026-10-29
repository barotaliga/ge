// CLIP HULLS: the tree of planes a world, a door or a box is swept through, the contents at a
// point, and the line trace that finds where a segment first passes into solid.
//
// A hull is `planes` (a normal, a dist and a type: 0, 1, 2 when the normal is exactly +x, +y,
// +z, else 3) and `nodes` (a plane and two children). A child is a node index, or a leaf: a
// negative contents number (`kContents*`). A point goes to the FRONT child when
// `normal . p - dist >= 0`, to the back child otherwise. A world keeps three hulls: hull 0 for
// points, hull 1 for the player's box (-16, -16, -24)..(16, 16, 32), hull 2 for large boxes; a
// box hull (`BoxHull`) is six axial planes built on the fly for a box grown by the mover's box.
//
// THE TRACE (`trace_hull`) walks the tree from the first node. Where the segment straddles a
// node's plane it splits at `clamp(t1 / (t1 - t2), 0, 1)` in float, traces the part on the
// start's side first, then the far part. An IMPACT is a near part that ended in the open and a
// far part that is solid: the plane faces the near side and the end point is placed 1/32 unit
// back toward the start (the only place the 1/32 is applied). A trace whose first leaf is solid
// STARTED IN SOLID; one that never reached an open or liquid leaf is ALL SOLID, ends at its start
// with fraction 1, and may still leave the solid (leaving a solid is allowed). Pure and
// allocation-free; the recursion is as deep as the tree. Held by tests/shooter_hull_test.cpp.
//
// ge:summary Clip hulls: planes and nodes, point contents, the box hull, the hull line trace
// ge:keywords clip-hull hull clipnode plane contents point-contents box-hull trace line-trace
// ge:keywords startsolid allsolid inopen inwater fraction impact dist-epsilon solid water
// ge:keywords slime lava trace_hull point_contents
// ge:skill shooter
// ge:test tests/shooter_hull_test.cpp
#pragma once

#include <cstdint>

#include "shooter/vec.hpp"

namespace ge::shooter {

// The format's contents numbers. "Water or worse" is a number of kContentsWater or less.
constexpr int kContentsEmpty = -1;
constexpr int kContentsSolid = -2;
constexpr int kContentsWater = -3;
constexpr int kContentsSlime = -4;
constexpr int kContentsLava = -5;

// How far back toward the start an impact's end point is placed, units.
constexpr float kDistEpsilon = 0.03125f;

struct Plane {
  Vec3f normal;
  float dist = 0;
  uint8_t type = 3;  // 0, 1, 2: the normal is exactly +x, +y, +z; 3 otherwise
};

// The type a plane with this normal takes.
uint8_t plane_type(Vec3f normal);

struct ClipNode {
  int32_t plane = 0;
  int32_t children[2] = {kContentsEmpty, kContentsEmpty};  // [0] front, [1] back
};

// A view of a hull's arrays; the arrays belong to whoever built it (a map, `BoxHull`,
// brush_hull.hpp).
struct Hull {
  const Plane* planes = nullptr;
  const ClipNode* nodes = nullptr;
  int32_t first_node = 0;
  int32_t last_node = -1;
};

// The leaf a point falls into: its contents number.
int point_contents(const Hull& hull, Vec3f p);

struct HullTrace {
  bool allsolid = true;     // never reached an open or liquid leaf
  bool startsolid = false;  // the first leaf reached was solid
  bool inopen = false;      // passed through an empty leaf
  bool inwater = false;     // passed through a liquid leaf
  float fraction = 1;       // share of the segment travelled before the impact
  Vec3f endpos;             // where the segment stops (the start when all solid)
  Plane plane;              // the plane hit, facing the start's side
  int ent = -1;             // which solid of a move's list (the mover fills it; -1 none)
};

// Sweeps the segment p1 -> p2 (in the hull's own space) through the hull.
HullTrace trace_hull(const Hull& hull, Vec3f p1, Vec3f p2);

// THE SIX-PLANE BOX HULL of a box (mins, maxs): planes +x at maxs.x and mins.x, +y at maxs.y and
// mins.y, +z at maxs.z and mins.z; node i uses plane i; an even node's front child is empty and
// its back child the next node, an odd node's back child is empty and its front child the next
// node, and the last node's front child is solid. So a point is solid when mins <= p < maxs on
// every axis.
struct BoxHull {
  Plane planes[6];
  ClipNode nodes[6];

  Hull hull() const { return {planes, nodes, 0, 5}; }
};
BoxHull make_box_hull(Vec3f mins, Vec3f maxs);

}  // namespace ge::shooter
