// The trace through a Quake II map's world and what moves in it: the brushes, each solid mover in
// its own frame, the monsters' and corpses' boxes and the teleporters' discs; the move's world.
//
// ge:summary Quake II's trace through the world, its movers, monsters, corpses and pads
// ge:keywords q2 map-trace sweep-box mover-trace monster-box teleporter-disc move-world
// ge:skill shooter
// ge:test tests/shooters_edge_q2_map_test.cpp
#include <algorithm>
#include <utility>

#include "brush/trace.hpp"
#include "shooters_edge/server/q2_map.hpp"
#include "shooters_edge/server/q2_server.hpp"

namespace shooters_edge::server {

namespace {

bool brush(const Q2Entity& e) { return !e.model.empty() && e.model[0] == '*'; }

// A turning mover's reach: the box it can sweep, any way round.
sh::Vec3f abs_min(const Q2Entity& e) {
  if (!e.turns) return e.origin + e.mins;
  const auto far = [](float a, float b) { return std::max(a < 0 ? -a : a, b < 0 ? -b : b); };
  const float x = far(e.mins.x, e.maxs.x), y = far(e.mins.y, e.maxs.y), z = far(e.mins.z, e.maxs.z);
  const float r = sh::root_of(x * x + y * y + z * z);
  return e.origin - sh::Vec3f{r, r, r};
}
sh::Vec3f abs_max(const Q2Entity& e) {
  if (!e.turns) return e.origin + e.maxs;
  return e.origin + (e.origin - abs_min(e));
}

// A point in a mover's own frame at `angles` (forward, left, up), and back.
sh::Vec3f to_local(sh::Vec3f d, sh::Vec3f angles) {
  const sh::ViewAxes a = sh::angle_vectors(angles);
  return {sh::dot(d, a.forward), -sh::dot(d, a.right), sh::dot(d, a.up)};
}
sh::Vec3f to_world(sh::Vec3f l, sh::Vec3f angles) {
  const sh::ViewAxes a = sh::angle_vectors(angles);
  return a.forward * l.x - a.right * l.y + a.up * l.z;
}

bool overlap(sh::Vec3f alo, sh::Vec3f ahi, sh::Vec3f blo, sh::Vec3f bhi) {
  return alo.x < bhi.x && ahi.x > blo.x && alo.y < bhi.y && ahi.y > blo.y && alo.z < bhi.z &&
         ahi.z > blo.z;
}

}  // namespace

float sweep_box(sh::Vec3f start, sh::Vec3f end, sh::Vec3f mins, sh::Vec3f maxs, sh::Vec3f lo,
                sh::Vec3f hi, sh::Vec3f& normal) {
  // The point's path against the box grown by the moving one.
  const float a[3] = {start.x, start.y, start.z}, b[3] = {end.x, end.y, end.z};
  const float l[3] = {lo.x - maxs.x, lo.y - maxs.y, lo.z - maxs.z};
  const float h[3] = {hi.x - mins.x, hi.y - mins.y, hi.z - mins.z};
  // Started inside (a monster stepped into him, or his snap to the eighth-unit grid put him a
  // fraction in): only a move that ends inside too is held; one out of it is free, else the two
  // stay locked together for good (base1: the script frozen beside a soldier for minutes).
  bool start_in = true, end_in = true;
  for (int k = 0; k < 3; k++) {
    start_in = start_in && a[k] > l[k] && a[k] < h[k];
    end_in = end_in && b[k] > l[k] && b[k] < h[k];
  }
  if (start_in) {
    normal = {};
    return end_in ? 0 : -1;
  }
  // (From below any start: a box touching a face and moving into it meets that face at 0 with
  // its normal; started at 0, the face was never taken, and a zero normal held him there for good,
  // falling in place beside a corpse on base2.)
  float t0 = -1e30f, t1 = 1;
  int axis = -1;
  float side = 0;
  for (int k = 0; k < 3; k++) {
    const float d = b[k] - a[k];
    if (d == 0) {
      if (a[k] <= l[k] || a[k] >= h[k]) return -1;
      continue;
    }
    float near = (l[k] - a[k]) / d, far = (h[k] - a[k]) / d;
    float face = -1;
    if (near > far) {
      std::swap(near, far);
      face = 1;
    }
    if (near > t0) {
      t0 = near;
      axis = k;
      side = face;
    }
    t1 = std::min(t1, far);
    if (t0 >= t1) return -1;
  }
  if (!(t0 > 0)) t0 = 0;    // (and no -0 from a face touched)
  if (t0 >= t1) return -1;  // touching and moving off it
  normal = {};
  if (axis == 0) normal.x = side;
  if (axis == 1) normal.y = side;
  if (axis == 2) normal.z = side;
  return t0;
}

ge::classic::Q2Trace map_trace(const Q2Server& s, sh::Vec3f start, sh::Vec3f mins, sh::Vec3f maxs,
                               sh::Vec3f end, int32_t mask, int skip) {
  // A turned mover is traced in its own frame: the line turned into it, the box kept upright.
  const auto one = [&](sh::Vec3f o, uint32_t model, sh::Vec3f angles) {
    sh::Vec3f from = start - o, to = end - o;
    if (angles.x != 0 || angles.y != 0 || angles.z != 0) {
      from = to_local(from, angles);
      to = to_local(to, angles);
    }
    const float a[3] = {from.x, from.y, from.z};
    const float b[3] = {to.x, to.y, to.z};
    const float lo[3] = {mins.x, mins.y, mins.z}, hi[3] = {maxs.x, maxs.y, maxs.z};
    return s.tracer->trace(a, b, lo, hi, mask, model);
  };
  const ge::brush::Trace w = one({}, 0, {});
  ge::classic::Q2Trace r;
  r.all_solid = w.all_solid;
  r.start_solid = w.start_solid;
  r.fraction = w.fraction;
  r.end = {w.end[0], w.end[1], w.end[2]};
  r.normal = {w.plane.n[0], w.plane.n[1], w.plane.n[2]};
  r.surface_flags = w.flags;
  r.contents = w.contents;
  r.entity = w.fraction < 1 || w.start_solid ? 0 : -1;
  for (size_t i = 0; i < s.entities.size(); i++) {
    const Q2Entity& e = s.entities[i];
    const int32_t meets = e.corpse ? sh::kSqDeadMonster : sh::kSqMonster;
    if (e.box && !e.gone && (mask & meets) != 0 && int(i) != skip) {  // a monster, or a corpse
      sh::Vec3f n;
      const float f = sweep_box(start, end, mins, maxs, e.origin + e.mins, e.origin + e.maxs, n);
      if (f >= 0 && (f < r.fraction || (f == 0 && !r.start_solid))) {
        // Started inside it, not only touching its face (a box between two touching ones was
        // held still by both as if buried: base2's bodies by the south pool).
        if (f == 0 && overlap(start + mins, start + maxs, e.origin + e.mins, e.origin + e.maxs))
          r.start_solid = true;
        r.fraction = f;
        r.end = start + (end - start) * f;
        r.normal = n;
        r.surface_flags = 0;
        r.contents = meets;
        r.entity = map_row(static_cast<int>(i));
      }
      continue;
    }
    if (e.pad != 0 && !e.gone && (mask & sh::kSqSolid) != 0 && int(i) != skip) {  // a disc
      sh::Vec3f n;
      const float f =
          sweep_box(start, end, mins, maxs, e.origin + kDiscMins, e.origin + kDiscMaxs, n);
      if (f >= 0 && (f < r.fraction || (f == 0 && !r.start_solid))) {
        if (f == 0 &&
            overlap(start + mins, start + maxs, e.origin + kDiscMins, e.origin + kDiscMaxs))
          r.start_solid = true;
        r.fraction = f;
        r.end = start + (end - start) * f;
        r.normal = n;
        r.surface_flags = 0;
        r.contents = sh::kSqSolid;
        r.entity = map_row(static_cast<int>(i));
      }
      continue;
    }
    if (e.gone || !e.solid || e.modelindex <= 0 || !brush(e)) continue;
    // Only when the sweep's box can reach the mover's.
    const sh::Vec3f slo{std::min(start.x, end.x) + mins.x, std::min(start.y, end.y) + mins.y,
                        std::min(start.z, end.z) + mins.z};
    const sh::Vec3f shi{std::max(start.x, end.x) + maxs.x, std::max(start.y, end.y) + maxs.y,
                        std::max(start.z, end.z) + maxs.z};
    if (!overlap(slo - sh::Vec3f{1, 1, 1}, shi + sh::Vec3f{1, 1, 1}, abs_min(e), abs_max(e)))
      continue;
    const ge::brush::Trace t = one(e.origin, uint32_t(e.modelindex), e.angles);
    const bool turned = e.angles.x != 0 || e.angles.y != 0 || e.angles.z != 0;
    if (t.all_solid) r.all_solid = true;
    if (t.start_solid || t.fraction < r.fraction) {
      r.start_solid = r.start_solid || t.start_solid;
      if (t.fraction < r.fraction) {
        r.fraction = t.fraction;
        r.end = sh::Vec3f{t.end[0], t.end[1], t.end[2]} + e.origin;
        r.normal = {t.plane.n[0], t.plane.n[1], t.plane.n[2]};
        if (turned) {
          r.end = start + (end - start) * t.fraction;
          r.normal = to_world(r.normal, e.angles);
        }
        r.surface_flags = t.flags;
        r.contents = t.contents;
      }
      r.entity = map_row(static_cast<int>(i));
    }
  }
  return r;
}

int32_t map_contents(const Q2Server& s, sh::Vec3f p) {
  if (s.tracer == nullptr) return 0;
  const float q[3] = {p.x, p.y, p.z};
  int32_t c = s.tracer->contents(q);
  for (const Q2Entity& e : s.entities) {
    if (e.gone || !e.solid || e.modelindex <= 0 || !brush(e)) continue;
    if (!overlap(p, p, abs_min(e), abs_max(e))) continue;
    sh::Vec3f l = p - e.origin;
    if (e.angles.x != 0 || e.angles.y != 0 || e.angles.z != 0) l = to_local(l, e.angles);
    const float lq[3] = {l.x, l.y, l.z};
    c |= s.tracer->contents(lq, uint32_t(e.modelindex));
  }
  return c;
}

namespace {

ge::classic::Q2Trace world_trace(void* ctx, sh::Vec3f start, sh::Vec3f mins, sh::Vec3f maxs,
                                 sh::Vec3f end) {
  const auto* w = static_cast<const MapWorld*>(ctx);
  ge::classic::Q2Trace t = map_trace(*w->s, start, mins, maxs, end, w->mask);
  if (t.entity < 0) t.entity = 0;  // the world answers every trace, hit or not
  return t;
}

int32_t world_contents(void* ctx, sh::Vec3f p) {
  return map_contents(*static_cast<const MapWorld*>(ctx)->s, p);
}

}  // namespace

ge::classic::Q2World MapWorld::world() { return {this, world_trace, world_contents}; }

}  // namespace shooters_edge::server
