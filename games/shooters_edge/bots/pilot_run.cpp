// The pilot's runs (pilot_run.hpp): a player on the nav course moved by run_command with each of
// the pilot's commands, and the arena's host with the pilot in its bot slot.
//
// ge:summary The pilot's runs: the course through run_command, the arena through a bot slot
// ge:keywords pilot-run run_course run_arena make_course make_arena course_routes arena_goals
// ge:skill shooter bots
// ge:test tests/shooter_pilot_test.cpp
#include "shooters_edge/bots/pilot_run.hpp"

#include <cstring>
#include <string>

#include "shooter/usercmd.hpp"
#include "shooters_edge/sim/arena.hpp"

namespace shooters_edge::bots {

namespace {

constexpr double kHz = 1000.0 / kPilotMsec;

uint64_t fold(uint64_t h, const sh::Vec3f& v) {
  for (int i = 0; i < 3; i++) {
    const float f = v[i];
    uint32_t b;
    std::memcpy(&b, &f, 4);
    for (int k = 0; k < 4; k++) {
      h ^= (b >> (8 * k)) & 0xff;
      h *= 1099511628211ull;
    }
  }
  return h;
}

float across(sh::Vec3f v) { return sh::root_of(v.x * v.x + v.y * v.y); }

}  // namespace

std::unique_ptr<Course> make_course() {
  auto c = std::make_unique<Course>();
  sim::build_nav_course(c->map);
  sh::map_world(c->map.world.model(), c->world);
  sh::build_nav(c->world, {}, sh::MoveSettings{}, sh::NavSettings{}, c->graph);
  return c;
}

std::vector<CourseRoute> course_routes(const Course& c) {
  return {
      {"platform to east ledge", c.map.platform, c.map.east_ledge},
      {"pit to crate", c.map.pit_floor, c.map.crate_top},
      {"east floor to platform", c.map.east_floor, c.map.platform},
      {"crate to pit", c.map.crate_top, c.map.pit_floor},
  };
}

CourseRoute course_long_straight(const Course&) {
  return {"east floor's straight", {112, 0, 24}, {496, 0, 24}};
}

sh::PilotSettings course_strafe_settings() {
  sh::PilotSettings s;
  s.strafe_min = 256;
  s.strafe_end = 64;
  return s;
}

RunResult run_course(const Course& c, sh::Vec3f from, sh::Vec3f to, sh::PilotHands hands,
                     uint32_t seed, const sh::PilotSettings& set, float limit_s) {
  RunResult r;
  const sh::NavGraph& g = c.graph;
  const int32_t a = sh::nav_nearest(g, from), b = sh::nav_nearest(g, to);
  if (a < 0 || b < 0) return r;
  ge::ai::Search search;
  r.proven_s = sh::nav_route(g, a, b, search).time_s;
  const std::vector<ge::ai::AxisHands> h = sh::pilot_hands(hands);
  sh::Pilot p;
  sh::PilotSettings ps = set;
  ps.lead_s = sh::pilot_lead_s(hands);
  sh::pilot_init(p, g, ps, h, kHz, seed);
  sh::MoveState st;
  st.origin = g.nodes[static_cast<size_t>(a)].origin;
  const sh::MoveSettings move;
  st.maxspeed = move.maxspeed;
  sh::PilotSense s;
  // One command standing still so the move categorises the ground under the player.
  sh::MoveCmd still;
  still.msec = kPilotMsec;
  sh::run_command(st, still, move, c.world);
  auto sense = [&] {
    s.origin = st.origin;
    s.velocity = st.velocity;
    s.onground = st.ground >= 0;
    s.maxspeed = st.maxspeed;
  };
  sense();
  if (!sh::pilot_go(p, b, s)) return r;
  r.route = p.route;
  r.plans.push_back(p.route);
  int32_t planned = p.stats.plans;
  uint64_t hash = 1469598103934665603ull;
  const int most = static_cast<int>(limit_s * 1000 / kPilotMsec);
  for (int i = 0; i < most; i++) {
    const sh::MoveCmd cmd = sh::pilot_step(p, s, kPilotMsec);
    if (p.stats.plans != planned) {
      planned = p.stats.plans;
      r.plans.push_back(p.route);
    }
    if (p.mode == sh::PilotMode::arrived || p.mode == sh::PilotMode::idle) {
      r.arrived = p.mode == sh::PilotMode::arrived;
      break;
    }
    sh::run_command(st, cmd, move, c.world);
    sense();
    hash = fold(fold(hash, st.origin), st.velocity);
    r.trail.push_back(st.origin);
    r.modes.push_back(static_cast<uint8_t>(p.mode));
    r.links.push_back(static_cast<int32_t>(p.k));
    r.yaw.push_back(p.pressed[0]);
    r.yaw_wanted.push_back(p.wanted[0]);
    r.forward.push_back(p.pressed[1]);
    r.forward_wanted.push_back(p.wanted[1]);
    r.side.push_back(p.pressed[2]);
    r.side_wanted.push_back(p.wanted[2]);
    const float v = across(st.velocity);
    if (v > r.top_speed) r.top_speed = v;
  }
  r.time_s = static_cast<float>(r.trail.size()) * (kPilotMsec * 0.001f);
  r.stats = p.stats;
  r.hash = hash;
  return r;
}

std::vector<sh::Vec3f> arena_goals() {
  return {
      {-300, -450, 24},  // the west room's south corner
      {-300, 450, 24},   // its north corner: the long straight
      {400, 0, 24},      // the east room, through the door
      {400, -448, 128},  // the ledge, up the plat
      {-416, 0, 24},     // the west room again: from the east the door stays shut
  };
}

bool make_arena(Arena& a, sh::PilotHands hands, uint32_t seed) {
  server::HostSettings hs;
  hs.seed = seed;
  std::string error;
  if (!server::host_create(a.host, hs, error)) return false;
  const sim::Rules& rules = a.host.rules;
  const sh::Server& sv = a.host.sv;
  sh::MoveWorld world;
  sh::map_world(a.host.models[0], world);
  const sh::Edict& door = sv.edicts[static_cast<size_t>(rules.door)];
  const sh::Edict& plat = sv.edicts[static_cast<size_t>(rules.plat)];
  sh::NavMover movers[2];
  movers[0].ent = rules.door;
  movers[0].absmin = door.absmin;
  movers[0].absmax = door.absmax;
  movers[1].ent = rules.plat;
  movers[1].absmin = plat.absmin;
  movers[1].absmax = plat.absmax;
  movers[1].model = &a.host.models[static_cast<size_t>(sim::kPlatModel) - 1];
  movers[1].travel = {0, 0, sim::kPlatRise};
  movers[1].ride_s = sim::kPlatWait + sim::kPlatRise / sim::kPlatSpeed;
  sh::build_nav(world, movers, sh::MoveSettings{}, sh::NavSettings{}, a.graph);
  const std::vector<ge::ai::AxisHands> h = sh::pilot_hands(hands);
  sh::PilotSettings ps;
  ps.lead_s = sh::pilot_lead_s(hands);
  sh::pilot_init(a.bot.pilot, a.graph, ps, h, kHz, seed);
  for (const sh::Vec3f& p : arena_goals()) a.bot.goals.push_back(sh::nav_nearest(a.graph, p));
  a.slot = server::host_add_bot(a.host, sh::pilot_source(a.bot));
  return a.slot >= 0;
}

ArenaResult run_arena(Arena& a, int frames) {
  ArenaResult r;
  bool west = false;
  const sim::Rules& rules = a.host.rules;
  for (int f = 0; f < frames; f++) {
    server::host_frame(a.host, nullptr, 0);
    const sh::Edict& me = a.host.sv.edicts[static_cast<size_t>(a.slot) + 1];
    const sh::Vec3f o = me.origin;
    const bool ground = (me.flags & sh::kFlagOnGround) != 0;
    if (o.x < 100) west = true;
    if (west && o.x > 150) r.through_door = true;
    if (ground && o.z > 120 && o.y < -384 && o.x > 192) r.on_ledge = true;
    r.trail.push_back(o);
    r.modes.push_back(static_cast<uint8_t>(a.bot.pilot.mode));
    r.door_z.push_back(a.host.sv.edicts[static_cast<size_t>(rules.door)].absmin.z);
    r.plat_z.push_back(a.host.sv.edicts[static_cast<size_t>(rules.plat)].absmin.z);
  }
  r.arrivals = a.bot.arrivals;
  r.stats = a.bot.pilot.stats;
  return r;
}

}  // namespace shooters_edge::bots
