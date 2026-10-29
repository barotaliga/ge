// CLIP HULLS AND THE LINE TRACE: the box hull's contents (solid when mins <= p < maxs), an
// impact's fraction, end point (1/32 back), plane facing the start; a start in solid; leaving a
// solid; open and liquid leaves.
//
// ge:summary Tests: point contents, the box hull and the hull line trace's every outcome
// ge:keywords test hull trace box-hull point-contents startsolid allsolid impact epsilon
// ge:skill shooter testing
#include "check.hpp"
#include "shooter/hull.hpp"

namespace sh = ge::shooter;

TEST(box_hull_contents) {
  const sh::BoxHull b = sh::make_box_hull({-10, -10, -10}, {10, 10, 10});
  const sh::Hull h = b.hull();
  CHECK(sh::point_contents(h, {0, 0, 0}) == sh::kContentsSolid);
  CHECK(sh::point_contents(h, {-10, -10, -10}) == sh::kContentsSolid);  // mins is inside
  CHECK(sh::point_contents(h, {10, 0, 0}) == sh::kContentsEmpty);       // maxs is outside
  CHECK(sh::point_contents(h, {0, 0, 10}) == sh::kContentsEmpty);
  CHECK(sh::point_contents(h, {0, 0, -10.001f}) == sh::kContentsEmpty);
  CHECK(sh::plane_type({1, 0, 0}) == 0 && sh::plane_type({0, 0, 1}) == 2);
  CHECK(sh::plane_type({-1, 0, 0}) == 3);
}

TEST(trace_hits_a_box_face_one_thirty_second_short) {
  const sh::BoxHull b = sh::make_box_hull({-10, -10, -10}, {10, 10, 10});
  // From +x into the box: the +x face at 10, reached at fraction 0.5 of 30 -> 10.
  const sh::HullTrace t = sh::trace_hull(b.hull(), {25, 0, 0}, {-5, 0, 0});
  CHECK(!t.startsolid && !t.allsolid && t.inopen);
  CHECK(t.plane.normal == (sh::Vec3f{1, 0, 0}) && t.plane.dist == 10);
  CHECK(t.endpos.x == 10.03125f);
  CHECK(t.fraction == (15 - 0.03125f) / 30);
  // From -x: the plane faces back toward the start.
  const sh::HullTrace u = sh::trace_hull(b.hull(), {-20, 0, 0}, {0, 0, 0});
  CHECK(u.plane.normal == (sh::Vec3f{-1, 0, 0}) && u.plane.dist == 10);
  CHECK(u.endpos.x == -10.03125f);
}

TEST(trace_that_misses_runs_its_length) {
  const sh::BoxHull b = sh::make_box_hull({-10, -10, -10}, {10, 10, 10});
  const sh::HullTrace t = sh::trace_hull(b.hull(), {20, 20, 0}, {-20, 20, 0});
  CHECK(t.fraction == 1 && !t.startsolid && t.inopen);
  CHECK(t.endpos == (sh::Vec3f{-20, 20, 0}));
}

TEST(trace_in_solid) {
  const sh::BoxHull b = sh::make_box_hull({-10, -10, -10}, {10, 10, 10});
  // Inside all the way: all solid, ends at its start, fraction 1.
  const sh::HullTrace t = sh::trace_hull(b.hull(), {0, 0, 0}, {5, 0, 0});
  CHECK(t.startsolid && t.allsolid);
  CHECK(t.fraction == 1 && t.endpos == (sh::Vec3f{0, 0, 0}));
  // Starting inside and leaving is allowed: started in solid, not all solid, runs out.
  const sh::HullTrace u = sh::trace_hull(b.hull(), {0, 0, 0}, {30, 0, 0});
  CHECK(u.startsolid && !u.allsolid && u.inopen);
  CHECK(u.fraction == 1 && u.endpos == (sh::Vec3f{30, 0, 0}));
}

TEST(a_liquid_leaf_marks_in_water) {
  // One plane: water below z = 0, open above.
  const sh::Plane planes[1] = {{{0, 0, 1}, 0, 2}};
  const sh::ClipNode nodes[1] = {{0, {sh::kContentsEmpty, sh::kContentsWater}}};
  const sh::Hull h{planes, nodes, 0, 0};
  CHECK(sh::point_contents(h, {0, 0, -1}) == sh::kContentsWater);
  CHECK(sh::point_contents(h, {0, 0, 0}) == sh::kContentsEmpty);
  const sh::HullTrace t = sh::trace_hull(h, {0, 0, 10}, {0, 0, -10});
  CHECK(t.inopen && t.inwater && !t.startsolid && t.fraction == 1);
}
