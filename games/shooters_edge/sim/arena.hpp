// THE TEST ARENA: a hand-built map for Shooter's Edge's loop, made of convex brushes (no compiled
// map yet: M18b), small enough to read from above and holding one of each thing the loop must
// handle. Two rooms on a floor at z = 0 split by a wall at x = 128..144 with a DOORWAY at
// y = -64..64 under a lintel; a DOOR (brush model 2) fills the doorway and slides up 120 units
// when a player touches the TRIGGER west of it, waits 3 s and comes back down (blocked by a
// player, it goes back up); a PLAT (brush model 3) in the east room at x = 192..320,
// y = -384..-256 rides 96 units up beside a LEDGE whose top is level with its top, waits and comes
// down, forever; a HEALTH box in the west room is an item (its trigger box grown 15 sideways).
// Units are the map format's: x and y across, z up; a player standing on the floor has
// origin z = 24.
//
// ge:summary Shooter's Edge's test arena: two rooms, a door and its trigger, a plat, a ledge
// ge:keywords arena map brushes test-map door plat ledge trigger health doorway build_arena
// ge:skill shooter
// ge:test tests/shooters_edge_match_test.cpp
#pragma once

#include <vector>

#include "shooter/brush_hull.hpp"

namespace shooters_edge::sim {

namespace sh = ge::shooter;

constexpr int kDoorModel = 2;  // the models' indices (1 is the world)
constexpr int kPlatModel = 3;

constexpr float kDoorLift = 120;    // units the door rises
constexpr float kDoorSpeed = 100;   // units/s
constexpr float kDoorWait = 3;      // s open before it closes
constexpr float kPlatRise = 96;     // units the plat rises
constexpr float kPlatSpeed = 64;    // units/s
constexpr float kPlatWait = 1.5f;   // s at each end

// A box as a brush: its six axial planes.
sh::Brush box_brush(sh::Vec3f mins, sh::Vec3f maxs, int contents = sh::kContentsSolid);

struct Arena {
  std::vector<sh::Brush> world_brushes;
  sh::Brush door_brush, plat_brush;
  sh::ModelData world, door, plat;
  // The trigger in front of the door and the health box, as boxes in the world.
  sh::Vec3f trigger_mins, trigger_maxs;
  sh::Vec3f health_origin;
  sh::Vec3f spawn[2];       // the two players' spawn origins
  float spawn_yaw[2] = {0, 0};

  // The models by index - 1 (world, door, plat). Views into this arena: keep it alive and unmoved.
  std::vector<sh::BrushModel> models() const;
};

void build_arena(Arena& a);

}  // namespace shooters_edge::sim
