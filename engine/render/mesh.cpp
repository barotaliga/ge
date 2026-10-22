// The shape builders, the bounding sphere and the packed vertex.
//
// ge:summary Faceted shape builders, bounding spheres and the packed vertex format
// ge:keywords render mesh box cylinder ellipsoid octahedral pack unpack
// ge:skill render
// ge:test tests/render_mesh_test.cpp
#include "render/mesh.hpp"

#include <algorithm>
#include <cmath>

namespace ge::render {

namespace {

constexpr float kPi = 3.14159265358979f;

void push(MeshData& m, const float p[3], const float n[3], uint8_t part) {
  m.positions.insert(m.positions.end(), {p[0], p[1], p[2]});
  m.normals.insert(m.normals.end(), {n[0], n[1], n[2]});
  m.parts.push_back(part);
  m.indices.push_back(static_cast<uint32_t>(m.parts.size() - 1));
}

// A flat quad a, b, c, d (in winding order) as two triangles.
void add_quad(MeshData& m, const float a[3], const float b[3], const float c[3], const float d[3],
              uint8_t part) {
  add_triangle(m, a, b, c, part);
  add_triangle(m, a, c, d, part);
}

uint32_t unorm16(float v) {
  return static_cast<uint32_t>(std::lround(std::clamp(v, 0.0f, 1.0f) * 65535.0f));
}

uint32_t snorm16(float v) {
  return static_cast<uint32_t>(static_cast<uint16_t>(
      static_cast<int16_t>(std::lround(std::clamp(v, -1.0f, 1.0f) * 32767.0f))));
}

float from_snorm16(uint32_t bits) {
  return std::max(static_cast<float>(static_cast<int16_t>(static_cast<uint16_t>(bits))) / 32767.0f,
                  -1.0f);
}

float sign_not_zero(float v) { return v >= 0 ? 1.0f : -1.0f; }

}  // namespace

void add_triangle(MeshData& m, const float a[3], const float b[3], const float c[3], uint8_t part) {
  const float u[3] = {b[0] - a[0], b[1] - a[1], b[2] - a[2]};
  const float v[3] = {c[0] - a[0], c[1] - a[1], c[2] - a[2]};
  float n[3] = {u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0]};
  const float len = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
  if (len <= 0) return;  // a degenerate face draws nothing
  for (float& x : n) x /= len;
  push(m, a, n, part);
  push(m, b, n, part);
  push(m, c, n, part);
}

void add_box(MeshData& m, const float c[3], const float h[3], uint8_t part) {
  // The corner k has x of sign bit 0, y bit 1, z bit 2.
  float p[8][3];
  for (int k = 0; k < 8; ++k) {
    p[k][0] = c[0] + ((k & 1) ? h[0] : -h[0]);
    p[k][1] = c[1] + ((k & 2) ? h[1] : -h[1]);
    p[k][2] = c[2] + ((k & 4) ? h[2] : -h[2]);
  }
  // Each face wound so its cross product points out of the box.
  add_quad(m, p[1], p[3], p[7], p[5], part);  // +x
  add_quad(m, p[0], p[4], p[6], p[2], part);  // -x
  add_quad(m, p[2], p[6], p[7], p[3], part);  // +y
  add_quad(m, p[0], p[1], p[5], p[4], part);  // -y
  add_quad(m, p[4], p[5], p[7], p[6], part);  // +z
  add_quad(m, p[0], p[2], p[3], p[1], part);  // -z
}

void add_cylinder(MeshData& m, float y0, float y1, float r0, float r1, int segments, uint8_t part) {
  segments = std::max(segments, 3);
  for (int i = 0; i < segments; ++i) {
    const float a0 = 2 * kPi * static_cast<float>(i) / static_cast<float>(segments);
    const float a1 = 2 * kPi * static_cast<float>(i + 1) / static_cast<float>(segments);
    const float c0 = std::cos(a0), s0 = std::sin(a0), c1 = std::cos(a1), s1 = std::sin(a1);
    const float b0[3] = {r0 * s0, y0, r0 * c0}, b1[3] = {r0 * s1, y0, r0 * c1};
    const float t0[3] = {r1 * s0, y1, r1 * c0}, t1[3] = {r1 * s1, y1, r1 * c1};
    // Round the y axis from +z toward +x; each face wound so its cross product points out.
    if (r0 > 0 && r1 > 0) {
      add_quad(m, b0, b1, t1, t0, part);
    } else if (r0 > 0) {
      add_triangle(m, b0, b1, t0, part);  // a cone: its apex at y1
    } else {
      add_triangle(m, b0, t1, t0, part);  // its apex at y0
    }
    const float bc[3] = {0, y0, 0}, tc[3] = {0, y1, 0};
    if (r0 > 0) add_triangle(m, bc, b1, b0, part);
    if (r1 > 0) add_triangle(m, tc, t0, t1, part);
  }
}

