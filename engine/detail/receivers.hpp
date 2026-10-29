// THE SURFACES A DECAL LANDS ON AND A PROP STANDS ON, FOUND FAST: a level's `TriMesh` binned into
// a grid of cells once, so a decal's box finds the few triangles under it and a prop's probe finds
// the nearest wall without walking the whole level. Built at load, read-only after; a query
// writes into a caller's scratch, so a frame that places decals allocates nothing.
//
// ge:summary A level's triangles in a grid: the triangles in a box, the nearest hit along a ray
// ge:keywords detail receivers grid triangle query box ray raycast hit spatial-hash scratch
// ge:keywords buried_at buried hidden-face
// ge:skill detail
// ge:test tests/detail_receivers_test.cpp
#pragma once

#include <cstdint>
#include <vector>

#include "detail/mesh.hpp"

namespace ge::detail {

struct Receivers {
  const TriMesh* mesh = nullptr;
  double cell_m = 1;
  Vec3 origin{0, 0, 0};
  int64_t nx = 0, ny = 0, nz = 0;
  std::vector<uint32_t> start;  // cell c's triangles: tris[start[c] .. start[c + 1])
  std::vector<uint32_t> tris;
};

// `mesh` binned by its triangles' boxes; `mesh` must outlive the result.
Receivers bin_receivers(const TriMesh& mesh, double cell_m = 1);

// What a query fills: the triangles found (each once), and the marks that keep them once.
struct ReceiverScratch {
  std::vector<uint32_t> found;
  std::vector<uint32_t> mark;  // a triangle's last query number
  uint32_t query = 0;
};

// Every triangle whose cell overlaps the box `lo`..`hi` (a superset of those touching it), into
// `scratch.found`.
void triangles_in(const Receivers& r, const Vec3& lo, const Vec3& hi, ReceiverScratch& scratch);

struct Hit {
  bool hit = false;
  double t = 0;  // along the ray, m (the direction is unit)
  uint32_t tri = 0;
  Vec3 at{0, 0, 0};
};
// The nearest triangle (either face) along `from` + `dir` * t for t in (0, `far_m`]; `dir` unit.
Hit raycast(const Receivers& r, const Vec3& from, const Vec3& dir, double far_m,
            ReceiverScratch& scratch);

// Whether the point `p` on a surface of front `n` is buried inside a solid (a floor's face under a
// wall standing on it): the first surface along `n` from just off it, within `far_m`, is met from
// behind. A level built of brushes keeps such faces; a decal or a prop there would never be seen.
bool buried_at(const Receivers& r, const Vec3& p, const Vec3& n, double far_m,
               ReceiverScratch& scratch);

}  // namespace ge::detail
