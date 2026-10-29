// engine/detail/receivers.hpp: a box query finds every triangle touching the box, each once; a ray
// finds the nearest triangle either face on, misses past its reach, and agrees with a walk of
// every triangle.
//
// ge:summary Test: receivers' box query and raycast against a brute-force walk
// ge:keywords test detail receivers grid query raycast
// ge:skill detail testing
#include <algorithm>
#include <cmath>
#include <vector>

#include "check.hpp"
#include "core/prng.hpp"
#include "detail/receivers.hpp"
#include "detail/solid.hpp"
#include "detail/test_hall.hpp"

namespace {

namespace gd = ge::detail;

gd::TriMesh hall_mesh() {
  gd::TriMesh m;
  for (const gd::Solid& s : gd::test_hall().solids) gd::triangulate(s, gd::polygons_of(s), m);
  return m;
}

}  // namespace

TEST(a_box_query_finds_every_triangle_once) {
  const gd::TriMesh m = hall_mesh();
  const gd::Receivers r = gd::bin_receivers(m, 1);
  gd::ReceiverScratch s;
  gd::triangles_in(r, {2.5, -0.1, 5.5}, {3.5, 1, 6.5}, s);
  std::vector<uint32_t> found = s.found;
  std::sort(found.begin(), found.end());
  CHECK(std::adjacent_find(found.begin(), found.end()) == found.end());
  // Every triangle with a corner in the box is among them.
  for (uint32_t t = 0; t < m.triangles(); ++t) {
    for (int k = 0; k < 3; ++k) {
      const gd::Vec3& v = gd::corner(m, t, k);
      if (v.x > 2.5 && v.x < 3.5 && v.y > -0.1 && v.y < 1 && v.z > 5.5 && v.z < 6.5) {
        CHECK(std::binary_search(found.begin(), found.end(), t));
      }
    }
  }
}

TEST(a_ray_finds_the_nearest_surface) {
  const gd::TriMesh m = hall_mesh();
  const gd::Receivers r = gd::bin_receivers(m, 1);
  gd::ReceiverScratch s;
  // Straight down onto the floor from 1 m.
  gd::Hit h = gd::raycast(r, {0.5, 1, 2}, {0, -1, 0}, 5, s);
  CHECK(h.hit && std::fabs(h.t - 1) < 1e-12 && std::fabs(h.at.y) < 1e-12);
  // Toward the right wall's inner face at x 6.
  h = gd::raycast(r, {5, 1, 2}, {1, 0, 0}, 5, s);
  CHECK(h.hit && std::fabs(h.t - 1) < 1e-12);
  // Short of it.
  CHECK(!gd::raycast(r, {5, 1, 2}, {1, 0, 0}, 0.5, s).hit);
  // Random rays agree with every triangle walked.
  ge::core::Rng rng = ge::core::make_rng(9);
  for (int i = 0; i < 300; ++i) {
    const gd::Vec3 from{ge::core::range(rng, -5.5, 5.5), ge::core::range(rng, 0.1, 3.5),
                        ge::core::range(rng, 0.2, 9.5)};
    const gd::Vec3 dir = gd::normalized(
        {ge::core::range(rng, -1, 1), ge::core::range(rng, -1, 1), ge::core::range(rng, -1, 1)});
    const gd::Hit got = gd::raycast(r, from, dir, 3, s);
    double best = 3;
    bool any = false;
    for (uint32_t t = 0; t < m.triangles(); ++t) {
      const gd::Vec3& a = gd::corner(m, t, 0);
      const gd::Vec3 n = gd::normal_of(m, t);
      const double den = gd::dot(n, dir);
      if (std::fabs(den) < 1e-12) continue;
      const double tt = gd::dot(n, gd::sub(a, from)) / den;
      if (tt <= 1e-9 || tt > best) continue;
      const gd::Vec3 p = gd::mad(from, dir, tt);
      // Inside the triangle: on the inner side of each edge.
      bool in = true;
      for (int k = 0; k < 3; ++k) {
        const gd::Vec3& p0 = gd::corner(m, t, k);
        const gd::Vec3& p1 = gd::corner(m, t, (k + 1) % 3);
        if (gd::dot(gd::cross(gd::sub(p1, p0), gd::sub(p, p0)), n) < -1e-9) in = false;
      }
      if (in) {
        best = tt;
        any = true;
      }
    }
    CHECK(got.hit == any);
    if (any) CHECK(std::fabs(got.t - best) < 1e-9);
  }
}
