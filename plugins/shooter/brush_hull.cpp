// The brush chain's builder (brush_hull.hpp states the layout).
//
// ge:summary Builds a hull's plane chain from brushes, moving each plane out by a box
// ge:keywords build_point_hull build_box_hull chain brush expand
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

}  // namespace ge::shooter
