// A LADDER IN A VERSION 38 MAP, climbed by the classic move as Quake II's climbs it: the shared
// rule's wish (200 up or down by the view or the up move, the level part held to 25) and its hold
// (gravity stops at 0); the classic move walking into a ladder wall climbs it, looking down comes
// back down, letting go of the keys holds on, and the same wall without the ladder bit is never
// climbed; the move says it held the rungs; a slide along a sloped wall of brushes goes on along
// it instead of stopping dead on the plane met twice.
//
// ge:summary Tests: the shared ladder rule, and the classic move climbing a version 38 ladder
// ge:keywords test ladder climb rungs ladder-wish ladder-hold remix-ladder version-38 player_move
// ge:keywords same-plane slide-move sloped-wall
// ge:skill shooter testing
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "brush/read.hpp"
#include "brush/trace.hpp"
#include "brush/write.hpp"
#include "check.hpp"
#include "shooter/ladder.hpp"
#include "shooter/pmove.hpp"
#include "shooter/world.hpp"

namespace br = ge::brush;
namespace sh = ge::shooter;

namespace {

br::WorldBrush wbox(float x0, float y0, float z0, float x1, float y1, float z1, int32_t contents) {
  const float lo[3] = {x0, y0, z0}, hi[3] = {x1, y1, z1};
  return br::box_brush(lo, hi, contents, "world/floor");
}

// A floor and a wall 256 high across +y at y 64..80: a ladder (solid and ladder) or a plain wall.
struct Wall {
  br::Map map;
  std::unique_ptr<br::BrushTracer> tracer;
  sh::MoveWorld world;
  explicit Wall(bool ladder) {
    br::BrushWorld w;
    w.models.resize(1);
    w.models[0].push_back(wbox(-512, -512, -16, 512, 512, 0, sh::kSqSolid));
    w.models[0].push_back(
        wbox(-128, 64, 0, 128, 80, 256, sh::kSqSolid | (ladder ? sh::kSqLadder : 0)));
    std::vector<uint8_t> bytes;
    std::string error;
    CHECK_MSG(br::write_brush_world(w, &bytes, &error), error);
    CHECK_MSG(br::read_bsp(bytes, &map, &error), error);
    tracer = std::make_unique<br::BrushTracer>(map);
    world.solids[0] = sh::brushes_solid(*tracer, 0, {}, {-512, -512, -16}, {512, 512, 256});
    world.count = 1;
  }
};

// Standing on the floor facing the wall (+y), its face 16 ahead of the box's.
sh::MoveState facing_the_wall() {
  sh::MoveState s;
  s.origin = {0, 32, 24};
  s.angles = {0, 90, 0};
  s.ground = 0;
  s.ground_normal = {0, 0, 1};
  return s;
}

sh::MoveCmd cmd(int16_t forward, float pitch, int16_t up = 0) {
  sh::MoveCmd c;
  c.msec = 13;
  c.angles = {pitch, 90, 0};
  c.forward = forward;
  c.up = up;
  return c;
}

}  // namespace

TEST(the_wish_climbs_by_the_view_or_the_up_move_and_is_held_to_the_rungs) {
  sh::Vec3f w{300, -300, 0};
  sh::ladder_wish(w, 0, -30, 400, 0);  // looking up, moving forward: climbs
  CHECK(w.z == 200 && w.x == 25 && w.y == -25);
  w = {0, 0, 0};
  sh::ladder_wish(w, 0, 30, 400, 0);  // looking down: descends
  CHECK(w.z == -200);
  w = {0, 0, 0};
  sh::ladder_wish(w, 0, 0, 400, 200);  // level: the up move climbs
  CHECK(w.z == 200);
  w = {0, 0, 0};
  sh::ladder_wish(w, 0, 0, 0, -200);  // crouch descends
  CHECK(w.z == -200);
  w = {0, 0, 0};
  sh::ladder_wish(w, 0, -30, 0, 0);  // looking up without moving: holds
  CHECK(w.z == 0);
  w = {300, 0, 7};
  sh::ladder_wish(w, -250, -30, 400, 0);  // falling past 200: the wish is left alone
  CHECK(w.x == 300 && w.z == 7);
}

