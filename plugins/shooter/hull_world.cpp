// The engine mover's trace through brush models, the contents at a point, and a hull 0's liquid
// leaves laid as water volumes (hull_world.hpp states the rules).
//
// ge:summary The hull world's trace, contents and liquid leaves walked into water volumes
// ge:keywords mover_world trace_models hull-for-box offset add_liquids leaf-walk half-space
// ge:skill shooter move water
// ge:test tests/shooter_hull_world_test.cpp
#include "shooter/hull_world.hpp"

#include "water/liquids.hpp"

namespace ge::shooter {

namespace {

Vec3f origin_of(const HullWorld& w, int i) { return w.origins ? w.origins[i] : Vec3f{}; }

// A map point in double: the engine's axes turned, nothing rounded yet.
struct MapD {
  double x, y, z;
};
MapD map_of(const move::Vec3& e) { return {e.z, -e.x, e.y}; }
Vec3f hull_point(const MapD& p, const MapD& shift) {
  return {static_cast<float>(p.x + shift.x), static_cast<float>(p.y + shift.y),
          static_cast<float>(p.z + shift.z)};
}

move::Hit trace_models(const void* ctx, const move::Box& box, const move::Vec3& from,
                       const move::Vec3& to) {
  const HullWorld& w = *static_cast<const HullWorld*>(ctx);
  // The box's mins in the map's axes about the feet, and the hull its width falls to.
  const MapD bmin = {box.mins.z, -box.maxs.x, box.mins.y};
  const double width = box.maxs.z - box.mins.z;
  const int h = width < 3 ? 0 : width <= kPlayerMaxs.x - kPlayerMins.x ? 1 : 2;
  const MapD p1 = map_of(from), p2 = map_of(to);

  HullTrace best;
  best.fraction = 2;  // any first trace wins
  MapD best_shift{0, 0, 0};
  bool solid_start = false;
  int hit = -1;
  for (int i = 0; i < w.count; i++) {
    const BrushModel& m = w.models[i];
    const Vec3f o = origin_of(w, i);
    // The feet to the hull's own point, added in double and rounded once, so a point on the
    // float grid of the hull's space (the map's origin) comes back on it.
    const MapD shift = {bmin.x - m.clip_mins[h].x - o.x, bmin.y - m.clip_mins[h].y - o.y,
                        bmin.z - m.clip_mins[h].z - o.z};
    HullTrace tr = trace_hull(m.hulls[h], hull_point(p1, shift), hull_point(p2, shift));
    const bool started = tr.startsolid || tr.allsolid;
    if (started) tr.fraction = 0;
    if (tr.fraction < best.fraction) {
      best = tr;
      best_shift = shift;
      solid_start = started;
      hit = tr.fraction < 1 ? i : -1;
    }
  }
  move::Hit out;
  if (solid_start) {
    out.fraction = 0;
    out.end = from;
    out.start_solid = out.all_solid = true;
    out.solid = static_cast<uint32_t>(hit + 1);
    return out;
  }
  if (w.count == 0) {  // no model at all: nothing in the way
    out.end = to;
    return out;
  }
  out.fraction = best.fraction;
  const MapD end = {best.endpos.x - best_shift.x, best.endpos.y - best_shift.y,
                    best.endpos.z - best_shift.z};
  out.end = {-end.y, end.z, end.x};
  if (hit >= 0) {
    out.normal = to_engine(best.plane.normal);
    out.solid = static_cast<uint32_t>(hit + 1);
  }
  return out;
}

// A model's hull 0 walked for its liquid leaves: the region's half-spaces so far.
struct Walk {
  const BrushModel& model;
  Vec3f origin;
  double upm;
  water::World& water;
  std::vector<int>& kinds;
  std::vector<water::Plane> path;  // engine axes, metres
  int laid = 0;
};

// n . (p - origin) <= d in the map is to_engine(n) . e <= d + n . origin in the engine.
water::Plane engine_plane(Vec3f n, double d, Vec3f origin, double upm) {
  const move::Vec3 e = to_engine(n);
  const double dist =
      d + (static_cast<double>(n.x) * origin.x + static_cast<double>(n.y) * origin.y +
           static_cast<double>(n.z) * origin.z);
  return {e.x, e.y, e.z, dist / upm};
}

void lay(Walk& w, int contents, const double lo[3], const double hi[3]) {
  water::Hull hull;
  hull.planes = w.path;
  for (int a = 0; a < 3; a++) {  // the region's box, as six more planes
    Vec3f n{};
    n[a] = 1;
    hull.planes.push_back(engine_plane(n, hi[a], w.origin, w.upm));
    n[a] = -1;
    hull.planes.push_back(engine_plane(n, -lo[a], w.origin, w.upm));
  }
  water::Stuff stuff = contents == kContentsSlime  ? water::liquid("slime")
                       : contents == kContentsLava ? water::liquid("lava")
                                                   : water::Stuff{};
  stuff.liquid.swim = {};  // the classic feel: every swim scale 1
  const water::Hull hulls[1] = {hull};
  water::add_volume(w.water, hulls, stuff);
  w.kinds.push_back(contents);
  w.laid++;
}

void walk(Walk& w, int node, const double lo[3], const double hi[3]) {
  if (node < 0) {
    if (node <= kContentsWater && node >= kContentsLava) lay(w, node, lo, hi);
    return;
  }
  const Hull& hull = w.model.hulls[0];
  const ClipNode& cn = hull.nodes[node];
  const Plane& pl = hull.planes[cn.plane];
  // The plane's least and greatest value over the box: a side the box cannot reach is dropped.
  double least = 0, most = 0;
  int axis = -1, axes = 0;
  for (int a = 0; a < 3; a++) {
    const double n = pl.normal[a];
    if (n == 0) continue;
    axes++;
    axis = a;
    least += n * (n > 0 ? lo[a] : hi[a]);
    most += n * (n > 0 ? hi[a] : lo[a]);
  }
  const double d = pl.dist;
  for (int side = 0; side < 2; side++) {  // 0 the front (n . p >= d), 1 the back
    if (side == 0 ? most <= d : least >= d) continue;
    double l2[3] = {lo[0], lo[1], lo[2]}, h2[3] = {hi[0], hi[1], hi[2]};
    if (axes == 1) {  // an axial plane cuts the box itself
      const double n = pl.normal[axis];
      const double at = d / n;
      if ((side == 0) == (n > 0))
        l2[axis] = at > l2[axis] ? at : l2[axis];
      else
        h2[axis] = at < h2[axis] ? at : h2[axis];
      if (!(l2[axis] < h2[axis])) continue;
    }
    w.path.push_back(side == 0 ? engine_plane(-pl.normal, -d, w.origin, w.upm)
                               : engine_plane(pl.normal, d, w.origin, w.upm));
    walk(w, cn.children[side], l2, h2);
    w.path.pop_back();
  }
}

}  // namespace

move::Vec3 to_engine(Vec3f m) {
  return {-static_cast<double>(m.y), static_cast<double>(m.z), static_cast<double>(m.x)};
}

Vec3f to_map(const move::Vec3& e) {
  return {static_cast<float>(e.z), static_cast<float>(-e.x), static_cast<float>(e.y)};
}

move::World mover_world(const HullWorld& hulls) { return {&hulls, trace_models}; }

int contents_at(const HullWorld& w, const move::Vec3& p) {
  const Vec3f m = to_map(p);
  int first = kContentsEmpty;
  for (int i = 0; i < w.count; i++) {
    const int c = point_contents(w.models[i].hulls[0], m - origin_of(w, i));
    if (c == kContentsSolid) return c;
    if (first == kContentsEmpty) first = c;
  }
  return first;
}

int add_liquids(const BrushModel& model, Vec3f origin, double units_per_metre, water::World& water,
                std::vector<int>& kinds) {
  if (model.hulls[0].last_node < model.hulls[0].first_node) return 0;
  Walk w{model, origin, units_per_metre, water, kinds, {}, 0};
  const double lo[3] = {model.mins.x, model.mins.y, model.mins.z};
  const double hi[3] = {model.maxs.x, model.maxs.y, model.maxs.z};
  walk(w, model.hulls[0].first_node, lo, hi);
  return w.laid;
}

}  // namespace ge::shooter
