// A HALL BUILT FROM BRUSHES for the detail passes' tests and labs: a floor, three walls (the
// fourth side open to the lens), a doorway of two jambs under a lintel, two pillars on plinths,
// a flight of three steps up to a dais, a block pushed flush into a corner, and a sunken basin with
// a kerb (the damp corner). Every piece is a convex `Solid`, and pieces meet flush the way a
// level's brushes do, so the bevel pass has buried edges to keep sharp. 12 x 10 m, 4 m high, floor
// at y 0.
//
// ge:summary A brush-built test hall (walls, doorway, pillars, steps, a basin) for detail labs
// ge:keywords detail test-hall fixture scene brushes hall pillar steps doorway basin damp
// ge:skill detail labs
// ge:test tests/detail_bevel_test.cpp
#pragma once

#include <cstdint>
#include <vector>

#include "detail/solid.hpp"

namespace ge::detail {

// The tags the hall's faces carry.
enum HallTag : uint8_t { kHallFloor = 0, kHallWall = 1, kHallTimber = 2, kHallBasin = 3 };

struct TestHall {
  std::vector<Solid> solids;
  Vec3 damp_centre{0, 0, 0};  // the basin's middle
  double damp_radius_m = 0;   // the damp reaches this far round it, falling off
};
TestHall test_hall();

// The hall's damp at `p`, 0..1: a `DampAt` for the props (ctx: the TestHall).
double hall_damp(const void* ctx, const Vec3& p);

}  // namespace ge::detail
