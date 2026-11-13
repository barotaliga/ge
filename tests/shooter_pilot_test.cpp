// THE PILOT: on the nav course (games/shooters_edge/sim/nav_course.hpp) it plays routes through the
// player move, its commands on the protocol's grid: across the pit by a jump, onto the crate by a
// jump at the link's speed, up the stairs, each in about the graph's proven time; a run replays to
// the bit; human hands arrive too; the strafe jump runs a long straight faster than a walk. On the
// server's arena (games/shooters_edge/server/host.hpp) a pilot filling a bot slot waits for the
// door and goes through it, rides the plat up to the ledge, and bars the door from the east
// side, where nothing opens it. The pilot's bits on the course are held by
// tests/golden/shooter_pilot.txt (GE_GOLDEN_WRITE=1 rewrites it; the PR says why).
//
// ge:summary Tests: the pilot's routes on the nav course and the arena, hands, strafe, bits
// ge:keywords test pilot route jump run-up ride plat door wait barred strafe-jump hands golden
// ge:keywords rocket-jump flip standing-flip look-ahead foresee other-players
// ge:skill shooter bots testing
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <string>

#include "check.hpp"
#include "golden.hpp"
#include "shooter/brush_hull.hpp"
#include "shooter/look_ahead.hpp"
#include "shooter/nav.hpp"
#include "shooter/pilot.hpp"
#include "shooters_edge/bots/pilot_run.hpp"

namespace sh = ge::shooter;
namespace pr = shooters_edge::bots;

namespace {

constexpr const char* kGolden = "tests/golden/shooter_pilot.txt";

bool env_set(const char* name) {
  const char* v = std::getenv(name);
  return v != nullptr && v[0] != '\0' && v[0] != '0';
}

const pr::Course& course() {
  static const std::unique_ptr<pr::Course> c = pr::make_course();
  return *c;
}

}  // namespace

TEST(the_pilot_plays_the_course_routes_in_about_their_proven_time) {
  const pr::Course& c = course();
  for (const pr::CourseRoute& r : pr::course_routes(c)) {
    const pr::RunResult run = pr::run_course(c, r.from, r.to, sh::PilotHands::none, 1);
    CHECK_MSG(run.arrived, std::string("the pilot did not arrive on ") + r.name +
                               ": read pilot_step's mode for the link it ended on");
    CHECK_MSG(run.time_s <= run.proven_s * 1.6 + 0.5,
              std::string(r.name) + " took " + std::to_string(run.time_s) + " s against " +
                  std::to_string(run.proven_s) + " s proven");
  }
}

TEST(the_pit_is_crossed_by_a_jump_and_the_crate_climbed_by_one) {
  const pr::Course& c = course();
  const pr::RunResult east =
      pr::run_course(c, c.map.platform, c.map.east_ledge, sh::PilotHands::none, 1);
  CHECK(east.arrived);
  CHECK_MSG(east.stats.jumps >= 1, "the way east crosses the pit with no jump");
  const pr::RunResult crate =
      pr::run_course(c, c.map.pit_floor, c.map.crate_top, sh::PilotHands::none, 1);
  CHECK(crate.arrived);
  CHECK_MSG(crate.stats.jumps >= 1, "the crate is climbed with no jump");
}

TEST(human_hands_arrive_too) {
  const pr::Course& c = course();
  int arrived = 0, runs = 0;
  std::string missed;
  for (const pr::CourseRoute& r : pr::course_routes(c)) {
    for (uint32_t seed = 1; seed <= 3; seed++) {
      runs++;
      if (pr::run_course(c, r.from, r.to, sh::PilotHands::human, seed).arrived)
        arrived++;
      else
        missed += std::string(" ") + r.name + " seed " + std::to_string(seed) + ";";
    }
  }
  CHECK_MSG(arrived == runs, "human hands arrived on " + std::to_string(arrived) + " of " +
                                 std::to_string(runs) + " runs; missed:" + missed +
                                 " (the se-pilot lab draws each run)");
}

