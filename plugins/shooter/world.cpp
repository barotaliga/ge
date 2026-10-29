// The move's trace over its list of solids (world.hpp states the rules).
//
// ge:summary The player box trace over a move's solids, the free-box test, point contents
// ge:keywords player_trace box_free brush_contents water_contents grown-box skip-bounds
// ge:skill shooter
// ge:test tests/shooter_world_test.cpp
#include "shooter/world.hpp"

#include "shooter/brush_hull.hpp"

namespace ge::shooter {

namespace {

// A box solid's extent grown by the player's box, about the solid's origin.
void grown_box(const Solid& s, Vec3f& mins, Vec3f& maxs) {
  mins = s.mins - kPlayerMaxs;
  maxs = s.maxs - kPlayerMins;
}

}  // namespace

HullTrace player_trace(const MoveWorld& world, Vec3f start, Vec3f end) {
  HullTrace total;
  total.allsolid = false;
  total.fraction = 1;
  total.endpos = end;
  total.ent = -1;

  Vec3f lo, hi;
  for (int i = 0; i < 3; i++) {
    lo[i] = (start[i] < end[i] ? start[i] : end[i]) - 1;
    hi[i] = (start[i] > end[i] ? start[i] : end[i]) + 1;
  }

  for (int i = 0; i < world.count; i++) {
    const Solid& s = world.solids[i];
    Vec3f gmins, gmaxs;
    grown_box(s, gmins, gmaxs);
    if (i > 0) {
      const Vec3f a = s.origin + gmins;
      const Vec3f b = s.origin + gmaxs;
      if (a.x > hi.x || a.y > hi.y || a.z > hi.z || b.x < lo.x || b.y < lo.y || b.z < lo.z)
        continue;
    }
    HullTrace tr;
    if (s.kind == SolidKind::box) {
      const BoxHull box = make_box_hull(gmins, gmaxs);
      tr = trace_hull(box.hull(), start - s.origin, end - s.origin);
    } else {
      tr = trace_hull(s.clip_hull, start - s.origin, end - s.origin);
    }
    if (tr.allsolid) tr.startsolid = true;
    if (tr.startsolid) tr.fraction = 0;
    if (tr.fraction < total.fraction) {
      tr.endpos = tr.endpos + s.origin;
      tr.ent = i;
      total = tr;
    }
  }
  return total;
}

bool box_free(const MoveWorld& world, Vec3f p) {
  for (int i = 0; i < world.count; i++) {
    const Solid& s = world.solids[i];
    int c;
    if (s.kind == SolidKind::box) {
      Vec3f gmins, gmaxs;
      grown_box(s, gmins, gmaxs);
      const BoxHull box = make_box_hull(gmins, gmaxs);
      c = point_contents(box.hull(), p - s.origin);
    } else {
      c = point_contents(s.clip_hull, p - s.origin);
    }
    if (c == kContentsSolid) return false;
  }
  return true;
}

int water_contents(const MoveWorld& world, Vec3f p) {
  if (world.count == 0) return kContentsEmpty;
  const Solid& w = world.solids[0];
  return point_contents(w.point_hull, p - w.origin);
}

int brush_contents(const MoveWorld& world, Vec3f p) {
  int first = kContentsEmpty;
  for (int i = 0; i < world.count; i++) {
    const Solid& s = world.solids[i];
    if (s.kind != SolidKind::brush) continue;
    const int c = point_contents(s.point_hull, p - s.origin);
    if (c == kContentsSolid) return c;
    if (first == kContentsEmpty) first = c;
  }
  return first;
}

}  // namespace ge::shooter
