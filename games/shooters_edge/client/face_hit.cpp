// The face a line meets (face_hit.hpp).
//
// ge:summary The ray against a classic map's leaf triangles, and $cl_surface's words
// ge:keywords face_hit cl_surface ray-triangle
// ge:skill shooter
// ge:test tests/shooters_edge_place_names_test.cpp
#include "shooters_edge/client/face_hit.hpp"

#include <cmath>
#include <cstdio>

#include "brush/light_probe.hpp"
#include "core/math.hpp"

namespace shooters_edge::client {

namespace sh = ge::shooter;

bool face_hit(const ge::brush::Map& m, const sh::Vec3f& from, const sh::Vec3f& at,
              uint32_t* texture, sh::Vec3f* n, ge::detail::SurfaceClass* surface, float light[3],
              float st[2]) {
  const double d[3] = {at.x - from.x, at.y - from.y, at.z - from.z};
  const double len = ge::core::hypot3(d[0], d[1], d[2]);
  if (len < 1e-3 || m.leaves.empty()) return false;
  const double u[3] = {d[0] / len, d[1] / len, d[2] / len};
  const float before[3] = {static_cast<float>(at.x - u[0] * 2), static_cast<float>(at.y - u[1] * 2),
                           static_cast<float>(at.z - u[2] * 2)};
  const uint32_t leaf = ge::brush::leaf_at(m, before);
  if (leaf >= m.leaves.size()) return false;
  const ge::brush::Leaf& lf = m.leaves[leaf];
  const double o[3] = {from.x, from.y, from.z};
  constexpr double kNear = 12;  // units either side of the point the server reported
  double best = kNear;
  bool found = false;
  for (uint32_t i = 0; i < lf.marks; ++i) {
    const uint32_t si = m.marks[lf.first_mark + i];
    if (si >= m.surfaces.size()) continue;
    const ge::brush::Surface& sf = m.surfaces[si];
    for (uint32_t t = 0; t + 2 < sf.count; t += 3) {
      const float* a = m.vertices[m.indices[sf.first + t]].pos;
      const float* b = m.vertices[m.indices[sf.first + t + 1]].pos;
      const float* c = m.vertices[m.indices[sf.first + t + 2]].pos;
      const double e1[3] = {b[0] - a[0], b[1] - a[1], b[2] - a[2]};
      const double e2[3] = {c[0] - a[0], c[1] - a[1], c[2] - a[2]};
      const double p[3] = {u[1] * e2[2] - u[2] * e2[1], u[2] * e2[0] - u[0] * e2[2],
                           u[0] * e2[1] - u[1] * e2[0]};
      const double det = e1[0] * p[0] + e1[1] * p[1] + e1[2] * p[2];
      if (std::abs(det) < 1e-9) continue;
      const double tv[3] = {o[0] - a[0], o[1] - a[1], o[2] - a[2]};
      const double bu = (tv[0] * p[0] + tv[1] * p[1] + tv[2] * p[2]) / det;
      if (bu < -1e-4 || bu > 1 + 1e-4) continue;
      const double q[3] = {tv[1] * e1[2] - tv[2] * e1[1], tv[2] * e1[0] - tv[0] * e1[2],
                           tv[0] * e1[1] - tv[1] * e1[0]};
      const double bv = (u[0] * q[0] + u[1] * q[1] + u[2] * q[2]) / det;
      if (bv < -1e-4 || bu + bv > 1 + 1e-4) continue;
      const double dist = (e2[0] * q[0] + e2[1] * q[1] + e2[2] * q[2]) / det;
      if (std::abs(dist - len) >= best) continue;
      best = std::abs(dist - len);
      found = true;
      double nn[3] = {e1[1] * e2[2] - e1[2] * e2[1], e1[2] * e2[0] - e1[0] * e2[2],
                      e1[0] * e2[1] - e1[1] * e2[0]};
      const double nl = ge::core::hypot3(nn[0], nn[1], nn[2]);
      const double facing = nn[0] * u[0] + nn[1] * u[1] + nn[2] * u[2];
      const double sign = facing > 0 ? -1.0 : 1.0;  // toward `from`
      *n = {static_cast<float>(sign * nn[0] / nl), static_cast<float>(sign * nn[1] / nl),
            static_cast<float>(sign * nn[2] / nl)};
      *texture = sf.texture;
      // The face's own baked light where the line crossed it (first style; -1: none).
      const float* la = m.vertices[m.indices[sf.first + t]].lm;
      const float* lb = m.vertices[m.indices[sf.first + t + 1]].lm;
      const float* lc = m.vertices[m.indices[sf.first + t + 2]].lm;
      const float wa = static_cast<float>(1 - bu - bv), wb = static_cast<float>(bu),
                  wc = static_cast<float>(bv);
      const float uv[2] = {wa * la[0] + wb * lb[0] + wc * lc[0],
                           wa * la[1] + wb * lb[1] + wc * lc[1]};
      if (!ge::brush::lightmap_at(m, sf.lightmap, uv, light)) light[0] = light[1] = light[2] = -1;
      // And where on its texture (texels where the family stores them so, else whole textures).
      const float* ta = m.vertices[m.indices[sf.first + t]].st;
      const float* tb = m.vertices[m.indices[sf.first + t + 1]].st;
      const float* tc = m.vertices[m.indices[sf.first + t + 2]].st;
      for (int k = 0; k < 2; ++k) st[k] = wa * ta[k] + wb * tb[k] + wc * tc[k];
      const ge::brush::Texture* tex =
          sf.texture < m.textures.size() ? &m.textures[sf.texture] : nullptr;
      *surface = tex == nullptr                           ? ge::detail::SurfaceClass::unknown
                 : tex->sky                               ? ge::detail::SurfaceClass::sky
                 : tex->liquid != ge::brush::Liquid::none ? ge::detail::SurfaceClass::liquid
                                                          : ge::detail::classify_name(tex->name);
    }
  }
  return found;
}

namespace {

// Whether the leaf at `p` holds water, slime or lava in the map's own codes.
bool in_liquid(const ge::brush::Map& m, const float p[3]) {
  const uint32_t leaf = ge::brush::leaf_at(m, p);
  if (leaf >= m.leaves.size()) return false;
  const int32_t c = m.leaves[leaf].contents;
  switch (m.family) {
    case ge::brush::Family::v29:
    case ge::brush::Family::bsp2:
      return c == -3 || c == -4 || c == -5;
    case ge::brush::Family::v38:
      return (c & (8 | 16 | 32)) != 0;  // lava, slime, water
    case ge::brush::Family::v46:
      return false;
  }
  return false;
}

}  // namespace

std::string surface_ahead(const ge::brush::Map& m, const sh::Vec3f& from,
                          const sh::Vec3f& solid_end) {
  const double d[3] = {solid_end.x - from.x, solid_end.y - from.y, solid_end.z - from.z};
  const double len = ge::core::hypot3(d[0], d[1], d[2]);
  if (len < 1) return " 0";
  sh::Vec3f at = solid_end;
  constexpr double kStep = 4;  // units between the points asked
  const int steps = static_cast<int>(len / kStep);
  for (int i = 1; i <= steps; ++i) {
    const double s = i * kStep;
    if (s >= len) break;
    const float p[3] = {static_cast<float>(from.x + d[0] * s / len),
                        static_cast<float>(from.y + d[1] * s / len),
                        static_cast<float>(from.z + d[2] * s / len)};
    if (in_liquid(m, p)) {
      at = {p[0], p[1], p[2]};
      break;
    }
  }
  uint32_t texture = 0;
  sh::Vec3f n;
  ge::detail::SurfaceClass sc{};
  float light[3], st[2];
  if (!face_hit(m, from, at, &texture, &n, &sc, light, st)) return " 0";
  return surface_words(m, texture);
}

std::string surface_words(const ge::brush::Map& m, uint32_t texture) {
  if (texture >= m.textures.size()) return " 0";
  const ge::brush::Texture& t = m.textures[texture];
  if (t.flags == 0) return t.name + " 0";
  char hex[16];
  std::snprintf(hex, sizeof hex, "0x%x", t.flags);
  return t.name + " " + hex;
}

}  // namespace shooters_edge::client
