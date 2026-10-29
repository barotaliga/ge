// The brush chain's builder (brush_hull.hpp states the layout).
//
// ge:summary Builds a hull's plane chain from brushes, moving each plane out by a box
// ge:keywords build_point_hull build_box_hull chain brush expand build_brush_model bounds
// ge:skill shooter
// ge:test tests/shooter_brush_hull_test.cpp
#include "shooter/brush_hull.hpp"

namespace ge::shooter {

namespace {

HullData chain(const std::vector<const Brush*>& kept, bool grow, Vec3f mins, Vec3f maxs) {
  HullData h;
  size_t count = 0;
  for (const Brush* b : kept) count += b->planes.size();
  h.planes.reserve(count);
  h.nodes.reserve(count);
  for (size_t bi = 0; bi < kept.size(); bi++) {
    const Brush& b = *kept[bi];
    const auto first = static_cast<int32_t>(h.nodes.size());
    const auto next_brush = static_cast<int32_t>(first + b.planes.size());
    for (size_t pi = 0; pi < b.planes.size(); pi++) {
      Plane pl = b.planes[pi];
      if (grow) {
        float shift = 0;
        for (int i = 0; i < 3; i++) shift += pl.normal[i] * (pl.normal[i] > 0 ? mins[i] : maxs[i]);
        pl.dist = pl.dist - shift;
      }
      pl.type = plane_type(pl.normal);
      ClipNode n;
      n.plane = static_cast<int32_t>(h.planes.size());
      n.children[0] = bi + 1 < kept.size() ? next_brush : kContentsEmpty;
      n.children[1] = pi + 1 < b.planes.size() ? first + static_cast<int32_t>(pi) + 1 : b.contents;
      h.planes.push_back(pl);
      h.nodes.push_back(n);
    }
  }
  if (h.nodes.empty()) {
    // No brush: one node whose both sides are open.
    h.planes.push_back({{0, 0, 1}, 0, 2});
    h.nodes.push_back({0, {kContentsEmpty, kContentsEmpty}});
  }
  return h;
}

}  // namespace

HullData build_point_hull(const std::vector<Brush>& brushes) {
  std::vector<const Brush*> kept;
  for (const Brush& b : brushes)
    if (!b.planes.empty()) kept.push_back(&b);
  return chain(kept, false, {}, {});
}

HullData build_box_hull(const std::vector<Brush>& brushes, Vec3f mins, Vec3f maxs) {
  std::vector<const Brush*> kept;
  for (const Brush& b : brushes)
    if (b.contents == kContentsSolid && !b.planes.empty()) kept.push_back(&b);
  return chain(kept, true, mins, maxs);
}

BrushModel ModelData::model() const {
  BrushModel m;
  for (int h = 0; h < 3; h++) m.hulls[h] = hulls[h].hull();
  m.clip_mins[0] = {};
  m.clip_mins[1] = kPlayerMins;
  m.clip_mins[2] = kLargeMins;
  m.mins = mins;
  m.maxs = maxs;
  return m;
}

ModelData build_brush_model(const std::vector<Brush>& brushes) {
  ModelData d;
  d.hulls[0] = build_point_hull(brushes);
  d.hulls[1] = build_box_hull(brushes, kPlayerMins, kPlayerMaxs);
  d.hulls[2] = build_box_hull(brushes, kLargeMins, kLargeMaxs);
  constexpr float kFar = 1e30f;
  d.mins = {kFar, kFar, kFar};
  d.maxs = {-kFar, -kFar, -kFar};
  for (const Brush& b : brushes) {
    for (const Plane& p : b.planes) {
      for (int i = 0; i < 3; i++) {
        Vec3f axis{};
        axis[i] = 1;
        if (p.normal == axis && p.dist > d.maxs[i]) d.maxs[i] = p.dist;
        if (p.normal == -axis && -p.dist < d.mins[i]) d.mins[i] = -p.dist;
      }
    }
  }
  if (d.mins.x > d.maxs.x) d.mins = d.maxs = {};  // no brush: a model of no size
  return d;
}

}  // namespace ge::shooter
