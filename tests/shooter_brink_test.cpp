// THE BRINK: beside a pit of lava, a forward key over the lip is let go of, a run into it is
// braked and its jump let go of, a key along the lip or away from it is kept, and far from any
// harm nothing changes; the nav marks the walks along a lip and no other.
//
// ge:summary Tests: the brink guard's keys by a lava pit, and the nav's brink links
// ge:keywords test brink brink_guard over_harm nav_mark_harm nav_harm_near lava-lip lava pit
// ge:skill shooter testing
#include <algorithm>

#include "check.hpp"
#include "shooter/brink.hpp"
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
