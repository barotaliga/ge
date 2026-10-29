// THE TRIANGLES A DETAIL PASS WORKS ON: a `TriMesh` is an indexed triangle list with one tag a
// triangle (the surface's material or class, whatever the caller numbers), the one shape decals
// are clipped to, props are scattered over and solids are written out as. A level's floors and
// walls reach the detail passes as one of these, whatever format they were loaded from.
//
// WINDING: a triangle's front is the side its (b - a) x (c - a) points to, the engine's own cross
// product (render/mesh.hpp); `normal_of` is that unit vector.
//
// ge:summary The triangle list every detail pass reads: positions, indices, a tag a triangle
// ge:keywords detail trimesh triangle mesh soup tag winding normal_of area_of append
// ge:skill detail
// ge:test tests/detail_solid_test.cpp
#pragma once

#include <cstdint>
#include <vector>

#include "detail/vec.hpp"

namespace ge::detail {

struct TriMesh {
  std::vector<Vec3> pos;
  std::vector<uint32_t> idx;  // three a triangle
  std::vector<uint16_t> tag;  // one a triangle
  uint32_t triangles() const { return static_cast<uint32_t>(tag.size()); }
};

inline const Vec3& corner(const TriMesh& m, uint32_t tri, int k) {
  return m.pos[m.idx[tri * 3 + k]];
}

// The triangle's front, unit; zero for a degenerate triangle.
inline Vec3 normal_of(const TriMesh& m, uint32_t tri) {
  const Vec3& a = corner(m, tri, 0);
  return normalized(cross(sub(corner(m, tri, 1), a), sub(corner(m, tri, 2), a)));
}

// The triangle's area, m^2.
inline double area_of(const TriMesh& m, uint32_t tri) {
  const Vec3& a = corner(m, tri, 0);
  return 0.5 * length(cross(sub(corner(m, tri, 1), a), sub(corner(m, tri, 2), a)));
}

// `src` appended to `dst`, its indices moved past `dst`'s vertices.
inline void append(TriMesh& dst, const TriMesh& src) {
  const auto base = static_cast<uint32_t>(dst.pos.size());
  dst.pos.insert(dst.pos.end(), src.pos.begin(), src.pos.end());
  for (uint32_t i : src.idx) dst.idx.push_back(base + i);
  dst.tag.insert(dst.tag.end(), src.tag.begin(), src.tag.end());
}

}  // namespace ge::detail
