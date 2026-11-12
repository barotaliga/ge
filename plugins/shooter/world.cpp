// The move's trace over its list of solids (world.hpp states the rules).
//
// ge:summary The player box trace over a move's solids, the free-box test, point contents
// ge:keywords player_trace box_free brush_contents water_contents grown-box skip-bounds
// ge:skill shooter
// ge:test tests/shooter_world_test.cpp
#include "shooter/world.hpp"

#include "brush/trace.hpp"
#include "shooter/brush_hull.hpp"
#include "shooter/sequel_move.hpp"

namespace ge::shooter {

namespace {

// A box solid's extent grown by the player's box, about the solid's origin.
void grown_box(const Solid& s, Vec3f& mins, Vec3f& maxs) {
  mins = s.mins - kPlayerMaxs;
  maxs = s.maxs - kPlayerMins;
}

// What a living player's box collides with in a version 38 map, but monsters' bodies: the list
// holds those as boxes.
constexpr int32_t kBrushesMask = kSqSolid | kSqPlayerClip | kSqWindow;

// A 38 map's contents bits as the hull family's number.
int contents_number(int32_t bits) {
  if ((bits & (kSqSolid | kSqWindow)) != 0) return kContentsSolid;
  if ((bits & kSqLava) != 0) return kContentsLava;
  if ((bits & kSqSlime) != 0) return kContentsSlime;
  if ((bits & kSqWater) != 0) return kContentsWater;
  return kContentsEmpty;
}

// A box (`lo`..`hi`) swept from `start` to `end` (the solid's own space) through its brushes, as a
// hull trace.
HullTrace brushes_trace(const Solid& s, Vec3f start, Vec3f end, Vec3f lo, Vec3f hi, int32_t mask) {
  const float a[3] = {start.x, start.y, start.z}, b[3] = {end.x, end.y, end.z};
  const float mn[3] = {lo.x, lo.y, lo.z}, mx[3] = {hi.x, hi.y, hi.z};
  const ge::brush::Trace t = s.tracer->trace(a, b, mn, mx, mask, s.model);
  HullTrace tr;
  tr.allsolid = t.all_solid;
  tr.startsolid = t.start_solid;
  tr.inopen = !t.all_solid;
  tr.fraction = t.fraction;
  tr.endpos = {t.end[0], t.end[1], t.end[2]};
  tr.plane.normal = {t.plane.n[0], t.plane.n[1], t.plane.n[2]};
  tr.plane.dist = t.plane.d;
  tr.plane.type = plane_type(tr.plane.normal);
  return tr;
}

// What is at `p` in a brush model of either family.
int solid_contents(const Solid& s, Vec3f p) {
  const Vec3f q = p - s.origin;
  if (s.kind == SolidKind::brushes) {
    const float at[3] = {q.x, q.y, q.z};
    return contents_number(s.tracer->contents(at, s.model));
  }
  return point_contents(s.point_hull, q);
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
    } else if (s.kind == SolidKind::brushes) {
      tr = brushes_trace(s, start - s.origin, end - s.origin, kPlayerMins, kPlayerMaxs,
                         kBrushesMask);
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
    } else if (s.kind == SolidKind::brushes) {
      const HullTrace tr =
          brushes_trace(s, p - s.origin, p - s.origin, kPlayerMins, kPlayerMaxs, kBrushesMask);
      c = tr.startsolid ? kContentsSolid : kContentsEmpty;
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
  return solid_contents(w, p);
}

int brush_contents(const MoveWorld& world, Vec3f p) {
  int first = kContentsEmpty;
  for (int i = 0; i < world.count; i++) {
    const Solid& s = world.solids[i];
    if (s.kind == SolidKind::box) continue;
    const int c = solid_contents(s, p);
    if (c == kContentsSolid) return c;
    if (first == kContentsEmpty) first = c;
  }
  return first;
}

HullTrace point_trace(const MoveWorld& world, Vec3f start, Vec3f end) {
  HullTrace tr;
  if (world.count == 0) {
    tr.allsolid = false;
    tr.endpos = end;
    return tr;
  }
  const Solid& w = world.solids[0];
  if (w.kind == SolidKind::brushes) {
    tr = brushes_trace(w, start - w.origin, end - w.origin, {}, {}, kSqSolid | kSqWindow);
  } else {
    tr = trace_hull(w.point_hull, start - w.origin, end - w.origin);
  }
  tr.endpos = tr.endpos + w.origin;
  return tr;
}

Solid brushes_solid(ge::brush::BrushTracer& tracer, uint32_t model, Vec3f origin, Vec3f mins,
                    Vec3f maxs) {
  Solid s;
  s.kind = SolidKind::brushes;
  s.tracer = &tracer;
  s.model = model;
  s.origin = origin;
  s.mins = mins;
  s.maxs = maxs;
  return s;
}

}  // namespace ge::shooter
