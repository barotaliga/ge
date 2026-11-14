// THE GAP TRICKS IN THE NAV (plugins/shooter/nav_gaps.hpp): over a lava pit 320 across, a trick
// at a hopper's speed is proven into a gap link (a jump past the maxspeed, `gap` set) that only a
// jumper who takes gaps plans, while a narrow pit a run's jump crosses gives none (the graph's
// own jumps hold it); and the look before a takeoff (`fly_jump`) lands across from the lip at a
// hopper's speed and short of it at a run's. Over a pool of water, a hopper walking at the gap
// runs on to its lip on the ground for speed (pilot_gaps.hpp `gap_run`), jumps it and lands.
//
// ge:summary Tests: gap tricks proven into gap links over a lava pit; fly_jump's landings
// ge:keywords test gap-tricks nav_add_gap_tricks fly_jump speed-jump gap-link lava pit gap_run
// ge:keywords run-to-the-lip pilot-gap-jump
// ge:skill shooter testing
#include <cmath>
#include <string>
#include <vector>

#include "check.hpp"
#include "shooter/brush_hull.hpp"
#include "shooter/hull.hpp"
#include "shooter/nav_gaps.hpp"
#include "shooter/pilot.hpp"
#include "shooter/pmove.hpp"

namespace sh = ge::shooter;

namespace {

sh::Brush box(sh::Vec3f lo, sh::Vec3f hi) {
  sh::Brush b;
  b.planes = {{{1, 0, 0}, hi.x},   {{-1, 0, 0}, -lo.x}, {{0, 1, 0}, hi.y},
              {{0, -1, 0}, -lo.y}, {{0, 0, 1}, hi.z},   {{0, 0, -1}, -lo.z}};
  return b;
}

// Floors at z 0 west of x 0 and east of x `width`, walled round; between, a pit floored at -192
// with lava (or `liquid`) to -128; its graph.
struct Pit {
  sh::HullData point, clip;
  sh::MoveWorld world;
  sh::NavGraph g;
  explicit Pit(float width, int liquid = sh::kContentsLava, sh::NavSettings set = {}) {
    sh::Brush lava = box({0, -256, -192}, {width, 256, -128});
    lava.contents = liquid;
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
    sh::build_nav(world, {}, sh::MoveSettings{}, set, g);
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
  // The graph's own jumps over it go aslant (from x -16 to 144, 128 to the side): the trick along
  // one of them is held.
  Pit pit(128);
  const sh::NavGapTrick trick{{-16, -16, 24}, {144, 112, 24}, 450, 0};
  const sh::NavGapCounts c =
      sh::nav_add_gap_tricks(pit.world, sh::MoveSettings{}, pit.g, {&trick, 1}, {});
  CHECK_MSG(c.held == 0, "held " + std::to_string(c.held));
  CHECK_MSG(c.run_makes == 1,
            "proofs " + std::to_string(c.proofs) + " run " + std::to_string(c.run_makes));
  CHECK(pit.gap_links() == 0);
}

TEST(a_pit_a_runs_jump_crosses_but_the_graph_never_tried_is_a_gap_link) {
  // The graph's own jumps reach only 96 across here: the run's jump over the pit is no link of
  // its, so the trick is not left to it (e1m2's YA walkway to the east walkway, 290 across).
  sh::NavSettings near;
  near.reach = 96;
  Pit pit(128, sh::kContentsLava, near);
  const sh::NavGapTrick trick{{-8, 0, 24}, {176, 0, 24}, 450, 0};
  const sh::NavGapCounts c =
      sh::nav_add_gap_tricks(pit.world, sh::MoveSettings{}, pit.g, {&trick, 1}, {});
  CHECK_MSG(c.held == 1, "held " + std::to_string(c.held) + " run " + std::to_string(c.run_makes));
  CHECK(c.run_makes == 0);
  CHECK(pit.gap_links() == 1);
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

TEST(a_hopper_walking_at_a_gap_over_water_runs_on_to_its_lip_and_jumps_it) {
  Pit pit(256, sh::kContentsWater);
  const sh::NavGapTrick trick{{-8, 0, 24}, {300, 0, 24}, 420, 0};
  const sh::NavGapCounts c =
      sh::nav_add_gap_tricks(pit.world, sh::MoveSettings{}, pit.g, {&trick, 1}, {});
  CHECK_MSG(c.held == 1, "held " + std::to_string(c.held) + " proofs " + std::to_string(c.proofs));
  sh::PilotSettings ps;
  ps.hop = true;
  ps.hop_circle = true;
  ps.gap_circle = true;
  ps.jumper.gap_build_s = 0;
  constexpr int kMsec = 13;
  sh::Pilot p;
  sh::pilot_init(p, pit.g, ps, sh::pilot_hands(sh::PilotHands::practised), 1000.0 / kMsec, 1);
  const sh::MoveSettings move;
  sh::MoveState st;
  // Off to the side of the jump's line, as people come to a gap: the run curves onto it.
  st.origin = pit.g.nodes[static_cast<size_t>(sh::nav_nearest(pit.g, {-150, -150, 24}))].origin;
  sh::MoveCmd still;
  still.msec = kMsec;
  sh::run_command(st, still, move, pit.world);
  st.velocity = {127, 127, 0};  // a walk at the lip at 180
  sh::pilot_face(p, 45);
  sh::PilotSense s;
  const auto sense = [&] {
    s.origin = st.origin;
    s.velocity = st.velocity;
    s.onground = st.ground >= 0;
    s.maxspeed = st.maxspeed;
    s.world = &pit.world;
  };
  sense();
  CHECK(sh::pilot_go(p, sh::nav_nearest(pit.g, {420, 0, 24}), s));
  float top = 0;
  for (int i = 0; i < 6000 / kMsec && p.mode != sh::PilotMode::arrived; i++) {
    sh::run_command(st, sh::pilot_step(p, s, kMsec), move, pit.world);
    sense();
    const float v = std::sqrt(st.velocity.x * st.velocity.x + st.velocity.y * st.velocity.y);
    if (st.origin.x < 0 && v > top) top = v;  // the speed built on this side of the lip
  }
  CHECK_MSG(p.stats.gap_jumps == 1 && p.stats.gap_landed == 1,
            "jumps " + std::to_string(p.stats.gap_jumps) + " landed " +
                std::to_string(p.stats.gap_landed) + " refused " +
                std::to_string(p.stats.gap_refused) + " top " + std::to_string(top));
  CHECK_MSG(top > 380, "the run to the lip built " + std::to_string(top) + " u/s");
  CHECK(p.mode == sh::PilotMode::arrived && st.origin.x > 256 && st.origin.z > 0);
}
