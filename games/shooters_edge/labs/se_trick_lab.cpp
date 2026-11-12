// LAB `se-trick`: CAN ONE BOT RUN A TRICK MAP FROM START TO FINISH, HOW FAST, AND WHERE DOES IT
// FAIL? A trick map is a course of jumps cut into SECTIONS by its teleporters: a miss falls into a
// trigger that sends the player back to the section's own start (a FAIL), and the section's exit
// is a trigger that sends him on to the next one. The course is read off the map's entities alone:
// it starts at the map's spawn, and each section's exit is the teleporter (to a start not yet
// visited) whose route on the bots' nav (bots/map_nav.hpp) from the section's start is the
// quickest with no teleporter on the way; a section no route reaches is run anyway at the trigger
// nearest its start, and tabled as "no route": the nav proves no way through it.
//
// ONE BOT, an expert's pilot through an expert's keyboard as in a match (bots/fighter.hpp
// `fighter_pilot`, `--features` over the expert's preset), runs each section from its start to its
// exit (bots/pilot_run.hpp `run_route`, no server: the lab watches the triggers itself), up to
// `--tries` times, each try a new seed, until one gets through. Every command's box is checked
// against every teleporter's: the first touched ends the try.
//
// What it shows: the sections (planned or not, the route's jumps, the try that got through, the
// time with the fails' time added: what a person pays restarting), every fail (where, the last
// takeoff before it: its place, speed and the pilot's reason, and the reason at the fall), each
// section from above (every try's trail, green through and red failed, the takeoffs, the fails
// ringed), and the speed along the course. Deterministic: the digest holds the counts and times.
//
// The maps are community trick maps the player fetches into the game folder's maps (the bots
// skill; `.bots/local.md` says where from): ztrain, zjumps, ztricks, hoppa2.
//
// ge:summary Lab: one bot runs a trick map's course section by section: times, fails, pictures
// ge:keywords lab se-trick trick-map course sections teleporter fail takeoff jump-course ztrain
// ge:keywords zjumps ztricks hoppa2 time-trial
// ge:skill bots labs
// ge:test tests/shooter_pilot_test.cpp
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "core/num_text.hpp"
#include "lab/chart.hpp"
#include "lab/lab.hpp"
#include "lab/plot.hpp"
#include "lab/raster.hpp"
#include "progs/entity_text.hpp"
#include "shooter/features.hpp"
#include "shooter/nav.hpp"
#include "shooter/nav_gaps.hpp"
#include "shooter/traits.hpp"
#include "shooters_edge/bots/fighter.hpp"
#include "shooters_edge/bots/map_nav.hpp"
#include "shooters_edge/bots/pilot_run.hpp"
#include "shooters_edge/client/files.hpp"
#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/server/qc_server.hpp"
#include "shooters_edge/server/rules_runner.hpp"

