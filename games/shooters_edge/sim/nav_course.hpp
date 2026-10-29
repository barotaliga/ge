// THE NAV COURSE: a hand-built map of convex brushes holding one of each way a player gets around,
// for the navigation graph's test and the `se-nav` lab (the test arena, arena.hpp, has only a
// floor, a door and a plat). Units are the map format's (x and y across, z up); the floor is at
// z = 0 inside walls 256 high round x = -512..512, y = -256..256. West to east:
// - a PLATFORM 64 high along the west wall (x < -384), climbed by three STAIRS of 16 at its south
//   end (y < -128) and left elsewhere by a drop of 64;
// - a CRATE 36 high (x = -192..-128, y = 64..128): onto it only by a jump;
// - a PIT 96 deep and 96 across (x = 0..96) from wall to wall: crossed only by a running jump,
//   dropped into anywhere and left up five steps of 16 at its north end (y > 96);
// - a RAMP rising 64 over 128 (27 degrees, x = 256..384, y < -128) up to an east LEDGE 64 high;
// - a STEEP RAMP rising 160 over 128 (51 degrees, y > 128) to a ledge 160 high: too steep to stand
//   on, so that ledge is an island no link reaches.
//
// ge:summary The nav course: stairs, a crate, a pit, a ramp and a steep ramp, for the nav graph
// ge:keywords nav-course nav map brushes stairs crate pit gap ramp steep-ramp ledge island
// ge:keywords build_nav_course ramp_brush
// ge:skill shooter bots
// ge:test tests/shooter_nav_test.cpp
#pragma once

#include <vector>

#include "shooter/brush_hull.hpp"

namespace shooters_edge::sim {

namespace sh = ge::shooter;

struct NavCourse {
  std::vector<sh::Brush> brushes;
  sh::ModelData world;
  // Standing origins the test and the lab route between.
  sh::Vec3f platform{-448, 0, 88};      // on the west platform
  sh::Vec3f crate_top{-160, 96, 60};    // on the crate
  sh::Vec3f pit_floor{48, -128, -72};   // in the pit
  sh::Vec3f east_floor{192, 0, 24};     // east of the pit
  sh::Vec3f east_ledge{448, -192, 88};  // on the ledge the ramp climbs to
  sh::Vec3f steep_top{448, 192, 184};   // on the ledge past the steep ramp (no way up)
};

// A ramp as a brush over (x0..x1, y0..y1) rising from z = 0 at x0 to z = rise at x1.
sh::Brush ramp_brush(float x0, float x1, float y0, float y1, float rise);

void build_nav_course(NavCourse& c);

}  // namespace shooters_edge::sim