TEST(holding_on_stops_at_zero) {
  sh::Vec3f v{0, 0, 5};
  sh::ladder_hold(v, 800, 0.013f);
  CHECK(v.z == 0);
  v = {0, 0, -20};
  sh::ladder_hold(v, 800, 0.013f);
  CHECK(v.z < -9 && v.z > -10);
}

TEST(the_classic_move_climbs_a_ladder_and_comes_back_down) {
  Wall wall(true);
  const sh::MoveSettings set;
  sh::MoveState s = facing_the_wall();
  bool held = false;
  for (int i = 0; i < 80; i++) held = sh::player_move(s, cmd(400, -30), set, wall.world).ladder;
  CHECK(held);
  CHECK_MSG(s.origin.z > 150, "climbed to " + std::to_string(s.origin.z));
  CHECK_MSG(s.velocity.z > 190 && s.velocity.z < 210,
            "climbing at " + std::to_string(s.velocity.z));
  // Letting go of the keys holds on: no fall.
  const float top = s.origin.z;
  for (int i = 0; i < 40; i++) sh::player_move(s, cmd(0, -30), set, wall.world);
  CHECK_MSG(s.origin.z > top - 4, "slid to " + std::to_string(s.origin.z));
  // Looking down while moving forward descends to the floor.
  for (int i = 0; i < 120; i++) sh::player_move(s, cmd(400, 30), set, wall.world);
  CHECK_MSG(s.origin.z < 25, "came down to " + std::to_string(s.origin.z));
}

TEST(a_wall_without_the_ladder_bit_is_never_climbed) {
  Wall wall(false);
  const sh::MoveSettings set;
  sh::MoveState s = facing_the_wall();
  for (int i = 0; i < 80; i++) CHECK(!sh::player_move(s, cmd(400, -30), set, wall.world).ladder);
  CHECK(s.origin.z < 25);
}

// A slide along a sloped wall of a version 38 map (its normal off the axes, as Quake II's maps'
// often are) goes on along it: the trace stops 1/32 short of the plane, so the slide meets the
// same plane again at no distance and is pushed off it rather than stopped dead.
TEST(a_slide_along_a_sloped_wall_of_brushes_goes_on) {
  br::BrushWorld w;
  w.models.resize(1);
  w.models[0].push_back(wbox(-512, -512, -16, 512, 512, 0, sh::kSqSolid));
  br::WorldBrush slope = wbox(0, -512, 0, 512, 512, 256, sh::kSqSolid);
  const float n[3] = {-0.894427f, 0.447214f, 0};  // faces -x, leaning toward +y
  br::add_side(&slope, n, 0);
  w.models[0].push_back(slope);
  std::vector<uint8_t> bytes;
  std::string error;
  br::Map map;
  CHECK_MSG(br::write_brush_world(w, &bytes, &error), error);
  CHECK_MSG(br::read_bsp(bytes, &map, &error), error);
  br::BrushTracer tracer(map);
  sh::MoveWorld world;
  world.solids[0] = sh::brushes_solid(tracer, 0, {}, {-512, -512, -16}, {512, 512, 256});
  world.count = 1;
  const sh::MoveSettings set;
  sh::MoveState s;
  s.origin = {0, 100, 24.03125f};  // the sloped face at x = y / 2 is 34 ahead of the box
  s.ground = 0;
  s.ground_normal = {0, 0, 1};
  float stalls = 0;
  for (int i = 0; i < 200; i++) {
    sh::MoveCmd c;
    c.msec = 13;
    c.angles = {0, 0, 0};  // straight at the wall: the slide carries the player along it
    c.forward = 400;
    const float y = s.origin.y;
    sh::player_move(s, c, set, world);
    if (i > 40 && s.origin.y - y < 0.5f) stalls += 1;
  }
  CHECK_MSG(stalls == 0, "stalled " + std::to_string(stalls) + " commands");
  CHECK_MSG(s.origin.y > 300, "slid to y " + std::to_string(s.origin.y));
}
