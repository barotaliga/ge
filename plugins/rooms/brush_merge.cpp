// Brush merging (brush_merge.hpp): a pair tried by the dent test and the volumes, the pairs found
// through the planes they face each other on.
//
// ge:summary Merges a level's touching brushes whose union is convex: the pair test, the greedy
// pass ge:keywords brush merge union convex volume dent plane-key greedy ge:skill rooms ge:test
// tests/rooms_brush_test.cpp
#include "rooms/brush_merge.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <span>
#include <tuple>
#include <vector>

#include "rooms/brush_build.hpp"

namespace ge::rooms {

namespace {

using PlaneKey = std::tuple<int64_t, int64_t, int64_t, int64_t>;

// A brush to be: its planes, the faces on them and their polygons, one a plane.
struct Proto {
  Contents contents = Contents::solid;
  uint16_t group = kWorld;
  std::vector<HalfSpace> planes;
  std::vector<Face> faces;
  std::vector<std::vector<double>> polys;
};

// A plane as a key, oriented (a face's outward normal), and the same plane facing the other way.
PlaneKey oriented_key(const HalfSpace& h) {
  return {std::llround(h.n[0] * 4096), std::llround(h.n[1] * 4096), std::llround(h.n[2] * 4096),
          std::llround(h.d * 1024)};
}
PlaneKey flipped(const PlaneKey& k) {
  return {-std::get<0>(k), -std::get<1>(k), -std::get<2>(k), -std::get<3>(k)};
}

// Whether two faces on one plane can be one face: the same material and shading, and
// projections equal to a ten-thousandth (each was solved from its own surface's triangle, so the
// same mapping can differ in its last bits). A face with no surface on it takes the other's look.
bool same_look(const Face& a, const Face& b, std::span<const Projection> projections) {
  if (a.hidden || b.hidden) return a.smooth == b.smooth;
  if (a.material != b.material || a.smooth != b.smooth) return false;
  const Projection& p = projections[a.projection];
  const Projection& q = projections[b.projection];
  for (int k = 0; k < 4; ++k) {
    if (std::fabs(p.s[k] - q.s[k]) > 1e-4f * std::max(1.0f, std::fabs(p.s[k]))) return false;
    if (std::fabs(p.t[k] - q.t[k]) > 1e-4f * std::max(1.0f, std::fabs(p.t[k]))) return false;
  }
  return true;
}

// The volume a convex brush's polygons bound (each wound either way: the fan's sign is taken
// against the plane's outward normal).
double volume_of(const Proto& p) {
  const double* o = nullptr;
  for (const std::vector<double>& poly : p.polys) {
    if (!poly.empty()) {
      o = poly.data();
      break;
    }
  }
  if (!o) return 0;
  double v = 0;
  for (size_t f = 0; f < p.polys.size(); ++f) {
    const std::vector<double>& poly = p.polys[f];
    const size_t n = poly.size() / 3;
    if (n < 3) continue;
    double area[3] = {0, 0, 0};
    for (size_t i = 1; i + 1 < n; ++i) {
      const double* a = &poly[0];
      const double* b = &poly[i * 3];
      const double* c = &poly[(i + 1) * 3];
      const double u[3] = {b[0] - a[0], b[1] - a[1], b[2] - a[2]};
      const double w[3] = {c[0] - a[0], c[1] - a[1], c[2] - a[2]};
      area[0] += u[1] * w[2] - u[2] * w[1];
      area[1] += u[2] * w[0] - u[0] * w[2];
      area[2] += u[0] * w[1] - u[1] * w[0];
    }
    const double* nn = p.planes[f].n;
    const double len = std::fabs(area[0] * nn[0] + area[1] * nn[1] + area[2] * nn[2]) / 2;
    const double h = nn[0] * (poly[0] - o[0]) + nn[1] * (poly[1] - o[1]) + nn[2] * (poly[2] - o[2]);
    v += len * h / 3;
  }
  return v;
}

// `a` and `b` (touching on a plane, facing each other) as one brush when their union is convex:
// every corner of each behind every plane of the other but the ones they touch on (else the
// union has a dent), and the planes left closing exactly the two volumes. A plane both have must
// look the same on both, so no face changes its texture.
bool try_merge(const Proto& a, const Proto& b, std::span<const Projection> projections,
               Proto* out) {
  constexpr double kIn = 1e-3;  // a corner this far in front of a plane still counts as behind
  if (a.polys.empty() || b.polys.empty()) return false;  // a brush that does not close
  const auto behind = [&](const HalfSpace& h, const Proto& p) {
    for (const std::vector<double>& poly : p.polys) {
      for (size_t v = 0; v + 2 < poly.size(); v += 3) {
        if (h.n[0] * poly[v] + h.n[1] * poly[v + 1] + h.n[2] * poly[v + 2] > h.d + kIn) {
          return false;
        }
      }
    }
    return true;
  };
  const auto has = [](const Proto& p, const PlaneKey& k) {
    for (const HalfSpace& h : p.planes) {
      if (oriented_key(h) == k) return true;
    }
    return false;
  };
  Proto m;
  m.contents = a.contents;
  m.group = a.group;
  std::map<PlaneKey, size_t> at;
  for (const Proto* p : {&a, &b}) {
    const Proto& other = p == &a ? b : a;
    for (size_t f = 0; f < p->planes.size(); ++f) {
      const PlaneKey k = oriented_key(p->planes[f]);
      if (has(other, flipped(k))) continue;  // where they touch
      const auto it = at.find(k);
      if (it != at.end()) {
        Face& kept = m.faces[it->second];
        if (!same_look(kept, p->faces[f], projections)) return false;
        if (kept.hidden) kept = p->faces[f];
        continue;
      }
      if (!behind(p->planes[f], other)) return false;
      at.emplace(k, m.planes.size());
      m.planes.push_back(p->planes[f]);
      m.faces.push_back(p->faces[f]);
    }
  }
  if (m.planes.size() < 4 || !cell_polygons(m.planes, &m.polys)) return false;
  const double want = volume_of(a) + volume_of(b);
  if (std::fabs(volume_of(m) - want) > 1e-6 + 1e-6 * want) return false;
  // Planes the union does not touch go.
  Proto kept;
  kept.contents = m.contents;
  kept.group = m.group;
  for (size_t f = 0; f < m.planes.size(); ++f) {
    if (m.polys[f].empty()) continue;
    kept.planes.push_back(m.planes[f]);
    kept.faces.push_back(m.faces[f]);
    kept.polys.push_back(std::move(m.polys[f]));
  }
  if (kept.faces.size() > kMaxFaces) return false;
  *out = std::move(kept);
  return true;
}

// Brushes of one contents and group touching face to face merged, greedily in their order, while
// any pair's union is convex. A merged-away brush is left with no faces. Returns the merges made.
size_t merge_protos(std::vector<Proto>& protos, std::span<const Projection> projections) {
  using Key = std::tuple<PlaneKey, uint8_t, uint16_t>;
  std::map<Key, std::vector<uint32_t>> having;
  const auto index = [&](uint32_t i) {
    for (const HalfSpace& h : protos[i].planes) {
      having[{oriented_key(h), uint8_t(protos[i].contents), protos[i].group}].push_back(i);
    }
  };
  for (uint32_t i = 0; i < protos.size(); ++i) index(i);
  size_t merges = 0;
  for (bool again = true; again;) {
    again = false;
    for (uint32_t i = 0; i < protos.size(); ++i) {
      for (size_t f = 0; f < protos[i].planes.size(); ++f) {
        const Key k{flipped(oriented_key(protos[i].planes[f])), uint8_t(protos[i].contents),
                    protos[i].group};
        const auto it = having.find(k);
        if (it == having.end()) continue;
        bool merged = false;
        for (const uint32_t j : it->second) {
          if (j == i || protos[j].faces.empty()) continue;
          Proto m;
          if (!try_merge(protos[i], protos[j], projections, &m)) continue;
          protos[i] = std::move(m);
          protos[j] = Proto{};
          index(i);
          ++merges;
          merged = again = true;
          break;
        }
        if (merged) f = size_t(-1);  // the brush changed: look at its planes again
      }
    }
  }
  return merges;
}

}  // namespace

size_t merge_brushes(Level* level) {
  std::vector<Proto> protos(level->brushes.size());
  for (size_t b = 0; b < level->brushes.size(); ++b) {
    const Brush& br = level->brushes[b];
    Proto& p = protos[b];
    p.contents = br.contents;
    p.group = br.group;
    for (uint32_t f = br.first_face; f < br.first_face + br.faces; ++f) {
      const Face& face = level->faces[f];
      p.planes.push_back(
          {{double(face.n[0]), double(face.n[1]), double(face.n[2])}, double(face.d)});
      p.faces.push_back(face);
    }
    if (!cell_polygons(p.planes, &p.polys)) p.polys.clear();  // never merged: no corners
  }
  const size_t merges = merge_protos(protos, level->projections);
  if (merges == 0) return 0;
  std::vector<Brush> brushes;
  std::vector<Face> faces;
  for (const Proto& p : protos) {
    if (p.faces.empty()) continue;
    Brush b;
    b.contents = p.contents;
    b.group = p.group;
    b.first_face = uint32_t(faces.size());
    b.faces = uint8_t(p.faces.size());
    faces.insert(faces.end(), p.faces.begin(), p.faces.end());
    brushes.push_back(b);
  }
  level->brushes.swap(brushes);
  level->faces.swap(faces);
  // The projections pooled again in the order faces first use them (as the text form pools
  // them), the ones only merged-away faces used dropped.
  std::vector<Projection> pooled;
  std::vector<int32_t> to(level->projections.size(), -1);
  for (Face& f : level->faces) {
    if (to[f.projection] < 0) {
      to[f.projection] = int32_t(pooled.size());
      pooled.push_back(level->projections[f.projection]);
    }
    f.projection = uint16_t(to[f.projection]);
  }
  level->projections.swap(pooled);
  return merges;
}

}  // namespace ge::rooms
