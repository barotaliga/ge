// The fixture's box hulls: for a box, the points where it fits inside the rooms, as a tree of
// axial planes (write_parts.hpp, `compile_hulls`).
//
// The rooms' sides cut space into a grid of cells, each inside a room or not. A box about a point
// fits when every cell it overlaps is inside a room. Moving the box changes which cells it
// overlaps only where one of its faces crosses a side, so the box's grid (every side moved by the
// box's mins and by its maxs) has cells that are wholly open or wholly solid: one test at a
// cell's middle classifies it. The tree splits a range of those cells at the middle of its widest
// axis until the range is uniform. The grid is padded by a solid cell at each end of each axis,
// so everything beyond the rooms is solid.
//
// ge:summary The fixture maps' player and large box hulls: the box's grid of cells, split to a tree
// ge:keywords fixture clip-hull box-hull hull1 hull2 grid cells split compile_hulls
// ge:skill brush
// ge:test tests/brush_hull_test.cpp
#include <algorithm>
#include <array>
#include <map>
#include <vector>

#include "brush/write_parts.hpp"

namespace ge::brush::detail {
namespace {

using Coords = std::array<std::vector<int32_t>, 3>;

struct RoomGrid {
  Coords at;                  // every side's coordinate, sorted, per axis
  std::vector<uint8_t> open;  // cell (i, j, k) inside a room, x fastest

  size_t cell(size_t i, size_t j, size_t k) const {
    return i + (at[0].size() - 1) * (j + (at[1].size() - 1) * k);
  }
};

RoomGrid room_grid(const Fixture& fx) {
  RoomGrid g;
  for (int a = 0; a < 3; ++a) {
    for (const FixtureRoom& r : fx.rooms) {
      g.at[size_t(a)].push_back(r.min[a]);
      g.at[size_t(a)].push_back(r.max[a]);
    }
    auto& v = g.at[size_t(a)];
    std::sort(v.begin(), v.end());
    v.erase(std::unique(v.begin(), v.end()), v.end());
  }
  const size_t n[3] = {g.at[0].size() - 1, g.at[1].size() - 1, g.at[2].size() - 1};
  g.open.assign(n[0] * n[1] * n[2], 0);
  for (size_t k = 0; k < n[2]; ++k) {
    for (size_t j = 0; j < n[1]; ++j) {
      for (size_t i = 0; i < n[0]; ++i) {
        const double mid[3] = {(g.at[0][i] + g.at[0][i + 1]) / 2.0,
                               (g.at[1][j] + g.at[1][j + 1]) / 2.0,
                               (g.at[2][k] + g.at[2][k + 1]) / 2.0};
        for (const FixtureRoom& r : fx.rooms) {
          bool in = true;
          for (int a = 0; a < 3; ++a) in = in && mid[a] > r.min[a] && mid[a] < r.max[a];
          if (in) g.open[g.cell(i, j, k)] = 1;
        }
      }
    }
  }
  return g;
}

// Whether the box lo..hi (a positive size on every axis) lies inside the rooms.
bool fits(const RoomGrid& g, const double lo[3], const double hi[3]) {
  size_t first[3], last[3];
  for (int a = 0; a < 3; ++a) {
    const auto& v = g.at[size_t(a)];
    if (lo[a] < v.front() || hi[a] > v.back()) return false;
    // The cells the box overlaps by more than a face: from the one holding lo to the one
    // holding hi.
    first[a] = size_t(std::upper_bound(v.begin(), v.end(), lo[a]) - v.begin()) - 1;
    last[a] = size_t(std::lower_bound(v.begin(), v.end(), hi[a]) - v.begin()) - 1;
  }
  for (size_t k = first[2]; k <= last[2]; ++k) {
    for (size_t j = first[1]; j <= last[1]; ++j) {
      for (size_t i = first[0]; i <= last[0]; ++i) {
        if (!g.open[g.cell(i, j, k)]) return false;
      }
    }
  }
  return true;
}

struct BoxGrid {
  Coords at;                   // the box's planes, sorted, per axis
  size_t n[3] = {};            // cells per axis: at.size() + 1 (the ends run to infinity)
  std::vector<uint8_t> solid;  // x fastest