namespace {

namespace gl = ge::lab;
namespace sh = ge::shooter;
namespace pg = ge::progs;
namespace cl = shooters_edge::client;
namespace sv = shooters_edge::server;
namespace bt = shooters_edge::bots;
using ge::core::num::to_fixed;

constexpr float kLift = 27;          // a teleporter's destination lifts the player this far
constexpr int kMaxSections = 64;     // a course longer than this is cut
constexpr int kSheetColumns = 4;     // the sections' close-ups, a row
constexpr int kCell = 360;           // each close-up's size, pixels
constexpr float kPad = 96;           // round a section's trails in its close-up, units
constexpr float kHullHalf = 16;      // the player's box across, half
constexpr float kHullDown = 24, kHullUp = 32;
constexpr float kExitNear = 32;  // a try that arrives this near its exit's trigger is through
constexpr float kPitDrop = 40;   // a floor this far under the start's is a pit to jump
// A pit's jump proven from the slowest of these speeds up, the first that lands kept (a route
// takes the quickest flight: a faster jump kept beside it would be the one it asks for), units/s.
constexpr float kGapSlowest = 330, kGapFastest = 720, kGapStep = 30;

struct Trigger {
  sh::Vec3f lo, hi;
  std::string target;
};

struct Try {
  bool through = false, failed = false;
  float time_s = 0;
  int32_t end = -1;  // the command it ended on (a trigger touched), or -1
  int32_t takeoff = -1;
  bt::RunResult run;
};

struct Section {
  std::string own;   // the destination a fail here sends back to ("" at a spawn with none)
  std::string next;  // the destination its exit sends on to
  sh::Vec3f start, exit;
  bool planned = false;
  float proven_s = 0;
  int jumps = 0, links = 0, gaps = 0;  // gaps: the pit's jump's proven speed (0: none)
  float pit = 0;  // the pit's width found on the way, units (0: none sought or found)
  sh::Vec3f reach;     // no route: the node the nav reaches nearest the exit
  float reach_left = 0;  // and how far it stands from it, units
  int32_t reach_n = 0;   // the nodes it reaches
  sh::Vec3f lip;  // the pit's takeoff, when found
  std::vector<Try> tries;
};

std::string v3(sh::Vec3f o) {
  return to_fixed(o.x, 0) + " " + to_fixed(o.y, 0) + " " + to_fixed(o.z - kHullDown, 0);
}

bool vec_of(const char* s, sh::Vec3f& out) {
  if (s == nullptr) return false;
  char* end = nullptr;
  out.x = std::strtof(s, &end);
  out.y = std::strtof(end, &end);
  out.z = std::strtof(end, &end);
  return true;
}

// A pit's trigger: flat (a floor a fall passes through), not a wall's slab or a doorway.
constexpr float kFlat = 48;
constexpr float kClosed_s = 1e6f;  // a link into a pit's floor costs this (close_pits)
bool flat(const Trigger& t) { return t.hi.z - t.lo.z < kFlat; }

bool touches(const Trigger& t, sh::Vec3f o) {
  return o.x + kHullHalf >= t.lo.x && o.x - kHullHalf <= t.hi.x && o.y + kHullHalf >= t.lo.y &&
         o.y - kHullHalf <= t.hi.y && o.z + kHullUp >= t.lo.z && o.z - kHullDown <= t.hi.z;
}

const sh::NavLink* link_between(const sh::NavGraph& g, int32_t a, int32_t b) {
  const sh::NavNode& n = g.nodes[static_cast<size_t>(a)];
  for (int32_t x = 0; x < n.links; x++) {
    const sh::NavLink& l = g.links[static_cast<size_t>(n.first_link) + static_cast<size_t>(x)];
    if (l.to == b) return &l;
  }
  return nullptr;
}

// A jump left the ground at command `i`: the velocity up from (about) none to a jump's.
bool takes_off(const bt::RunResult& r, size_t i) {
  return i > 0 && r.velocity[i].z > 150 && r.velocity[i - 1].z < 40;
}

// The node nearest `p` (any distance: a course's start may stand far from the grid's nodes).
int32_t nearest_node(const sh::NavGraph& g, sh::Vec3f p) {
  int32_t best = -1;
  float best_d = 1e30f;
  for (size_t k = 0; k < g.nodes.size(); k++) {
    const sh::Vec3f o = g.nodes[k].origin;
    const float d = (o.x - p.x) * (o.x - p.x) + (o.y - p.y) * (o.y - p.y) + 4 * (o.z - p.z) * (o.z - p.z);
    if (d < best_d) best_d = d, best = static_cast<int32_t>(k);
  }
  return best;
}

// The quickest route from `from` into trigger `t` with no teleporter on it; empty with none.
sh::NavRoute route_into(const sh::NavGraph& g, int32_t from, const Trigger& t, ge::ai::Search& s,
                        const sh::NavJumper& jumper) {
  // The nodes in the trigger first; only when none has a route, those a step short of a thin one
  // (its slab between two rows of the grid).
  sh::NavRoute best;
  for (int pass = 0; pass < 2 && best.nodes.empty(); pass++)
    for (size_t k = 0; k < g.nodes.size(); k++) {
      const sh::Vec3f o = g.nodes[k].origin;
      if (pass == 0 && !touches(t, o)) continue;
      if (pass == 1) {
        const float ox = std::max({t.lo.x - o.x, 0.0f, o.x - t.hi.x});
        const float oy = std::max({t.lo.y - o.y, 0.0f, o.y - t.hi.y});
        if (touches(t, o) || std::hypot(ox, oy) > kHullHalf + kExitNear ||
            o.z + kHullUp < t.lo.z || o.z - kHullDown > t.hi.z)
          continue;
      }
      sh::NavRoute r = sh::nav_route(g, from, static_cast<int32_t>(k), s, 1, {}, {}, jumper);
      if (r.nodes.empty() || r.time_s >= kClosed_s ||
          (!best.nodes.empty() && r.time_s >= best.time_s))
        continue;
      bool teleported = false;
      for (size_t i = 0; i + 1 < r.nodes.size() && !teleported; i++) {
        const sh::NavLink* l = link_between(g, r.nodes[i], r.nodes[i + 1]);
        teleported = l != nullptr && l->teleport;
      }
      if (!teleported) best = std::move(r);
    }
  return best;
}

// THE PIT ON THE WAY: along the line from `from` (a node, feet 24 under) toward `to`, the floor
// traced down every 4 units; the first stretch under the start's floor by `kPitDrop` is the pit,
// the last floor before it the takeoff and the first floor after it the landing (both as a
// player's origin). False with no pit, or a wall before the far side.
bool pit_on_line(const sh::MoveWorld& w, sh::Vec3f from, sh::Vec3f to, sh::Vec3f& takeoff,
                 sh::Vec3f& landing) {
  const sh::Hull& hull = w.solids[0].point_hull;
  const float feet = from.z - kHullDown;
  const float dx = to.x - from.x, dy = to.y - from.y;
  const float len = std::sqrt(dx * dx + dy * dy);
  if (len < 8) return false;
  bool in_pit = false;
  float last_floor = feet;
  sh::Vec3f last{from.x, from.y, feet};
  for (float d = 0; d <= len; d += 4) {
    const float x = from.x + dx * d / len, y = from.y + dy * d / len;
    const sh::HullTrace tr = sh::trace_hull(hull, {x, y, feet + 64}, {x, y, feet - 1024});
    if (tr.startsolid) return false;
    const float z = tr.endpos.z;
    if (!in_pit) {
      if (z < feet - kPitDrop) {
        in_pit = true;
        takeoff = {last.x, last.y, last_floor + kHullDown};
      } else {
        last = {x, y, z};
        last_floor = z;
      }
    } else if (z >= feet - kPitDrop) {
      const float in = std::min(d + 24, len);
      landing = {from.x + dx * in / len, from.y + dy * in / len, z + kHullDown};
      return true;
    }
  }
  return false;
}

// THE PITS CLOSED: a node under a teleporter's trigger (inside its box across, its feet under the
// trigger's top) is never stood on: a player falling there is sent away on the way down (zjumps'
// triggers hang 100 units over the pits' floors). Every link into one costs `kClosed_s`, so no
// route runs through a pit's floor; `dead` marks them. Counts the links closed.
int close_pits(sh::NavGraph& g, const std::vector<Trigger>& triggers, std::vector<uint8_t>& dead) {
  dead.assign(g.nodes.size(), 0);
  for (size_t k = 0; k < g.nodes.size(); k++) {
    const sh::Vec3f o = g.nodes[k].origin;
    for (const Trigger& t : triggers)
      if (flat(t) && o.x + kHullHalf >= t.lo.x && o.x - kHullHalf <= t.hi.x && o.y + kHullHalf >= t.lo.y &&
          o.y - kHullHalf <= t.hi.y && o.z - kHullDown <= t.hi.z)
        dead[k] = 1;
  }
  int closed = 0;
  for (sh::NavLink& l : g.links)
    if (dead[static_cast<size_t>(l.to)] != 0 && !l.teleport) {
      l.time_s = kClosed_s;
      closed++;
    }
  return closed;
}

// THE PITS FROM THE FAIL TRIGGERS: a teleporter's trigger the floor drops into (a link from a
// node outside it to a node in it) is a pit; its RIM the nodes with such a link. From each rim
// node, the nearest rim nodes across the same pit (the jump's middle over the trigger, the two
// within `kRimFar`, no walk between them as short as twice the jump's length) are proven as jumps
// at the speed ladder, the slowest that lands kept. Counts the pairs tried and the jumps kept.
constexpr float kRimNear = 64, kRimFar = 600;
constexpr int kRimAcross = 6;  // each rim node's nearest crossings tried, a landing spot each
constexpr float kSpot = 96;     // landings this near one taken are the same spot
constexpr float kHopLand = 48;     // a hop comes down within this across of its landing node
constexpr float kHopCost_s = 0.2f; // and costs its flight and this (a miss is a fall)
constexpr int kPilotMsec = 13;     // the pilot's command (bots/pilot_run.hpp)
void prove_pits(const sh::MoveWorld& world, sh::NavGraph& g, const std::vector<Trigger>& triggers,
                const sh::NavJumper& jumper, int& tried, int& kept) {
  ge::ai::Search search;
  std::vector<std::pair<int32_t, int32_t>> pairs;
  // The rim: nodes over a pit's trigger (64 round its box), above it, with the floor 32 units off
  // in some way more than `kPitDrop` under their feet (a lip; the nav links no drop that deep).
  const auto over = [&](const Trigger& t, sh::Vec3f o, float m) {
    return o.x >= t.lo.x - m && o.x <= t.hi.x + m && o.y >= t.lo.y - m && o.y <= t.hi.y + m;
  };
  std::vector<int32_t> rim;
  const sh::Hull& hull = world.solids[0].point_hull;
  for (size_t a = 0; a < g.nodes.size(); a++) {
    const sh::Vec3f o = g.nodes[a].origin;
    bool near = false;
    for (const Trigger& t : triggers)
      near = near || (flat(t) && !touches(t, o) && o.z - kHullDown >= t.hi.z && over(t, o, 64));
    if (!near) continue;
    const float feet = o.z - kHullDown;
    constexpr float kOff[4][2] = {{32, 0}, {-32, 0}, {0, 32}, {0, -32}};
    for (const auto& d : kOff) {
      const sh::HullTrace tr = sh::trace_hull(hull, {o.x + d[0], o.y + d[1], feet + 8},
                                              {o.x + d[0], o.y + d[1], feet - 1024});
      if (!tr.startsolid && tr.endpos.z < feet - kPitDrop) {
        rim.push_back(static_cast<int32_t>(a));
        break;
      }
    }
  }
  // Each rim node's nearest crossings: to a rim node no higher, its jump's middle over a pit.
  for (const int32_t a : rim) {
    const sh::Vec3f A = g.nodes[static_cast<size_t>(a)].origin;
    std::vector<std::pair<float, int32_t>> across;
    for (const int32_t b : rim) {
      const sh::Vec3f B = g.nodes[static_cast<size_t>(b)].origin;
      const float d = std::hypot(B.x - A.x, B.y - A.y);
      if (d < kRimNear || d > kRimFar || B.z > A.z + 40) continue;
      const sh::Vec3f mid{(A.x + B.x) / 2, (A.y + B.y) / 2, A.z};
      bool pit = false;
      for (const Trigger& t : triggers) pit = pit || (flat(t) && over(t, mid, 0));
      if (pit) across.emplace_back(d, b);
    }
    std::sort(across.begin(), across.end());
    int taken = 0;
    std::vector<sh::Vec3f> spots;  // one crossing a landing spot: the nearest block's nodes do
                                   // not crowd out the next block's
    for (const auto& [d, b] : across) {
      if (taken >= kRimAcross) break;
      const sh::Vec3f B = g.nodes[static_cast<size_t>(b)].origin;
      bool seen = false;
      for (const sh::Vec3f& o : spots) seen = seen || std::hypot(o.x - B.x, o.y - B.y) < kSpot;
      if (seen) continue;
      const sh::NavRoute r = sh::nav_route(g, a, b, search, 1, {}, {}, jumper);
      if (!r.nodes.empty() && r.time_s < 2 * d / 320) continue;  // walked round as quickly
      pairs.emplace_back(a, b);
      spots.push_back(B);
      taken++;
    }
  }
  sh::MoveSettings move;
  std::vector<sh::NavExtraLink> hops;
  for (const auto& [a, b] : pairs) {
    tried++;
    const sh::Vec3f A = g.nodes[static_cast<size_t>(a)].origin;
    const sh::Vec3f B = g.nodes[static_cast<size_t>(b)].origin;
    const float d = std::hypot(B.x - A.x, B.y - A.y);
    for (float v = kGapSlowest; v <= kGapFastest; v += kGapStep) {
      // A HOP'S PROOF: the jump flown at `v` toward B comes DOWN by it (the pilot's own test of a
      // gap's jump, pilot_gaps.hpp `gap_lands`), not standing on it: a block too small to stop on
      // at this speed is jumped from again on its landing (the pilot's hop does that).
      const sh::Vec3f vel{(B.x - A.x) / d * v, (B.y - A.y) / d * v, 0};
      const sh::JumpFlight f = sh::fly_jump(world, move, A, vel, B, kPilotMsec);
      const float dz = f.end.z - B.z;
      if (!f.landed || std::hypot(f.end.x - B.x, f.end.y - B.y) > kHopLand || dz > 32 || dz < -32)
        continue;
      sh::NavExtraLink l{a, b, sh::NavKind::jump, f.time_s + kHopCost_s};
      l.speed = v;
      l.gap = true;
      hops.push_back(l);
      kept++;
      break;
    }
  }
  sh::nav_add_links(g, hops);
}

// THE NAV'S REACH, the first section no route crosses: the whole map from above (the floor shaded
// by height), the nodes the start reaches in green and the rest grey, the fail triggers in red, the
// pits' proven jumps in blue, the start ringed and the exit's trigger in black.
void draw_reach(gl::Lab& lab, const std::string& name, const sh::MoveWorld& world,
                const sh::NavGraph& g, const ge::ai::Search& reach,
                const std::vector<uint8_t>& dead, const std::vector<Trigger>& triggers, sh::Vec3f start, const Trigger& exit) {
  sh::Vec3f lo{1e9f, 1e9f, 1e9f}, hi{-1e9f, -1e9f, -1e9f};
  for (const sh::NavNode& n : g.nodes) {
    lo = {std::min(lo.x, n.origin.x), std::min(lo.y, n.origin.y), std::min(lo.z, n.origin.z)};
    hi = {std::max(hi.x, n.origin.x), std::max(hi.y, n.origin.y), std::max(hi.z, n.origin.z)};
  }
  lo = {lo.x - 64, lo.y - 64, lo.z - 128};
  hi = {hi.x + 64, hi.y + 64, hi.z + 64};
  constexpr int kSide = 1400;
  const float span = std::max(hi.x - lo.x, hi.y - lo.y);
  const float sc = static_cast<float>(kSide) / span;
  gl::Picture p = gl::create_picture(kSide, kSide, {245, 245, 240, 255});
  const auto px = [&](float x) { return (x - lo.x) * sc; };
  const auto py = [&](float y) { return (lo.y + span - y) * sc; };
  const sh::Hull& hull = world.solids[0].point_hull;
  for (int y = 0; y < kSide; y += 2)
    for (int x = 0; x < kSide; x += 2) {
      const float wx = lo.x + (static_cast<float>(x) + 1) / sc;
      const float wy = lo.y + span - (static_cast<float>(y) + 1) / sc;
      const sh::HullTrace tr = sh::trace_hull(hull, {wx, wy, hi.z}, {wx, wy, lo.z});
      const float f = tr.startsolid || tr.fraction >= 1 ? 0 : (tr.endpos.z - lo.z) / (hi.z - lo.z);
      const auto v = static_cast<uint8_t>(tr.startsolid ? 70 : 140 + 105 * std::clamp(f, 0.0f, 1.0f));
      gl::fill_rect(p, x, y, 2, 2, {static_cast<uint8_t>(v - 10), v, static_cast<uint8_t>(v - 20), 255});
    }
  for (const Trigger& t : triggers) {
    const gl::Rgba c = &t == &exit ? gl::Rgba{0, 0, 0, 255} : gl::Rgba{210, 40, 40, 255};
    gl::draw_line(p, px(t.lo.x), py(t.lo.y), px(t.hi.x), py(t.lo.y), c);
    gl::draw_line(p, px(t.hi.x), py(t.lo.y), px(t.hi.x), py(t.hi.y), c);
    gl::draw_line(p, px(t.hi.x), py(t.hi.y), px(t.lo.x), py(t.hi.y), c);
    gl::draw_line(p, px(t.lo.x), py(t.hi.y), px(t.lo.x), py(t.lo.y), c);
  }
  for (size_t k = 0; k < g.nodes.size(); k++) {
    const sh::NavNode& n = g.nodes[k];
    for (int32_t x = 0; x < n.links; x++) {
      const sh::NavLink& l = g.links[static_cast<size_t>(n.first_link) + static_cast<size_t>(x)];
      if (!l.gap) continue;
      const sh::Vec3f b = g.nodes[static_cast<size_t>(l.to)].origin;
      gl::draw_line(p, px(n.origin.x), py(n.origin.y), px(b.x), py(b.y), {40, 80, 210, 255});
    }
  }
  for (size_t k = 0; k < g.nodes.size(); k++) {
    const sh::Vec3f o = g.nodes[k].origin;
    if (dead[k] != 0) continue;
    if (reach.reached(static_cast<int32_t>(k)))
      gl::fill_rect(p, px(o.x) - 1, py(o.y) - 1, 3, 3, {20, 170, 50, 255});
    else
      gl::set_pixel(p, static_cast<int>(px(o.x)), static_cast<int>(py(o.y)), {150, 150, 150, 255});
  }
  gl::draw_ring(p, px(start.x), py(start.y), 10, {0, 0, 0, 255});
  gl::draw_ring(p, px(start.x), py(start.y), 11, {0, 0, 0, 255});
  lab.shot({"reach-" + name, name + ": WHAT THE NAV REACHES FROM THE START",
            "The whole map from above, the floor shaded by height. Green dots: the nav's nodes the "
            "section's start reaches (no teleporter); grey: the rest. Red boxes: teleporters' "
            "triggers (the fails), black: the exit's; blue lines: the pits' jumps proven at speed; "
            "the black ring the start.",
            "where the green stops short of the exit: the pit no proven jump crosses",
            {{"map", name}}},
           p);
}

struct Course {
  std::string name, error;
  int pit_pairs = 0, pit_jumps = 0;  // the fail triggers' pits: crossings tried, jumps kept
  int closed = 0;                    // links into a pit's floor closed (close_pits)
  int brinks = 0;                    // links along the pits' lips marked brinks
  std::vector<Section> sections;
  sh::Vec3f lo{1e9f, 1e9f, 1e9f}, hi{-1e9f, -1e9f, -1e9f};
};

void run_course_map(gl::Lab& lab, std::span<const uint8_t> bytes, const std::string& name,
                    const pg::Progs& progs, Course& out, std::ostream* trace) {
  out.name = name;
  std::unique_ptr<cl::LoadedMap> map = cl::map_from_bytes(bytes, name, out.error);
  if (!map) return;
  sv::QcServerOptions so;
  so.loop.max_edicts = 1024;
  std::unique_ptr<sv::QcServer> qc = sv::make_qc_server(progs, so, out.error);
  if (!qc) return;
  sv::RulesRunner rules = sv::qc_runner(std::move(qc));
  sv::QcServer& q = *sv::qc_of(rules);
  rules.console("deathmatch 1");
  if (!rules.start(map->spec, out.error)) return;
  sv::use_map_sets(q, map->bsp);
  bt::MapNav nav;
  sh::NavSettings nset;
  nset.spacing = static_cast<float>(lab.arg_num("spacing"));
  if (!bt::build_map_nav(q, map->spec, "info_player_deathmatch", nav, out.error, nset)) return;
  sh::NavGraph& g = nav.graph;

  // The map's teleporters and their destinations, and the spawn.
  std::vector<pg::EntityBlock> blocks;
  if (!pg::parse_entity_text(map->spec.entities.c_str(), blocks, out.error)) return;
  std::vector<Trigger> triggers;
  std::vector<std::pair<std::string, sh::Vec3f>> dests;
  sh::Vec3f spawn;
  bool spawned = false;
  for (const pg::EntityBlock& b : blocks) {
    const char* cn = pg::block_value(b, "classname");
    if (cn == nullptr) continue;
    sh::Vec3f o;
    if (std::strcmp(cn, "trigger_teleport") == 0) {
      const char* model = pg::block_value(b, "model");
      const char* target = pg::block_value(b, "target");
      if (model == nullptr || model[0] != '*' || target == nullptr) continue;
      const int m = std::atoi(model + 1);
      if (m <= 0 || static_cast<size_t>(m) >= map->spec.models.size()) continue;
      const sh::BrushModel& box = map->spec.models[static_cast<size_t>(m)];
      triggers.push_back({box.mins, box.maxs, target});
    } else if (std::strcmp(cn, "info_teleport_destination") == 0) {
      const char* tn = pg::block_value(b, "targetname");
      if (tn != nullptr && vec_of(pg::block_value(b, "origin"), o))
        dests.emplace_back(tn, sh::Vec3f{o.x, o.y, o.z + kLift});
    } else if ((std::strcmp(cn, "info_player_start") == 0 ||
                (!spawned && std::strcmp(cn, "info_player_deathmatch") == 0)) &&
               vec_of(pg::block_value(b, "origin"), o)) {
      spawn = {o.x, o.y, o.z + 1};
      spawned = true;
    }
  }
  if (!spawned || triggers.empty()) {
    out.error = "no spawn or no teleporter: not a course of sections";
    return;
  }
  const auto dest_at = [&](const std::string& n, sh::Vec3f& at) {
    for (const auto& [dn, o] : dests)
      if (dn == n) return at = o, true;
    return false;
  };

  // The expert's pilot through an expert's keyboard, as a fighter's.
  sh::Features features = sh::features_of(sh::AimSkill::expert);
  std::string error;
  if (!sh::apply_features(lab.arg("features"), features, error)) {
    out.error = error;
    return;
  }
  const sh::Traits traits = sh::traits_of(sh::AimSkill::expert, sh::Temper::balanced);
  sh::PilotHands hands = sh::PilotHands::none;
  sh::PilotSettings ps = bt::fighter_pilot(features, traits, sh::AimSkill::expert, hands);
  ps.jumper.gap_build_s = features[sh::Feature::gapbuild] ? bt::kGapBuild_s : -1;
  ps.jumper.gaps = true;  // the pits' jumps proven at speed (below) are this course's way on
  const sh::Features* keys = features[sh::Feature::keys] ? &features : nullptr;
  const int tries = std::max(1, static_cast<int>(lab.arg_num("tries")));
  const auto limit = static_cast<float>(lab.arg_num("limit"));

  std::vector<uint8_t> dead;
  out.closed = close_pits(g, triggers, dead);
  // The pits' floors as harm, as lava's are: the walks along their lips are brinks, where a
  // hop's stretch ends (pilot.hpp `brink`) rather than running on over the lip.
  if (lab.arg("harm") == "1") {
    std::vector<uint8_t> harm = nav.harm;
    harm.resize(g.nodes.size(), 0);
    for (size_t k = 0; k < g.nodes.size(); k++) harm[k] = harm[k] | dead[k];
    out.brinks = static_cast<int>(sh::nav_mark_harm(g, harm));
  }
  if (lab.arg("pits") != "line") prove_pits(nav.world, g, triggers, ps.jumper, out.pit_pairs, out.pit_jumps);

  // The sections, one after another.
  std::string own;
  float near = 512;
  for (const auto& [dn, o] : dests) {
    const float d = std::hypot(o.x - spawn.x, o.y - spawn.y, o.z - spawn.z);
    if (d < near) near = d, own = dn;
  }
  std::set<std::string> visited{own};
  sh::Vec3f start = spawn;
  ge::ai::Search search;
  bool drew_reach = false;
  for (int s = 0; s < kMaxSections; s++) {
    const int32_t a = nearest_node(g, start);
    if (a < 0) break;
    Section sec;
    sec.own = own;
    sec.start = start;
    int pick = -1;
    sh::NavRoute best;
    float nearest = 1e30f;
    int near_pick = -1;
    for (size_t t = 0; t < triggers.size(); t++) {
      const Trigger& tr = triggers[t];
      if (tr.target == own || visited.count(tr.target) != 0) continue;
      sh::Vec3f at;
      if (!dest_at(tr.target, at)) continue;
      const sh::NavRoute r = route_into(g, a, tr, search, ps.jumper);
      if (!r.nodes.empty() && (best.nodes.empty() || r.time_s < best.time_s)) {
        best = r;
        pick = static_cast<int>(t);
      }
      const sh::Vec3f c{(tr.lo.x + tr.hi.x) / 2, (tr.lo.y + tr.hi.y) / 2, (tr.lo.z + tr.hi.z) / 2};
      const float d = std::hypot(c.x - start.x, c.y - start.y, c.z - start.z);
      if (d < nearest) nearest = d, near_pick = static_cast<int>(t);
    }
    if (pick < 0) pick = near_pick;
    if (pick < 0) break;
    const Trigger& exit = triggers[static_cast<size_t>(pick)];
    sec.next = exit.target;
    if (best.nodes.empty()) {
      // No way the nav proves: the pit on the straight way to the exit, its jump proven at a
      // hopper's speeds (nav_gaps.hpp), and the route asked again.
      const sh::Vec3f to{(exit.lo.x + exit.hi.x) / 2, (exit.lo.y + exit.hi.y) / 2, start.z};
      sh::Vec3f off, on;
      if (pit_on_line(nav.world, g.nodes[static_cast<size_t>(a)].origin, to, off, on)) {
        sec.pit = std::hypot(on.x - off.x, on.y - off.y);
        sec.lip = off;
        sh::MoveSettings move;
        sh::NavGapSettings gs;
        gs.faster = 1;  // each speed alone: the ladder is the search
        for (float v = kGapSlowest; v <= kGapFastest && sec.gaps == 0; v += kGapStep) {
          const sh::NavGapTrick trick{off, on, v, 0};
          const sh::NavGapCounts made = sh::nav_add_gap_tricks(nav.world, move, g, {&trick, 1}, {}, gs);
          if (made.held > 0) sec.gaps = static_cast<int>(v);
        }
        best = route_into(g, a, exit, search, ps.jumper);
      }
    }
    sec.planned = !best.nodes.empty();
    if (!sec.planned) {
      // How far the nav gets: of what it reaches from the start (no teleporter), nearest the exit.
      sh::NavJumper walker = ps.jumper;
      ge::ai::Search reach;
      sec.reach_n = sh::nav_reach(g, a, reach, walker);
      const sh::Vec3f c{(exit.lo.x + exit.hi.x) / 2, (exit.lo.y + exit.hi.y) / 2,
                        (exit.lo.z + exit.hi.z) / 2};
      if (!drew_reach) {
        drew_reach = true;
        draw_reach(lab, name, nav.world, g, reach, dead, triggers, sec.start, exit);
      }
      sec.reach_left = 1e30f;
      for (size_t k = 0; k < g.nodes.size(); k++) {
        if (!reach.reached(static_cast<int32_t>(k)) || dead[k] != 0) continue;
        const sh::Vec3f o = g.nodes[k].origin;
        bool in_trigger = false;
        for (const Trigger& t : triggers) in_trigger = in_trigger || touches(t, o);
        if (in_trigger) continue;
        const float d = std::hypot(o.x - c.x, o.y - c.y, o.z - c.z);
        if (d < sec.reach_left) sec.reach_left = d, sec.reach = o;
      }
    }
    if (sec.planned) {
      sec.exit = g.nodes[static_cast<size_t>(best.nodes.back())].origin;
      sec.proven_s = best.time_s;
      for (size_t i = 0; i + 1 < best.nodes.size(); i++) {
        const sh::NavLink* l = link_between(g, best.nodes[i], best.nodes[i + 1]);
        sec.links++;
        sec.jumps += l != nullptr && l->kind == sh::NavKind::jump;
      }
    } else {
      sec.exit = {(exit.lo.x + exit.hi.x) / 2, (exit.lo.y + exit.hi.y) / 2, exit.lo.z + kHullDown};
    }
    // The tries: each run to the first teleporter touched.
    for (int k = 0; k < tries; k++) {
      Try tr;
      tr.run = bt::run_route(nav.world, g, g.nodes[static_cast<size_t>(a)].origin, sec.exit, hands, static_cast<uint32_t>(k + 1),
                             ps, limit, {}, keys, traits[sh::Trait::moves]);
      const bt::RunResult& r = tr.run;
      for (size_t i = 0; i < r.trail.size() && tr.end < 0; i++)
        for (const Trigger& t : triggers)
          if (touches(t, r.trail[i])) {
            tr.end = static_cast<int32_t>(i);
            tr.through = t.target != own;
            tr.failed = !tr.through;
            break;
          }
      if (tr.end < 0 && r.arrived && !r.trail.empty()) {
        // Arrived by the exit (its node is beside a thin trigger, not in it): a step on is in it.
        const sh::Vec3f o = r.trail.back();
        const float ox = std::max({exit.lo.x - o.x, 0.0f, o.x - exit.hi.x});
        const float oy = std::max({exit.lo.y - o.y, 0.0f, o.y - exit.hi.y});
        if (std::hypot(ox, oy) <= kHullHalf + kExitNear) {
          tr.end = static_cast<int32_t>(r.trail.size()) - 1;
          tr.through = true;
        }
      }
      const size_t n = tr.end >= 0 ? static_cast<size_t>(tr.end) + 1 : r.trail.size();
      tr.time_s = static_cast<float>(n) * 0.013f;
      for (size_t i = 1; i < n; i++)
        if (takes_off(r, i)) tr.takeoff = static_cast<int32_t>(i);
      for (size_t i = 0; i < n; i++) {
        const sh::Vec3f o = r.trail[i];
        out.lo = {std::min(out.lo.x, o.x), std::min(out.lo.y, o.y), std::min(out.lo.z, o.z)};
        out.hi = {std::max(out.hi.x, o.x), std::max(out.hi.y, o.y), std::max(out.hi.z, o.z)};
      }
      if (trace != nullptr)
        for (size_t i = 0; i < n; i++) {
          const sh::Vec3f o = r.trail[i], v = r.velocity[i];
          *trace << name << '\t' << s + 1 << '\t' << k + 1 << '\t' << to_fixed(0.013 * static_cast<double>(i + 1), 3)
                 << '\t' << to_fixed(o.x, 0) << '\t' << to_fixed(o.y, 0) << '\t'
                 << to_fixed(o.z - kHullDown, 0) << '\t' << to_fixed(std::hypot(v.x, v.y), 0) << '\t'
                 << to_fixed(v.z, 0) << '\t' << sh::pilot_mode_name(static_cast<sh::PilotMode>(r.modes[i]))
                 << '\t' << r.links[i] << '\t' << to_fixed(r.yaw[i], 1) << '\t'
                 << to_fixed(r.forward[i], 2) << '\t' << to_fixed(r.side[i], 2) << '\t'
                 << (r.because[i] != nullptr ? r.because[i] : "-") << '\n';
        }
      const bool through = tr.through;
      sec.tries.push_back(std::move(tr));
      if (through) break;
    }
    out.sections.push_back(std::move(sec));
    visited.insert(exit.target);
    own = exit.target;
    if (!dest_at(own, start)) break;
  }

  // The close-ups: each section from above, its tries' trails over the floor.
  const sh::Hull& hull = nav.world.solids[0].point_hull;
  const int rows = (static_cast<int>(out.sections.size()) + kSheetColumns - 1) / kSheetColumns;
  gl::Picture sheet = gl::create_picture(kSheetColumns * kCell, std::max(1, rows) * kCell,
                                         {255, 255, 255, 255});
  for (size_t s = 0; s < out.sections.size(); s++) {
    const Section& sec = out.sections[s];
    sh::Vec3f lo = sec.start, hi = sec.start;
    const auto grow = [&](sh::Vec3f o) {
      lo = {std::min(lo.x, o.x), std::min(lo.y, o.y), std::min(lo.z, o.z)};
      hi = {std::max(hi.x, o.x), std::max(hi.y, o.y), std::max(hi.z, o.z)};
    };
    grow(sec.exit);
    for (const Try& t : sec.tries)
      for (size_t i = 0; i < t.run.trail.size() && (t.end < 0 || i <= static_cast<size_t>(t.end));
           i++)
        grow(t.run.trail[i]);
    lo = {lo.x - kPad, lo.y - kPad, lo.z - 256};
    hi = {hi.x + kPad, hi.y + kPad, hi.z + 64};
    const float span = std::max(hi.x - lo.x, hi.y - lo.y);
    const float scale = static_cast<float>(kCell - 20) / span;  // pixels a unit
    gl::Picture cell = gl::create_picture(kCell, kCell, {245, 245, 240, 255});
    const auto px = [&](float x) { return 10 + (x - lo.x) * scale; };
    const auto py = [&](float y) { return 10 + (lo.y + span - y) * scale; };
    for (int y = 10; y < kCell - 10; y++)
      for (int x = 10; x < kCell - 10; x++) {
        const float wx = lo.x + (static_cast<float>(x - 10) + 0.5f) / scale;
        const float wy = lo.y + span - (static_cast<float>(y - 10) + 0.5f) / scale;
        const sh::HullTrace tr = sh::trace_hull(hull, {wx, wy, hi.z}, {wx, wy, lo.z});
        if (tr.startsolid) {
          gl::set_pixel(cell, x, y, {70, 74, 72, 255});
          continue;
        }
        const float f = tr.fraction >= 1 ? 0 : (tr.endpos.z - lo.z) / (hi.z - lo.z);
        const auto v = static_cast<uint8_t>(120 + 125 * std::clamp(f, 0.0f, 1.0f));
        gl::set_pixel(cell, x, y, {static_cast<uint8_t>(v - 20), v, static_cast<uint8_t>(v - 35), 255});
      }
    for (const Try& t : sec.tries) {
      const gl::Rgba c = t.through ? gl::Rgba{30, 150, 60, 255}
                         : t.failed ? gl::Rgba{210, 40, 40, 255}
                                    : gl::Rgba{120, 120, 120, 255};
      const size_t n = t.end >= 0 ? static_cast<size_t>(t.end) + 1 : t.run.trail.size();
      for (size_t i = 1; i < n; i++)
        gl::draw_line(cell, px(t.run.trail[i - 1].x), py(t.run.trail[i - 1].y),
                      px(t.run.trail[i].x), py(t.run.trail[i].y), c, 2);
      for (size_t i = 1; i < n; i++)
        if (takes_off(t.run, i))
          gl::fill_disc(cell, px(t.run.trail[i].x), py(t.run.trail[i].y), 3, {40, 80, 210, 255});
      if (t.failed) {
        const sh::Vec3f f = t.run.trail[static_cast<size_t>(t.end)];
        gl::draw_ring(cell, px(f.x), py(f.y), 8, {210, 40, 40, 255});
        gl::draw_ring(cell, px(f.x), py(f.y), 9, {210, 40, 40, 255});
      }
    }
    gl::draw_ring(cell, px(sec.start.x), py(sec.start.y), 7, {0, 0, 0, 255});
    gl::fill_disc(cell, px(sec.exit.x), py(sec.exit.y), 5, {0, 0, 0, 255});
    const Try* last = sec.tries.empty() ? nullptr : &sec.tries.back();
    std::string label = std::to_string(s + 1) + " " + sec.own + ">" + sec.next;
    label += !sec.planned          ? " NO ROUTE"
             : last && last->through ? " THROUGH ON " + std::to_string(sec.tries.size())
                                     : " NOT THROUGH";
    gl::fill_rect(cell, 0, 0, kCell, 12, {255, 255, 255, 230});
    gl::draw_text(cell, 3, 2, label, {0, 0, 0, 255});
    gl::blit(sheet, cell, static_cast<int>(s % kSheetColumns) * kCell,
             static_cast<int>(s / kSheetColumns) * kCell);
  }
  lab.shot({"sections-" + name, name + ": EVERY SECTION FROM ABOVE",
            "Each section of the course to its own scale, the floor shaded by height (dark: solid). "
            "Every try's trail: green got through, red fell into a teleporter back to the start, "
            "grey ran out of time; blue dots the takeoffs, red rings the falls; the black ring "
            "the section's start, the black dot the point in its exit the route runs to.",
            "where the red trails leave the green one: the jump the bot does not make",
            {{"map", name}, {"tries", lab.arg("tries")}, {"features", lab.arg("features")}}},
           sheet);

  // The speed along the course: each section's last try, one after another.
  gl::LineChart ch;
  ch.frame = {name + ": the bot's speed along the course", "each section's last try, back to back",
              "t (s, the course's clock)", "speed across (u/s)"};
  gl::Trace sp{"speed", {}, {}};
  double clock = 0;
  for (size_t s = 0; s < out.sections.size(); s++) {
    const Section& sec = out.sections[s];
    if (sec.tries.empty()) continue;
    ch.marks.push_back({clock, std::to_string(s + 1)});
    const Try& t = sec.tries.back();
    const size_t n = t.end >= 0 ? static_cast<size_t>(t.end) + 1 : t.run.trail.size();
    for (size_t i = 0; i < n; i += 4) {
      const sh::Vec3f v = t.run.velocity[i];
      sp.x.push_back(clock + 0.013 * static_cast<double>(i));
      sp.y.push_back(std::sqrt(v.x * v.x + v.y * v.y));
    }
    clock += 0.013 * static_cast<double>(n);
  }
  ch.traces.push_back(std::move(sp));
  ch.spikes = false;
  lab.shot({"speed-" + name, name + ": SPEED ALONG THE COURSE",
            "The bot's speed across, each section's last try back to back; the marks are where "
            "each section starts (its number).",
            "speed built and kept through a section, not lost at each jump", {{"map", name}}},
           gl::to_picture(gl::line_chart(ch).plot));
}

// The last try's takeoff nearest the pit's lip (within 96 units): its speed across; "-" none.
std::string lip_speed(const Section& sec) {
  if (sec.pit <= 0 || sec.tries.empty()) return "-";
  const bt::RunResult& r = sec.tries.back().run;
  float best = 96, v = -1;
  for (size_t i = 1; i < r.trail.size(); i++) {
    if (!takes_off(r, i)) continue;
    const float d = std::hypot(r.trail[i].x - sec.lip.x, r.trail[i].y - sec.lip.y);
    if (d < best) best = d, v = std::hypot(r.velocity[i - 1].x, r.velocity[i - 1].y);
  }
  return v < 0 ? "-" : to_fixed(v, 0);
}

void run(gl::Lab& lab) {
  std::string error;
  pg::Progs progs;
  if (!cl::baseline_progs(progs, error)) {
    lab.fail("se-trick: the baseline mod: " + error,
             "next: run tests/shooters_edge_baseline_mod_test.cpp");
    return;
  }
  if (lab.arg("basedir").empty()) {
    lab.note("se-trick: no --basedir: the trick maps are the player's own; nothing run");
    return;
  }
  cl::SearchPath files;
  if (!cl::open_game_dir(lab.arg("basedir"), lab.arg("game"), files, error)) {
    lab.fail(error, "Pass --basedir=<the folder that holds the game folder>. Skill: bots.");
    return;
  }
  gl::LabTable sections{"each section: its exit, the route the nav plans (its links and jumps, "
                        "its proven time), the try that got through and the time with the fails'",
                        {"map", "#", "from > to", "start (x y feet)", "route", "reached nearest (left)", "links", "jumps",
                         "proven s", "pit", "pit jump u/s", "lip takeoff u/s", "tries",
                         "through", "time s",
                         "fails' s"},
                        {}};
  gl::LabTable fails{"every fail: where it fell, the last takeoff before it (place, speed, the "
                     "pilot's reason) and the reason at the fall",
                     {"map", "#", "try", "fell at", "after s", "takeoff at", "takeoff u/s",
                      "takeoff because", "because at the fall"},
                     {}};
  gl::LabTable stalls{"every try that neither got through nor fell: how long it ran, where it "
                      "ended, the pilot's mode and reason at the end, the routes it planned",
                      {"map", "#", "try", "ran s", "ended at", "mode", "because", "plans",
                       "arrived"},
                      {}};
  // THE TRACE (`--trace=<tsv>`): every command of every try, a row.
  std::ofstream trace;
  if (!lab.arg("trace").empty()) {
    std::error_code ec;
    const auto dir = std::filesystem::path(lab.arg("trace")).parent_path();
    if (!dir.empty()) std::filesystem::create_directories(dir, ec);
    trace.open(lab.arg("trace"));
    trace << "map\tsection\ttry\tt\tx\ty\tz\tspeed\tvz\tmode\tlink\tyaw\tforward\tside\tbecause\n";
  }
  std::istringstream list(lab.arg("maps"));
  std::string name;
  while (std::getline(list, name, ',')) {
    std::vector<uint8_t> store;
    const std::span<const uint8_t> f = cl::find_file(files, "maps/" + name + ".bsp", store);
    if (f.empty()) {
      lab.fail("se-trick: no maps/" + name + ".bsp in the game folder",
               "next: fetch the trick maps into <basedir>/<game>/maps (.bots/local.md)");
      continue;
    }
    Course c;
    run_course_map(lab, f, name, progs, c, trace.is_open() ? &trace : nullptr);
    if (!c.error.empty()) {
      lab.fail("se-trick: " + name + ": " + c.error, "next: run se-reach --maps=" + name);
      continue;
    }
    int planned = 0, through = 0, first = 0, fell = 0;
    double time = 0;
    for (size_t s = 0; s < c.sections.size(); s++) {
      const Section& sec = c.sections[s];
      planned += sec.planned;
      double lost = 0;
      for (size_t k = 0; k < sec.tries.size(); k++) {
        const Try& t = sec.tries[k];
        if (t.through) continue;
        lost += t.time_s;
        if (!t.failed) {
          const bt::RunResult& r = t.run;
          stalls.rows.push_back(
              {name, std::to_string(s + 1), std::to_string(k + 1), to_fixed(t.time_s, 2),
               r.trail.empty() ? "-" : v3(r.trail.back()),
               r.modes.empty() ? "-" : sh::pilot_mode_name(static_cast<sh::PilotMode>(r.modes.back())),
               !r.because.empty() && r.because.back() != nullptr ? r.because.back() : "-",
               std::to_string(r.plans.size()), r.arrived ? "yes" : "no"});
          continue;
        }
        fell++;
        const bt::RunResult& r = t.run;
        const auto at = static_cast<size_t>(t.end);
        std::string off = "-", speed = "-", why_off = "-";
        if (t.takeoff >= 0) {
          const auto j = static_cast<size_t>(t.takeoff);
          off = v3(r.trail[j]);
          speed = to_fixed(std::hypot(r.velocity[j - 1].x, r.velocity[j - 1].y), 0);
          why_off = r.because[j - 1] != nullptr ? r.because[j - 1] : "-";
        }
        fails.rows.push_back({name, std::to_string(s + 1), std::to_string(k + 1), v3(r.trail[at]),
                              to_fixed(t.time_s, 2), off, speed, why_off,
                              r.because[at] != nullptr ? r.because[at] : "-"});
      }
      const bool ok = !sec.tries.empty() && sec.tries.back().through;
      through += ok;
      first += ok && sec.tries.size() == 1;
      if (ok) time += lost + sec.tries.back().time_s;
      sections.rows.push_back(
          {name, std::to_string(s + 1), (sec.own.empty() ? "spawn" : sec.own) + " > " + sec.next,
           v3(sec.start), sec.planned ? "planned" : "no route",
           sec.planned ? "-"
                       : v3(sec.reach) + " (" + to_fixed(sec.reach_left, 0) + ", " +
                             std::to_string(sec.reach_n) + " nodes)",
           std::to_string(sec.links),
           std::to_string(sec.jumps), sec.planned ? to_fixed(sec.proven_s, 2) : "-",
           sec.pit > 0 ? to_fixed(sec.pit, 0) : "-", sec.gaps > 0 ? std::to_string(sec.gaps) : "-", lip_speed(sec),
           std::to_string(sec.tries.size()), ok ? "yes" : "no",
           ok ? to_fixed(lost + sec.tries.back().time_s, 2) : "-", to_fixed(lost, 2)});
    }
    const std::string m = "trick." + name + ".";
    lab.metric(m + "sections", static_cast<double>(c.sections.size()), "count");
    lab.metric(m + "planned", planned, "count");
    lab.metric(m + "through", through, "count");
    lab.metric(m + "first-try", first, "count");
    lab.metric(m + "fails", fell, "count");
    lab.metric(m + "time", time, "s");
    lab.metric(m + "pit-crossings", c.pit_pairs, "count");
    lab.metric(m + "pit-jumps", c.pit_jumps, "count");
    lab.metric(m + "pit-links-closed", c.closed, "count");
    lab.metric(m + "brink-links", c.brinks, "count");
  }
  lab.table(sections);
  lab.table(fails);
  lab.table(stalls);
}

const gl::LabSpec kSpec{
    "se-trick",
    gl::LabKind::drive,
    "game:shooters_edge",
    "one bot on a trick map's course (games/shooters_edge/labs/se_trick_lab.cpp, "
    "bots/pilot_run.hpp)",
    "bots",
    "Can one bot run a trick map from start to finish, how fast, and where does it fail?",
    {
        {"basedir", "", "a folder holding the game folder (the trick maps in its maps)"},
        {"game", "id1", "the game folder under --basedir"},
        {"maps", "ztrain", "the trick maps, comma separated: ztrain, zjumps, ztricks, hoppa2"},
        {"tries", "5", "each section tried at most this many times (a new seed each)"},
        {"limit", "20", "each try given this long, s"},
        {"features", "", "the bot's switches over an expert's preset, as se-team's --features"},
        {"trace", "", "every command of every try to this TSV (map, section, try, t, feet, speed, "
                      "mode, link, view, keys, the pilot's reason)"},
        {"harm", "0", "1: the pits' floors marked as harm, their lips' links brinks (tried: a hop's stretch then ends short of every lip, ztrain 10 -> 3 through)"},
        {"spacing", "32", "the nav's grid, units (16: a trick map's narrow platforms get nodes)"},
        {"pits", "triggers", "the pits' jumps: triggers (every fail trigger's rim, and the line) or "
                             "line (only the straight way of a section no route crosses)"},
    },
    "",
    run,
};
GE_LAB(kSpec);

}  // namespace
