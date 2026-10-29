// Receivers: the binning, the box query and the ray's walk (the cells of the ray's box, then
// Moller-Trumbore on each triangle found).
//
// ge:summary Receivers' binning, box query and ray test
// ge:keywords detail receivers bin grid moller-trumbore ray triangle
// ge:skill detail
// ge:test tests/detail_receivers_test.cpp
#include "detail/receivers.hpp"

#include <algorithm>
#include <cmath>

namespace ge::detail {
namespace {

struct CellBox {
  int64_t x0, y0, z0, x1, y1, z1;
};

CellBox cells_of(const Receivers& r, const Vec3& lo, const Vec3& hi) {
  auto c = [&](double v, double o, int64_t n) {
    return std::clamp(static_cast<int64_t>(std::floor((v - o) / r.cell_m)), int64_t{0}, n - 1);
  };
  return {c(lo.x, r.origin.x, r.nx), c(lo.y, r.origin.y, r.ny), c(lo.z, r.origin.z, r.nz),
          c(hi.x, r.origin.x, r.nx), c(hi.y, r.origin.y, r.ny), c(hi.z, r.origin.z, r.nz)};
}

void tri_box(const TriMesh& m, uint32_t t, Vec3& lo, Vec3& hi) {
  lo = hi = corner(m, t, 0);
  for (int k = 1; k < 3; ++k) {
    const Vec3& v = corner(m, t, k);
    lo = {std::min(lo.x, v.x), std::min(lo.y, v.y), std::min(lo.z, v.z)};
    hi = {std::max(hi.x, v.x), std::max(hi.y, v.y), std::max(hi.z, v.z)};
  }
}

}  // namespace

Receivers bin_receivers(const TriMesh& mesh, double cell_m) {
  Receivers r;
  r.mesh = &mesh;
  r.cell_m = cell_m;
  const uint32_t n = mesh.triangles();
  if (n == 0) return r;
  Vec3 lo{1e300, 1e300, 1e300}, hi{-1e300, -1e300, -1e300};
  for (const Vec3& v : mesh.pos) {
    lo = {std::min(lo.x, v.x), std::min(lo.y, v.y), std::min(lo.z, v.z)};
    hi = {std::max(hi.x, v.x), std::max(hi.y, v.y), std::max(hi.z, v.z)};
  }
  r.origin = lo;
  r.nx = static_cast<int64_t>(std::floor((hi.x - lo.x) / cell_m)) + 1;
  r.ny = static_cast<int64_t>(std::floor((hi.y - lo.y) / cell_m)) + 1;
  r.nz = static_cast<int64_t>(std::floor((hi.z - lo.z) / cell_m)) + 1;
  const auto cells = static_cast<size_t>(r.nx * r.ny * r.nz);
  std::vector<uint32_t> count(cells + 1, 0);
  auto each = [&](uint32_t t, auto&& fn) {
    Vec3 tlo, thi;
    tri_box(mesh, t, tlo, thi);
    const CellBox b = cells_of(r, tlo, thi);
    for (int64_t z = b.z0; z <= b.z1; ++z) {
      for (int64_t y = b.y0; y <= b.y1; ++y) {
        for (int64_t x = b.x0; x <= b.x1; ++x) fn(static_cast<size_t>((z * r.ny + y) * r.nx + x));
      }
    }
  };
  for (uint32_t t = 0; t < n; ++t) each(t, [&](size_t c) { ++count[c + 1]; });
  for (size_t c = 0; c < cells; ++c) count[c + 1] += count[c];
  r.start = count;
  r.tris.resize(count[cells]);
  std::vector<uint32_t> fill(count.begin(), count.end() - 1);
  for (uint32_t t = 0; t < n; ++t) each(t, [&](size_t c) { r.tris[fill[c]++] = t; });
  return r;
}

void triangles_in(const Receivers& r, const Vec3& lo, const Vec3& hi, ReceiverScratch& s) {
  s.found.clear();
  if (r.start.empty()) return;
  if (s.mark.size() < r.mesh->triangles()) s.mark.assign(r.mesh->triangles(), 0);
  if (++s.query == 0) {  // the counter wrapped: forget every mark
    std::fill(s.mark.begin(), s.mark.end(), 0);
    s.query = 1;
  }
  const CellBox b = cells_of(r, lo, hi);
  for (int64_t z = b.z0; z <= b.z1; ++z) {
    for (int64_t y = b.y0; y <= b.y1; ++y) {
      for (int64_t x = b.x0; x <= b.x1; ++x) {
        const auto c = static_cast<size_t>((z * r.ny + y) * r.nx + x);
        for (uint32_t k = r.start[c]; k < r.start[c + 1]; ++k) {
          const uint32_t t = r.tris[k];
          if (s.mark[t] == s.query) continue;
          s.mark[t] = s.query;
          s.found.push_back(t);
        }
      }
    }
  }
}

Hit raycast(const Receivers& r, const Vec3& from, const Vec3& dir, double far_m,
            ReceiverScratch& s) {
  Hit best;
  const Vec3 to = mad(from, dir, far_m);
  triangles_in(r, {std::min(from.x, to.x), std::min(from.y, to.y), std::min(from.z, to.z)},
               {std::max(from.x, to.x), std::max(from.y, to.y), std::max(from.z, to.z)}, s);
  double nearest = far_m;
  for (uint32_t t : s.found) {
    const Vec3& a = corner(*r.mesh, t, 0);
    const Vec3 e1 = sub(corner(*r.mesh, t, 1), a);
    const Vec3 e2 = sub(corner(*r.mesh, t, 2), a);
    const Vec3 p = cross(dir, e2);
    const double det = dot(e1, p);
    if (det > -1e-12 && det < 1e-12) continue;
    const double inv = 1 / det;
    const Vec3 o = sub(from, a);
    const double u = dot(o, p) * inv;
    if (u < 0 || u > 1) continue;
    const Vec3 q = cross(o, e1);
    const double v = dot(dir, q) * inv;
    if (v < 0 || u + v > 1) continue;
    const double tt = dot(e2, q) * inv;
    if (tt <= 1e-9 || tt > nearest) continue;
    nearest = tt;
    best = {true, tt, t, mad(from, dir, tt)};
  }
  return best;
}

bool buried_at(const Receivers& r, const Vec3& p, const Vec3& n, double far_m,
               ReceiverScratch& scratch) {
  const Hit h = raycast(r, mad(p, n, 0.005), n, far_m, scratch);
  return h.hit && dot(normal_of(*r.mesh, h.tri), n) > 0;
}

}  // namespace ge::detail
