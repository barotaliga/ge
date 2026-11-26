// The hull trace's recursion and the box hull's tables (hull.hpp states the rules).
//
// ge:summary The hull trace's recursion, point contents and the six-plane box hull
// ge:keywords trace_hull recursive-trace split impact box-hull point_contents
// ge:skill shooter
// ge:test tests/shooter_hull_test.cpp
#include "shooter/hull.hpp"

#include "core/parallel.hpp"

namespace ge::shooter {

namespace {

float distance_to(const Plane& pl, Vec3f p) {
  if (pl.type < 3) return p[pl.type] - pl.dist;
  return dot(pl.normal, p) - pl.dist;
}

// The contents a point falls into walking down from node `num`.
int contents_from(const Hull& hull, int32_t num, Vec3f p) {
  while (num >= 0) {
    const ClipNode& node = hull.nodes[num];
    num = node.children[distance_to(hull.planes[node.plane], p) < 0 ? 1 : 0];
  }
  return num;
}

// THE ORIGINAL WALK: one call over p1..p2 (fractions p1f..p2f of the whole trace): true "go on",
// false "stop".
bool check_original(const Hull& hull, int32_t num, float p1f, float p2f, Vec3f p1, Vec3f p2,
                    HullTrace& tr) {
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
      } else {
        tr.startsolid = true;
      }
      return true;
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

  // The segment straddles this plane: the split already stands 1/32 back on p1's side, and the
  // far part starts there.
  float frac = t1 < 0 ? (t1 + kDistEpsilon) / (t1 - t2) : (t1 - kDistEpsilon) / (t1 - t2);
  if (frac < 0) frac = 0;
  if (frac > 1) frac = 1;
  float midf = p1f + (p2f - p1f) * frac;
  Vec3f mid = {p1.x + frac * (p2.x - p1.x), p1.y + frac * (p2.y - p1.y),
               p1.z + frac * (p2.z - p1.z)};
  const int side = t1 < 0 ? 1 : 0;

  if (!check_original(hull, node->children[side], p1f, midf, p1, mid, tr)) return false;
  if (contents_from(hull, node->children[side ^ 1], mid) != kContentsSolid)
    return check_original(hull, node->children[side ^ 1], midf, p2f, mid, p2, tr);
  if (tr.allsolid) return false;  // never out of the solid: no impact

