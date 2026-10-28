// The test hall's pieces.
//
// ge:summary The test hall's brushes and its damp field
// ge:keywords detail test-hall brushes damp
// ge:skill detail labs
// ge:test tests/detail_bevel_test.cpp
#include "detail/test_hall.hpp"

#include <cmath>

#include "core/libm/libm.hpp"
#include "core/math.hpp"

namespace ge::detail {

TestHall test_hall() {
  TestHall h;
  auto box = [&](Vec3 lo, Vec3 hi, int tag) { h.solids.push_back(box_solid(lo, hi, tag)); };
  // The floor slab, in four pieces round the basin's hole, and the walls: back (z 10), left
  // (x -6) and right (x 6); the walls stand ON the slab, flush with its top.
  box({-6.5, -0.5, 4.3}, {6.5, 0, 10.5}, kHallFloor);
  box({-6.5, -0.5, -0.5}, {6.5, 0, 0.7}, kHallFloor);
  box({-6.5, -0.5, 0.7}, {-5.3, 0, 4.3}, kHallFloor);
  box({-1.7, -0.5, 0.7}, {6.5, 0, 4.3}, kHallFloor);
  box({-6.5, 0, 10}, {6.5, 4, 10.5}, kHallWall);
  box({-6.5, 0, -0.5}, {-6, 4, 10}, kHallWall);
  box({6, 0, -0.5}, {6.5, 4, 10}, kHallWall);
  // A doorway in the back wall's face: two jambs and a timber lintel standing proud of it.
  box({-1.4, 0, 9.6}, {-0.9, 2.6, 10}, kHallWall);
  box({0.9, 0, 9.6}, {1.4, 2.6, 10}, kHallWall);
  box({-1.6, 2.6, 9.55}, {1.6, 3.0, 10}, kHallTimber);
  // Two octagonal pillars on square plinths.
  for (const double px : {-3.0, 3.0}) {
    box({px - 0.55, 0, 5.45}, {px + 0.55, 0.4, 6.55}, kHallWall);
    std::vector<double> oct;
    for (int k = 0; k < 8; ++k) {
      const double a = (k + 0.5) * core::kTau / 8;
      oct.push_back(px + 0.42 * core::libm::cos(a));
      oct.push_back(6 + 0.42 * core::libm::sin(a));
    }
    h.solids.push_back(prism_solid(oct, 0.4, 4, kHallWall, kHallWall));
  }
  // Three steps up to a dais along the right wall: each step a block on the one below.
  box({3.6, 0, 7.2}, {6, 0.2, 10}, kHallFloor);
  box({3.9, 0.2, 7.5}, {6, 0.4, 10}, kHallFloor);
  box({4.2, 0.4, 7.8}, {6, 0.6, 10}, kHallFloor);
  // A block pushed flush into the back-left corner.
  box({-6, 0, 8.8}, {-4.8, 0.9, 10}, kHallWall);
  // The basin: a kerb of four blocks round a sunken floor at the front left.
  box({-5, -0.5, 1}, {-2, -0.3, 4}, kHallBasin);
  box({-5.3, -0.5, 0.7}, {-1.7, 0.25, 1}, kHallWall);
  box({-5.3, -0.5, 4}, {-1.7, 0.25, 4.3}, kHallWall);
  box({-5.3, -0.5, 1}, {-5, 0.25, 4}, kHallWall);
  box({-2, -0.5, 1}, {-1.7, 0.25, 4}, kHallWall);
  h.damp_centre = {-3.5, 0, 2.5};
  h.damp_radius_m = 3.2;
  return h;
}

double hall_damp(const void* ctx, const Vec3& p) {
  const auto* h = static_cast<const TestHall*>(ctx);
  const double d = core::hypot(p.x - h->damp_centre.x, p.z - h->damp_centre.z);
  return ramp(h->damp_radius_m, h->damp_radius_m * 0.35, d);
}

}  // namespace ge::detail
