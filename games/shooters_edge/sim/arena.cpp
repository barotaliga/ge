// The arena's brushes and models (arena.hpp describes the map).
//
// ge:summary The arena's brushes: floor, walls, the doorway wall, the ledge, the door, the plat
// ge:keywords build_arena box_brush brushes models
// ge:skill shooter
// ge:test tests/shooters_edge_match_test.cpp
#include "shooters_edge/sim/arena.hpp"

namespace shooters_edge::sim {

sh::Brush box_brush(sh::Vec3f mins, sh::Vec3f maxs, int contents) {
  sh::Brush b;
  b.contents = contents;
  b.planes = {{{1, 0, 0}, maxs.x, 0},  {{-1, 0, 0}, -mins.x, 3}, {{0, 1, 0}, maxs.y, 1},
              {{0, -1, 0}, -mins.y, 3}, {{0, 0, 1}, maxs.z, 2},  {{0, 0, -1}, -mins.z, 3}};
  return b;
}

std::vector<sh::BrushModel> Arena::models() const {
  return {world.model(), door.model(), plat.model()};
}

void build_arena(Arena& a) {
  a.world_brushes = {
      box_brush({-528, -528, -64}, {528, 528, 0}),    // the floor
      box_brush({-528, -528, 0}, {-512, 528, 256}),   // the west wall
      box_brush({512, -528, 0}, {528, 528, 256}),     // the east wall
      box_brush({-512, -528, 0}, {512, -512, 256}),   // the south wall
      box_brush({-512, 512, 0}, {512, 528, 256}),     // the north wall
      box_brush({128, -512, 0}, {144, -64, 256}),     // the dividing wall, south of the doorway
      box_brush({128, 64, 0}, {144, 512, 256}),       // ... north of it
      box_brush({128, -64, 128}, {144, 64, 256}),     // the lintel over the doorway
      box_brush({192, -512, 0}, {512, -384, 104}),    // the ledge beside the plat
  };
  a.door_brush = box_brush({128, -64, 0}, {144, 64, 128});
  a.plat_brush = box_brush({192, -384, 0}, {320, -256, 8});
  a.world = sh::build_brush_model(a.world_brushes);
  a.door = sh::build_brush_model({a.door_brush});
  a.plat = sh::build_brush_model({a.plat_brush});
  a.trigger_mins = {-32, -64, 0};  // west of the doorway only: a player standing in it is not
  a.trigger_maxs = {108, 64, 128};  // touching the trigger, so the closing door can meet him
  a.health_origin = {-272, -8, 0};
  a.spawn[0] = {-416, 0, 24};
  a.spawn_yaw[0] = 0;
  a.spawn[1] = {256, -320, 32};  // standing on the plat
  a.spawn_yaw[1] = 90;
}

}  // namespace shooters_edge::sim
