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
// ge:skill shooter bots testing
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <string>

#include "check.hpp"
#include "golden.hpp"
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
  for (const pr::CourseRoute& r : pr::course_routes(c)) {
    for (uint32_t seed = 1; seed <= 3; seed++) {
      runs++;
      if (pr::run_course(c, r.from, r.to, sh::PilotHands::human, seed).arrived) arrived++;
    }
  }
  CHECK_MSG(arrived == runs, "human hands arrived on " + std::to_string(arrived) + " of " +
                                 std::to_string(runs) + " runs");
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
