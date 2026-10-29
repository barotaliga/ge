// THE SOLIDS OF ONE MOVE: the player's box traced through the world and a box, the smallest
// fraction winning (ties to the earlier solid, -1 when none is hit), a start in solid counted as
// fraction 0, a box's grown bounds skipping it when the sweep misses; the free-box test and the
// two point contents.
//
// ge:summary Tests: the player trace over a move's solids, box_free, water and brush contents
// ge:keywords test player_trace box_free physent box solids water_contents brush_contents
// ge:skill shooter testing
#include "check.hpp"
#include "shooter/brush_hull.hpp"
#include "shooter/world.hpp"

namespace sh = ge::shooter;

namespace {

sh::Brush box(int contents, sh::Vec3f lo, sh::Vec3f hi) {
  sh::Brush b;
  b.contents = contents;
  b.planes = {{{1, 0, 0}, hi.x},   {{-1, 0, 0}, -lo.x}, {{0, 1, 0}, hi.y},
              {{0, -1, 0}, -lo.y}, {{0, 0, 1}, hi.z},   {{0, 0, -1}, -lo.z}};
  return b;
}

struct Scene {
  sh::HullData point, clip;
  sh::MoveWorld world;
  Scene() {
    const std::vector<sh::Brush> brushes = {
        box(sh::kContentsSolid, {-1024, -1024, -64}, {1024, 1024, 0}),
        box(sh::kContentsWater, {200, -100, 0}, {400, 100, 64})};
    point = sh::build_point_hull(brushes);
    clip = sh::build_box_hull(brushes, sh::kPlayerMins, sh::kPlayerMaxs);
    world.solids[0].point_hull = point.hull();
    world.solids[0].clip_hull = clip.hull();
    // Another player's box at x = 100.
    world.solids[1].kind = sh::SolidKind::box;
    world.solids[1].origin = {100, 0, 24};
    world.solids[1].mins = sh::kPlayerMins;
    world.solids[1].maxs = sh::kPlayerMaxs;
    world.count = 2;
  }
};

}  // namespace

TEST(the_nearest_solid_wins) {
  const Scene s;
  // Running along the floor into the box: it stops 32 short of the box's origin, 1/32 back.
  const sh::HullTrace t = sh::player_trace(s.world, {0, 0, 24}, {200, 0, 24});
  CHECK(t.ent == 1);
  CHECK(t.endpos.x == 100 - 32 - 0.03125f);
  CHECK(t.plane.normal == (sh::Vec3f{-1, 0, 0}));
  // Down into the floor: the world.
  const sh::HullTrace d = sh::player_trace(s.world, {0, 0, 40}, {0, 0, 0});
  CHECK(d.ent == 0 && d.plane.normal.z == 1);
  CHECK(d.endpos.z == 24.03125f);
  // Nothing in the way: no solid.
  const sh::HullTrace n = sh::player_trace(s.world, {0, 0, 24}, {0, 300, 24});
  CHECK(n.ent == -1 && n.fraction == 1);
  CHECK(n.endpos == (sh::Vec3f{0, 300, 24}));
}

TEST(a_start_in_solid_is_fraction_zero) {
  const Scene s;
  const sh::HullTrace t = sh::player_trace(s.world, {0, 0, 10}, {50, 0, 10});
  CHECK(t.startsolid && t.fraction == 0 && t.ent == 0);
}

TEST(box_free_and_contents) {
  const Scene s;
  CHECK(sh::box_free(s.world, {0, 0, 24}));
  CHECK(!sh::box_free(s.world, {0, 0, 23}));
  CHECK(!sh::box_free(s.world, {110, 0, 24}));  // inside the other player
  CHECK(sh::water_contents(s.world, {300, 0, 10}) == sh::kContentsWater);
  CHECK(sh::water_contents(s.world, {0, 0, 10}) == sh::kContentsEmpty);
  CHECK(sh::brush_contents(s.world, {0, 0, -1}) == sh::kContentsSolid);
  CHECK(sh::brush_contents(s.world, {300, 0, 10}) == sh::kContentsWater);
}
