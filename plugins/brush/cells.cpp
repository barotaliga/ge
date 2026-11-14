// A point tree's cells as brushes (cells.hpp): the paths walked, each cell's faces cut in
// doubles, the planes that bound nothing left out, the axial bevels added.
//
// ge:summary The cells' paths walked, their faces cut, their bevels added, their leaves split
// ge:keywords cells brushes point-tree polygon-clip bevel leaf-split
// ge:skill brush
// ge:test tests/brush_cells_test.cpp
#include "brush/cells.hpp"

#include <array>
#include <cstddef>
#include <map>
#include <utility>
#include <vector>

#include "brush/survey.hpp"

namespace ge::brush {
namespace {

using V3 = std::array<double, 3>;

struct Facing {
  V3 n;
  double d;
};

double dot(const V3& a, const V3& b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }

V3 cross(const V3& a, const V3& b) {
  return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
}

// The polygon `poly` cut to the part behind (or on) `cut`.
void clip(std::vector<V3>& poly, std::vector<V3>& scratch, const Facing& cut) {
  constexpr double kOn = 1.0 / 4096;
  scratch.clear();
  const size_t n = poly.size();
  for (size_t i = 0; i < n; ++i) {
    const V3& a = poly[i];
    const V3& b = poly[(i + 1) % n];
    const double da = dot(cut.n, a) - cut.d;
    const double db = dot(cut.n, b) - cut.d;
    if (da <= kOn) scratch.push_back(a);
    if ((da < -kOn && db > kOn) || (da > kOn && db < -kOn)) {
      const double t = da / (da - db);
      scratch.push_back(
          {a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t, a[2] + (b[2] - a[2]) * t});
    }
  }
  poly.swap(scratch);
}

// A quad on `f` covering the box (lo, hi), wound either way.
std::vector<V3> plane_quad(const Facing& f, const V3& lo, const V3& hi) {
  V3 centre{(lo[0] + hi[0]) / 2, (lo[1] + hi[1]) / 2, (lo[2] + hi[2]) / 2};
  const double off = dot(f.n, centre) - f.d;
  for (int a = 0; a < 3; ++a) centre[size_t(a)] -= f.n[size_t(a)] * off;  // onto the plane
  double reach = 0;
  for (int a = 0; a < 3; ++a) reach += hi[size_t(a)] - lo[size_t(a)];
  int major = 0;
  for (int a = 1; a < 3; ++a) {
    const double m = f.n[size_t(a)] < 0 ? -f.n[size_t(a)] : f.n[size_t(a)];
    const double best = f.n[size_t(major)] < 0 ? -f.n[size_t(major)] : f.n[size_t(major)];
    if (m > best) major = a;
  }
  V3 up{0, 0, 0};
  up[major == 2 ? 0 : 2] = 1;
  V3 u = cross(f.n, up);
  // |u|² is at least 1/3 (n's largest component is not on `up`'s axis), and the quad only has to
  // cover the box: scaled by reach / |u|², its half side is at least `reach`.
  const double us = reach / dot(u, u);
  for (double& x : u) x *= us;
  V3 v = cross(f.n, u);
  return {V3{centre[0] + u[0] + v[0], centre[1] + u[1] + v[1], centre[2] + u[2] + v[2]},
          V3{centre[0] - u[0] + v[0], centre[1] - u[1] + v[1], centre[2] - u[2] + v[2]},
          V3{centre[0] - u[0] - v[0], centre[1] - u[1] - v[1], centre[2] - u[2] - v[2]},
          V3{centre[0] + u[0] - v[0], centre[1] + u[1] - v[1], centre[2] + u[2] - v[2]}};
}

int32_t cell_bits(Cell c) {
  switch (c) {
    case Cell::solid:
    case Cell::sky:
      return kCellSolid;
    case Cell::water:
      return kCellWater;
    case Cell::slime:
      return kCellSlime;
    case Cell::lava:
      return kCellLava;
    case Cell::empty:
      return 0;
  }
  return 0;
}

struct Walk {
  Map& m;
  CellCount count;
  std::vector<Cell> cells;                          // each original leaf's
  std::vector<int64_t> flipped;                     // a plane's flipped copy, or −1
  std::map<std::pair<int, float>, uint32_t> axial;  // (axis × 2 + side, d) → plane
  std::vector<std::pair<uint32_t, int>> path;       // (plane, child) from the head node
  std::vector<uint8_t> seen;                        // nodes walked
  V3 lo{}, hi{};
  std::vector<V3> poly, scratch;

  uint32_t facing_plane(uint32_t plane, int child) {
    if (child == 1) return plane;  // behind: the region lies behind the plane as stored
    if (flipped[plane] < 0) {
      const Plane& p = m.planes[plane];
      Plane q;
      for (int a = 0; a < 3; ++a) q.n[a] = -p.n[a];
      q.d = -p.d;
      q.type = 3;
      flipped[plane] = int64_t(m.planes.size());
      m.planes.push_back(q);
    }
    return uint32_t(flipped[plane]);
  }

  uint32_t axial_plane(int axis, bool positive, float d) {
    const auto key = std::make_pair(axis * 2 + (positive ? 1 : 0), d);
    if (auto it = axial.find(key); it != axial.end()) return it->second;
    Plane q;
    q.n[axis] = positive ? 1.0f : -1.0f;
    q.d = positive ? d : -d;
    q.type = positive ? uint8_t(axis) : uint8_t(3);
    const auto at = uint32_t(m.planes.size());
    m.planes.push_back(q);
    flipped.push_back(-1);
    axial.emplace(key, at);
    return at;
  }

