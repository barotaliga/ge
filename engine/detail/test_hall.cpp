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

TestYard test_yard() {
  TestYard y;
  y.textures = {
      {"grass_lawn", {0.22, 0.34, 0.12}},   // 0: a lawn its name says is grass
      {"dirt_path", {0.33, 0.26, 0.18}},    // 1
      {"ground_02", {0.25, 0.36, 0.14}},    // 2: "ground" by name, grass by its green
      {"stone_flags", {0.45, 0.43, 0.4}},   // 3
      {"brick_wall", {0.42, 0.25, 0.2}},    // 4
      {"metal_plate", {0.36, 0.37, 0.38}},  // 5
      {"wood_floor", {0.4, 0.28, 0.16}},    // 6
      {"roof", {0.3, 0.3, 0.32}},           // 7
  };
  auto box = [&](Vec3 lo, Vec3 hi, int tag) { y.solids.push_back(box_solid(lo, hi, tag)); };
  // The ground, its top at y 0: the lawn, the path, the green ground and the court.
  box({-10, -0.4, 0}, {-2, 0, 16}, 0);
  box({-2, -0.4, 0}, {1, 0, 16}, 1);
  box({1, -0.4, 0}, {10, 0, 6}, 2);
  box({1, -0.4, 6}, {10, 0, 16}, 3);
  // A low garden wall along the lawn's back and a boulder on it, so stones have a foot to gather
  // at out in the open.
  box({-10, 0, 15.4}, {-2, 0.8, 16}, 4);
  box({-6.5, 0, 6.5}, {-5.3, 0.7, 7.6}, 3);
  // The room on the court: the floors (a metal deck and timber) laid on the flags, the brick shell
  // with a doorway in its front, and the roof.
  box({3, 0, 9}, {6, 0.08, 15}, 5);
  box({6, 0, 9}, {9, 0.08, 15}, 6);
  box({2.7, 0, 15}, {9.3, 3, 15.3}, 4);
  box({2.7, 0, 8.7}, {3, 3, 15}, 4);
  box({9, 0, 8.7}, {9.3, 3, 15}, 4);
  box({3, 0, 8.7}, {5.2, 3, 9}, 4);
  box({6.4, 0, 8.7}, {9, 3, 9}, 4);
  box({5.2, 2.3, 8.7}, {6.4, 3, 9}, 4);
  box({2.5, 3, 8.5}, {9.5, 3.25, 15.5}, 7);
  y.damp_centre = {5.8, 0, 8.2};
  y.damp_radius_m = 2.2;
  return y;
}

double yard_damp(const void* ctx, const Vec3& p) {
  const auto* y = static_cast<const TestYard*>(ctx);
  const double d = core::hypot(p.x - y->damp_centre.x, p.z - y->damp_centre.z);
  return ramp(y->damp_radius_m, y->damp_radius_m * 0.35, d);
}

}  // namespace ge::detail
