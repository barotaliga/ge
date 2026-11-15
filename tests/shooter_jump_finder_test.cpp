// The jump finder (plugins/shooter/jump_finder.hpp): a ledge just inside a rocket jump's reach is
// found by its calculated arc; one past any reach is not; a ramp in front of a ledge too high for
// a plain jump lifts a running jump onto it; the reach and the arc as calculated; the finder is the
// same twice.
//
// ge:summary Tests: the jump finder's calculated arcs, reach, ramp lift and determinism
// ge:keywords test jump-finder calc_rocket_jump rocket_reach find_jumps ramp-lift
// ge:skill shooter map-design testing
#include <cmath>
#include <vector>

#include "check.hpp"
#include "shooter/brush_hull.hpp"
#include "shooter/hull.hpp"
#include "shooter/jump_finder.hpp"
#include "shooter/pmove.hpp"

namespace sh = ge::shooter;

namespace {

sh::Brush box(sh::Vec3f lo, sh::Vec3f hi) {
  sh::Brush b;
  b.planes = {{{1, 0, 0}, hi.x},   {{-1, 0, 0}, -lo.x}, {{0, 1, 0}, hi.y},
              {{0, -1, 0}, -lo.y}, {{0, 0, 1}, hi.z},   {{0, 0, -1}, -lo.z}};
  return b;
}

// A hall 1024 long with a ledge `high` up across its far end, and before it a ramp up to `step`
// (0: none).
struct Hall {
  sh::HullData point, clip;
  sh::MoveWorld world;
  sh::NavGraph g;
  float high = 0;
  Hall(float high_, float step) : high(high_) {
    std::vector<sh::Brush> b = {
        box({-544, -288, -64}, {544, 288, 0}),   box({-544, -288, 1024}, {544, 288, 1056}),
        box({-544, -288, 0}, {-512, 288, 1024}), box({512, -288, 0}, {544, 288, 1024}),
        box({-544, -288, 0}, {544, -256, 1024}), box({-544, 256, 0}, {544, 288, 1024}),
        box({256, -256, 0}, {512, 256, high})};
    // The ramp up to it: from the floor at x 0 to `step` at the ledge's foot (x 256).
    if (step > 0) {
      const float len = 256, k = 1 / std::sqrt(len * len + step * step);
      sh::Brush ramp = box({0, -256, 0}, {256, 256, step});
      ramp.planes[4] = {{-step * k, 0, len * k}, 0};  // the slope through (0, y, 0)
      b.push_back(ramp);
    }
    point = sh::build_point_hull(b);
    clip = sh::build_box_hull(b, sh::kPlayerMins, sh::kPlayerMaxs);
    world.solids[0].point_hull = point.hull();
    world.solids[0].clip_hull = clip.hull();
    world.solids[0].mins = {-544, -288, -64};
    world.solids[0].maxs = {544, 288, 1056};
    world.count = 1;
    sh::build_nav(world, {}, sh::MoveSettings{}, {}, g);
  }
  std::vector<sh::FoundJump> find(sh::JumpFinderCounts* c = nullptr) const {
    // The ledge holds a place (an item on it): no walk reaches it.
    sh::JumpFinderSettings set;
    set.places = {{384, 0, high + 24}};
    return sh::find_jumps(world, sh::MoveSettings{}, g, set, c);
  }
};

}  // namespace

TEST(a_ledge_just_under_a_rocket_jumps_reach_is_found_and_proven) {
  const Hall h(208, 0);
  sh::JumpFinderCounts c;
  const std::vector<sh::FoundJump> j = h.find(&c);
  CHECK(j.size() == 1);
  if (j.empty()) return;
  CHECK(!j[0].lifted);
  CHECK(j[0].to.z > 208 + 20 && j[0].to.z < 208 + 28);
  CHECK(j[0].from.z < 28);
  CHECK(j[0].health > 0);
  CHECK(c.ledges >= 1 && c.arcs >= 1 && c.plain == 1);
}

TEST(a_ledge_past_any_reach_gives_no_jump) {
  const Hall h(640, 0);
  CHECK(h.find().empty());
}

TEST(a_ramp_before_a_ledge_too_high_for_a_plain_jump_lifts_a_run_up_onto_it) {
  const Hall h(330, 64);
  const std::vector<sh::FoundJump> j = h.find();
  bool lifted = false;
  for (const sh::FoundJump& f : j) lifted = lifted || (f.lifted && f.by == sh::JumpLift::ramp);
  CHECK(lifted);
}

TEST(a_standing_rocket_jumps_reach_is_calculated_near_what_the_move_flies) {
  // The map-moves lab flies a 256-unit ledge and not the next 32 up.
  const float r = sh::rocket_reach(sh::MoveSettings{});
  CHECK(r > 240 && r < 288);
}

TEST(a_calculated_arc_lands_on_the_ledge_and_stops_at_a_wall) {
  const Hall h(208, 0);
  const sh::RocketArc up = sh::calc_rocket_jump(h.world, sh::MoveSettings{}, {0, 0, 24.03125f},
                                                {300, 0, 232}, 320, 0, false, 80);
  CHECK(up.fired && up.landed);
  CHECK(up.end.z > 208 + 20 && up.end.x > 256);
  // Straight at the far wall from close by, over the ledge's height: it slides down the wall.
  const Hall tall(640, 0);
  const sh::RocketArc wall = sh::calc_rocket_jump(
      tall.world, sh::MoveSettings{}, {200, 0, 24.03125f}, {300, 0, 664}, 0, 0, false, 89);
  CHECK(wall.landed && wall.end.z < 40);
}

TEST(the_finder_is_the_same_twice) {
  const Hall h(208, 0);
  const std::vector<sh::FoundJump> a = h.find(), b = h.find();
  CHECK(a.size() == b.size());
  for (size_t i = 0; i < a.size() && i < b.size(); ++i) {
    CHECK_BITS(a[i].to.x, b[i].to.x);
    CHECK_BITS(a[i].health, b[i].health);
  }
}
