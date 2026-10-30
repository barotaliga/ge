// The patches: a recipe for every new vertex (a corner, a point on an edge, a point inside a
// triangle) made once from the topology, then evaluated for each frame.
//
// ge:summary Point-normal triangle patches over a morph model, every frame from one recipe
// ge:keywords pn-triangles bezier patch recipe edge corner interior crease frame normals
// ge:skill morph
// ge:test tests/morph_smooth_test.cpp
#include "morph/smooth.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <tuple>

#include "core/libm/libm.hpp"
#include "core/num.hpp"

namespace ge::morph {

namespace {

struct V {
  double x = 0, y = 0, z = 0;
};
V operator+(V a, V b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
V operator-(V a, V b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
V operator*(V a, double s) { return {a.x * s, a.y * s, a.z * s}; }
double dot(V a, V b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
V cross(V a, V b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
V unit(V a) {
  const double l = std::sqrt(dot(a, a));
  return l > 0 ? a * (1.0 / l) : a;
}

V at(const std::vector<float>& p, size_t i) { return {p[i * 3], p[i * 3 + 1], p[i * 3 + 2]}; }

// The control point near `a` on the edge toward `b`: a third of the way along, pulled onto the
// tangent plane at `a`.
V near(V a, V na, V b) { return (a * 2.0 + b - na * dot(b - a, na)) * (1.0 / 3.0); }

// A recipe: corners name one vertex; an edge point names its two ends (in a canonical order) and
// how far along it lies; an inside point names its triangle and its barycentric weights.
struct Recipe {
  enum Kind : uint8_t { corner, edge, inside } kind = corner;
  uint32_t a = 0, b = 0, c = 0;  // vertices (corner: a; edge: a, b; inside: a, b, c)
  double wa = 1, wb = 0, wc = 0;
};

}  // namespace

std::vector<float> frame_normals(const Model& m, const Frame& f) {
  const size_t n = m.vertex_count();
  int most = 0;
  for (int s : m.source_vertex) most = std::max(most, s);
  std::vector<V> by_source(size_t(most) + 1);
  for (size_t t = 0; t + 2 < m.indices.size(); t += 3) {
    const V a = at(f.positions, m.indices[t]), b = at(f.positions, m.indices[t + 1]),
            c = at(f.positions, m.indices[t + 2]);
    const V face = cross(b - a, c - a);  // area-weighted
    for (int k = 0; k < 3; k++) {
      V& s = by_source[size_t(m.source_vertex[m.indices[t + size_t(k)]])];
      s = s + face;
    }
  }
  std::vector<float> out(n * 3);
  for (size_t i = 0; i < n; i++) {
    const V u = unit(by_source[size_t(m.source_vertex[i])]);
    out[i * 3] = float(u.x);
    out[i * 3 + 1] = float(u.y);
    out[i * 3 + 2] = float(u.z);
  }
  return out;
}

Model smooth_model(const Model& m, const SmoothOptions& opts) {
  const int level = std::max(1, opts.level);
  Model out = m;
  out.st.clear();
  out.indices.clear();
  out.source_vertex.clear();
  std::vector<Recipe> recipes;
  std::map<std::tuple<uint32_t, uint32_t, int>, uint32_t> edges;
  std::vector<uint32_t> corners(m.vertex_count(), UINT32_MAX);
  // The canonical order of an edge's ends: by position (source vertex), then by vertex, so both
  // triangles of an edge, and both sides of a seam, evaluate it the same way round.
  auto before = [&](uint32_t x, uint32_t y) {
    const int sx = m.source_vertex[x], sy = m.source_vertex[y];
    return sx != sy ? sx < sy : x < y;
  };
  auto add = [&](const Recipe& r, double s, double t, int source) {
    recipes.push_back(r);
    out.st.push_back(float(s));
    out.st.push_back(float(t));
    out.source_vertex.push_back(source);
    return uint32_t(recipes.size() - 1);
  };
  std::vector<uint32_t> grid;
  for (size_t tri = 0; tri + 2 < m.indices.size(); tri += 3) {
    const uint32_t v[3] = {m.indices[tri], m.indices[tri + 1], m.indices[tri + 2]};
    grid.assign(size_t(level + 1) * size_t(level + 1), 0);
    for (int i = 0; i <= level; i++) {
      for (int j = 0; j <= level - i; j++) {
        const int k = level - i - j;
        const double a = double(i) / level, b = double(j) / level, c = double(k) / level;
        const double s =
            a * m.st[size_t{v[0]} * 2] + b * m.st[size_t{v[1]} * 2] + c * m.st[size_t{v[2]} * 2];
        const double t = a * m.st[size_t{v[0]} * 2 + 1] + b * m.st[size_t{v[1]} * 2 + 1] +
                         c * m.st[size_t{v[2]} * 2 + 1];
        uint32_t id;
        const int zeros = (i == 0) + (j == 0) + (k == 0);
        if (zeros == 2) {  // a corner
          const uint32_t w = i == level ? v[0] : j == level ? v[1] : v[2];
          if (corners[w] == UINT32_MAX) {
            corners[w] = add({Recipe::corner, w, 0, 0, 1, 0, 0}, m.st[size_t{w} * 2],
                             m.st[size_t{w} * 2 + 1], m.source_vertex[w]);
          }
          id = corners[w];
        } else if (zeros == 1) {  // on an edge: the two corners it runs between and how far
          uint32_t p, q;
          int steps;  // from p
          if (k == 0)
            p = v[0], q = v[1], steps = j;
          else if (i == 0)
            p = v[1], q = v[2], steps = k;
          else
            p = v[2], q = v[0], steps = i;
          if (!before(p, q)) std::swap(p, q), steps = level - steps;
          const auto key = std::make_tuple(p, q, steps);
          auto it = edges.find(key);
          if (it == edges.end()) {
            it = edges
                     .emplace(key, add({Recipe::edge, p, q, 0, 1.0 - double(steps) / level,
                                        double(steps) / level, 0},
                                       s, t, -1))
                     .first;
          }
          id = it->second;
        } else {
          id = add({Recipe::inside, v[0], v[1], v[2], a, b, c}, s, t, -1);
        }
        grid[size_t(i) * size_t(level + 1) + size_t(j)] = id;
      }
    }
    // Cut the grid into triangles, keeping the original's winding.
    auto g = [&](int i, int j) { return grid[size_t(i) * size_t(level + 1) + size_t(j)]; };
    for (int i = 0; i < level; i++) {
      for (int j = 0; j < level - i; j++) {
        out.indices.insert(out.indices.end(), {g(i + 1, j), g(i, j + 1), g(i, j)});
        if (j + 1 < level - i)
          out.indices.insert(out.indices.end(), {g(i + 1, j), g(i + 1, j + 1), g(i, j + 1)});
      }
    }
  }
  // A new vertex that is not a corner has no position of its own in the file; give it a unique
  // source id past the file's, so normals are summed over it alone (and its seam twin, if an edge).
  int next = 0;
  for (int s : m.source_vertex) next = std::max(next, s + 1);
  std::map<std::tuple<int, int, uint32_t>, int> edge_source;
  for (size_t r = 0; r < recipes.size(); r++) {
    if (out.source_vertex[r] >= 0) continue;
    const Recipe& q = recipes[r];
    if (q.kind == Recipe::edge) {
      const auto key = std::make_tuple(m.source_vertex[q.a], m.source_vertex[q.b],
                                       uint32_t(core::num::round(q.wb * 4096.0)));
      auto [it, fresh] = edge_source.try_emplace(key, next);
      if (fresh) next++;
      out.source_vertex[r] = it->second;
    } else {
      out.source_vertex[r] = next++;
    }
  }
  const double crease = core::libm::cos(opts.crease_deg * 3.14159265358979323846 / 180.0);
  for (size_t f = 0; f < m.frames.size(); f++) {
    const std::vector<float>& p = m.frames[f].positions;
    const std::vector<float> nf = frame_normals(m, m.frames[f]);
    auto P = [&](uint32_t i) { return at(p, i); };
    auto N = [&](uint32_t i) { return at(nf, i); };
    // The edge's two inner control points, or the straight third-points across a crease.
    auto controls = [&](uint32_t a, uint32_t b, V* ab, V* ba) {
      if (dot(N(a), N(b)) < crease) {
        *ab = P(a) + (P(b) - P(a)) * (1.0 / 3.0);
        *ba = P(b) + (P(a) - P(b)) * (1.0 / 3.0);
      } else {
        *ab = near(P(a), N(a), P(b));
        *ba = near(P(b), N(b), P(a));
      }
    };
    std::vector<float>& q = out.frames[f].positions;
    q.assign(recipes.size() * 3, 0.0f);
    for (size_t r = 0; r < recipes.size(); r++) {
      const Recipe& c = recipes[r];
      V x;
      if (c.kind == Recipe::corner) {
        x = P(c.a);
      } else if (c.kind == Recipe::edge) {
        V ab, ba;
        controls(c.a, c.b, &ab, &ba);
        const double u = c.wa, w = c.wb;
        x = P(c.a) * (u * u * u) + ab * (3 * u * u * w) + ba * (3 * u * w * w) +
            P(c.b) * (w * w * w);
      } else {
        V b210, b120, b021, b012, b102, b201;
        controls(c.a, c.b, &b210, &b120);
        controls(c.b, c.c, &b021, &b012);
        controls(c.c, c.a, &b102, &b201);
        const V e = (b210 + b120 + b021 + b012 + b102 + b201) * (1.0 / 6.0);
        const V mid = (P(c.a) + P(c.b) + P(c.c)) * (1.0 / 3.0);
        const V b111 = e + (e - mid) * 0.5;
        const double a = c.wa, b = c.wb, d = c.wc;
        x = P(c.a) * (a * a * a) + P(c.b) * (b * b * b) + P(c.c) * (d * d * d) +
            b210 * (3 * a * a * b) + b120 * (3 * a * b * b) + b201 * (3 * a * a * d) +
            b021 * (3 * b * b * d) + b102 * (3 * a * d * d) + b012 * (3 * b * d * d) +
            b111 * (6 * a * b * d);
      }
      q[r * 3] = float(x.x);
      q[r * 3 + 1] = float(x.y);
      q[r * 3 + 2] = float(x.z);
    }
  }
  return out;
}

}  // namespace ge::morph
