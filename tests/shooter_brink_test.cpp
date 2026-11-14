// THE BRINK: beside a pit of lava, a forward key over the lip is let go of, a run into it is
// braked and its jump let go of, a key along the lip or away from it is kept, and far from any
// harm nothing changes; the nav marks the walks along a lip and no other, and a walk whose side
// step falls into lava far below; a run off a stair's step whose flight lands in the lava is
// braked in the air (`fall`), and one that lands on a wide step left alone.
//
// ge:summary Tests: the brink guard's keys by a lava pit, and the nav's brink links
// ge:keywords test brink brink_guard over_harm nav_mark_harm nav_harm_near lava-lip lava pit
// ge:keywords fall_into_harm stair-fall
// ge:skill shooter testing
#include <algorithm>
#include <vector>

#include "check.hpp"
#include "shooter/brink.hpp"
#include "shooter/brush_hull.hpp"
#include "shooter/hull.hpp"
#include "shooter/usercmd.hpp"
#include "shooter_loop_bench.hpp"

namespace sh = ge::shooter;
using ge::test::loop::Bench;
using ge::test::loop::box;

namespace {

// A floor at z 0 west of x 0 and east of x 512; between, a pit floored at -128 and lava to -64.
void pit(Bench& b) {
  sh::Brush lava = box({0, -1024, -128}, {512, 1024, -64});
  lava.contents = sh::kContentsLava;
  b.start({box({-1024, -1024, -64}, {0, 1024, 0}), box({512, -1024, -64}, {1024, 1024, 0}),
           box({-1024, -1024, -192}, {1024, 1024, -128}), lava});
}

// A graph on a 32-unit grid over the bench's world holding `nodes` (each in its own column, in
// column order), those under the lava marked harm, and `links` (from, to) walks.
sh::NavGraph graph(std::vector<sh::Vec3f> nodes, const std::vector<std::pair<int, int>>& links) {
  sh::NavGraph g;
  g.lo = {-1024, -1024, -256};
  g.hi = {1024, 1024, 256};
  g.cols = g.rows = 64;
  const auto col = [&](sh::Vec3f o) {
    return static_cast<int32_t>((o.x - g.lo.x) / 32) * g.rows +
           static_cast<int32_t>((o.y - g.lo.y) / 32);
  };
  std::sort(nodes.begin(), nodes.end(), [&](sh::Vec3f a, sh::Vec3f c) { return col(a) < col(c); });
  const size_t columns = static_cast<size_t>(g.cols) * static_cast<size_t>(g.rows);
  g.col_first.assign(columns, 0);
  g.col_n.assign(columns, 0);
  for (const sh::Vec3f o : nodes) g.col_n[static_cast<size_t>(col(o))]++;
  int32_t at = 0;
  for (size_t c = 0; c < g.col_first.size(); c++) {
    g.col_first[c] = at;
    at += g.col_n[c];
  }
  std::vector<uint8_t> harm;
  for (const sh::Vec3f o : nodes) {
    sh::NavNode n;
    n.origin = o;
    n.first_link = static_cast<int32_t>(g.links.size());
    for (const auto& [a, to] : links)
      if (a == static_cast<int>(g.nodes.size())) {
        sh::NavLink l;
        l.to = to;
        g.links.push_back(l);
        n.links++;
      }
    g.nodes.push_back(n);
    harm.push_back(o.z < -64 ? 1 : 0);
  }
  sh::nav_mark_harm(g, harm);
  return g;
}

sh::MoveCmd keys(float yaw, float forward, float side) {
  sh::MoveCmd c;
  c.angles = {0, yaw, 0};
  c.forward = sh::quantize_axis(forward);
  c.side = sh::quantize_axis(side);
  c.buttons = sh::kButtonJump;
  return c;
}

}  // namespace

