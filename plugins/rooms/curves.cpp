// Curve finding (curves.hpp): the visible faces' polygons, their shared edges found through a
// hash of the lines they lie on, coplanar pieces joined into patches, the creases walked into runs
// along one axis, the patches as wide as a facet kept, each run fitted and held to its circle.
//
// ge:summary Finds faceted curves: shared edges hashed by line, runs walked, circles fitted
// ge:keywords curve arch pillar edge line-hash crease run fit least-squares circle
// ge:skill rooms
// ge:test tests/rooms_curves_test.cpp
#include "rooms/curves.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <tuple>

#include "core/libm/libm.hpp"
#include "rooms/brush_build.hpp"

namespace ge::rooms {

namespace {

constexpr double kPi = 3.14159265358979323846;

struct V3 {
  double x, y, z;
};
V3 sub(const V3& a, const V3& b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
double dot(const V3& a, const V3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
V3 scale(const V3& a, double k) { return {a.x * k, a.y * k, a.z * k}; }
double len(const V3& a) { return std::sqrt(dot(a, a)); }

struct Poly {
  uint32_t face = 0;
  V3 n{0, 0, 0};
  V3 mid{0, 0, 0};
  std::vector<V3> pts;
};

struct Edge {
  uint32_t poly;
  double t0, t1;  // along the line's direction
  V3 d;           // that direction
  V3 near;        // the line's nearest point to the origin
};

struct Link {
  uint32_t to;
  V3 dir;       // the shared edge's direction
  int8_t bend;  // 0 coplanar, +1 convex, -1 concave
};

using DirKey = std::tuple<int64_t, int64_t, int64_t>;

}  // namespace

std::vector<Curve> find_curves(const Level& level, const BrushMesh& mesh, const CurveOptions& o) {
  // The drawn triangles of the solid brushes (what CSG left of their faces), each a polygon.
  std::vector<Poly> polys;
  for (size_t t = 0; t < mesh.triangles(); ++t) {
    if (mesh.hidden[t] || mesh.contents[t] != Contents::solid) continue;
    const Face& f = level.faces[mesh.face[t]];
    Poly p;
    p.face = mesh.face[t];
    p.n = {f.n[0], f.n[1], f.n[2]};
    for (size_t k = 0; k < 3; ++k) {
      const size_t v = mesh.idx[t * 3 + k];
      const V3 q{double(mesh.pos[v * 3]), double(mesh.pos[v * 3 + 1]), double(mesh.pos[v * 3 + 2])};
      p.pts.push_back(q);
      p.mid = {p.mid.x + q.x / 3, p.mid.y + q.y / 3, p.mid.z + q.z / 3};
    }
    polys.push_back(std::move(p));
  }
  // Every edge filed under its direction (signed to point the same way), with the line's nearest
  // point to the origin; edges are on one line when those points lie within `line_tol_m`.
  std::map<DirKey, std::vector<Edge>> dirs;
  for (uint32_t i = 0; i < polys.size(); ++i) {
    const std::vector<V3>& pts = polys[i].pts;
    for (size_t k = 0; k < pts.size(); ++k) {
      const V3 a = pts[k], b = pts[(k + 1) % pts.size()];
      V3 d = sub(b, a);
      const double l = len(d);
      if (l < o.min_edge_m) continue;
      d = scale(d, 1 / l);
      if (d.x < -1e-9 ||
          (std::fabs(d.x) <= 1e-9 && (d.y < -1e-9 || (std::fabs(d.y) <= 1e-9 && d.z < 0)))) {
        d = scale(d, -1);
      }
      double t0 = dot(a, d), t1 = dot(b, d);
      if (t0 > t1) std::swap(t0, t1);
      const DirKey key{std::llround(d.x * 256), std::llround(d.y * 256), std::llround(d.z * 256)};
      dirs[key].push_back({i, t0, t1, d, sub(a, scale(d, dot(a, d)))});
    }
  }
  // Links between faces with edges on one line that overlap or nearly meet end to end (the
  // staggered slices an ornate arch is built of).
  const double cos_min = core::libm::cos(o.min_crease_deg * kPi / 180);
  const double cos_max = core::libm::cos(o.max_crease_deg * kPi / 180);
  std::vector<std::vector<Link>> links(polys.size());
  for (auto& [key, edges] : dirs) {
    // Swept along the axis the direction least follows, where the lines spread out.
    const V3& d0 = edges.front().d;
    const int along = std::fabs(d0.x) <= std::fabs(d0.y) && std::fabs(d0.x) <= std::fabs(d0.z) ? 0
                      : std::fabs(d0.y) <= std::fabs(d0.z)                                     ? 1
                                                                                               : 2;
    const auto coord = [along](const Edge& e) {
      return along == 0 ? e.near.x : along == 1 ? e.near.y : e.near.z;
    };
    std::sort(edges.begin(), edges.end(),
              [&coord](const Edge& x, const Edge& y) { return coord(x) < coord(y); });
    for (size_t a = 0; a < edges.size(); ++a) {
      for (size_t b = a + 1; b < edges.size() && coord(edges[b]) - coord(edges[a]) <= o.line_tol_m;
           ++b) {
        const Edge& ea = edges[a];
        const Edge& eb = edges[b];
        if (ea.poly == eb.poly || len(sub(ea.near, eb.near)) > o.line_tol_m) continue;
        if (dot(ea.d, eb.d) < 0.99999) continue;
        if (std::min(ea.t1, eb.t1) - std::max(ea.t0, eb.t0) < -o.join_gap_m) continue;
        const Poly& pa = polys[ea.poly];
        const Poly& pb = polys[eb.poly];
        const double c = dot(pa.n, pb.n);
        int8_t bend;
        if (c >= 0.99999) {
          bend = 0;
        } else if (c <= cos_min && c >= cos_max) {
          // Convex when the other face falls away behind this one's plane.
          bend = dot(sub(pb.mid, pa.pts[0]), pa.n) < 0 ? 1 : -1;
        } else {
          continue;
        }
        links[ea.poly].push_back({eb.poly, ea.d, bend});
        links[eb.poly].push_back({ea.poly, ea.d, bend});
      }
    }
  }
  // Patches: the faces joined edge to edge in one plane (a facet the level's cells cut up).
  std::vector<uint32_t> root(polys.size());
  for (uint32_t i = 0; i < polys.size(); ++i) root[i] = i;
  const auto find = [&root](uint32_t i) {
    while (root[i] != i) i = root[i] = root[root[i]];
    return i;
  };
  for (uint32_t i = 0; i < polys.size(); ++i) {
    for (const Link& l : links[i]) {
      if (l.bend == 0) root[find(l.to)] = find(i);
    }
  }
  std::vector<uint32_t> patch_of(polys.size());
  std::vector<std::vector<uint32_t>> patches;
  {
    std::vector<uint32_t> index(polys.size(), UINT32_MAX);
    for (uint32_t i = 0; i < polys.size(); ++i) {
      const uint32_t r = find(i);
      if (index[r] == UINT32_MAX) index[r] = uint32_t(patches.size()), patches.emplace_back();
      patch_of[i] = index[r];
      patches[index[r]].push_back(i);
    }
  }
  // The creases between patches, each once.
  struct Crease {
    uint32_t to;
    V3 dir;
    int8_t bend;
  };
  std::vector<std::vector<Crease>> creases(patches.size());
  for (uint32_t i = 0; i < polys.size(); ++i) {
    for (const Link& l : links[i]) {
      if (l.bend == 0) continue;
      const uint32_t a = patch_of[i], b = patch_of[l.to];
      if (a == b) continue;
      bool known = false;
      for (const Crease& c : creases[a]) known = known || c.to == b;
      if (!known) creases[a].push_back({b, l.dir, l.bend});
    }
  }
  // Runs: from each patch with a crease, the patches creased to it along one axis, bending one
  // way; then, in each run, the patches as wide as a facet of the curve they turn round.
  std::vector<Curve> out;
  std::vector<uint8_t> used(patches.size(), 0);
  const auto parallel = [](const V3& a, const V3& b) { return std::fabs(dot(a, b)) >= 0.999; };
  for (uint32_t s = 0; s < patches.size(); ++s) {
    if (used[s] || creases[s].empty()) continue;
    for (const Crease& seed : creases[s]) {
      if (used[s]) break;
      const V3 axis = seed.dir;
      const int8_t bend = seed.bend;
      std::vector<uint32_t> run{s}, todo{s};
      std::vector<uint8_t> in(patches.size(), 0);
      in[s] = 1;
      while (!todo.empty()) {
        const uint32_t at = todo.back();
        todo.pop_back();
        for (const Crease& c : creases[at]) {
          if (in[c.to] || used[c.to] || c.bend != bend || !parallel(c.dir, axis)) continue;
          in[c.to] = 1;
          run.push_back(c.to);
          todo.push_back(c.to);
        }
      }
      if (run.size() < o.min_facets) continue;
      // Each patch's width across the axis, and the radius its width and its neighbours' turns
      // imply: a facet of a circle of radius r between turns a and b is r (tan a/2 + tan b/2)
      // wide.
      const size_t n = run.size();
      std::vector<double> width(n, 0), implied(n, 0);
      for (size_t k = 0; k < n; ++k) {
        const V3& nk = polys[patches[run[k]].front()].n;
        const V3 t{axis.y * nk.z - axis.z * nk.y, axis.z * nk.x - axis.x * nk.z,
                   axis.x * nk.y - axis.y * nk.x};
        double lo = 1e30, hi = -1e30;
        for (const uint32_t p : patches[run[k]]) {
          for (const V3& q : polys[p].pts)
            lo = std::min(lo, dot(q, t)), hi = std::max(hi, dot(q, t));
        }
        width[k] = hi - lo;
        double half_tans = 0;
        size_t sides = 0;
        for (const Crease& c : creases[run[k]]) {
          if (!in[c.to] || c.bend != bend || !parallel(c.dir, axis)) continue;
          const double cs = std::clamp(dot(nk, polys[patches[c.to].front()].n), -1.0, 1.0);
          // tan(a/2) = sin a / (1 + cos a), sin a from cos a.
          half_tans += std::sqrt(1 - cs * cs) / (1 + cs);
          ++sides;
        }
        if (sides == 1) half_tans *= 2;  // an end: taken as turning as much on its open side
        implied[k] = sides == 0 ? 0 : width[k] / half_tans;
      }
      std::vector<double> sorted;
      for (size_t k = 0; k < n; ++k) {
        if (implied[k] > 0) sorted.push_back(implied[k]);
      }
      if (sorted.empty()) continue;
      std::sort(sorted.begin(), sorted.end());
      const double typical = sorted[sorted.size() / 2];
      std::vector<uint32_t> kept;
      for (size_t k = 0; k < n; ++k) {
        if (implied[k] >= typical / o.width_spread && implied[k] <= typical * o.width_spread) {
          kept.push_back(run[k]);
        }
      }
      // The fit: the kept patches' distinct normals, the centre their planes turn round and the
      // radius they keep from it.
      struct Fit {
        std::vector<V3> normals;
        double cx = 0, cy = 0, radius = 0;
      };
      const V3 ref = std::fabs(axis.y) < 0.9 ? V3{0, 1, 0} : V3{1, 0, 0};
      V3 u = sub(ref, scale(axis, dot(ref, axis)));
      u = scale(u, 1 / len(u));
      const V3 v{axis.y * u.z - axis.z * u.y, axis.z * u.x - axis.x * u.z,
                 axis.x * u.y - axis.y * u.x};
      const auto fit = [&](const std::vector<uint32_t>& set, Fit* f) {
        std::vector<V3> mids;
        std::vector<double> weights;
        f->normals.clear();
        for (const uint32_t pt : set) {
          for (const uint32_t p : patches[pt]) {
            size_t k = 0;
            while (k < f->normals.size() && dot(f->normals[k], polys[p].n) < 0.99999) ++k;
            if (k == f->normals.size()) {
              f->normals.push_back(polys[p].n);
              mids.push_back({0, 0, 0});
              weights.push_back(0);
            }
            mids[k] = {mids[k].x + polys[p].mid.x, mids[k].y + polys[p].mid.y,
                       mids[k].z + polys[p].mid.z};
            weights[k] += 1;
          }
        }
        if (f->normals.size() < o.min_facets) return false;
        for (size_t k = 0; k < mids.size(); ++k) mids[k] = scale(mids[k], 1 / weights[k]);
        // The centre c minimising the squared distance to the lines through each facet's middle
        // along its normal: each line's row (ny, -nx) . c = px ny - py nx, two unknowns.
        double a11 = 0, a12 = 0, a22 = 0, b1 = 0, b2 = 0;
        for (size_t k = 0; k < f->normals.size(); ++k) {
          const double px = dot(mids[k], u), py = dot(mids[k], v);
          const double nx = dot(f->normals[k], u), ny = dot(f->normals[k], v);
          const double r = px * ny - py * nx;
          a11 += ny * ny, a12 += -ny * nx, a22 += nx * nx;
          b1 += ny * r, b2 += -nx * r;
        }
        const double det = a11 * a22 - a12 * a12;
        if (std::fabs(det) < 1e-12) return false;
        f->cx = (b1 * a22 - b2 * a12) / det, f->cy = (a11 * b2 - a12 * b1) / det;
        // The radius: the mean distance from the centre to the facets' planes; every plane must
        // lie near it, or the faces are not one circle's facets.
        std::vector<double> dist;
        f->radius = 0;
        for (size_t k = 0; k < f->normals.size(); ++k) {
          const double px = dot(mids[k], u) - f->cx, py = dot(mids[k], v) - f->cy;
          dist.push_back(std::fabs(px * dot(f->normals[k], u) + py * dot(f->normals[k], v)));
          f->radius += dist.back() / double(f->normals.size());
        }
        bool round = f->radius >= o.min_radius_m && f->radius <= o.max_radius_m;
        for (const double d : dist)
          round = round && std::fabs(d - f->radius) <= o.fit_tolerance * f->radius;
        return round;
      };
      Fit fitted;
      if (!fit(kept, &fitted)) continue;
      // Facets the walk or the width test missed (a facet cut off from its neighbours by another
      // brush, or cut narrower): any free patch square to the axis, on the circle's tangent on
      // the curve's side and within the run's length and the circle's reach, joins it.
      {
        double alo = 1e30, ahi = -1e30;
        for (const uint32_t pt : kept) {
          for (const uint32_t p : patches[pt]) {
            for (const V3& q : polys[p].pts)
              alo = std::min(alo, dot(q, axis)), ahi = std::max(ahi, dot(q, axis));
          }
        }
        const double r = fitted.radius, tol = o.fit_tolerance * r;
        const double side = bend < 0 ? -1 : 1;
        std::vector<uint8_t> taken(patches.size(), 0);
        for (const uint32_t pt : kept) taken[pt] = 1;
        const auto on_circle = [&](const Poly& p) {
          if (std::fabs(dot(p.n, axis)) > 1e-3) return false;
          const double px = dot(p.mid, u) - fitted.cx, py = dot(p.mid, v) - fitted.cy;
          const double d = side * (px * dot(p.n, u) + py * dot(p.n, v));
          if (std::fabs(d - r) > tol) return false;
          for (const V3& q : p.pts) {
            const double at = dot(q, axis);
            if (at < alo - o.line_tol_m || at > ahi + o.line_tol_m) return false;
            const double qx = dot(q, u) - fitted.cx, qy = dot(q, v) - fitted.cy;
            const double reach = std::sqrt(qx * qx + qy * qy);
            if (reach > (r + tol) / core::libm::cos(o.max_crease_deg * kPi / 180)) return false;
          }
          return true;
        };
        std::vector<uint32_t> grown = kept;
        for (uint32_t pt = 0; pt < patches.size(); ++pt) {
          if (taken[pt] || used[pt]) continue;
          bool all = true;
          for (const uint32_t p : patches[pt]) all = all && on_circle(polys[p]);
          if (all) grown.push_back(pt);
        }
        Fit refit;
        if (grown.size() > kept.size() && fit(grown, &refit)) kept = grown, fitted = refit;
      }
      const std::vector<V3>& normals = fitted.normals;
      const double cx = fitted.cx, cy = fitted.cy, radius = fitted.radius;
      Curve c;
      c.concave = bend < 0;
      c.facets = normals.size();
      std::vector<double> angles;
      for (size_t k = 0; k < normals.size(); ++k) {
        angles.push_back(core::libm::atan2(dot(normals[k], v), dot(normals[k], u)));
      }
      // The sweep: the circle less its open side, the one turn between neighbouring normals wider
      // than a crease. Two such turns: the facets do not follow one another round, not a curve.
      std::sort(angles.begin(), angles.end());
      const double crease = o.max_crease_deg * kPi / 180 + 1e-6;
      double gap = angles.front() + 2 * kPi - angles.back();
      size_t open = gap > crease ? 1 : 0;
      for (size_t k = 1; k < angles.size(); ++k) {
        const double g = angles[k] - angles[k - 1];
        open += g > crease ? 1 : 0;
        gap = std::max(gap, g);
      }
      if (open > 1) continue;
      c.sweep_deg = open == 0 ? 360 : (2 * kPi - gap) * 180 / kPi;
      double alo = 1e30, ahi = -1e30;
      for (const uint32_t pt : kept) {
        used[pt] = 1;
        for (const uint32_t p : patches[pt]) {
          for (const V3& q : polys[p].pts) {
            alo = std::min(alo, dot(q, axis)), ahi = std::max(ahi, dot(q, axis));
          }
          c.faces.push_back(polys[p].face);
        }
      }
      c.radius = radius;
      c.length = ahi - alo;
      const double mid_axis = (alo + ahi) / 2;
      const double uk[3] = {u.x, u.y, u.z}, vk[3] = {v.x, v.y, v.z};
      const double ak[3] = {axis.x, axis.y, axis.z};
      for (size_t k = 0; k < 3; ++k) {
        c.centre[k] = cx * uk[k] + cy * vk[k] + mid_axis * ak[k];
        c.axis[k] = ak[k];
      }
      std::sort(c.faces.begin(), c.faces.end());
      c.faces.erase(std::unique(c.faces.begin(), c.faces.end()), c.faces.end());
      out.push_back(std::move(c));
    }
  }
  // NOLINTBEGIN(clang-analyzer-cplusplus.Move): the analyzer's model of libc++'s sort compares an
  // element it moved from; the sort itself is sound
  std::sort(out.begin(), out.end(),
            [](const Curve& a, const Curve& b) { return a.faces.front() < b.faces.front(); });
  // NOLINTEND(clang-analyzer-cplusplus.Move)
  return out;
}

}  // namespace ge::rooms