TEST(a_run_replays_to_the_bit_and_matches_its_golden) {
  const pr::Course& c = course();
  uint64_t h = 1469598103934665603ull;
  for (const pr::CourseRoute& r : pr::course_routes(c)) {
    const pr::RunResult a = pr::run_course(c, r.from, r.to, sh::PilotHands::human, 7);
    const pr::RunResult b = pr::run_course(c, r.from, r.to, sh::PilotHands::human, 7);
    CHECK_MSG(a.hash == b.hash, std::string("two runs of ") + r.name + " differ");
    h = (h ^ a.hash) * 1099511628211ull;
  }
  if (env_set("GE_GOLDEN_WRITE")) {
    std::ofstream out(ge::test::root_path(kGolden));
    out << "# plugins/shooter/pilot.hpp on the nav course's routes, human hands seed 7: every "
           "command's origin and velocity folded (tests/shooter_pilot_test.cpp)\n";
    char line[32];
    std::snprintf(line, sizeof line, "%016llx\n", static_cast<unsigned long long>(h));
    out << line;
    std::printf("wrote %s\n", kGolden);
    return;
  }
  const auto rows = ge::test::golden_rows(kGolden);
  CHECK_MSG(rows.size() == 1 && rows[0].size() == 1,
            std::string(kGolden) + " is missing or malformed: write it with GE_GOLDEN_WRITE=1");
  if (rows.size() == 1 && rows[0].size() == 1)
    CHECK_MSG(rows[0][0] == h,
              "the pilot's runs moved off their golden bits: if the pilot, the graph or the move "
              "changed on purpose, rewrite with GE_GOLDEN_WRITE=1 and say why in the PR");
}

TEST(the_strafe_jump_runs_a_long_straight_faster_than_a_walk) {
  const pr::Course& c = course();
  const pr::CourseRoute r = pr::course_long_straight(c);
  sh::PilotSettings walk = pr::course_strafe_settings();
  walk.strafe = false;
  const pr::RunResult a = pr::run_course(c, r.from, r.to, sh::PilotHands::none, 1, walk);
  const pr::RunResult b =
      pr::run_course(c, r.from, r.to, sh::PilotHands::none, 1, pr::course_strafe_settings());
  CHECK(a.arrived && b.arrived);
  CHECK_MSG(b.stats.strafes >= 1, "the long straight was not strafed");
  CHECK_MSG(b.top_speed > a.top_speed + 20, "strafing reached " + std::to_string(b.top_speed) +
                                                " u/s against walking's " +
                                                std::to_string(a.top_speed));
}

TEST(on_the_server_the_pilot_opens_the_door_rides_the_plat_and_bars_a_shut_door) {
  pr::Arena run;
  CHECK(pr::make_arena(run));
  const pr::ArenaResult res = pr::run_arena(run, 2400);
  CHECK_MSG(res.through_door, "the pilot never went east through the doorway");
  CHECK_MSG(res.on_ledge, "the pilot never stood on the ledge the plat rides up to");
  CHECK_MSG(res.stats.rides >= 1, "the pilot never rode the plat");
  CHECK_MSG(res.stats.waits >= 1, "the pilot never waited for a mover");
  CHECK_MSG(res.stats.barred >= 1, "the door shut from the east was never barred");
  CHECK_MSG(res.arrivals >= 3, "the pilot reached " + std::to_string(res.arrivals) + " goals");
}

TEST(human_hands_ride_to_the_ledge_too) {
  for (uint32_t seed = 1; seed <= 3; seed++) {
    pr::Arena run;
    CHECK(pr::make_arena(run, sh::PilotHands::human, seed));
    const pr::ArenaResult res = pr::run_arena(run, 2400);
    CHECK_MSG(res.through_door && res.on_ledge,
              "human hands (seed " + std::to_string(seed) +
                  ") missed the door or the ledge: draw it with make lab LAB=se-pilot");
    CHECK_MSG(res.arrivals >= 3, "human hands (seed " + std::to_string(seed) + ") reached " +
                                     std::to_string(res.arrivals) + " goals");
  }
}