TEST(a_key_over_the_lip_is_let_go_of_and_a_key_along_it_kept) {
  Bench b;
  pit(b);
  // Nodes in column order: the lip's two (x -16), the pit's floor (x 16, 48), one far west.
  const sh::NavGraph g =
      graph({{-16, -64, 24}, {-16, 64, 24}, {16, 0, -104}, {48, 0, -104}, {-600, 0, 24}}, {});
  const sh::Vec3f lip{-24, 0, 24};
  // Facing the pit (yaw 0, +x), forward full: let go of, and the jump too.
  sh::MoveCmd c = keys(0, 320, 0);
  const sh::BrinkGuard in = sh::brink_guard(b.sv, g, -1, lip, {}, c);
  CHECK(in.ways >= 1);
  CHECK(in.changed);
  CHECK(c.forward <= 0);
  CHECK((c.buttons & sh::kButtonJump) == 0);
  // Along the lip (yaw 90, +y): the forward key kept, a side key into the pit (right is +x) let go.
  c = keys(90, 320, 320);
  sh::brink_guard(b.sv, g, -1, lip, {}, c);
  CHECK(c.forward == sh::quantize_axis(320));
  CHECK(c.side <= 0);
  // Away from it (yaw 180): kept.
  c = keys(180, 320, 0);
  sh::brink_guard(b.sv, g, -1, lip, {}, c);
  CHECK(c.forward == sh::quantize_axis(320));
}

TEST(a_run_into_the_pit_is_braked_and_far_from_harm_nothing_changes) {
  Bench b;
  pit(b);
  const sh::NavGraph g = graph({{16, 0, -104}, {-600, 0, 24}}, {});
  // Running east at 320 from x -40, facing north and pressing nothing toward the pit: braked.
  sh::MoveCmd c = keys(90, 0, 0);
  const sh::BrinkGuard run = sh::brink_guard(b.sv, g, -1, {-40, 0, 24}, {320, 0, 0}, c);
  CHECK(run.run_in);
  CHECK(c.side < 0);  // the back of the run: west, the view's left
  // Far from the pit: no change at all, the jump kept.
  c = keys(0, 320, 0);
  const sh::BrinkGuard far = sh::brink_guard(b.sv, g, -1, {-600, 0, 24}, {320, 0, 0}, c);
  CHECK(!far.changed && far.ways == 0);
  CHECK(c.forward == sh::quantize_axis(320) && (c.buttons & sh::kButtonJump) != 0);
}

TEST(the_nav_marks_the_walks_along_a_lip_and_no_other) {
  Bench b;
  pit(b);
  // Column order: (-600,-64) 0, (-600,64) 1, (-16,-64) 2, (-16,64) 3, the pit's (16,0) 4.
  const sh::NavGraph g =
      graph({{-600, -64, 24}, {-600, 64, 24}, {-16, -64, 24}, {-16, 64, 24}, {16, 0, -104}},
            {{0, 1}, {2, 3}, {4, 2}});
  CHECK(g.harm.size() == g.nodes.size());
  CHECK(!g.links[0].brink);  // the far walk
  CHECK(g.links[1].brink);   // along the lip
  CHECK(!g.links[2].brink);  // out of the pit: a harm node's own link
  CHECK(sh::nav_harm_near(g, {-16, 0, 24}, 40));
  CHECK(!sh::nav_harm_near(g, {-600, 0, 24}, 40));
  CHECK(!sh::nav_harm_near(g, {16, 0, -104}, 40));  // the pit's own floor: not under it
}

TEST(a_walk_whose_side_step_falls_into_lava_far_below_is_marked_and_one_further_in_not) {
  // A ledge (y 0..400, its top at z 0) high over a pit (y < 0) whose lava lies 150 down: the
  // pit's floor nodes are far across from any walk on the ledge, so no walk is a `brink`.
  const auto brush = [](int contents, sh::Vec3f lo, sh::Vec3f hi) {
    sh::Brush b = box(lo, hi);
    b.contents = contents;
    return b;
  };
  const std::vector<sh::Brush> brushes = {
      brush(sh::kContentsSolid, {-512, 0, -256}, {512, 400, 0}),
      brush(sh::kContentsSolid, {-512, -512, -256}, {512, 0, -200}),
      brush(sh::kContentsLava, {-512, -512, -200}, {512, 0, -150})};
  const sh::HullData point = sh::build_point_hull(brushes);
  const sh::HullData clip = sh::build_box_hull(brushes, sh::kPlayerMins, sh::kPlayerMaxs);
  sh::MoveWorld world;
  world.solids[0].point_hull = point.hull();
  world.solids[0].clip_hull = clip.hull();
  world.solids[0].mins = {-512, -512, -256};
  world.solids[0].maxs = {512, 400, 0};
  world.count = 1;
  // Column order: the pit's floor (0,-160) 0, the edge walk's (-64,24) 1 and (64,24) 3, the
  // middle walk's (-64,120) 2 and (64,120) 4.
  sh::NavGraph g =
      graph({{0, -160, -176}, {-64, 24, 24}, {-64, 120, 24}, {64, 24, 24}, {64, 120, 24}},
            {{1, 3}, {2, 4}});
  CHECK(!g.links[0].brink && !g.links[1].brink);  // nothing harms within a lip's reach
  CHECK(sh::nav_mark_side_harm(g, world) == 1);
  CHECK(g.links[0].brink_side);   // 24 from the edge: a side step off it falls in
  CHECK(!g.links[1].brink_side);  // 120 in: a side step lands on the ledge
}

