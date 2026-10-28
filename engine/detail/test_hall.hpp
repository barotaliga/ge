// A HALL BUILT FROM BRUSHES for the detail passes' tests and labs: a floor, three walls (the
// fourth side open to the lens), a doorway of two jambs under a lintel, two pillars on plinths,
// a flight of three steps up to a dais, a block pushed flush into a corner, and a sunken basin with
// a kerb (the damp corner). Every piece is a convex `Solid`, and pieces meet flush the way a
// level's brushes do, so the bevel pass has buried edges to keep sharp. 12 x 10 m, 4 m high, floor
// at y 0.
//
// And A YARD, for the clutter that depends on what a floor is made of and on the open sky: under
// the sky a lawn named grass, a strip of bare earth, a second lawn known only by its green
// colour, and a flagged court; on the court a roofed room with a brick shell, a metal deck and a
// timber floor, a doorway out to the court, and a damp patch by the door. 20 x 16 m. Each tag
// carries a texture's name and mean colour, as a level would hand them over.
//
// ge:summary Brush-built test scenes for detail labs: a hall, and a yard of mixed floors and a room
// ge:keywords detail test-hall test-yard fixture scene brushes hall pillar steps doorway basin damp
// ge:keywords yard lawn dirt court room roof texture-name surface-class
// ge:skill detail labs
// ge:test tests/detail_bevel_test.cpp
#pragma once

#include <cstdint>
#include <string>
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

struct YardTexture {
  std::string name;
  double rgb[3];  // its mean colour, 0..1
};
struct TestYard {
  std::vector<Solid> solids;
  std::vector<YardTexture> textures;  // by tag
  Vec3 damp_centre{0, 0, 0};
  double damp_radius_m = 0;
};
TestYard test_yard();
// The yard's damp at `p` (ctx: the TestYard).
double yard_damp(const void* ctx, const Vec3& p);

}  // namespace ge::detail