  // THE IMPACT: the plane facing p1's side; the end point stepped back while it is in solid.
  if (side == 0) {
    tr.plane = *pl;
  } else {
    tr.plane.normal = -pl->normal;
    tr.plane.dist = -pl->dist;
    tr.plane.type = pl->type;
  }
  while (contents_from(hull, hull.first_node, mid) == kContentsSolid) {
    frac -= 0.1f;
    if (frac < 0) break;
    tr.backups++;
    midf = p1f + (p2f - p1f) * frac;
    mid = {p1.x + frac * (p2.x - p1.x), p1.y + frac * (p2.y - p1.y), p1.z + frac * (p2.z - p1.z)};
  }
  tr.fraction = midf;
  tr.endpos = mid;
  return false;
}

// THE NETWORKED WALK: the split on the plane itself, the 1/32 applied only at the impact and
// clamped within that split's segment (so at a slant-and-wall corner the end can stand on the
// plane: tests/golden/shooter_recording.txt's steep_climb holds it).
// What one stretch of the segment did: stayed out of solid all the way, was (or ended) in
// solid with no impact recorded, or recorded an impact.
enum class Leg : uint8_t { empty, solid, blocked };

Leg check_networked(const Hull& hull, int32_t num, float p1f, float p2f, Vec3f p1, Vec3f p2,
                    HullTrace& tr) {
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

  const Leg near = check_networked(hull, node->children[side], p1f, midf, p1, mid, tr);
  if (near == Leg::blocked) return near;
  if (near == Leg::solid && !tr.allsolid) return near;  // in solid after the open: it stands

  const Leg far = check_networked(hull, node->children[side ^ 1], midf, p2f, mid, p2, tr);
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
  if (hull.cells) {
    const int c = hull.cells->contents(p);
    if (c != 0) return c;
  }
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
  check_networked(hull, hull.first_node, 0, 1, p1, p2, tr);
  if (tr.allsolid) {
    tr.startsolid = true;
    tr.fraction = 1;
    tr.endpos = p1;
  }
  return tr;
}

HullTrace trace_hull_original(const Hull& hull, Vec3f p1, Vec3f p2) {
  HullTrace tr;
  tr.endpos = p2;
  check_original(hull, hull.first_node, 0, 1, p1, p2, tr);
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

int ContentsCells::contents(Vec3f p) const {
  int32_t i[3];
  for (int a = 0; a < 3; a++) {
    const float f = (p[a] - lo[a]) / cell;
    if (!(f >= 0) || f >= float(n[a])) return 0;  // off the grid (or NaN)
    i[a] = int32_t(f);
  }
  return at[(size_t(i[2]) * size_t(n[1]) + size_t(i[1])) * size_t(n[0]) + size_t(i[0])];
}

ContentsCells contents_cells(const Hull& hull, Vec3f mins, Vec3f maxs, float cell) {
  ContentsCells g;
  // At most about four million cubes.
  for (;;) {
    double count = 1;
    for (int a = 0; a < 3; a++) count *= double(maxs[a] - mins[a]) / cell + 3;
    if (count <= 4e6) break;
    cell *= 2;
  }
  g.cell = cell;
  for (int a = 0; a < 3; a++) {
    g.lo[a] = mins[a] - cell;
    g.n[a] = int32_t((maxs[a] - mins[a]) / cell) + 3;
  }
  g.at.assign(size_t(g.n[0]) * size_t(g.n[1]) * size_t(g.n[2]), 0);
  // A cube's box is grown by a unit a side, far more than the float rounding of a plane's
  // distance or of the cube's index: a point the walk at run time sends into a leaf lies in a box
  // this walk sent into that leaf too.
  constexpr double kGrow = 1.0;
  // Each cube on its own: the slabs of z in bands over the workers, the same grid on any count.
  core::parallel_bands(size_t(g.n[2]), [&](size_t z_lo, size_t z_hi) {
    std::vector<int32_t> stack;
    for (int32_t z = int32_t(z_lo); z < int32_t(z_hi); z++) {
      for (int32_t y = 0; y < g.n[1]; y++) {
        for (int32_t x = 0; x < g.n[0]; x++) {
          const int32_t i[3] = {x, y, z};
          double lo[3], hi[3];
          for (int a = 0; a < 3; a++) {
            lo[a] = double(g.lo[a]) + double(cell) * i[a] - kGrow;
            hi[a] = double(g.lo[a]) + double(cell) * (i[a] + 1) + kGrow;
          }
          int32_t one = 0;  // the contents met so far; 0 none, 1 more than one
          stack.clear();
          stack.push_back(hull.first_node);
          // A damaged tree may loop: a cube that visits more nodes than the tree holds, four
          // times over, is left to the walk.
          int64_t visits = 0;
          const int64_t most = 4 * (int64_t(hull.last_node) + 1) + 16;
          while (!stack.empty() && one != 1) {
            if (++visits > most) {
              one = 1;
              break;
            }
            const int32_t num = stack.back();
            stack.pop_back();
            if (num < 0) {
              one = one == 0 || one == num ? num : 1;
              continue;
            }
            const ClipNode& node = hull.nodes[num];
            const Plane& pl = hull.planes[node.plane];
            double near = -double(pl.dist), far = -double(pl.dist);
            for (int a = 0; a < 3; a++) {
              const double k = pl.type < 3 ? (a == pl.type ? 1.0 : 0.0) : double(pl.normal[a]);
              near += k * (k >= 0 ? lo[a] : hi[a]);
              far += k * (k >= 0 ? hi[a] : lo[a]);
            }
            if (near >= 0) {
              stack.push_back(node.children[0]);
            } else if (far < 0) {
              stack.push_back(node.children[1]);
            } else {
              stack.push_back(node.children[0]);
              stack.push_back(node.children[1]);
            }
          }
          g.at[(size_t(z) * size_t(g.n[1]) + size_t(y)) * size_t(g.n[0]) + size_t(x)] =
              int8_t(one < 0 ? one : 0);
        }
      }
    }
  });
  return g;
}

}  // namespace ge::shooter
