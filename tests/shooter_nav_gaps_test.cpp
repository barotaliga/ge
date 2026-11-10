// THE GAP TRICKS IN THE NAV (plugins/shooter/nav_gaps.hpp): over a lava pit 320 across, a trick
// at a hopper's speed is proven into a gap link (a jump past the maxspeed, `gap` set) that only a
// jumper who takes gaps plans, while a narrow pit a run's jump crosses gives none (the graph's
// own jumps hold it); and the look before a takeoff (`fly_jump`) lands across from the lip at a
// hopper's speed and short of it at a run's.
//
// ge:summary Tests: gap tricks proven into gap links over a lava pit; fly_jump's landings
// ge:keywords test gap-tricks nav_add_gap_tricks fly_jump speed-jump gap-link lava pit
// ge:skill shooter testing
#include <string>
#include <vector>

#include "check.hpp"
#include "shooter/brush_hull.hpp"
#include "shooter/hull.hpp"
#include "shooter/nav_gaps.hpp"

namespace sh = ge::shooter;

namespace {

sh::Brush box(sh::Vec3f lo, sh::Vec3f hi) {
  sh::Brush b;
  b.planes = {{{1, 0, 0}, hi.x},   {{-1, 0, 0}, -lo.x}, {{0, 1, 0}, hi.y},
              {{0, -1, 0}, -lo.y}, {{0, 0, 1}, hi.z},   {{0, 0, -1}, -lo.z}};
  return b;
}

// Floors at z 0 west of x 0 and east of x `width`, walled round; between, a pit floored at -192
// with lava to -128; its graph.
struct Pit {
  sh::HullData point, clip;
  sh::MoveWorld world;
  sh::NavGraph g;
  explicit Pit(float width) {
    sh::Brush lava = box({0, -256, -192}, {width, 256, -128});
    lava.contents = sh::kContentsLava;
    const float e = width + 512;
    const std::vector<sh::Brush> brushes = {
        box({-512, -256, -64}, {0, 256, 0}),          box({width, -256, -64}, {e, 256, 0}),
        box({-512, -256, -256}, {e, 256, -192}),      lava,
        box({-544, -288, -256}, {-512, 288, 256}),    box({e, -288, -256}, {e + 32, 288, 256}),
        box({-544, -288, -256}, {e + 32, -256, 256}), box({-544, 256, -256}, {e + 32, 288, 256})};
    point = sh::build_point_hull(brushes);
    clip = sh::build_box_hull(brushes, sh::kPlayerMins, sh::kPlayerMaxs);
    world.solids[0].point_hull = point.hull();
    world.solids[0].clip_hull = clip.hull();
    world.solids[0].mins = {-544, -288, -256};
    world.solids[0].maxs = {e + 32, 288, 256};
    world.count = 1;
    sh::build_nav(world, {}, sh::MoveSettings{}, sh::NavSettings{}, g);
  }
  int gap_links() const {
    int n = 0;
    for (const sh::NavLink& l : g.links) n += l.gap;
    return n;
  }
};

}  // namespace

TEST(a_hoppers_trick_over_a_wide_pit_is_a_gap_link_only_a_gap_taker_plans) {
  Pit pit(320);
  const int before = pit.gap_links();
  const sh::NavGapTrick trick{{-8, 0, 24}, {360, 0, 24}, 480, 0};
  const sh::NavGapCounts c =
      sh::nav_add_gap_tricks(pit.world, sh::MoveSettings{}, pit.g, {&trick, 1}, {});
  CHECK_MSG(c.held == 1, "held " + std::to_string(c.held) + " proofs " + std::to_string(c.proofs));
  CHECK(pit.gap_links() == before + 1);
  CHECK(c.from.size() == 1 && c.from[0] >= 0);
  if (c.from.empty() || c.from[0] < 0) return;
  const sh::NavNode& n = pit.g.nodes[static_cast<size_t>(c.from[0])];
  const sh::NavLink* gap = nullptr;
  for (int32_t k = 0; k < n.links; k++) {
    const size_t at = static_cast<size_t>(n.first_link) + static_cast<size_t>(k);
    if (pit.g.links[at].gap) gap = &pit.g.links[at];
  }
  CHECK(gap != nullptr);
  if (gap == nullptr) return;
  CHECK(gap->kind == sh::NavKind::jump && gap->speed > 320);
  CHECK(pit.g.nodes[static_cast<size_t>(gap->to)].origin.x > 320);
  sh::NavJumper walker, hopper;
  hopper.gaps = true;
  CHECK(!walker.takes(*gap));
  CHECK(hopper.takes(*gap));
  hopper.level = 0;
  sh::NavLink expert = *gap;
  expert.level = 2;
  CHECK(!hopper.takes(expert));  // a trick above his level
}

TEST(a_narrow_pit_a_runs_jump_crosses_gives_no_gap_link) {
  Pit pit(128);
  const sh::NavGapTrick trick{{-8, 0, 24}, {176, 0, 24}, 450, 0};
  const sh::NavGapCounts c =
      sh::nav_add_gap_tricks(pit.world, sh::MoveSettings{}, pit.g, {&trick, 1}, {});
  CHECK_MSG(c.held == 0, "held " + std::to_string(c.held));
  CHECK_MSG(c.run_makes == 1,
            "proofs " + std::to_string(c.proofs) + " run " + std::to_string(c.run_makes));
  CHECK(pit.gap_links() == 0);
}

TEST(the_look_before_a_takeoff_lands_across_at_a_hoppers_speed_and_short_at_a_runs) {
  const Pit pit(320);
  const sh::JumpFlight fast =
      sh::fly_jump(pit.world, sh::MoveSettings{}, {-8, 0, 24}, {480, 0, 0}, {400, 0, 24}, 13);
  CHECK(fast.landed && fast.end.x > 300 && fast.end.z > 23);  // the box's edge on the far floor
  const sh::JumpFlight slow =
      sh::fly_jump(pit.world, sh::MoveSettings{}, {-8, 0, 24}, {320, 0, 0}, {400, 0, 24}, 13);
  CHECK(!(slow.landed && slow.end.z > 23));  // down in the pit
}
