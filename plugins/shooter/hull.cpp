// The hull trace's recursion and the box hull's tables (hull.hpp states the rules).
//
// ge:summary The hull trace's recursion, point contents and the six-plane box hull
// ge:keywords trace_hull recursive-trace split impact box-hull point_contents
// ge:skill shooter
// ge:test tests/shooter_hull_test.cpp
#include "shooter/hull.hpp"

namespace ge::shooter {

namespace {

// What one stretch of the segment did: stayed out of solid all the way, was (or ended) in
// solid with no impact recorded, or recorded an impact.
enum class Leg : uint8_t { empty, solid, blocked };

float distance_to(const Plane& pl, Vec3f p) {
  if (pl.type < 3) return p[pl.type] - pl.dist;
  return dot(pl.normal, p) - pl.dist;
}

Leg check(const Hull& hull, int32_t num, float p1f, float p2f, Vec3f p1, Vec3f p2, HullTrace& tr) {
  float t1 = 0, t2 = 0;
  const Plane* pl = nullptr;
  const ClipNode* node = nullptr;
  // Down the tree while the whole segment is on one side of each plane.
  for (;;) {
    if (num < 0) {
      if (num != kContentsSolid) {
        tr.allsolid = false;
        if (num == kContentsEmpty)
          tr.inopen = true;
        else
          tr.inwater = true;
        return Leg::empty;
      }
      if (tr.allsolid) tr.startsolid = true;  // no open leaf yet: this is where it started
      return Leg::solid;
    }
    node = &hull.nodes[num];
    pl = &hull.planes[node->plane];
    t1 = distance_to(*pl, p1);
    t2 = distance_to(*pl, p2);
    if (t1 >= 0 && t2 >= 0) {
      num = node->children[0];
    } else if (t1 < 0 && t2 < 0) {
      num = node->children[1];
    } else {
      break;
    }
  }

  // The segment straddles this plane: split it, near side first.
  float frac = t1 / (t1 - t2);
  if (frac < 0) frac = 0;
  if (frac > 1) frac = 1;
  const float midf = p1f + (p2f - p1f) * frac;
  const Vec3f mid = {p1.x + frac * (p2.x - p1.x), p1.y + frac * (p2.y - p1.y),
                     p1.z + frac * (p2.z - p1.z)};
  const int side = t1 < t2 ? 1 : 0;

  const Leg near = check(hull, node->children[side], p1f, midf, p1, mid, tr);
  if (near == Leg::blocked) return near;
  if (near == Leg::solid && !tr.allsolid) return near;  // in solid after the open: it stands

  const Leg far = check(hull, node->children[side ^ 1], midf, p2f, mid, p2, tr);
  if (far != Leg::solid) return far;  // open (a start in solid left it) or an impact deeper
  if (near != Leg::empty) return Leg::solid;

  // THE IMPACT: open on the near side, solid on the far side of this plane.
  if (side == 0) {
    tr.plane = *pl;
  } else {
    tr.plane.normal = -pl->normal;
    tr.plane.dist = -pl->dist;
    tr.plane.type = pl->type;
  }
  frac = t1 < t2 ? (t1 + kDistEpsilon) / (t1 - t2) : (t1 - kDistEpsilon) / (t1 - t2);
  if (frac < 0) frac = 0;
  if (frac > 1) frac = 1;
  tr.fraction = p1f + (p2f - p1f) * frac;
  tr.endpos = {p1.x + frac * (p2.x - p1.x), p1.y + frac * (p2.y - p1.y),
               p1.z + frac * (p2.z - p1.z)};
  return Leg::blocked;
}

}  // namespace

uint8_t plane_type(Vec3f n) {
  if (n.x == 1 && n.y == 0 && n.z == 0) return 0;
  if (n.x == 0 && n.y == 1 && n.z == 0) return 1;
  if (n.x == 0 && n.y == 0 && n.z == 1) return 2;
  return 3;
}

int point_contents(const Hull& hull, Vec3f p) {
  int32_t num = hull.first_node;
  while (num >= 0) {
    const ClipNode& node = hull.nodes[num];
    num = node.children[distance_to(hull.planes[node.plane], p) < 0 ? 1 : 0];
  }
  return num;
}

HullTrace trace_hull(const Hull& hull, Vec3f p1, Vec3f p2) {
  HullTrace tr;
  tr.endpos = p2;
  check(hull, hull.first_node, 0, 1, p1, p2, tr);
  if (tr.allsolid) {
    tr.startsolid = true;
    tr.fraction = 1;
    tr.endpos = p1;
  }
  return tr;
}

BoxHull make_box_hull(Vec3f mins, Vec3f maxs) {
  BoxHull b;
  for (int i = 0; i < 6; i++) {
    const int axis = i >> 1;
    Plane& pl = b.planes[i];
    pl.normal = {};
    pl.normal[axis] = 1;
    pl.type = static_cast<uint8_t>(axis);
    pl.dist = (i & 1) == 0 ? maxs[axis] : mins[axis];
    ClipNode& n = b.nodes[i];
    n.plane = i;
    const int32_t next = i == 5 ? kContentsSolid : i + 1;
    if ((i & 1) == 0) {
      n.children[0] = kContentsEmpty;
      n.children[1] = next;
    } else {
      n.children[0] = next;
      n.children[1] = kContentsEmpty;
    }
  }
  return b;
}

}  // namespace ge::shooter
