// The map compiler's light (parts.hpp § Light).
//
// ge:summary Defines the light step: lights read, luxels placed and lit with shadows and the sun
// ge:keywords map-compiler light luxel shadow sun minlight style lightmap extents
// ge:skill brush
// ge:test tests/brush_compile_test.cpp
#include <algorithm>
#include <cmath>
#include <sstream>

#include "brush/compile/parts.hpp"
#include "core/libm/libm.hpp"

namespace ge::brush::mapc {

namespace {

bool read_v3(const std::string* s, V3* out) {
  if (s == nullptr) return false;
  std::istringstream in(*s);
  return bool(in >> (*out)[0] >> (*out)[1] >> (*out)[2]);
}

double read_num(const std::string* s, double fallback) {
  if (s == nullptr) return fallback;
  std::istringstream in(*s);
  double v = fallback;
  in >> v;
  return in ? v : fallback;
}

// The first opaque contents a segment meets (0 when none), front to back.
uint32_t first_hit(const Tree& t, const Planes& planes, int n, const V3& a, const V3& b) {
  const Node& node = t.nodes[size_t(n)];
  if (node.plane < 0) return opaque(node.contents) ? node.contents : 0;
  const PlaneD& p = planes.canonical(node.plane);
  const double da = dist(p, a), db = dist(p, b);
  if (da >= 0 && db >= 0) return first_hit(t, planes, node.child[0], a, b);
  if (da < 0 && db < 0) return first_hit(t, planes, node.child[1], a, b);
  const V3 m = add(a, mul(sub(b, a), da / (da - db)));
  const int near = da >= 0 ? 0 : 1;
  const uint32_t h = first_hit(t, planes, node.child[near], a, m);
  if (h) return h;
  return first_hit(t, planes, node.child[1 - near], m, b);
}

bool inside(const Tree& t, const Planes& planes, const V3& p) {
  return opaque(t.nodes[size_t(leaf_at(t, planes, p))].contents);
}

}  // namespace

LightSet lights_of(const Source& src) {
  LightSet set;
  if (src.entities.empty()) return set;
  const Entity& world = src.entities[0];
  set.minlight = read_num(world.get("_minlight"), read_num(world.get("light"), 0));
  set.sun.value = read_num(world.get("_sunlight"), 0);
  V3 mangle = {0, -90, 0};
  read_v3(world.get("_sun_mangle"), &mangle);
  const double yaw = mangle[0] * 3.14159265358979323846 / 180;
  const double pitch = mangle[1] * 3.14159265358979323846 / 180;
  // The mangle is the way the light travels; toward the sun is against it.
  const V3 travel = {core::libm::cos(yaw) * core::libm::cos(pitch),
                     core::libm::sin(yaw) * core::libm::cos(pitch), core::libm::sin(pitch)};
  set.sun.toward = normalized(mul(travel, -1));
  for (size_t i = 1; i < src.entities.size(); ++i) {
    const Entity& e = src.entities[i];
    const std::string* cls = e.get("classname");
    if (cls == nullptr || cls->rfind("light", 0) != 0) continue;
    Light l;
    if (!read_v3(e.get("origin"), &l.at)) continue;
    l.value = read_num(e.get("light"), 300);
    l.wait = read_num(e.get("wait"), 1);
    if (!(l.wait > 0)) l.wait = 1;
    l.style = uint8_t(std::clamp(read_num(e.get("style"), 0), 0.0, 254.0));
    set.lights.push_back(l);
  }
  return set;
}

void light_faces(const LightSet& set, const Tree& world, const Planes& planes,
                 const std::vector<TexInfo>& texinfos, Tree* tree, size_t* count) {
  for (Face& f : tree->faces) {
    const TexInfo& ti = texinfos[size_t(f.texinfo)];
    f.luxels.clear();
    if (ti.flags & 1) {
      f.size[0] = f.size[1] = 0;
      continue;
    }
    // The extents, as a reader works them out: texel coordinates of the float corners.
    double lo[2] = {1e30, 1e30}, hi[2] = {-1e30, -1e30};
    for (const V3& p : f.w) {
      const float x[3] = {float(p[0]), float(p[1]), float(p[2])};
      for (int a = 0; a < 2; ++a) {
        const auto st =
            float(double(x[0]) * double(ti.vecs[a][0]) + double(x[1]) * double(ti.vecs[a][1]) +
                  double(x[2]) * double(ti.vecs[a][2]) + double(ti.vecs[a][3]));
        lo[a] = std::min(lo[a], double(st));
        hi[a] = std::max(hi[a], double(st));
      }
    }
    for (int a = 0; a < 2; ++a) {
      f.base[a] = int64_t(std::floor(lo[a] / 16.0));
      f.size[a] = int64_t(std::ceil(hi[a] / 16.0)) - f.base[a] + 1;
    }
    PlaneD pl = planes.canonical(f.plane);
    if (f.back) pl = flip(pl);
    const V3 c = add(centre(f.w), pl.n);
    // Solve s, t and the plane for a point: rows vs, vt, n.
    const V3 rs = {ti.vecs[0][0], ti.vecs[0][1], ti.vecs[0][2]};
    const V3 rt = {ti.vecs[1][0], ti.vecs[1][1], ti.vecs[1][2]};
    const double det = dot(rs, cross(rt, pl.n));
    const size_t n = size_t(f.size[0] * f.size[1]);
    std::vector<V3> pts(n);
    for (int64_t j = 0; j < f.size[1]; ++j) {
      for (int64_t i = 0; i < f.size[0]; ++i) {
        const double s = double((f.base[0] + i) * 16) - ti.vecs[0][3];
        const double t = double((f.base[1] + j) * 16) - ti.vecs[1][3];
        V3 p = c;
        if (std::fabs(det) > 1e-12) {
          // Cramer's rule on [rs; rt; n] p = [s; t; d].
          const V3 rhs = {s, t, pl.d};
          const V3 c0 = {rs[0], rt[0], pl.n[0]}, c1 = {rs[1], rt[1], pl.n[1]},
                   c2 = {rs[2], rt[2], pl.n[2]};
          const double dd = dot(c0, cross(c1, c2));
          p = {dot(rhs, cross(c1, c2)) / dd, dot(c0, cross(rhs, c2)) / dd,
               dot(c0, cross(c1, rhs)) / dd};
          p = add(p, pl.n);
        }
        for (int k = 0; k < 6 && inside(world, planes, p); ++k) p = mul(add(p, c), 0.5);
        pts[size_t(j * f.size[0] + i)] = p;
      }
    }
    // The styles this face takes: 0 first, then any other that reaches it.
    std::vector<uint8_t> styles = {0};
    std::vector<std::vector<double>> maps(1, std::vector<double>(n, 0));
    const auto map_of = [&](uint8_t style) -> std::vector<double>* {
      for (size_t k = 0; k < styles.size(); ++k) {
        if (styles[k] == style) return &maps[k];
      }
      if (styles.size() >= 4) return nullptr;
      styles.push_back(style);
      maps.emplace_back(n, 0);
      return &maps.back();
    };
    for (const Light& l : set.lights) {
      const double reach = l.value / l.wait;
      if (dist(pl, l.at) <= 0) continue;
      std::vector<double>* m = nullptr;
      for (size_t k = 0; k < n; ++k) {
        const V3 d = sub(l.at, pts[k]);
        const double r = length(d);
        if (!(r < reach)) continue;
        const double cos_t = r > 0 ? dot(d, pl.n) / r : 1;
        if (cos_t <= 0) continue;
        if (first_hit(world, planes, world.head, pts[k], l.at) != 0) continue;
        const double add_v = (l.value - r * l.wait) * (0.5 + 0.5 * cos_t);
        if (add_v <= 0) continue;
        if (m == nullptr) m = map_of(l.style);
        if (m == nullptr) break;
        (*m)[k] += add_v;
      }
    }
    if (set.sun.value > 0 && dot(pl.n, set.sun.toward) > 0) {
      const double cos_t = dot(pl.n, set.sun.toward);
      for (size_t k = 0; k < n; ++k) {
        const V3 far = add(pts[k], mul(set.sun.toward, 65536));
        if (first_hit(world, planes, world.head, pts[k], far) == kSky) {
          maps[0][k] += set.sun.value * (0.5 + 0.5 * cos_t);
        }
      }
    }
    for (size_t s = 0; s < 4; ++s) f.styles[s] = s < styles.size() ? styles[s] : 255;
    f.luxels.reserve(n * styles.size());
    for (size_t s = 0; s < styles.size(); ++s) {
      for (size_t k = 0; k < n; ++k) {
        double v = maps[s][k];
        if (s == 0) v = std::max(v, set.minlight);
        f.luxels.push_back(uint8_t(std::min(255.0, std::max(0.0, v) + 0.5)));
      }
    }
    *count += n;
  }
}

}  // namespace ge::brush::mapc