  size_t cell(size_t i, size_t j, size_t k) const { return i + n[0] * (j + n[1] * k); }
};

BoxGrid box_grid(const RoomGrid& rooms, const int32_t mins[3], const int32_t maxs[3]) {
  BoxGrid g;
  for (int a = 0; a < 3; ++a) {
    auto& v = g.at[size_t(a)];
    for (int32_t c : rooms.at[size_t(a)]) {
      v.push_back(c - maxs[a]);
      v.push_back(c - mins[a]);
    }
    std::sort(v.begin(), v.end());
    v.erase(std::unique(v.begin(), v.end()), v.end());
    g.n[a] = v.size() + 1;
  }
  const auto mid = [&](int a, size_t i) {
    const auto& v = g.at[size_t(a)];
    if (i == 0) return double(v.front()) - 1;
    if (i == v.size()) return double(v.back()) + 1;
    return (v[i - 1] + v[i]) / 2.0;
  };
  g.solid.assign(g.n[0] * g.n[1] * g.n[2], 1);
  for (size_t k = 0; k < g.n[2]; ++k) {
    for (size_t j = 0; j < g.n[1]; ++j) {
      for (size_t i = 0; i < g.n[0]; ++i) {
        const double p[3] = {mid(0, i), mid(1, j), mid(2, k)};
        double lo[3], hi[3];
        for (int a = 0; a < 3; ++a) lo[a] = p[a] + mins[a], hi[a] = p[a] + maxs[a];
        g.solid[g.cell(i, j, k)] = fits(rooms, lo, hi) ? 0 : 1;
      }
    }
  }
  return g;
}

struct Range {
  size_t lo[3], hi[3];  // cells lo..hi - 1 per axis
};

// The range's contents when every cell agrees, else 0.
int32_t uniform(const BoxGrid& g, const Range& r) {
  const uint8_t first = g.solid[g.cell(r.lo[0], r.lo[1], r.lo[2])];
  for (size_t k = r.lo[2]; k < r.hi[2]; ++k) {
    for (size_t j = r.lo[1]; j < r.hi[1]; ++j) {
      for (size_t i = r.lo[0]; i < r.hi[0]; ++i) {
        if (g.solid[g.cell(i, j, k)] != first) return 0;
      }
    }
  }
  return first ? -2 : -1;
}

struct Builder {
  const BoxGrid& g;
  Compiled& c;
  std::map<std::pair<int, int32_t>, uint32_t>& planes;

  uint32_t plane(int axis, int32_t d) {
    const auto [it, added] = planes.emplace(std::make_pair(axis, d), uint32_t(c.planes.size()));
    if (added) c.planes.push_back(CPlane{axis, float(d)});
    return it->second;
  }

  int32_t build(const Range& r) {
    if (const int32_t leaf = uniform(g, r)) return leaf;
    int axis = 0;
    for (int a = 1; a < 3; ++a) {
      if (r.hi[a] - r.lo[a] > r.hi[axis] - r.lo[axis]) axis = a;
    }
    // Cell m starts at plane m - 1: the cells from the middle up are in front of it.
    const size_t m = (r.lo[axis] + r.hi[axis]) / 2;
    const int32_t at = int32_t(c.clips.size());
    c.clips.push_back(CClip{plane(axis, g.at[size_t(axis)][m - 1]), {}});
    Range front = r, back = r;
    front.lo[axis] = m;
    back.hi[axis] = m;
    const int32_t f = build(front);
    const int32_t b = build(back);
    c.clips[size_t(at)].child[0] = f;
    c.clips[size_t(at)].child[1] = b;
    return at;
  }
};

}  // namespace

void compile_hulls(const Fixture& fx, Compiled* c) {
  const RoomGrid rooms = room_grid(fx);
  const int32_t boxes[2][2][3] = {{{-16, -16, -24}, {16, 16, 32}}, {{-32, -32, -24}, {32, 32, 64}}};
  // Every plane the point tree already uses keeps its index.
  std::map<std::pair<int, int32_t>, uint32_t> planes;
  for (size_t i = 0; i < c->planes.size(); ++i) {
    planes.emplace(std::make_pair(c->planes[i].axis, int32_t(c->planes[i].d)), uint32_t(i));
  }
  c->clips.clear();
  for (int h = 0; h < 2; ++h) {
    const BoxGrid g = box_grid(rooms, boxes[h][0], boxes[h][1]);
    Builder b{g, *c, planes};
    const Range all{{0, 0, 0}, {g.n[0], g.n[1], g.n[2]}};
    const size_t first = c->clips.size();
    const int32_t head = b.build(all);
    // A hull that is one leaf still needs a node to start from: a plane with both sides alike.
    if (head < 0) c->clips.push_back(CClip{b.plane(0, 0), {head, head}});
    c->hull_head[h] = int32_t(first);
  }
}

}  // namespace ge::brush::detail