  // The cell the path reaches made a brush; its index, or −1 when it bounds nothing.
  int64_t brush_of(Cell c) {
    std::vector<uint32_t> planes;
    planes.reserve(path.size());
    for (const auto& [plane, child] : path) {
      const uint32_t p = facing_plane(plane, child);
      bool twice = false;
      for (uint32_t q : planes) twice = twice || q == p;
      if (!twice) planes.push_back(p);
    }
    std::vector<Facing> f;
    f.reserve(planes.size() + 6);
    for (uint32_t p : planes) {
      const Plane& q = m.planes[p];
      f.push_back({{double(q.n[0]), double(q.n[1]), double(q.n[2])}, double(q.d)});
    }
    std::vector<Facing> box;
    for (int a = 0; a < 3; ++a) {
      Facing hi_side{{0, 0, 0}, hi[size_t(a)]};
      hi_side.n[size_t(a)] = 1;
      Facing lo_side{{0, 0, 0}, -lo[size_t(a)]};
      lo_side.n[size_t(a)] = -1;
      box.push_back(hi_side);
      box.push_back(lo_side);
    }
    std::vector<uint32_t> kept;
    V3 mins{hi}, maxs{lo};
    bool any = false;
    const auto grow = [&](const std::vector<V3>& pts) {
      for (const V3& p : pts)
        for (size_t a = 0; a < 3; ++a) {
          mins[a] = p[a] < mins[a] ? p[a] : mins[a];
          maxs[a] = p[a] > maxs[a] ? p[a] : maxs[a];
        }
    };
    for (size_t i = 0; i < f.size(); ++i) {
      poly = plane_quad(f[i], lo, hi);
      for (size_t j = 0; j < f.size() && poly.size() >= 3; ++j)
        if (j != i) clip(poly, scratch, f[j]);
      for (const Facing& b : box)
        if (poly.size() >= 3) clip(poly, scratch, b);
      if (poly.size() < 3) {
        ++count.dropped;
        continue;
      }
      kept.push_back(planes[i]);
      grow(poly);
      any = true;
    }
    if (!any) return -1;
    // The box's own sides close the cell where its planes leave it open: their faces count too.
    for (size_t i = 0; i < box.size(); ++i) {
      poly = plane_quad(box[i], lo, hi);
      for (const Facing& g : f)
        if (poly.size() >= 3) clip(poly, scratch, g);
      for (size_t j = 0; j < box.size() && poly.size() >= 3; ++j)
        if (j != i) clip(poly, scratch, box[j]);
      if (poly.size() >= 3) grow(poly);
    }
    // The bevels: an axial side wherever the cell has none.
    const auto first = uint32_t(m.brush_sides.size());
    const int32_t flags = c == Cell::sky ? kCellSkyFlag : 0;
    for (uint32_t p : kept) m.brush_sides.push_back({p, -1, flags, 0});
    for (int a = 0; a < 3; ++a)
      for (const bool positive : {false, true}) {
        bool has = false;
        for (uint32_t p : kept) {
          const Plane& q = m.planes[p];
          has = has || (q.n[a] == (positive ? 1.0f : -1.0f) && q.n[(a + 1) % 3] == 0 &&
                        q.n[(a + 2) % 3] == 0);
        }
        if (has) continue;
        const float d = float(positive ? maxs[size_t(a)] : mins[size_t(a)]);
        m.brush_sides.push_back({axial_plane(a, positive, d), -1, flags, 0});
        ++count.bevels;
      }
    Brush b;
    b.first_side = first;
    b.sides = uint32_t(m.brush_sides.size()) - first;
    b.contents = cell_bits(c);
    count.sides += b.sides;
    ++count.brushes;
    const auto at = int64_t(m.brushes.size());
    m.brushes.push_back(b);
    return at;
  }

  void node(int32_t n) {
    if (n < 0 || size_t(n) >= m.nodes.size() || seen[size_t(n)] != 0) return;
    seen[size_t(n)] = 1;
    for (int child = 0; child < 2; ++child) {
      path.emplace_back(m.nodes[size_t(n)].plane, child);
      const int32_t to = m.nodes[size_t(n)].child[child];
      if (to >= 0) {
        node(to);
      } else if (const auto leaf = size_t(-(to + 1)); leaf < cells.size()) {
        const Cell c = cells[leaf];
        if (c != Cell::empty) {
          const int64_t b = brush_of(c);
          if (b >= 0) {
            Leaf copy = m.leaves[leaf];
            copy.contents = cell_bits(c);
            copy.first_brush = uint32_t(m.leaf_brushes.size());
            copy.brushes = 1;
            m.leaf_brushes.push_back(uint32_t(b));
            m.nodes[size_t(n)].child[child] = -int32_t(m.leaves.size()) - 1;
            m.leaves.push_back(copy);
          }
        }
      }
      path.pop_back();
    }
  }
};

}  // namespace

CellCount cells_as_brushes(Map& map) {
  if (!map.brushes.empty()) return {};
  Walk w{map, {}, {}, std::vector<int64_t>(map.planes.size(), -1), {}, {}, {}, {}, {}, {}, {}};
  w.cells.reserve(map.leaves.size());
  for (const Leaf& l : map.leaves) w.cells.push_back(leaf_cell(map, l));
  for (size_t i = 0; i < map.leaves.size(); ++i) {
    map.leaves[i].contents = cell_bits(w.cells[i]);
    map.leaves[i].brushes = 0;
  }
  w.seen.assign(map.nodes.size(), 0);
  for (const Model& model : map.models) {
    for (size_t a = 0; a < 3; ++a) {
      w.lo[a] = double(model.mins[a]) - kCellMargin;
      w.hi[a] = double(model.maxs[a]) + kCellMargin;
    }
    w.path.clear();
    w.node(model.head_node);
  }
  return w.count;
}

}  // namespace ge::brush
