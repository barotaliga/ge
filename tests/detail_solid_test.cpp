// engine/detail/solid.hpp: a box's six faces wind outward and close; a prism stands on its
// outline; a plane the others cut away leaves no face; `contains` holds the inside by a margin;
// the triangles carry their planes' tags.
//
// ge:summary Test: convex solids' faces, winding, prisms, contains and triangles
// ge:keywords test detail solid polygon winding prism contains triangulate
// ge:skill detail testing
#include <cmath>
#include <vector>

#include "check.hpp"
#include "detail/mesh.hpp"
#include "detail/solid.hpp"

namespace {

namespace gd = ge::detail;

gd::Vec3 centroid(const gd::Polygon& p) {
  gd::Vec3 c{0, 0, 0};
  for (const gd::Vec3& v : p.loop) c = gd::add(c, v);
  return gd::mul(c, 1.0 / static_cast<double>(p.loop.size()));
}

}  // namespace

TEST(a_box_has_six_faces_wound_outward) {
  const gd::Solid s = gd::box_solid({-1, 0, -2}, {1, 3, 2}, 7);
  const auto polys = gd::polygons_of(s);
  CHECK(polys.size() == 6);
  for (const gd::Polygon& p : polys) {
    CHECK(p.loop.size() == 4);
    const gd::Vec3& n = s.planes[static_cast<size_t>(p.plane)].n;
    const gd::Vec3 w = gd::cross(gd::sub(p.loop[1], p.loop[0]), gd::sub(p.loop[2], p.loop[0]));
    CHECK(gd::dot(w, n) > 0);
    // Out of the box: the face's middle is on the side of its normal from the box's middle.
    CHECK(gd::dot(gd::sub(centroid(p), {0, 1.5, 0}), n) > 0);
    for (const gd::Vec3& v : p.loop) {
      CHECK(std::fabs(v.x) <= 1 + 1e-9 && v.y >= -1e-9 && v.y <= 3 + 1e-9);
    }
  }
  gd::TriMesh m;
  gd::triangulate(s, polys, m);
  CHECK(m.triangles() == 12);
  double area = 0;
  for (uint32_t t = 0; t < m.triangles(); ++t) {
    CHECK(m.tag[t] == 7);
    area += gd::area_of(m, t);
  }
  CHECK(std::fabs(area - 2 * (2 * 3 + 2 * 4 + 3 * 4)) < 1e-9);
}

TEST(a_plane_cut_away_leaves_no_face) {
  gd::Solid s = gd::box_solid({0, 0, 0}, {1, 1, 1});
  s.planes.push_back({{1, 0, 0}, 5, 0});  // outside the box: it bounds nothing
  CHECK(gd::polygons_of(s).size() == 6);
  s.planes.push_back({gd::normalized({1, 1, 0}), 1.2, 3});  // a cut off one edge
  const auto polys = gd::polygons_of(s);
  CHECK(polys.size() == 7);
  CHECK(polys.back().plane == 7);
}

TEST(a_prism_stands_on_its_outline) {
  const double xz[] = {0, 0, 2, 0, 2, 1, 0, 1};  // either winding works
  const double zx[] = {0, 0, 0, 1, 2, 1, 2, 0};
  for (const double* o : {xz, zx}) {
    const gd::Solid s = gd::prism_solid({o, 8}, 0.5, 1.5, 1, 2);
    CHECK(s.planes.size() == 6);
    CHECK(gd::contains(s, {1, 1, 0.5}, 0.4));
    CHECK(!gd::contains(s, {1, 1, 0.5}, 0.6));
    CHECK(!gd::contains(s, {3, 1, 0.5}));
    CHECK(gd::polygons_of(s).size() == 6);
  }
}
