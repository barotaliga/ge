// THE SOLIDS OF ONE MOVE: the player's box traced through the world and a box, the smallest
// fraction winning (ties to the earlier solid, -1 when none is hit), a start in solid counted as
// fraction 0, a box's grown bounds skipping it when the sweep misses; the free-box test and the
// two point contents. The brushes kind: a version 38 world traced through the brush tracer, the
// same answers as the tracer itself, the free-box test, liquids as contents numbers, the point
// trace (M18v2c).
//
// ge:summary Tests: the player trace over a move's solids, box_free, water and brush contents
// ge:keywords test player_trace box_free physent box solids water_contents brush_contents
// ge:keywords brushes brushes_solid point_trace version-38
// ge:skill shooter testing
#include <string>
#include <vector>

#include "brush/read.hpp"
#include "brush/trace.hpp"
#include "brush/write.hpp"
#include "check.hpp"
#include "shooter/brush_hull.hpp"
#include "shooter/sequel_move.hpp"
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

namespace {

namespace br = ge::brush;

br::WorldBrush wbox(float x0, float y0, float z0, float x1, float y1, float z1, int32_t contents) {
  const float lo[3] = {x0, y0, z0}, hi[3] = {x1, y1, z1};
  return br::box_brush(lo, hi, contents, "world/floor");
}

// A floor, a wall at x = 256 and a pool of water standing on the floor at x < -128.
struct Brushes {
  br::Map map;
  br::BrushTracer* tracer = nullptr;
  sh::MoveWorld world;
  Brushes() {
    br::BrushWorld w;
    w.models.resize(1);
    w.models[0].push_back(wbox(-512, -512, -16, 512, 512, 0, sh::kSqSolid));
    w.models[0].push_back(wbox(256, -512, 0, 272, 512, 256, sh::kSqSolid));
    w.models[0].push_back(wbox(-512, -512, 0, -128, 512, 64, sh::kSqWater));
    std::vector<uint8_t> bytes;
    std::string error;
    CHECK_MSG(br::write_brush_world(w, &bytes, &error), error);
    CHECK_MSG(br::read_bsp(bytes, &map, &error), error);
    tracer = new br::BrushTracer(map);
    world.solids[0] = sh::brushes_solid(*tracer, 0, {}, {-512, -512, -16}, {512, 512, 256});
    world.count = 1;
  }
  ~Brushes() { delete tracer; }
  Brushes(const Brushes&) = delete;
  Brushes& operator=(const Brushes&) = delete;
};

}  // namespace

TEST(brushes_trace_as_the_tracer_does) {
  Brushes b;
  const sh::HullTrace d = sh::player_trace(b.world, {0, 0, 64}, {0, 0, 0});
  const float a[3] = {0, 0, 64}, e[3] = {0, 0, 0};
  const float lo[3] = {-16, -16, -24}, hi[3] = {16, 16, 32};
  const br::Trace t =
      b.tracer->trace(a, e, lo, hi, sh::kSqSolid | sh::kSqPlayerClip | sh::kSqWindow, 0);
  CHECK(d.ent == 0 && d.plane.normal.z == 1);
  CHECK(d.fraction == t.fraction);
  CHECK(d.endpos.z == t.end[2]);
  CHECK(d.endpos.z > 24 && d.endpos.z < 24.1f);
  // Into the wall.
  const sh::HullTrace w = sh::player_trace(b.world, {0, 0, 24.5f}, {400, 0, 24.5f});
  CHECK(w.ent == 0 && w.plane.normal.x == -1);
  CHECK(w.endpos.x < 256 - 16 && w.endpos.x > 256 - 16.1f);
  // Open air: nothing hit.
  const sh::HullTrace n = sh::player_trace(b.world, {0, 0, 40}, {0, 200, 40});
  CHECK(n.ent == -1 && n.fraction == 1);
}

TEST(brushes_free_box_contents_and_point_trace) {
  Brushes b;
  CHECK(sh::box_free(b.world, {0, 0, 40}));
  CHECK(!sh::box_free(b.world, {0, 0, 10}));    // feet in the floor
  CHECK(!sh::box_free(b.world, {250, 0, 40}));  // inside the wall
  CHECK(sh::box_free(b.world, {-300, 0, 40}));  // water is not solid to the box
  CHECK(sh::water_contents(b.world, {-300, 0, 10}) == sh::kContentsWater);
  CHECK(sh::water_contents(b.world, {0, 0, 10}) == sh::kContentsEmpty);
  CHECK(sh::brush_contents(b.world, {0, 0, -1}) == sh::kContentsSolid);
  const sh::HullTrace p = sh::point_trace(b.world, {0, 0, 100}, {400, 0, 100});
  CHECK(p.fraction < 1 && p.plane.normal.x == -1);
  CHECK(p.endpos.x < 256 && p.endpos.x > 255.9f);
  const sh::HullTrace q = sh::point_trace(b.world, {0, 0, 100}, {0, 100, 100});
  CHECK(q.fraction == 1);
}
