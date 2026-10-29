// CONVEX SOLIDS AS PLANES: the shape a brush is in every brush-built level format, and the shape
// the bevel pass (bevel.hpp) cuts. A `Solid` is the points inside every one of its planes; its
// faces are not stored but found (`polygons_of`): each plane's own polygon, a large square on it
// clipped by every other plane. Because a bevel only ADDS planes, the corners where three bevels
// meet come out of the same clip with no special case.
//
// A plane keeps a `tag` (the face's material, whatever the caller numbers); the polygons and the
// triangles written from them carry it.
//
// ge:summary Convex solids as planes: boxes, prisms, their face polygons and triangles, inside
// ge:keywords detail solid brush convex plane half-space polygon clip polygons_of box_solid
// ge:keywords prism_solid contains triangulate face winding
// ge:skill detail
// ge:test tests/detail_solid_test.cpp
#pragma once

#include <span>
#include <vector>

#include "detail/mesh.hpp"
#include "detail/vec.hpp"

namespace ge::detail {

// The half-space dot(n, p) <= d; `n` unit, pointing out of the solid.
struct Plane {
  Vec3 n;
  double d;
  int tag = 0;
};

struct Solid {
  std::vector<Plane> planes;
};

// One face: the plane it lies on (an index into the solid's planes) and its corners, wound so
// (b - a) x (c - a) points along the plane's normal (out of the solid).
struct Polygon {
  int plane = 0;
  std::vector<Vec3> loop;
};

// How near a point must be to a plane to be on it, m.
inline constexpr double kOnPlane = 1e-7;

// The box from `lo` to `hi`, every face tagged `tag`.
Solid box_solid(const Vec3& lo, const Vec3& hi, int tag = 0);
// A convex polygon `xz` (x, z pairs, either winding) stood up from `y0` to `y1`: a pillar, a
// step, a plinth. Its top and bottom take `cap_tag`, its sides `side_tag`.
Solid prism_solid(std::span<const double> xz, double y0, double y1, int side_tag = 0,
                  int cap_tag = 0);

// Every face of `s` with an area, one a plane at most (a plane the others cut away has none).
std::vector<Polygon> polygons_of(const Solid& s);

// Whether `p` is inside `s` by more than `margin` (every plane at least `margin` away).
bool contains(const Solid& s, const Vec3& p, double margin = 0);

// The faces fanned into triangles and appended to `out`, each tagged by its plane's tag.
void triangulate(const Solid& s, std::span<const Polygon> polys, TriMesh& out);

}  // namespace ge::detail