void add_ellipsoid(MeshData& m, const float c[3], const float r[3], int rings, int segments,
                   uint8_t part) {
  rings = std::max(rings, 2);
  segments = std::max(segments, 3);
  auto at = [&](int ring, int seg, float out[3]) {
    const float t = kPi * static_cast<float>(ring) / static_cast<float>(rings);  // from +y
    const float a = 2 * kPi * static_cast<float>(seg) / static_cast<float>(segments);
    out[0] = c[0] + r[0] * std::sin(t) * std::sin(a);
    out[1] = c[1] + r[1] * std::cos(t);
    out[2] = c[2] + r[2] * std::sin(t) * std::cos(a);
  };
  for (int i = 0; i < rings; ++i) {
    for (int j = 0; j < segments; ++j) {
      float a[3], b[3], d[3], e[3];
      at(i, j, a);
      at(i + 1, j, b);
      at(i + 1, j + 1, d);
      at(i, j + 1, e);
      if (i == 0) {
        add_triangle(m, a, b, d, part);
      } else if (i == rings - 1) {
        add_triangle(m, a, b, e, part);
      } else {
        add_quad(m, a, b, d, e, part);
      }
    }
  }
}

void bounding_sphere(const MeshData& m, float out[4]) {
  float lo[3] = {1e30f, 1e30f, 1e30f}, hi[3] = {-1e30f, -1e30f, -1e30f};
  const size_t n = m.positions.size() / 3;
  for (size_t i = 0; i < n; ++i) {
    for (int k = 0; k < 3; ++k) {
      lo[k] = std::min(lo[k], m.positions[i * 3 + k]);
      hi[k] = std::max(hi[k], m.positions[i * 3 + k]);
    }
  }
  if (n == 0) {
    out[0] = out[1] = out[2] = out[3] = 0;
    return;
  }
  for (int k = 0; k < 3; ++k) out[k] = (lo[k] + hi[k]) * 0.5f;
  float r2 = 0;
  for (size_t i = 0; i < n; ++i) {
    float d2 = 0;
    for (int k = 0; k < 3; ++k) {
      const float d = m.positions[i * 3 + k] - out[k];
      d2 += d * d;
    }
    r2 = std::max(r2, d2);
  }
  out[3] = std::sqrt(r2) * 1.0001f;
}

PackedMesh pack_mesh(const MeshData& m) {
  PackedMesh p;
  const uint32_t n = m.vertex_count();
  float hi[3] = {-1e30f, -1e30f, -1e30f};
  for (int k = 0; k < 3; ++k) p.box_min[k] = n ? 1e30f : 0.0f;
  for (uint32_t i = 0; i < n; ++i) {
    for (int k = 0; k < 3; ++k) {
      p.box_min[k] = std::min(p.box_min[k], m.positions[i * 3 + k]);
      hi[k] = std::max(hi[k], m.positions[i * 3 + k]);
    }
  }
  for (int k = 0; k < 3; ++k) hi[k] = n ? std::max(hi[k] - p.box_min[k], 1e-6f) : 1e-6f;
  return pack_mesh(m, p.box_min, hi);
}

PackedMesh pack_mesh(const MeshData& m, const float box_min[3], const float box_size[3]) {
  PackedMesh p;
  const uint32_t n = m.vertex_count();
  for (int k = 0; k < 3; ++k) {
    p.box_min[k] = box_min[k];
    p.box_size[k] = std::max(box_size[k], 1e-6f);
  }
  p.words.reserve(size_t{n} * 3);
  for (uint32_t i = 0; i < n; ++i) {
    uint32_t q[3];
    for (int k = 0; k < 3; ++k) {
      q[k] = unorm16((m.positions[i * 3 + k] - p.box_min[k]) / p.box_size[k]);
    }
    // Octahedral: the normal onto the octahedron |x| + |y| + |z| = 1, the lower half folded out.
    const float* nv = &m.normals[size_t{i} * 3];
    const float l1 = std::fabs(nv[0]) + std::fabs(nv[1]) + std::fabs(nv[2]);
    float ox = nv[0] / l1, oy = nv[2] / l1;
    if (nv[1] < 0) {
      const float fx = (1 - std::fabs(oy)) * sign_not_zero(ox);
      const float fy = (1 - std::fabs(ox)) * sign_not_zero(oy);
      ox = fx;
      oy = fy;
    }
    p.words.push_back(q[0] | (q[1] << 16));
    p.words.push_back(q[2] | (uint32_t{m.parts[i]} << 16));
    p.words.push_back(snorm16(ox) | (snorm16(oy) << 16));
  }
  p.indices = m.indices;
  return p;
}

void unpack_vertex(const PackedMesh& p, uint32_t i, float pos[3], float normal[3], uint32_t* part) {
  const uint32_t w0 = p.words[size_t{i} * 3], w1 = p.words[size_t{i} * 3 + 1],
                 w2 = p.words[size_t{i} * 3 + 2];
  const uint32_t q[3] = {w0 & 0xffffu, w0 >> 16, w1 & 0xffffu};
  for (int k = 0; k < 3; ++k) {
    pos[k] = p.box_min[k] + static_cast<float>(q[k]) / 65535.0f * p.box_size[k];
  }
  *part = w1 >> 16;
  const float ox = from_snorm16(w2 & 0xffffu), oy = from_snorm16(w2 >> 16);
  float n[3] = {ox, 1 - std::fabs(ox) - std::fabs(oy), oy};
  if (n[1] < 0) {
    const float fx = (1 - std::fabs(n[2])) * sign_not_zero(n[0]);
    const float fz = (1 - std::fabs(n[0])) * sign_not_zero(n[2]);
    n[0] = fx;
    n[2] = fz;
  }
  const float len = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
  for (int k = 0; k < 3; ++k) normal[k] = n[k] / len;
}

}  // namespace ge::render
