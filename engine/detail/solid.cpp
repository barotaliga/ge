// Convex solids: the clip that finds a plane's polygon, and the builders.
//
// ge:summary Convex solids' clip (a plane's square cut by the others), boxes, prisms, triangles
// ge:keywords detail solid clip sutherland-hodgman polygon box prism triangulate
// ge:skill detail
// ge:test tests/detail_solid_test.cpp
#include "detail/solid.hpp"

#include <cstddef>

namespace ge::detail {
namespace {

// Half the side of the square a plane's polygon starts from, m: far past any level's extent, and
// small enough that a cut through its corners keeps 1e-11 m of precision.
constexpr double kStart = 1e5;

// `loop` cut to the half-space of `p` (Sutherland-Hodgman), into `out`.
void clip(const std::vector<Vec3>& loop, const Plane& p, std::vector<Vec3>& out) {
  out.clear();
  const size_t n = loop.size();
  for (size_t i = 0; i < n; ++i) {
    const Vec3& a = loop[i];
    const Vec3& b = loop[(i + 1) % n];
    const double da = dot(p.n, a) - p.d;
    const double db = dot(p.n, b) - p.d;
    if (da <= kOnPlane) out.push_back(a);
    if ((da < -kOnPlane && db > kOnPlane) || (da > kOnPlane && db < -kOnPlane)) {
      out.push_back(mad(a, sub(b, a), da / (da - db)));
    }
  }
}

// `loop` without corners closer than a micrometre to the one before them.
void weld(std::vector<Vec3>& loop) {
  std::vector<Vec3> kept;
  for (const Vec3& v : loop) {
    if (!kept.empty() && length(sub(v, kept.back())) < 1e-6) continue;
    kept.push_back(v);
  }
  while (kept.size() > 1 && length(sub(kept.front(), kept.back())) < 1e-6) kept.pop_back();
  loop.swap(kept);
}

double loop_area(const std::vector<Vec3>& loop, const Vec3& n) {
  Vec3 sum{0, 0, 0};
  for (size_t i = 1; i + 1 < loop.size(); ++i) {
    sum = add(sum, cross(sub(loop[i], loop[0]), sub(loop[i + 1], loop[0])));
  }
  return 0.5 * dot(sum, n);
}

}  // namespace

Solid box_solid(const Vec3& lo, const Vec3& hi, int tag) {
  Solid s;
  s.planes = {{{1, 0, 0}, hi.x, tag},   {{-1, 0, 0}, -lo.x, tag}, {{0, 1, 0}, hi.y, tag},
              {{0, -1, 0}, -lo.y, tag}, {{0, 0, 1}, hi.z, tag},   {{0, 0, -1}, -lo.z, tag}};
  return s;
}

Solid prism_solid(std::span<const double> xz, double y0, double y1, int side_tag, int cap_tag) {
  Solid s;
  const size_t n = xz.size() / 2;
  double cx = 0, cz = 0;
  for (size_t i = 0; i < n; ++i) {
    cx += xz[i * 2];
    cz += xz[i * 2 + 1];
  }
  cx /= static_cast<double>(n);
  cz /= static_cast<double>(n);
  for (size_t i = 0; i < n; ++i) {
    const double ax = xz[i * 2], az = xz[i * 2 + 1];
    const double bx = xz[(i + 1) % n * 2], bz = xz[(i + 1) % n * 2 + 1];
    Vec3 nrm = normalized(Vec3{bz - az, 0, -(bx - ax)});
    if (nrm.x * (cx - ax) + nrm.z * (cz - az) > 0) nrm = mul(nrm, -1);
    s.planes.push_back({nrm, nrm.x * ax + nrm.z * az, side_tag});
  }
  s.planes.push_back({{0, 1, 0}, y1, cap_tag});
  s.planes.push_back({{0, -1, 0}, -y0, cap_tag});
  return s;
}

std::vector<Polygon> polygons_of(const Solid& s) {
  std::vector<Polygon> out;
  std::vector<Vec3> loop, next;
  for (size_t i = 0; i < s.planes.size(); ++i) {
    const Plane& p = s.planes[i];
    const Vec3 u = any_perpendicular(p.n);
    const Vec3 v = cross(p.n, u);  // u x v = n: the square winds round the normal
    const Vec3 c = mul(p.n, p.d);
    loop = {mad(mad(c, u, -kStart), v, -kStart), mad(mad(c, u, kStart), v, -kStart),
            mad(mad(c, u, kStart), v, kStart), mad(mad(c, u, -kStart), v, kStart)};
    for (size_t j = 0; j < s.planes.size() && loop.size() >= 3; ++j) {
      if (j == i) continue;
      clip(loop, s.planes[j], next);
      loop.swap(next);
    }
    weld(loop);
    if (loop.size() < 3 || loop_area(loop, p.n) < 1e-10) continue;
    out.push_back({static_cast<int>(i), loop});
  }
  return out;
}

bool contains(const Solid& s, const Vec3& p, double margin) {
  for (const Plane& pl : s.planes) {
    if (dot(pl.n, p) - pl.d > -margin) return false;
  }
  return true;
}

void triangulate(const Solid& s, std::span<const Polygon> polys, TriMesh& out) {
  for (const Polygon& poly : polys) {
    const auto base = static_cast<uint32_t>(out.pos.size());
    out.pos.insert(out.pos.end(), poly.loop.begin(), poly.loop.end());
    const auto tag = static_cast<uint16_t>(s.planes[static_cast<size_t>(poly.plane)].tag);
    for (uint32_t k = 1; k + 1 < poly.loop.size(); ++k) {
      out.idx.insert(out.idx.end(), {base, base + k, base + k + 1});
      out.tag.push_back(tag);
    }
  }
}

}  // namespace ge::detail