namespace {

// A room of box brushes, walled round as a map's room is (a late hand's circle does not walk off
// into the void), its graph built and its pockets bridged with rocket jumps from `from`'s part.
struct FlipRoom {
  sh::HullData point, clip;
  sh::MoveWorld world;
  sh::NavGraph g;
  sh::NavBridgeCounts bridged;
  std::vector<sh::NavLink> rockets;  // the rocket links the bridge made
  FlipRoom(std::vector<sh::Brush> brushes, sh::Vec3f lo, sh::Vec3f hi, sh::Vec3f from) {
    brushes.push_back(box({lo.x - 32, lo.y - 32, lo.z}, {lo.x, hi.y + 32, hi.z}));
    brushes.push_back(box({hi.x, lo.y - 32, lo.z}, {hi.x + 32, hi.y + 32, hi.z}));
    brushes.push_back(box({lo.x - 32, lo.y - 32, lo.z}, {hi.x + 32, lo.y, hi.z}));
    brushes.push_back(box({lo.x - 32, hi.y, lo.z}, {hi.x + 32, hi.y + 32, hi.z}));
    point = sh::build_point_hull(brushes);
    clip = sh::build_box_hull(brushes, sh::kPlayerMins, sh::kPlayerMaxs);
    world.solids[0].point_hull = point.hull();
    world.solids[0].clip_hull = clip.hull();
    world.solids[0].mins = {lo.x - 32, lo.y - 32, lo.z};
    world.solids[0].maxs = {hi.x + 32, hi.y + 32, hi.z};
    world.count = 1;
    sh::build_nav(world, {}, sh::MoveSettings{}, sh::NavSettings{}, g);
    ge::ai::Search search;
    sh::nav_reach(g, sh::nav_nearest(g, from), search);
    std::vector<uint8_t> main(g.nodes.size());
    for (size_t n = 0; n < main.size(); n++) main[n] = search.reached(static_cast<int32_t>(n));
    bridged = sh::nav_bridge_pockets(world, sh::MoveSettings{}, g, main);
    for (const sh::NavLink& l : g.links)
      if (l.kind == sh::NavKind::rocket) rockets.push_back(l);
  }
  static sh::Brush box(sh::Vec3f lo, sh::Vec3f hi) {
    sh::Brush b;
    b.planes = {{{1, 0, 0}, hi.x},   {{-1, 0, 0}, -lo.x}, {{0, 1, 0}, hi.y},
                {{0, -1, 0}, -lo.y}, {{0, 0, 1}, hi.z},   {{0, 0, -1}, -lo.z}};
    return b;
  }
};

// The pilot from `from` to `to` with rocket jumps, with no hands and a human's: arrives, by a
// flip, its rocket bursting by the jumper.
void flies_the_flip(const FlipRoom& room, sh::Vec3f from, sh::Vec3f to, const std::string& what) {
  sh::PilotSettings set;
  set.jumper.rockets = true;
  for (const sh::PilotHands hands : {sh::PilotHands::none, sh::PilotHands::human}) {
    const pr::RunResult r = pr::run_route(room.world, room.g, from, to, hands, 1, set);
    const std::string who = what + (hands == sh::PilotHands::none ? ", no hands" : ", human hands");
    CHECK_MSG(r.arrived, who + ": the pilot did not arrive (" + std::to_string(r.stats.flips) +
                             " flips, " + std::to_string(r.shots) + " shots, " +
                             std::to_string(r.stats.run_ups) + " run-ups)");
    CHECK_MSG(r.stats.flips >= 1, who + ": climbed with no flip");
    CHECK_MSG(r.shots >= 1 && r.blast > 0, who + ": no rocket burst near the jumper");
  }
}

}  // namespace

TEST(the_pilot_lands_a_flip_onto_the_block_no_jump_climbs) {
  // A floor and a block 160 high on it: only a rocket jump climbs it, and the bridge makes it a
  // running flip; the pilot turns into it strafing, fires behind with the jump and lands on top.
  const FlipRoom room({FlipRoom::box({-384, -128, -64}, {384, 128, 0}),
                       FlipRoom::box({96, -128, 0}, {384, 128, 160})},
                      {-384, -128, -64}, {384, 128, 512}, {-256, 0, 24});
  CHECK_MSG(room.bridged.by_flip >= 1, "the block's top was not bridged by a flip");
  CHECK_MSG(!room.rockets.empty() && room.rockets[0].speed > 0,
            "the flip onto the block is not run at: the floor before it has room");
  flies_the_flip(room, {-256, 0, 24}, {320, 0, 184}, "the running flip");
}

