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
// ge:keywords rocket-jump flip standing-flip
// ge:skill shooter bots testing
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <string>

#include "check.hpp"
#include "golden.hpp"
#include "shooter/brush_hull.hpp"
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