namespace {

// A stair down east: the top step (z 0) west of x 0, a lower step (z -64) from x 0 to `lower_to`,
// and past it a pit floored at -192 with lava to -128; with `wall`, a wall north of y 64.
void stair(Bench& b, float lower_to, bool wall = false) {
  sh::Brush lava = box({lower_to, -1024, -192}, {1024, 1024, -128});
  lava.contents = sh::kContentsLava;
  std::vector<sh::Brush> brushes = {box({-1024, -1024, -64}, {0, 1024, 0}),
                                    box({0, -1024, -128}, {lower_to, 1024, -64}),
                                    box({-1024, -1024, -256}, {1024, 1024, -192}), lava};
  if (wall) brushes.push_back(box({-1024, 64, -256}, {1024, 128, 128}));
  b.start(brushes);
}

}  // namespace

TEST(a_run_off_a_step_whose_flight_lands_in_the_lava_is_braked_in_time) {
  // A narrow lower step (96 across): off the top at 360 the box flies ~140 and lands in the lava.
  Bench b;
  stair(b, 96);
  const sh::NavGraph g = graph({{160, 0, -152}, {-600, 0, 24}}, {});
  const sh::Vec3f top{-8, 0, 24};
  // Without the flight looked along: too far from the pit to run, nothing changes.
  sh::MoveCmd c = keys(0, 320, 0);
  sh::BrinkGuard bg = sh::brink_guard(b.sv, g, -1, top, {360, 0, 0}, c);
  CHECK(!bg.changed);
  // With it: the flight lands in, the forward key turned back and the jump let go of.
  c = keys(0, 320, 0);
  bg = sh::brink_guard(b.sv, g, -1, top, {360, 0, 0}, c, true);
  CHECK(bg.ran && bg.run_in && bg.fall_in && bg.changed);
  CHECK(c.forward < 0);
  CHECK((c.buttons & sh::kButtonJump) == 0);
  // Already in the air over the lower step, falling: braked too.
  c = keys(0, 0, 0);
  bg = sh::brink_guard(b.sv, g, -1, {30, 0, 10}, {360, 0, -200}, c, true);
  CHECK(bg.fall_in && c.forward < 0);
  CHECK(sh::fall_into_harm(b.sv, top, {360, 0, 0}, 80, -1));
}

TEST(a_run_off_a_step_that_lands_on_a_wide_one_is_left_alone) {
  // A wide lower step (256 across): the flight lands at ~150 and its run on stops short of the lip.
  Bench b;
  stair(b, 256);
  const sh::NavGraph g = graph({{300, 0, -152}, {-600, 0, 24}}, {});
  sh::MoveCmd c = keys(0, 320, 0);
  const sh::BrinkGuard bg = sh::brink_guard(b.sv, g, -1, {-8, 0, 24}, {360, 0, 0}, c, true);
  CHECK(bg.ran && !bg.run_in && !bg.changed);
  CHECK(c.forward == sh::quantize_axis(320) && (c.buttons & sh::kButtonJump) != 0);
  CHECK(!sh::fall_into_harm(b.sv, {-8, 0, 24}, {360, 0, 0}, 80, -1));
}

TEST(a_run_into_a_wall_that_slides_it_off_the_lip_is_braked) {
  // On the wide lower step 36 from its lip, running north-east into a wall: the move slides it
  // east along the wall's face and off the lip; a box stopped dead at the wall would see no fall.
  Bench b;
  stair(b, 256, true);
  const sh::NavGraph g = graph({{300, 0, -152}, {-600, 0, 24}}, {});
  const sh::Vec3f at{220, 40, -40};
  const sh::Vec3f run{200, 300, 0};
  CHECK(sh::fall_into_harm(b.sv, at, run, 80, -1));
  sh::MoveCmd c = keys(90, 320, 0);
  const sh::BrinkGuard bg = sh::brink_guard(b.sv, g, -1, at, run, c, true);
  CHECK(bg.fall_in && bg.changed);
}