TEST(a_standing_flip_climbs_a_block_with_no_room_to_run_at_it) {
  // The floor before a block 128 high is 64 deep: room for no run and turn into a flip, so the
  // bridge makes it a standing one; the pilot turns round on the spot, fires behind and lands.
  const FlipRoom room(
      {FlipRoom::box({0, -64, -64}, {384, 64, 0}), FlipRoom::box({64, -64, 0}, {384, 64, 128})},
      {0, -64, -64}, {384, 64, 512}, {32, 0, 24});
  CHECK_MSG(room.bridged.by_flip >= 1, "the block's top was not bridged by a flip");
  CHECK_MSG(!room.rockets.empty() && room.rockets[0].back && room.rockets[0].speed == 0,
            "the flip onto the block is not a standing one");
  flies_the_flip(room, {32, 0, 24}, {320, 0, 152}, "the standing flip");
}

TEST(a_grenade_jump_climbs_a_block_for_a_player_with_no_rocket_launcher) {
  // A block 112 high: no jump climbs it. Proven as a grenade jump (the grenade at the feet, a
  // walk to past where it lies, the jump as its fuse runs out), the pilot with the grenade
  // launcher alone plays it: fires, waits out the fuse and lands on top.
  FlipRoom room({FlipRoom::box({-384, -128, -64}, {384, 128, 0}),
                 FlipRoom::box({96, -128, 0}, {384, 128, 112})},
                {-384, -128, -64}, {384, 128, 512}, {-256, 0, 24});
  sh::NavTrick up;
  up.from = {32, 0, 24};
  up.to = {160, 0, 136};
  sh::NavTrickSettings ts;
  ts.rockets = false;
  ts.grenades = true;
  const sh::NavTrickCounts made = sh::nav_add_tricks(room.world, sh::MoveSettings{}, room.g,
                                                     std::span<const sh::NavTrick>(&up, 1), {}, ts);
  CHECK_MSG(made.grenades == 1, "no grenade jump onto the block was proven (" +
                                    std::to_string(made.proofs) + " proofs)");
  sh::PilotSettings set;
  set.jumper.grenades = true;
  for (const sh::PilotHands hands : {sh::PilotHands::none, sh::PilotHands::human}) {
    const pr::RunResult r =
        pr::run_route(room.world, room.g, {-256, 0, 24}, {288, 0, 136}, hands, 1, set);
    const std::string who = hands == sh::PilotHands::none ? "no hands" : "human hands";
    CHECK_MSG(r.arrived, who + ": the pilot did not arrive on the block (" +
                             std::to_string(r.stats.grenade_jumps) + " grenades, " +
                             std::to_string(r.shots) + " shots)");
    CHECK_MSG(r.stats.grenade_jumps >= 1 && r.blast > 0, who + ": no grenade burst by the jumper");
  }
}

TEST(the_walk_foreseen_flies_round_a_player_standing_on_its_line) {
  // An open floor: running east at 320 u/s toward a point 600 on, nothing in the way. A player
  // standing 100 units ahead on the line: the pursuit's flight runs into his box, and a way that
  // bumps less (or not at all) is pressed instead; without him in the flight nothing is flown.
  FlipRoom room({FlipRoom::box({-512, -512, -64}, {1024, 512, 0})}, {-512, -512, -64},
                {1024, 512, 256}, {0, 0, 24});
  sh::WalkStart st;
  st.origin = {0, 0, 24};
  st.velocity = {320, 0, 0};
  st.pressed = {0, 1, 0};
  st.delay = 0.1f;
  st.horizon = 0.35f;
  const sh::Vec3f aim{600, 0, 24}, near{300, 160, 24};
  sh::WalkKeys keys;
  sh::WalkSeen seen[sh::kWalkWays];
  bool flown = true;
  sh::WalkWay way = sh::foresee_walk(room.world, st, aim, near, keys, seen, flown);
  CHECK_MSG(!flown && way == sh::WalkWay::pursue,
            "an open floor was flown: the walk foresaw a bump with nobody in the way");
  const sh::OtherBox him{{100, 0, 24}, {0, 0, 0}};
  st.others = std::span<const sh::OtherBox>(&him, 1);
  way = sh::foresee_walk(room.world, st, aim, near, keys, seen, flown);
  CHECK_MSG(flown && seen[0].bump > 0,
            "the pursuit's flight did not meet the player standing on its line (bump " +
                std::to_string(seen[0].bump) + ")");
  CHECK_MSG(way != sh::WalkWay::pursue && seen[static_cast<int>(way)].bump < seen[0].bump,
            std::string("the walk pressed ") + sh::walk_way_name(way) +
                " into the player: a way that bumps less was not chosen");
}

