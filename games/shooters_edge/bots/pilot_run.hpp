// THE PILOT'S RUNS: Shooter's Edge's pilot (plugins/shooter/pilot.hpp) played on its two maps, for
// the pilot's test and the `se-pilot` lab.
// - The NAV COURSE (sim/nav_course.hpp), with no server: a player standing on a route's first node
//   is moved by `run_command` with each command the pilot gives, its commands on the protocol's
//   grid, until it stands on the goal or runs out of time. Every command's origin and velocity
//   are kept (the trail) and folded into the run's hash.
// - The ARENA on the server (server/host.hpp), roofed: the pilot fills a bot slot through the
//   host's command-source seam and visits its goals in turn (`arena_goals`): the west room's far
//   corners (a long straight it strafes), the east room through the door (opened by the trigger
//   it runs through), the ledge up the plat (it waits for the plat at rest and rides it), and the
//   west room again (from the east the door stays shut: it waits, bars it and moves on).
//
// ge:summary The pilot's runs: the nav course through the move, the arena through the server
// ge:keywords pilot pilot-run course arena route trail goals run_course run_arena make_course
// ge:keywords make_arena strafe ride door barred
// ge:skill shooter bots
// ge:test tests/shooter_pilot_test.cpp
#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "shooter/pilot.hpp"
#include "shooters_edge/server/host.hpp"
#include "shooters_edge/sim/nav_course.hpp"

namespace shooters_edge::bots {

namespace sh = ge::shooter;

constexpr int kPilotMsec = 13;  // the commands' length: the server's 77 a second

struct Course {
  sim::NavCourse map;
  sh::MoveWorld world;
  sh::NavGraph graph;
};
std::unique_ptr<Course> make_course();

struct CourseRoute {
  const char* name;
  sh::Vec3f from, to;
};
// The routes the test and the lab play: each of the course's ways, once.
std::vector<CourseRoute> course_routes(const Course& c);
// The east floor's straight from wall to wall, and the settings that strafe it (a shorter stretch
// than the default's: the course is 1024 across).
CourseRoute course_long_straight(const Course& c);
sh::PilotSettings course_strafe_settings();

struct RunResult {
  bool arrived = false;
  float time_s = 0, proven_s = 0;  // the run's, and the route's proven time
  float top_speed = 0;             // across, units/s
  uint64_t hash = 0;               // every command's origin and velocity
  sh::PilotStats stats;
  std::vector<sh::Vec3f> trail;             // the origin after each command
  std::vector<uint8_t> modes;               // the pilot's mode at each command (PilotMode)
  std::vector<int32_t> links;               // the route index of the link it was on at each command
  std::vector<int32_t> route;               // the route first planned
  std::vector<std::vector<int32_t>> plans;  // every route planned, the first included
  // What was pressed and wanted at each command, for the humanness metrics.
  std::vector<double> yaw, yaw_wanted, forward, forward_wanted, side, side_wanted;
};
RunResult run_course(const Course& c, sh::Vec3f from, sh::Vec3f to, sh::PilotHands hands,
                     uint32_t seed, const sh::PilotSettings& set = sh::PilotSettings{},
                     float limit_s = 20);

struct Arena {
  server::Host host;
  sh::NavGraph graph;
  sh::PilotBot bot;
  int slot = -1;
};
std::vector<sh::Vec3f> arena_goals();
// The host, the graph with the door and the plat, the pilot in slot 0 with `hands`.
bool make_arena(Arena& a, sh::PilotHands hands = sh::PilotHands::none, uint32_t seed = 1);

struct ArenaResult {
  bool through_door = false;  // stood east of the dividing wall after standing west of it
  bool on_ledge = false;      // stood on the ledge beside the plat
  int32_t arrivals = 0;
  sh::PilotStats stats;
  std::vector<sh::Vec3f> trail;  // the player's origin after each server frame
  std::vector<uint8_t> modes;
  std::vector<float> door_z, plat_z;  // the movers' bottoms after each frame
};
ArenaResult run_arena(Arena& a, int frames);

}  // namespace shooters_edge::bots
