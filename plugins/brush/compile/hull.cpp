// The map compiler's box hulls (parts.hpp § Hulls).
//
// ge:summary The box hulls: brushes bevelled, grown by the box, split and folded to clip nodes
// ge:keywords map-compiler hull clip-node bevel expand grow fold contents
// ge:skill brush
// ge:test tests/brush_compile_test.cpp
#include <cmath>

#include "brush/compile/parts.hpp"

namespace ge::brush::mapc {

namespace {

// The tree folded to clip nodes: a leaf is its contents number; a node whose children fold to one
// contents is that contents.
int32_t fold(const Tree& t, int n, Hull* hull) {
  const Node& node = t.nodes[size_t(n)];
  if (node.plane < 0) return opaque(node.contents) || node.contents == kClip ? -2 : -1;
  const int32_t f = fold(t, node.child[0], hull);
  const int32_t b = fold(t, node.child[1], hull);
  if (f < 0 && f == b) return f;
  hull->nodes.push_back({node.plane, {f, b}});
  return int32_t(hull->nodes.size()) - 1;
}

}  // namespace

Hull build_hull(const std::vector<Brush>& brushes, const V3& mins, const V3& maxs, const V3& lo,
                const V3& hi, Planes* planes) {
  std::vector<Brush> grown;
  std::vector<std::vector<int>> texinfos;
  std::vector<BrushSides> sides;
  std::vector<std::string> warnings;
  for (const Brush& b : brushes) {
    if (!(opaque(b.contents) || b.contents == kClip)) continue;
    // The brush's corners, for its bounds.
    const std::vector<Piece> one =
        make_pieces({b}, {std::vector<int>(b.sides.size(), -1)}, planes, &sides, &warnings);
    if (one.empty()) continue;
    Brush g;
    g.contents = kSolid;
    g.sides = b.sides;
    for (size_t k = 0; k < 3; ++k) {
      for (int s = 0; s < 2; ++s) {
        Side side;
        side.plane.n = {0, 0, 0};
        side.plane.n[k] = s ? 1 : -1;
        side.plane.d = s ? std::ceil(one[0].hi[k] - 0.01) : -std::floor(one[0].lo[k] + 0.01);
        bool have = false;
        for (const Side& x : b.sides) {
          bool fx = false, fs = false;
          const PlaneD a = canonical_plane(x.plane, &fx), c = canonical_plane(side.plane, &fs);
          have = have || (fx == fs && a.n == c.n && std::fabs(a.d - c.d) < 0.01);
        }
        if (!have) g.sides.push_back(side);
      }
    }
    for (Side& s : g.sides) {
      double off = 0;
      for (size_t k = 0; k < 3; ++k) off += s.plane.n[k] * (s.plane.n[k] > 0 ? mins[k] : maxs[k]);
      s.plane.d -= off;
    }
    texinfos.emplace_back(g.sides.size(), -1);
    grown.push_back(std::move(g));
  }
  Hull hull;
  std::vector<Piece> pieces = make_pieces(grown, texinfos, planes, &sides, &warnings);
  Tree t;
  build_tree(std::move(pieces), sub(lo, {64, 64, 64}), add(hi, {64, 64, 64}), planes, &t);
  const int32_t head = fold(t, t.head, &hull);
  if (head < 0) {
    // One node, both sides the one contents: a hull is entered through a node.
    PlaneD p;
    p.n = {0, 0, 1};
    p.d = 0;
    hull.nodes.push_back({planes->find(p) >> 1, {head, head}});
    hull.head = 0;
  } else {
    hull.head = head;
  }
  return hull;
}

}  // namespace ge::brush::mapc