TEST(a_player_standing_on_the_way_is_walked_round_or_waited_for) {
  // A player standing 100 units along the walk's line on an open floor: the walk aims beside him
  // and just past him. In a corridor no wider than two boxes there is no room: it waits.
  FlipRoom open({FlipRoom::box({-512, -512, -64}, {1024, 512, 0})}, {-512, -512, -64},
                {1024, 512, 256}, {0, 0, 24});
  const sh::OtherBox him{{100, 4, 24}, {0, 0, 0}};
  const std::span<const sh::OtherBox> others(&him, 1);
  sh::Vec3f round;
  CHECK_MSG(sh::body_on_way(open.world, {0, 0, 24}, {400, 0, 24}, others, round),
            "a player standing on the line was not seen in the way");
  CHECK_MSG(round.x > 100 && round.y < -40,
            "the way round him is not beside and past him on the open side (" +
                std::to_string(round.x) + ", " + std::to_string(round.y) + ")");
  CHECK_MSG(!sh::body_on_way(open.world, {0, 0, 24}, {400, 0, 24}, std::span<const sh::OtherBox>(),
                             round),
            "nobody stands there, yet the way was put round someone");
  const sh::OtherBox runner{{100, 4, 24}, {300, 0, 0}};
  CHECK_MSG(!sh::body_on_way(open.world, {0, 0, 24}, {400, 0, 24},
                             std::span<const sh::OtherBox>(&runner, 1), round),
            "a player running on was taken for one standing in the way");
  FlipRoom corridor({FlipRoom::box({-512, -48, -64}, {1024, 48, 0})}, {-512, -48, -64},
                    {1024, 48, 256}, {0, 0, 24});
  CHECK_MSG(sh::body_on_way(corridor.world, {0, 0, 24}, {400, 0, 24}, others, round) &&
                round.x == 0 && round.y == 0,
            "no room round him in a corridor, yet the walk did not wait");
}

TEST(a_goal_chained_past_a_pickup_is_run_through_not_stopped_at) {
  // An open floor: from the west to an item 384 east, and on past it 384 north (`pilot_chain`).
  // The route runs on through the item's node, the walk runs through the item itself (beside its
  // node) at a run, and arrives at the chain's goal; cut back (`pilot_unchain`), the goal is the
  // item again.
  FlipRoom room({FlipRoom::box({-512, -512, -64}, {1024, 1024, 0})}, {-512, -512, -64},
                {1024, 1024, 256}, {0, 0, 24});
  const sh::NavGraph& g = room.g;
  const int32_t start = sh::nav_nearest(g, {0, 0, 24}), item = sh::nav_nearest(g, {384, 0, 24});
  const int32_t then = sh::nav_nearest(g, {384, 384, 24});
  const sh::Vec3f item_at = g.nodes[static_cast<size_t>(item)].origin + sh::Vec3f{8, -12, -24};
  sh::Pilot p;
  sh::pilot_init(p, g, sh::PilotSettings{}, sh::pilot_hands(sh::PilotHands::none), 77, 1);
  sh::MoveState st;
  st.origin = g.nodes[static_cast<size_t>(start)].origin;
  const sh::MoveSettings move;
  st.maxspeed = move.maxspeed;
  sh::MoveCmd still;
  still.msec = pr::kPilotMsec;
  sh::run_command(st, still, move, room.world);
  sh::PilotSense s;
  auto sense = [&] {
    s.origin = st.origin;
    s.velocity = st.velocity;
    s.onground = st.ground >= 0;
    s.maxspeed = st.maxspeed;
    s.world = &room.world;
  };
  sense();
  CHECK(sh::pilot_go(p, item, s));
  const size_t to_item = p.route.size();
  CHECK_MSG(sh::pilot_chain(p, then, item_at) && p.goal == then && p.route.back() == then &&
                p.via_k == static_cast<int32_t>(to_item) - 1,
            "the route was not chained on past the item to the next goal");
  sh::pilot_unchain(p);
  CHECK_MSG(p.goal == item && p.route.size() == to_item && p.via_k < 0,
            "cut back, the route does not end at the item");
  CHECK(sh::pilot_chain(p, then, item_at));
  bool arrived = false;
  for (int i = 0; i < 77 * 6 && !arrived; i++) {
    const sh::MoveCmd cmd = sh::pilot_step(p, s, pr::kPilotMsec);
    sh::run_command(st, cmd, move, room.world);
    sense();
    arrived = p.mode == sh::PilotMode::arrived;
  }
  CHECK_MSG(arrived, "the chain's goal was not reached: read pilot_step's mode on the way");
  CHECK_MSG(p.stats.via_touched == 1 && p.stats.via_missed == 0,
            "the item was not run through (" + std::to_string(p.stats.via_touched) + " through, " +
                std::to_string(p.stats.via_missed) + " missed)");
  CHECK_MSG(p.stats.via_speed > 200, "the item was run through at " +
                                         std::to_string(p.stats.via_speed) +
                                         " u/s: the walk slowed into it as into a goal");
}

