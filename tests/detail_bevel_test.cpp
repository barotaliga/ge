// engine/detail/bevel.hpp: a lone box has all twelve edges rounded, stays inside its box and loses
// its corners; two boxes flush side by side keep the edges of the face they share sharp; a step on
// a step keeps the edges under it; a slab too thin for the radius keeps its edges as `thin`; a
// shallow fold stays `flat`; and the test hall's bevels keep their bits in
// tests/golden/detail_bevel.txt.
//
// A deliberate change to the bevels moves the golden: rewrite it with
// `GE_GOLDEN_WRITE=1 build/release/tests/detail_bevel_test` and put the `detail-bevel` lab's
// pictures before and after in the PR.
//
// ge:summary Test: bevels on exposed edges, buried and thin edges kept sharp; golden
// ge:keywords test detail bevel chamfer round buried thin flat golden
// ge:skill detail testing
#include <cmath>
#include <vector>

#include "check.hpp"
#include "detail/bevel.hpp"
#include "detail/test_hall.hpp"
#include "detail_support.hpp"

namespace {

namespace gd = ge::detail;

int count(const gd::BevelResult& r, gd::EdgeVerdict v) {
  int n = 0;
  for (const gd::BevelEdge& e : r.edges) n += e.verdict == v ? 1 : 0;
  return n;
}

}  // namespace

TEST(a_lone_box_is_rounded_on_every_edge) {
  for (int seg : {1, 3}) {
    gd::BevelSpec spec;
    spec.radius_m = 0.1;
    spec.segments = seg;
    const gd::Solid box = gd::box_solid({0, 0, 0}, {1, 1, 1});
    const gd::BevelResult r = gd::bevel(box, spec);
    CHECK(r.edges.size() == 12);
    CHECK(count(r, gd::EdgeVerdict::bevelled) == 12);
    CHECK(r.solid.planes.size() == 6 + 12 * static_cast<size_t>(seg));
    const auto polys = gd::polygons_of(r.solid);
    CHECK(polys.size() == r.solid.planes.size());
    double nearest_corner = 9;
    for (const gd::Polygon& p : polys) {
      for (const gd::Vec3& v : p.loop) {
        CHECK(v.x > -1e-9 && v.x < 1 + 1e-9 && v.y > -1e-9 && v.y < 1 + 1e-9 && v.z > -1e-9 &&
              v.z < 1 + 1e-9);
        nearest_corner = std::fmin(nearest_corner, gd::length(gd::sub(v, {1, 1, 1})));
      }
    }
    // The corner is cut back: no vertex within a third of the radius of it.
    CHECK(nearest_corner > 0.03);
    // A face's middle is untouched: the top still reaches y 1 at its centre.
    CHECK(gd::contains(r.solid, {0.5, 0.999, 0.5}));
    CHECK(!gd::contains(r.solid, {0.995, 0.995, 0.5}));
  }
}

TEST(flush_neighbours_keep_their_shared_edges_sharp) {
  const std::vector<gd::Solid> two = {gd::box_solid({0, 0, 0}, {1, 1, 1}),
                                      gd::box_solid({1, 0, 0}, {2, 1, 1})};
  const auto r = gd::bevel_all(two, {});
  for (const gd::BevelResult& b : r) {
    // The four edges round the shared face are buried; the other eight are rounded.
    CHECK(count(b, gd::EdgeVerdict::buried) == 4);
    CHECK(count(b, gd::EdgeVerdict::bevelled) == 8);
  }
  // A step on a step: the lower block's top edges under the upper one are buried, the ones
  // out in the open are not.
  const std::vector<gd::Solid> steps = {gd::box_solid({0, 0, 0}, {2, 0.2, 1}),
                                        gd::box_solid({0.5, 0.2, 0}, {2, 0.4, 1})};
  const auto s = gd::bevel_all(steps, {});
  CHECK(count(s[0], gd::EdgeVerdict::bevelled) > 0 && count(s[0], gd::EdgeVerdict::buried) > 0);
  // The lower step's nose (the top edge at x 0) is rounded.
  bool nose = false;
  for (const gd::BevelEdge& e : s[0].edges) {
    if (std::fabs(e.p0.x) < 1e-9 && std::fabs(e.p1.x) < 1e-9 && std::fabs(e.p0.y - 0.2) < 1e-9 &&
        std::fabs(e.p1.y - 0.2) < 1e-9) {
      nose = e.verdict == gd::EdgeVerdict::bevelled;
    }
  }
  CHECK(nose);
}

TEST(thin_and_shallow_edges_stay_sharp) {
  gd::BevelSpec spec;
  spec.radius_m = 0.05;
  const gd::BevelResult thin = gd::bevel(gd::box_solid({0, 0, 0}, {1, 0.02, 1}), spec);
  CHECK(count(thin, gd::EdgeVerdict::thin) == 8);      // the slab's rim
  CHECK(count(thin, gd::EdgeVerdict::bevelled) == 4);  // its four upright corners
  // An octagon's sides fold by 45 degrees: rounded at the default 25, flat at 50.
  std::vector<double> oct;
  for (int k = 0; k < 8; ++k) {
    oct.push_back(std::cos(k * 0.7853981633974483));
    oct.push_back(std::sin(k * 0.7853981633974483));
  }
  const gd::Solid pillar = gd::prism_solid(oct, 0, 2);
  CHECK(count(gd::bevel(pillar, spec), gd::EdgeVerdict::flat) == 0);
  spec.min_turn_deg = 50;
  CHECK(count(gd::bevel(pillar, spec), gd::EdgeVerdict::flat) == 8);
}

TEST(the_hall_keeps_its_bits) {
  const gd::TestHall hall = gd::test_hall();
  std::vector<std::vector<uint64_t>> got;
  for (int seg : {1, 3}) {
    gd::BevelSpec spec;
    spec.segments = seg;
    const auto r = gd::bevel_all(hall.solids, spec);
    uint64_t verdicts[5] = {};
    ge::test::BitsHash h;
    for (const gd::BevelResult& b : r) {
      for (const gd::BevelEdge& e : b.edges) ++verdicts[static_cast<int>(e.verdict)];
      for (const gd::Plane& p : b.solid.planes) {
        h.add(p.n);
        h.add(p.d);
      }
    }
    got.push_back({static_cast<uint64_t>(seg), verdicts[0], verdicts[1], verdicts[2], verdicts[3],
                   verdicts[4], h.h});
  }
  ge::test::check_golden("tests/golden/detail_bevel.txt", got,
                         "per segments: bevelled flat knife thin buried, the planes' hash "
                         "(detail_bevel_test)",
                         "rewrite with GE_GOLDEN_WRITE=1 on the release build and put the "
                         "detail-bevel lab's pictures before and after in the PR (skill: detail)");
}