// A route's every node passed in one command (a fast run over a dense stretch, a flight landing
// at the goal): the pass loop's guard ends on the route's last node, which is the goal, not a
// node to read past (the next node read past the route's end: a crash in a 4on4 on dm2).
TEST(a_route_passed_to_its_last_node_in_one_command_arrives) {
  FlipRoom room({FlipRoom::box({-512, -512, -64}, {1024, 1024, 0})}, {-512, -512, -64},
                {1024, 1024, 256}, {0, 0, 24});
  const sh::NavGraph& g = room.g;
  const int32_t start = sh::nav_nearest(g, {0, 0, 24});
  sh::Pilot p;
  sh::pilot_init(p, g, sh::PilotSettings{}, sh::pilot_hands(sh::PilotHands::none), 77, 1);
  sh::PilotSense s;
  s.origin = g.nodes[static_cast<size_t>(start)].origin;
  s.onground = true;
  s.world = &room.world;
  // A goal east whose route is nine nodes: eight links, the guard's eight passes.
  int32_t goal = -1;
  for (float x = 192; x <= 384 && goal < 0; x += 8) {
    const int32_t n = sh::nav_nearest(g, {x, 0, 24});
    CHECK(sh::pilot_go(p, n, s));
    if (p.route.size() == 9) goal = n;
  }
  CHECK_MSG(goal >= 0, "no goal east has a route of nine nodes: the floor's grid changed");
  if (goal < 0) return;
  s.origin = g.nodes[static_cast<size_t>(goal)].origin;  // already there: every node passed
  sh::pilot_step(p, s, pr::kPilotMsec);
  CHECK_MSG(p.mode == sh::PilotMode::arrived,
            "a route passed to its goal in one command did not arrive (pilot_step's pass loop)");
}

// `jumpfly`: on human hands the run at the pit's jump reaches its start off the line or the speed,
// and a jump that lands from there is taken as it is (flown by the move first): no more run-ups
// than lining up again each time, and still across.
TEST(a_jump_that_lands_from_off_its_line_is_taken_not_lined_up_again) {
  const pr::Course& c = course();
  int32_t ups_off = 0, ups_on = 0, flown = 0;
  for (uint32_t seed = 1; seed <= 6; seed++) {
    sh::PilotSettings set;
    const pr::RunResult off =
        pr::run_course(c, c.map.platform, c.map.east_ledge, sh::PilotHands::human, seed, set);
    set.jump_fly = true;
    const pr::RunResult on =
        pr::run_course(c, c.map.platform, c.map.east_ledge, sh::PilotHands::human, seed, set);
    CHECK_MSG(on.arrived, "seed " + std::to_string(seed) +
                              ": with `jumpfly` the pilot did not cross the pit (a flown jump "
                              "fell short: read jump_flies's landing check)");
    ups_off += off.stats.run_ups;
    ups_on += on.stats.run_ups;
    flown += on.stats.jumps_flown;
  }
  std::printf("pit, human hands, 6 seeds: run-ups %d without jumpfly, %d with (%d flown)\n",
              ups_off, ups_on, flown);
  CHECK_MSG(ups_on <= ups_off, "jumpfly lined up more often than without it");
}
