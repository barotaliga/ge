// LAB `se-reach`: CAN A BOT GET TO EVERY ITEM, on every map? For each map of `--maps` (from
// `--basedir`'s game folder; without one, the fixture halls) the bots' own nav is built
// (bots/map_nav.hpp: movers, teleporters, water closed) and every item the map places is asked two
// things: does a route lead to it from the map's main part (the most any start spot reaches), and
// does one lead from it back there (an item in a pocket a player cannot leave is a trap)? An item
// fails either way is listed with its place, its nav node and how far that node stands from it.
//
// The yardstick for the nav: "any place to any item is a path". Deterministic: the digest holds
// the counts.
//
// Two probes of the first map, for finding where the nav falls short: `--route="x y z > x y z"`
// tables the quickest route between the nodes nearest two points (feet), node by node with the
// link onto each, its kind, time and jump speed; `--floors="x0 y0 x1 y1 [step]"` tables every
// floor a standing player finds straight down along a line beside the nav's nodes in that grid
// column (a tread with no node is one the grid missed). The `se-spot` lab draws the same place.
// `--escapes=fly` flies every rocket out of a pit's liquid (shooterbots/escape.hpp) with the pilot
// (bots/pilot_run.hpp `run_route`) from the liquid node at its takeoff, and tables which fired and
// which landed: a staged run of a rare moment (a bot fallen into lava) a match seldom shows.
// `--gaps=fly` runs every gap trick (shooterbots/nav_gaps.hpp) with an expert fighter's pilot
// (`--gap-features`) from points 160-640 units behind its takeoff, walking in at people's 180, and
// tables whether the route planned the gap, curved into it, jumped it (at what speed), landed or
// gave it up: the gap jump staged, where a match would need minutes to show one.
// `--rockets=fly` (`--rocket-near='x y r'`) runs every rocket trick of the first map the same way,
// the launcher in hand, and tables whether it was planned, fired (at what speed), landed or fell
// short into a pit: a trick bots shoot short of found in seconds.
//
// ge:summary Lab: every item of every map asked: a route to it from the spawns, and one back
// ge:keywords lab se-reach reachability coverage nav-gaps items pockets unreachable route maps
// ge:keywords dm1 dm2 dm3 dm4 dm5 dm6 e1m2 route-probe floors-probe nav-debug escapes-fly lava
// ge:keywords gaps-fly gap-jump-staged rockets-fly rocket-jump-staged
// ge:skill bots labs
// ge:test tests/shooters_edge_team_test.cpp
#include <array>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "brush/write.hpp"
#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "core/num_text.hpp"
#include "lab/lab.hpp"
#include "lab/raster.hpp"
#include "progs/entity_text.hpp"
#include "shooterbots/nav.hpp"
#include "shooterbots/nav_field.hpp"
#include "shooters_edge/bots/buttons.hpp"
#include "shooters_edge/bots/fighter.hpp"
#include "shooters_edge/bots/knowledge.hpp"
#include "shooters_edge/bots/map_nav.hpp"
#include "shooters_edge/bots/pilot_run.hpp"
#include "shooters_edge/bots/quake_maps.hpp"
#include "shooters_edge/client/files.hpp"
#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/labs/nav_top.hpp"
#include "shooters_edge/net/qw_team.hpp"
#include "shooters_edge/server/qc_server.hpp"
#include "shooters_edge/server/rules_runner.hpp"

namespace {

namespace gl = ge::lab;
namespace sh = ge::shooter;
namespace sb = ge::shooterbots;
namespace pg = ge::progs;
namespace la = shooters_edge::labs;
namespace cl = shooters_edge::client;
namespace sv = shooters_edge::server;
namespace bt = shooters_edge::bots;
namespace cn = shooters_edge::net;
using ge::core::num::to_fixed;

constexpr float kEscapeLimit_s = 8;  // --escapes: a flight given this long to land
// --gaps=fly: each gap trick run up to from these far back of its takeoff, units, at people's
// walk into it (180 units/s), on these seeds, given this long.
constexpr float kGapBack[] = {160, 320, 480, 640};
constexpr float kGapWalk = 180;
constexpr uint32_t kGapSeeds = 3;
constexpr float kGapLimit_s = 6;
// --walk: the runs' start speeds (units/s), their headings off the way to the first node
// (degrees), the seeds, the time each is given.
constexpr float kWalkSpeeds[] = {250, 320, 400};
constexpr float kWalkTurns[] = {-45, -20, 0, 20, 45};
constexpr uint32_t kWalkSeeds = 4;
constexpr float kWalkLimit_s = 6;

struct Gap {
  std::string item, why;
  sh::Vec3f at;
  float node_off = 0;
};

struct MoverRow {
  int ent = 0;
  bool plat = false;
  sh::Vec3f lo, hi, travel;
  int rides = 0;  // ride links up off it
};

struct MapReach {
  std::string name, error;
  int items = 0, reached = 0, back = 0, by_rocket = 0;
  int64_t rocket_links = 0, flips = 0;  // the graph's rocket jumps, and those played as the flip
  int64_t nodes = 0, swim_nodes = 0, swim_links = 0, harm_nodes = 0;
  std::vector<Gap> gaps;
  std::vector<MoverRow> movers;
  std::vector<std::vector<std::string>> tricks;  // the tricks table's rows
  int64_t tricks_held = 0, grenade_links = 0;
  std::vector<std::vector<std::string>> speed_jumps;  // the gap tricks table's rows
  int64_t speed_jumps_held = 0, speed_jumps_run = 0;
  int64_t risky = 0, risky_removed = 0;  // jumps over lava or slime, and those removed
  int64_t escapes = 0, escape_links = 0, escape_proofs = 0;  // rockets out of a pit's liquid
  int64_t escapes_flown = 0, escapes_fired = 0, escapes_landed = 0;  // --escapes: the pilot's
  std::vector<std::vector<std::string>> escape_rows;
  std::vector<std::vector<std::string>> gap_rows;  // --gaps=fly: each trick run up to
  int64_t gap_runs = 0, gap_planned = 0, gap_jumped = 0, gap_landed = 0, gap_refused = 0;
  std::vector<std::vector<std::string>> route_rows, floor_rows;  // --route's and --floors' rows
  std::vector<std::vector<std::string>> field_rows, field_ways, errand_rows;  // --field's
  gl::Picture field_map;                              // --field-map: the ways to every item drawn
  std::vector<std::vector<std::string>> field_items;  // and tabled
  std::vector<std::vector<std::string>> script_rows;  // the scripted routes, held or refused
  std::vector<std::vector<std::string>> walk_rows;    // --walk: each variant's runs
  std::vector<std::vector<std::string>> rocket_rows;  // --rockets=fly: each trick run up to
  int64_t rocket_runs = 0, rocket_planned = 0, rocket_fired = 0, rocket_landed = 0,
          rocket_short = 0;
};

// The probes asked of the first map: a route between two points, each node and the link onto
// it; and the floors down the grid's columns along a line (where the nav samples a stair).
struct Probe {
  bool route = false, floors = false;
  bool field = false;      // --field: the way off the goal's field (shooterbots/nav_field.hpp)
  bool field_map = false;  // --field-map: from `map_from` the way to every item, drawn
  sh::Vec3f map_from;
  int map_jumper = 1;  // --field-jumper: 0 on foot, 1 the gap tricks, 2 rocket jumps too
  sh::Vec3f field_from, field_to;
  sh::Vec3f from, to;  // --route's ends
  float wet = 1;       // --route-wet: every link into a wet node priced this many times its proof
  float fx0 = 0, fy0 = 0, fx1 = 0, fy1 = 0, fstep = 8;  // --floors' line and step
  // --walk: a route walked from `walk_from` to `walk_to` (origins) by an expert's pilot through an
  // expert's keyboard, already running at each of `kWalkSpeeds` on each of `kWalkTurns` off the way
  // to its first node, on `kWalkSeeds` seeds, under each variant's switches (name, features).
  bool walk = false;
  sh::Vec3f walk_from, walk_to;
  std::vector<std::pair<std::string, sb::Features>> walk_variants;
  float walk_view_lag_s = 0.3f;  // --walk-view-lag: the view trails the run's heading (s)
  // --rockets=fly: each rocket trick whose takeoff lies within `rocket_r` of (`rocket_x`,
  // `rocket_y`) (--rocket-near) run up to by an expert's pilot with `rocket_features`.
  bool rockets = false;
  float rocket_x = 0, rocket_y = 0, rocket_r = 1e9f;
  sb::Features rocket_features = sb::features_all();
};

bool vec_of(const char* s, sh::Vec3f& out) {
  if (!s) return false;
  std::istringstream in(s);
  return static_cast<bool>(in >> out.x >> out.y >> out.z);
}

// Every floor a standing player finds straight down at (x, y), top first.
void floors_at(const sh::MoveWorld& w, float x, float y, float top, float bottom,
               std::vector<float>& out) {
  out.clear();
  float z = top;
  while (z > bottom) {
    if (!sh::box_free(w, {x, y, z})) {
      z -= 4;
      continue;
    }
    const sh::HullTrace tr = sh::player_trace(w, {x, y, z}, {x, y, bottom});
    if (tr.startsolid || tr.fraction == 1) return;
    if (tr.plane.normal.z >= 0.7f) out.push_back(tr.endpos.z - 24);  // the feet
    z = tr.endpos.z - 57;
  }
}

// --walk: the runs of one variant; each run's lava or slime entered (its feet in it, any
// command), arrival and time.
void walk_runs(const bt::MapNav& nav, const Probe& pr, const std::string& name,
               const sb::Features& f, MapReach& out) {
  const sb::NavGraph& g = nav.graph;
  const int32_t a = sb::nav_nearest(g, pr.walk_from), b = sb::nav_nearest(g, pr.walk_to);
  if (a < 0 || b < 0) return;
  ge::ai::Search rs;
  const sb::NavRoute route = sb::nav_route(g, a, b, rs);
  if (route.nodes.size() < 2) return;
  const sh::Vec3f from = g.nodes[static_cast<size_t>(a)].origin;
  const sh::Vec3f first = g.nodes[static_cast<size_t>(route.nodes[1])].origin;
  const double way = ge::core::libm::atan2(static_cast<double>(first.y - from.y),
                                           static_cast<double>(first.x - from.x));
  const sb::Traits traits = sb::traits_of(sb::AimSkill::expert, sb::Temper::balanced);
  sb::PilotHands hands = sb::PilotHands::none;
  sb::PilotSettings ps = bt::fighter_pilot(f, traits, sb::AimSkill::expert, hands);
  const sb::Features* keys = f[sb::Feature::keys] ? &f : nullptr;
  for (const float speed : kWalkSpeeds) {
    int runs = 0, fell = 0, arrived = 0;
    float time = 0;
    std::string where;
    for (const float turn : kWalkTurns) {
      const double h = way + static_cast<double>(turn) * (ge::core::kPi / 180.0);
      const sh::Vec3f v{static_cast<float>(ge::core::libm::cos(h)) * speed,
                        static_cast<float>(ge::core::libm::sin(h)) * speed, 0};
      for (uint32_t seed = 1; seed <= kWalkSeeds; seed++) {
        const bt::RunResult r =
            bt::run_route(nav.world, g, from, pr.walk_to, hands, seed, ps, kWalkLimit_s, v, keys,
                          traits[sb::Trait::moves], pr.walk_view_lag_s);
        runs++;
        bool in = false;
        for (const sh::Vec3f o : r.trail) {
          const int c = sh::water_contents(nav.world, {o.x, o.y, o.z - 22});
          if (c == sh::kContentsLava || c == sh::kContentsSlime) {
            in = true;
            if (where.size() < 120)
              where += (where.empty() ? "" : ", ") + to_fixed(o.x, 0) + " " + to_fixed(o.y, 0);
            break;
          }
        }
        fell += in;
        if (r.arrived && !in) arrived++, time += r.time_s;
      }
    }
    out.walk_rows.push_back({name, to_fixed(speed, 0), std::to_string(runs), std::to_string(fell),
                             std::to_string(arrived),
                             arrived > 0 ? to_fixed(time / static_cast<float>(arrived), 2) : "-",
                             where.empty() ? "-" : where});
  }
}

// --rockets=fly: each rocket trick (a link of `NavKind::rocket`: the maps' tricks people fly,
// bots/quake_maps.hpp) run up to by an expert fighter's pilot with the launcher in hand, from
// the floor's node nearest each point `kGapBack` behind its takeoff, walking in at 180, through an
// expert's keyboard under `keys`: planned through the link, fired, the speed at the shot, landed
// on its node, or SHORT (the run after the shot sank 48 under both ends: into a pit). A trick
// people fly at a run the bots shoot short of is found here in seconds, not in matches.
void rocket_runs(const bt::MapNav& nav, const Probe& pr, const std::string& name,
                 std::ostream* trace, MapReach& out) {
  const sb::NavGraph& g = nav.graph;
  int32_t trick = 0;
  for (size_t a = 0; a < g.nodes.size(); a++) {
    const sb::NavNode& n = g.nodes[a];
    for (int32_t x = 0; x < n.links; x++) {
      const sb::NavLink& l = g.links[static_cast<size_t>(n.first_link) + static_cast<size_t>(x)];
      if (l.kind != sb::NavKind::rocket || l.escape >= 0) continue;  // not a pit's way out
      trick++;
      const sh::Vec3f S = n.origin, T = g.nodes[static_cast<size_t>(l.to)].origin;
      const float nx = S.x - pr.rocket_x, ny = S.y - pr.rocket_y;
      if (nx * nx + ny * ny > pr.rocket_r * pr.rocket_r) continue;
      sh::Vec3f dir{T.x - S.x, T.y - S.y, 0};
      const float len = std::sqrt(dir.x * dir.x + dir.y * dir.y);
      if (len < 1) continue;
      dir = dir * (1 / len);
      for (const float back : kGapBack) {
        const sh::Vec3f want{S.x - dir.x * back, S.y - dir.y * back, S.z};
        int32_t best = -1;
        float best_d = 96;
        for (size_t i = 0; i < g.nodes.size(); i++) {
          const sh::Vec3f o = g.nodes[i].origin;
          if (o.z > S.z + 20 || o.z < S.z - 20 || (i < nav.wet.size() && nav.wet[i] != 0)) continue;
          const float d =
              std::sqrt((o.x - want.x) * (o.x - want.x) + (o.y - want.y) * (o.y - want.y));
          if (d < best_d) best_d = d, best = static_cast<int32_t>(i);
        }
        if (best < 0) continue;
        const sh::Vec3f from = g.nodes[static_cast<size_t>(best)].origin;
        sh::Vec3f v{S.x - from.x, S.y - from.y, 0};
        const float vl = std::sqrt(v.x * v.x + v.y * v.y);
        v = vl > 1 ? v * (kGapWalk / vl) : sh::Vec3f{};
        const sb::Traits traits = sb::traits_of(sb::AimSkill::expert, sb::Temper::balanced);
        sb::PilotHands hands = sb::PilotHands::none;  // (fighter_pilot sets the expert's)
        sb::PilotSettings ps =
            bt::fighter_pilot(pr.rocket_features, traits, sb::AimSkill::expert, hands);
        ps.jumper.rockets = true;  // the launcher in hand, a rocket and the health for the blast
        ps.jumper.level = l.level;
        const sb::Features* keys =
            pr.rocket_features[sb::Feature::keys] ? &pr.rocket_features : nullptr;
        int planned = 0, fired = 0, landed = 0, shrt = 0;
        float speed = 0;
        for (uint32_t seed = 1; seed <= kGapSeeds; seed++) {
          const bt::RunResult r = bt::run_route(nav.world, g, from, T, hands, seed, ps, kGapLimit_s,
                                                v, keys, traits[sb::Trait::moves]);
          bool through = false;
          for (size_t i = 0; i + 1 < r.route.size(); i++)
            through = through || (r.route[i] == static_cast<int32_t>(a) && r.route[i + 1] == l.to);
          planned += through;
          // The shot's command: the first whose clock passes the first rocket's (`RunResult`).
          size_t at = r.trail.size();
          if (!r.rockets.empty())
            at = std::min(r.trail.size() - 1,
                          static_cast<size_t>(r.rockets.front().t0 * 1000 / bt::kPilotMsec));
          const bool shot = r.stats.rocket_jumps > 0 && at < r.trail.size();
          fired += shot;
          bool sank = false;
          if (shot) {
            const sh::Vec3f vv = r.velocity[at];
            speed += std::sqrt(vv.x * vv.x + vv.y * vv.y);
            for (size_t i = at; i < r.trail.size(); i++)
              sank = sank || r.trail[i].z < std::min(S.z, T.z) - 48;
          }
          shrt += sank;
          landed += shot && !sank && r.arrived;
          if (trace != nullptr && seed == 1) {
            for (size_t i = 0; i < r.trail.size(); i++) {
              const sh::Vec3f o = r.trail[i], vv = r.velocity[i];
              *trace << "r" << trick << '\t' << back << '\t'
                     << to_fixed(0.013 * static_cast<double>(i + 1), 3) << '\t' << to_fixed(o.x, 0)
                     << '\t' << to_fixed(o.y, 0) << '\t' << to_fixed(o.z - 24, 0) << '\t'
                     << to_fixed(std::sqrt(vv.x * vv.x + vv.y * vv.y), 0) << '\t'
                     << to_fixed(vv.z, 0) << '\t'
                     << sb::pilot_mode_name(static_cast<sb::PilotMode>(r.modes[i])) << '\t'
                     << r.links[i] << '\t' << to_fixed(r.yaw[i], 1) << '\t'
                     << to_fixed(r.forward[i], 2) << '\t' << to_fixed(r.side[i], 2) << '\t'
                     << r.because[i] << '\n';
            }
          }
          out.rocket_runs++;
        }
        out.rocket_planned += planned, out.rocket_fired += fired, out.rocket_landed += landed;
        out.rocket_short += shrt;
        auto ft = [](sh::Vec3f o) {
          return to_fixed(o.x, 0) + " " + to_fixed(o.y, 0) + " " + to_fixed(o.z - 24, 0);
        };
        out.rocket_rows.push_back({name, "r" + std::to_string(trick), ft(S), ft(T),
                                   l.back ? "flip" : "feet", to_fixed(l.speed, 0),
                                   to_fixed(l.pitch, 0), std::to_string(l.level), to_fixed(back, 0),
                                   std::to_string(planned), std::to_string(fired),
                                   fired > 0 ? to_fixed(speed / static_cast<float>(fired), 0) : "-",
                                   std::to_string(landed), std::to_string(shrt)});
      }
    }
  }
}

// --field: the map's buttons as errands (bots/buttons.hpp `field_errands`), the field to the node
// nearest `field_to` for each jumper (on foot, with the gap tricks, with rocket jumps), its time
// from the node nearest `field_from` and the errand its way starts with; the first way that has
// one node by node, the press marked.
void field_probe(const bt::MapNav& nav, const ge::shooterbots::BotView& s,
                 const std::string& entities, const Probe& pr, MapReach& out) {
  const sb::NavGraph& g = nav.graph;
  bt::Fighter f;
  f.arsenal = &bt::first_arsenal();
  bt::buttons_init(f, g, s, entities.c_str());
  const std::vector<sb::NavErrand> errands = bt::field_errands(f, g, s);
  auto at = [&](int32_t n) {
    if (n < 0) return std::string("-");
    const sh::Vec3f o = g.nodes[static_cast<size_t>(n)].origin;
    return to_fixed(o.x, 0) + " " + to_fixed(o.y, 0) + " " + to_fixed(o.z - 24, 0);
  };
  for (size_t k = 0; k < errands.size(); k++) {
    const sb::NavErrand& e = errands[k];
    std::string doors;
    for (const int32_t d : e.doors)
      if (d >= 0) doors += (doors.empty() ? "" : ",") + std::to_string(d);
    std::string all;
    for (const int d : f.button_spots[k].doors) all += (all.empty() ? "" : ",") + std::to_string(d);
    out.errand_rows.push_back(
        {std::to_string(k),
         to_fixed(f.button_spots[k].at.x, 0) + " " + to_fixed(f.button_spots[k].at.y, 0) + " " +
             to_fixed(f.button_spots[k].at.z, 0),
         f.button_spots[k].shoot ? "shot" : "touch", at(e.button), all.empty() ? "-" : all,
         doors.empty() ? "-" : doors, to_fixed(e.open_s, 1), to_fixed(e.press_s, 1),
         std::to_string(e.also.size() + (e.button >= 0 ? 1 : 0))});
  }
  sb::NavFields fs;
  sb::nav_fields_init(fs, g, errands);
  const int32_t a = sb::nav_nearest(g, pr.field_from), b = sb::nav_nearest(g, pr.field_to);
  sb::NavJumper foot, gaps, rockets;
  gaps.gap_build_s = bt::kGapBuild_s;
  rockets = gaps;
  rockets.rockets = true;
  bool shown = false;
  for (const auto& [name, j] : {std::pair<const char*, sb::NavJumper>{"on foot", foot},
                                {"gap tricks", gaps},
                                {"rocket jumps", rockets}}) {
    const sb::NavField& fl = sb::nav_field(fs, b, j);
    sb::NavFieldWay w;
    const bool ok = sb::nav_field_way(fs, fl, a, w);
    out.field_rows.push_back(
        {name, at(a), at(b), ok ? to_fixed(w.time_s, 2) : "none",
         ok && w.errand >= 0 ? std::to_string(w.errand) + " at " + at(w.nodes[w.press_at]) : "-",
         ok ? std::to_string(w.nodes.size()) : "-"});
    if (!ok || shown || (w.errand < 0 && name != std::string("rocket jumps"))) continue;
    shown = true;
    for (size_t k = 0; k < w.nodes.size(); k++)
      out.field_ways.push_back(
          {name, std::to_string(k), std::to_string(w.nodes[k]), at(w.nodes[k]),
           w.errand >= 0 && k == w.press_at ? "press " + std::to_string(w.errand) : ""});
  }
}

// --field-map: the map from above (nav_top.hpp, scaled to fit kFieldMapPx) with the way off each
// item's field from the node nearest `map_from` for the jumper asked: a key item's in its own
// colour and thick, the rest thin; a teleporter's hop not drawn but its two ends ringed; the
// node a way presses a button at ringed in magenta with the button's number; each item labelled
// with its call and its time, or crossed out grey when no way leads there.
void field_map(const bt::MapNav& nav, const ge::shooterbots::BotView& s,
               const std::string& entities, const Probe& pr, MapReach& out) {
  constexpr int kFieldMapPx = 1600;
  const sb::NavGraph& g = nav.graph;
  bt::Fighter f;
  f.arsenal = &bt::first_arsenal();
  bt::buttons_init(f, g, s, entities.c_str());
  sb::NavFields fs;
  sb::nav_fields_init(fs, g, bt::field_errands(f, g, s));
  sb::NavJumper j;
  if (pr.map_jumper >= 1) j.gap_build_s = bt::kGapBuild_s;
  if (pr.map_jumper >= 2) j.rockets = true;
  const int32_t start = sb::nav_nearest(g, pr.map_from);
  la::TopView v{g.lo, g.hi};
  // Every roof stepped through (nav_top.hpp `floor_below`): the highest floor under it shows.
  const gl::Picture floor = la::floor_picture(nav.world, v, v.hi.z - 1);
  const double k =
      std::min(1.0, kFieldMapPx / static_cast<double>(std::max(floor.width, floor.height)));
  gl::Picture p =
      gl::resized(floor, static_cast<int>(floor.width * k), static_cast<int>(floor.height * k));
  const auto sx = [&](float x) { return v.px(x) * k; };
  const auto sy = [&](float y) { return v.py(y) * k; };
  struct Item {
    std::string call;
    sh::Vec3f at;
    int32_t node;
    bool key;
  };
  std::vector<Item> items;
  std::vector<pg::EntityBlock> blocks;
  std::string error;
  if (!pg::parse_entity_text(entities.c_str(), blocks, error)) return;
  for (const pg::EntityBlock& b : blocks) {
    const char* cn = pg::block_value(b, "classname");
    const char* flags = pg::block_value(b, "spawnflags");
    bt::ItemSpot it;
    sh::Vec3f at;
    if (!cn || !bt::item_of(cn, flags ? std::atoi(flags) : 0, it, bt::first_arsenal()) ||
        !vec_of(pg::block_value(b, "origin"), at))
      continue;
    const std::string call = bt::item_call(it);
    const bool key = call == "ra" || call == "ya" || call == "rl" || call == "quad" ||
                     call == "pent" || call == "mh" || call == "ring" || call == "lg" ||
                     call == "gl";
    items.push_back({call.empty() ? std::string(cn) : call, at,
                     sb::nav_nearest(g, {at.x, at.y, at.z + 24}), key});
  }
  const auto colour = [](const std::string& c) -> gl::Rgba {
    if (c == "ra") return {210, 30, 30, 255};
    if (c == "ya") return {230, 170, 0, 255};
    if (c == "rl") return {140, 70, 20, 255};
    if (c == "quad") return {40, 90, 230, 255};
    if (c == "mh") return {30, 160, 60, 255};
    if (c == "gl" || c == "lg") return {120, 60, 170, 255};
    return {90, 90, 90, 255};
  };
  // The ways first (the thin ones under the thick), the labels over them.
  std::vector<std::string> labels(items.size());
  for (int pass = 0; pass < 2; pass++)
    for (size_t i = 0; i < items.size(); i++) {
      const Item& it = items[i];
      if (it.key != (pass == 1)) continue;
      const sb::NavField& fl = sb::nav_field(fs, it.node, j);
      sb::NavFieldWay w;
      const bool ok = start >= 0 && it.node >= 0 && sb::nav_field_way(fs, fl, start, w);
      const gl::Rgba c = it.key ? colour(it.call) : gl::Rgba{120, 120, 120, 200};
      std::string press = "-";
      if (ok) {
        for (size_t n = 0; n + 1 < w.nodes.size(); n++) {
          const sh::Vec3f a = g.nodes[static_cast<size_t>(w.nodes[n])].origin;
          const sh::Vec3f b = g.nodes[static_cast<size_t>(w.nodes[n + 1])].origin;
          const sb::NavNode& na = g.nodes[static_cast<size_t>(w.nodes[n])];
          bool teleport = false;
          for (int32_t x = 0; x < na.links; x++) {
            const sb::NavLink& l =
                g.links[static_cast<size_t>(na.first_link) + static_cast<size_t>(x)];
            if (l.to == w.nodes[n + 1]) teleport = l.teleport;
          }
          if (teleport) {
            gl::draw_ring(p, sx(a.x), sy(a.y), 7, c);
            gl::draw_ring(p, sx(b.x), sy(b.y), 7, c);
            continue;
          }
          gl::draw_line(p, sx(a.x), sy(a.y), sx(b.x), sy(b.y), c, it.key ? 3 : 1);
        }
        if (w.errand >= 0) {
          const sh::Vec3f q = g.nodes[static_cast<size_t>(w.nodes[w.press_at])].origin;
          gl::draw_ring(p, sx(q.x), sy(q.y), 9, {220, 0, 200, 255});
          gl::draw_ring(p, sx(q.x), sy(q.y), 10, {220, 0, 200, 255});
          gl::draw_text(p, sx(q.x) + 12, sy(q.y) - 4, "B" + std::to_string(w.errand),
                        {220, 0, 200, 255}, 2);
          press = std::to_string(w.errand);
        }
        labels[i] = it.call + " " + to_fixed(w.time_s, 1) + "s" +
                    (w.errand >= 0 ? " B" + std::to_string(w.errand) : "");
      } else {
        labels[i] = it.call + " --";
      }
      out.field_items.push_back(
          {it.call, to_fixed(it.at.x, 0) + " " + to_fixed(it.at.y, 0) + " " + to_fixed(it.at.z, 0),
           ok ? to_fixed(w.time_s, 2) : "no way", press,
           ok ? std::to_string(w.nodes.size()) : "-"});
    }
  // Labels key items first, each moved down past any drawn before it overlaps.
  std::vector<std::array<double, 4>> taken;  // x0, y0, x1, y1 of the labels drawn
  std::vector<size_t> order;
  for (size_t i = 0; i < items.size(); i++)
    if (items[i].key) order.push_back(i);
  for (size_t i = 0; i < items.size(); i++)
    if (!items[i].key) order.push_back(i);
  for (const size_t i : order) {
    const Item& it = items[i];
    const bool none = labels[i].ends_with("--");
    const gl::Rgba c = none ? gl::Rgba{150, 150, 150, 255}
                            : (it.key ? colour(it.call) : gl::Rgba{60, 60, 60, 255});
    const double x = sx(it.at.x), y = sy(it.at.y);
    if (none) {
      gl::draw_line(p, x - 6, y - 6, x + 6, y + 6, c, 2);
      gl::draw_line(p, x - 6, y + 6, x + 6, y - 6, c, 2);
    } else {
      gl::fill_disc(p, x, y, it.key ? 6 : 3, c);
    }
    const int scale = it.key ? 2 : 1;
    const double lw = gl::text_width(labels[i], scale) + 2, lh = 8.0 * scale + 2;
    double ly = y - 2;
    for (bool moved = true; moved;) {
      moved = false;
      for (const auto& r : taken)
        if (x + 8 < r[2] && x + 8 + lw > r[0] && ly < r[3] && ly + lh > r[1]) {
          ly = r[3] + 1;
          moved = true;
        }
    }
    taken.push_back({x + 8, ly, x + 8 + lw, ly + lh});
    if (ly > y + 4) gl::draw_line(p, x + 4, y, x + 8, ly + lh / 2, c, 1);
    gl::fill_rect(p, x + 8, ly, lw, lh, {255, 255, 255, 210});
    gl::draw_text(p, x + 9, ly + 1, labels[i], c, scale);
  }
  if (start >= 0) {
    const sh::Vec3f o = g.nodes[static_cast<size_t>(start)].origin;
    gl::draw_line(p, sx(o.x) - 12, sy(o.y), sx(o.x) + 12, sy(o.y), {200, 0, 0, 255}, 3);
    gl::draw_line(p, sx(o.x), sy(o.y) - 12, sx(o.x), sy(o.y) + 12, {200, 0, 0, 255}, 3);
  }
  constexpr const char* kJumper[3] = {"ON FOOT", "WITH GAP TRICKS", "WITH ROCKET JUMPS"};
  gl::draw_text(p, 8, 6,
                std::string("FROM THE RED CROSS, ") + kJumper[pr.map_jumper] +
                    ": SECONDS, BN = BUTTON N ON THE WAY, X = NO WAY",
                {20, 20, 20, 255}, 2);
  out.field_map = std::move(p);
}

void probe_map(const bt::MapNav& nav, const Probe& pr, MapReach& out) {
  const sb::NavGraph& g = nav.graph;
  if (pr.walk)
    for (const auto& [name, f] : pr.walk_variants) walk_runs(nav, pr, name, f, out);
  if (pr.route) {
    const int32_t a = sb::nav_nearest(g, pr.from), b = sb::nav_nearest(g, pr.to);
    ge::ai::Search rs;
    sb::NavGraph priced;  // --route-wet: the water's links priced up (a copy; `g` as built)
    if (pr.wet != 1) {
      priced = g;
      for (sb::NavLink& l : priced.links)
        if (priced.nodes[static_cast<size_t>(l.to)].wet) l.time_s *= pr.wet;
    }
    const sb::NavGraph& rg = pr.wet != 1 ? priced : g;
    const sb::NavRoute r = a >= 0 && b >= 0 ? sb::nav_route(rg, a, b, rs) : sb::NavRoute{};
    float total = 0, wet = 0;  // the route's proof times (as built), and those into wet nodes
    if (r.nodes.empty())
      out.route_rows.push_back(
          {"-", "no route", std::to_string(a) + " -> " + std::to_string(b), "", "", ""});
    for (size_t k = 0; k < r.nodes.size(); k++) {
      const sb::NavNode& n = g.nodes[static_cast<size_t>(r.nodes[k])];
      std::string kind = "start", t = "", speed = "";
      if (k > 0) {
        const sb::NavNode& p = g.nodes[static_cast<size_t>(r.nodes[k - 1])];
        for (int32_t x = 0; x < p.links; x++) {
          const sb::NavLink& l =
              g.links[static_cast<size_t>(p.first_link) + static_cast<size_t>(x)];
          if (l.to != r.nodes[k]) continue;
          kind = sb::nav_kind_name(l.kind);
          t = to_fixed(l.time_s, 2);
          total += l.time_s;
          if (n.wet) wet += l.time_s;
          speed = to_fixed(l.speed, 0);
          break;
        }
      }
      out.route_rows.push_back({std::to_string(k), std::to_string(r.nodes[k]),
                                to_fixed(n.origin.x, 0) + " " + to_fixed(n.origin.y, 0) + " " +
                                    to_fixed(n.origin.z - 24, 0),
                                kind, t, speed});
    }
    if (!r.nodes.empty())
      out.route_rows.push_back(
          {"total", "", "wet " + to_fixed(wet, 2) + " s", "", to_fixed(total, 2), ""});
  }
  if (pr.floors) {
    const float dx = pr.fx1 - pr.fx0, dy = pr.fy1 - pr.fy0;
    const float len = std::sqrt(dx * dx + dy * dy);
    const int n = pr.fstep > 0 ? static_cast<int>(len / pr.fstep) : 0;
    std::vector<float> zs;
    for (int k = 0; k <= n; k++) {
      const float f = n > 0 ? static_cast<float>(k) / static_cast<float>(n) : 0;
      const float x = pr.fx0 + dx * f, y = pr.fy0 + dy * f;
      floors_at(nav.world, x, y, g.hi.z - 33, g.lo.z, zs);
      std::string list;
      for (const float z : zs) list += (list.empty() ? "" : " ") + to_fixed(z, 0);
      // The nav's own nodes in the grid column holding (x, y).
      std::string nodes;
      const int32_t i = static_cast<int32_t>((x - g.lo.x) / g.set.spacing);
      const int32_t j = static_cast<int32_t>((y - g.lo.y) / g.set.spacing);
      if (i >= 0 && j >= 0 && i < g.cols && j < g.rows) {
        const size_t c =
            static_cast<size_t>(i) * static_cast<size_t>(g.rows) + static_cast<size_t>(j);
        for (int32_t q = 0; q < g.col_n[c]; q++) {
          const sb::NavNode& nd =
              g.nodes[static_cast<size_t>(g.col_first[c]) + static_cast<size_t>(q)];
          nodes += (nodes.empty() ? "" : " ") + to_fixed(nd.origin.x, 0) + "," +
                   to_fixed(nd.origin.y, 0) + "," + to_fixed(nd.origin.z - 24, 0);
        }
      }
      out.floor_rows.push_back({to_fixed(x, 0) + " " + to_fixed(y, 0), list, nodes});
    }
  }
}

void reach_map(std::span<const uint8_t> bytes, const std::string& name, const std::string& entities,
               const ge::progs::Progs& progs, const Probe* probe, bool fly_escapes,
               const sb::Features* fly_gaps, std::ostream* gap_trace, MapReach& out) {
  out.name = name;
  std::unique_ptr<cl::LoadedMap> map = cl::map_from_bytes(bytes, name, out.error);
  if (!map) return;
  if (!entities.empty()) map->spec.entities = entities;
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
  if (!bt::build_map_nav(q, map->spec, bt::quake_maps(), "info_player_deathmatch", nav, out.error))
    return;
  const sb::NavGraph& g = nav.graph;
  if (probe != nullptr) probe_map(nav, *probe, out);
  if (probe != nullptr && probe->field) field_probe(nav, q, map->spec.entities, *probe, out);
  if (probe != nullptr && probe->field_map) field_map(nav, q, map->spec.entities, *probe, out);
  if (probe != nullptr && probe->rockets) rocket_runs(nav, *probe, name, gap_trace, out);
  for (const bt::MapNav::ScriptMade& sm : nav.scripts_made) {
    auto at = [&](int32_t n) {
      if (n < 0) return std::string("-");
      const sh::Vec3f o = g.nodes[static_cast<size_t>(n)].origin;
      return to_fixed(o.x, 0) + " " + to_fixed(o.y, 0) + " " + to_fixed(o.z - 24, 0);
    };
    out.script_rows.push_back({name, sm.name, sm.held ? "held" : "refused", at(sm.from), at(sm.to),
                               sm.held ? to_fixed(sm.time_s, 2) : "-",
                               sm.mover >= 0 ? std::to_string(sm.mover) : "-"});
  }
  out.nodes = static_cast<int64_t>(g.nodes.size());
  out.swim_nodes = nav.swim_nav.cells;
  out.swim_links = g.counts.by_kind[static_cast<int>(sb::NavKind::swim)];
  for (const uint8_t h : nav.harm) out.harm_nodes += h;
  out.risky = nav.hardened.over;
  out.risky_removed = nav.hardened.removed;
  out.escapes = nav.escapes_made.held;
  out.escape_links = nav.escapes_made.links;
  out.escape_proofs = nav.escapes_made.proofs;
  // --escapes: each escape flown by the pilot from the liquid node at its takeoff to its ledge
  // (bots/pilot_run.hpp `run_route`: the move, the rocket's burst and blast as the proof's).
  for (size_t x = 0; fly_escapes && x < g.escapes.size(); x++) {
    const sb::NavEscape& e = g.escapes[x];
    sb::PilotSettings ps;
    ps.jumper.escapes = true;
    const sh::Vec3f to = g.nodes[static_cast<size_t>(e.to)].origin;
    const bt::RunResult r = bt::run_route(nav.world, g, e.from, to, sb::PilotHands::human,
                                          static_cast<uint32_t>(1 + x), ps, kEscapeLimit_s);
    const bool fired = r.stats.escapes > 0, landed = fired && r.arrived;
    out.escapes_flown++;
    out.escapes_fired += fired;
    out.escapes_landed += landed;
    auto at = [](sh::Vec3f o) {
      return to_fixed(o.x, 0) + " " + to_fixed(o.y, 0) + " " + to_fixed(o.z - 24, 0);
    };
    out.escape_rows.push_back({name, std::to_string(x), at(e.from), at(to), to_fixed(e.pitch, 0),
                               std::to_string(e.fire_level), fired ? "yes" : "no",
                               landed ? "yes" : "no", to_fixed(r.time_s, 2), to_fixed(r.blast, 0)});
  }

  // --gaps=fly: each gap trick run up to by an expert fighter's pilot (bots/fighter.hpp
  // `fighter_pilot`, the features given; under `keys` on an expert's keyboard), from the floor's
  // node nearest each point `kGapBack` behind its takeoff, walking into it at people's 180:
  // planned through the gap, curved into it (the circle), jumped, landed or given up, the
  // takeoff's speed, and arrived across.
  for (size_t k = 0; fly_gaps != nullptr && k < nav.gaps_made.from.size(); k++) {
    const int32_t a = nav.gaps_made.from[k];
    if (a < 0) continue;
    const sb::NavNode& n = g.nodes[static_cast<size_t>(a)];
    const sb::NavLink* gl = nullptr;
    for (int32_t x = 0; x < n.links && gl == nullptr; x++) {
      const sb::NavLink& l = g.links[static_cast<size_t>(n.first_link) + static_cast<size_t>(x)];
      if (l.gap) gl = &l;
    }
    if (gl == nullptr) continue;
    const sh::Vec3f S = n.origin, T = g.nodes[static_cast<size_t>(gl->to)].origin;
    sh::Vec3f dir{T.x - S.x, T.y - S.y, 0};
    const float len = std::sqrt(dir.x * dir.x + dir.y * dir.y);
    if (len < 1) continue;
    dir = dir * (1 / len);
    for (const float back : kGapBack) {
      // The start: the dry node on the takeoff's floor nearest the point behind it.
      const sh::Vec3f want{S.x - dir.x * back, S.y - dir.y * back, S.z};
      int32_t best = -1;
      float best_d = 96;
      for (size_t i = 0; i < g.nodes.size(); i++) {
        const sh::Vec3f o = g.nodes[i].origin;
        if (o.z > S.z + 20 || o.z < S.z - 20 || (i < nav.wet.size() && nav.wet[i] != 0)) continue;
        const float d =
            std::sqrt((o.x - want.x) * (o.x - want.x) + (o.y - want.y) * (o.y - want.y));
        if (d < best_d) best_d = d, best = static_cast<int32_t>(i);
      }
      if (best < 0) continue;
      const sh::Vec3f from = g.nodes[static_cast<size_t>(best)].origin;
      sh::Vec3f v{S.x - from.x, S.y - from.y, 0};
      const float vl = std::sqrt(v.x * v.x + v.y * v.y);
      v = vl > 1 ? v * (kGapWalk / vl) : sh::Vec3f{};
      sb::PilotHands hands = sb::PilotHands::none;
      const sb::Traits traits = sb::traits_of(sb::AimSkill::expert, sb::Temper::balanced);
      sb::PilotSettings ps = bt::fighter_pilot(*fly_gaps, traits, sb::AimSkill::expert, hands);
      // Under `keys` the commands go through an expert's keyboard, as a fighter's do.
      const sb::Features* keys = (*fly_gaps)[sb::Feature::keys] ? fly_gaps : nullptr;
      ps.jumper.gap_build_s = (*fly_gaps)[sb::Feature::gapbuild] ? bt::kGapBuild_s : -1;
      int planned = 0, jumped = 0, landed = 0, refused = 0, arrived = 0, circles = 0, jumps = 0;
      float speed = 0, time = 0;
      for (uint32_t seed = 1; seed <= kGapSeeds; seed++) {
        const bt::RunResult r = bt::run_route(nav.world, g, from, T, hands, seed, ps, kGapLimit_s,
                                              v, keys, traits[sb::Trait::moves]);
        if (gap_trace != nullptr && seed == 1) {
          for (size_t i = 0; i < r.trail.size(); i++) {
            const sh::Vec3f o = r.trail[i], vv = r.velocity[i];
            *gap_trace << k + 1 << '\t' << back << '\t'
                       << to_fixed(0.013 * static_cast<double>(i + 1), 3) << '\t'
                       << to_fixed(o.x, 0) << '\t' << to_fixed(o.y, 0) << '\t'
                       << to_fixed(o.z - 24, 0) << '\t'
                       << to_fixed(std::sqrt(vv.x * vv.x + vv.y * vv.y), 0) << '\t'
                       << to_fixed(vv.z, 0) << '\t'
                       << sb::pilot_mode_name(static_cast<sb::PilotMode>(r.modes[i])) << '\t'
                       << r.links[i] << '\t' << to_fixed(r.yaw[i], 1) << '\t'
                       << to_fixed(r.forward[i], 2) << '\t' << to_fixed(r.side[i], 2) << '\t'
                       << r.because[i] << '\n';
          }
        }
        bool through = false;
        for (size_t i = 0; i + 1 < r.route.size(); i++)
          through = through || (r.route[i] == a && r.route[i + 1] == gl->to);
        planned += through;
        jumped += r.stats.gap_jumps > 0;
        landed += r.stats.gap_landed > 0;
        refused += r.stats.gap_refused > 0;
        circles += r.stats.circles;
        speed += r.stats.gap_speed;
        jumps += r.stats.gap_jumps;
        arrived += r.arrived;
        if (r.arrived) time += r.time_s;
        out.gap_runs++;
      }
      out.gap_planned += planned, out.gap_jumped += jumped, out.gap_landed += landed;
      out.gap_refused += refused;
      auto at = [](sh::Vec3f o) {
        return to_fixed(o.x, 0) + " " + to_fixed(o.y, 0) + " " + to_fixed(o.z - 24, 0);
      };
      out.gap_rows.push_back(
          {name, std::to_string(k + 1), at(S), at(T), to_fixed(gl->speed, 0), to_fixed(back, 0),
           at(from), std::to_string(planned), std::to_string(circles), std::to_string(jumped),
           jumps > 0 ? to_fixed(speed / static_cast<float>(jumps), 0) : "-", std::to_string(landed),
           std::to_string(refused), std::to_string(arrived),
           arrived > 0 ? to_fixed(time / static_cast<float>(arrived), 2) : "-"});
    }
  }

  for (const sb::NavLink& l : g.links) {
    out.grenade_links += l.kind == sb::NavKind::grenade;
    if (l.kind == sb::NavKind::rocket) {
      out.rocket_links++;
      out.flips += l.back;
    }
  }
  // The map's tricks: each one's link, its flight against the way on foot to where it lands.
  {
    const std::span<const bt::RocketTrick> tricks = bt::rocket_tricks(name);
    constexpr const char* kLevel[3] = {"amateur", "intermediate", "expert"};
    ge::ai::Search rs;
    for (size_t k = 0; k < tricks.size() && k < nav.tricks_made.rocket_from.size(); k++) {
      const bt::RocketTrick& tr = tricks[k];
      const int32_t a = nav.tricks_made.rocket_from[k];
      const int32_t ga = nav.tricks_made.grenade_from[k];
      std::string jump_s = "-", foot_s = "-", lands = "-", grenade_s = "-";
      if (ga >= 0) {
        const sb::NavNode& n = g.nodes[static_cast<size_t>(ga)];
        for (int32_t x = n.links - 1; x >= 0; x--) {
          const sb::NavLink& l =
              g.links[static_cast<size_t>(n.first_link) + static_cast<size_t>(x)];
          if (l.kind != sb::NavKind::grenade) continue;
          grenade_s = to_fixed(l.time_s, 2) + " (lead " + to_fixed(l.lead, 0) + ")";
          break;
        }
      }
      if (a >= 0) {
        out.tricks_held++;
        const sb::NavNode& n = g.nodes[static_cast<size_t>(a)];
        for (int32_t x = n.links - 1; x >= 0; x--) {
          const sb::NavLink& l =
              g.links[static_cast<size_t>(n.first_link) + static_cast<size_t>(x)];
          if (l.kind != sb::NavKind::rocket || l.level != tr.level) continue;
          jump_s = to_fixed(l.time_s, 2);
          const sh::Vec3f o = g.nodes[static_cast<size_t>(l.to)].origin;
          lands = to_fixed(o.x, 0) + " " + to_fixed(o.y, 0) + " " + to_fixed(o.z, 0);
          const sb::NavRoute r = sb::nav_route(g, a, l.to, rs);
          foot_s = r.nodes.empty() ? "none" : to_fixed(r.time_s, 2);
          break;
        }
      }
      out.tricks.push_back(
          {name, std::to_string(k + 1),
           to_fixed(tr.from.x, 0) + " " + to_fixed(tr.from.y, 0) + " " + to_fixed(tr.from.z, 0),
           to_fixed(tr.to.x, 0) + " " + to_fixed(tr.to.y, 0) + " " + to_fixed(tr.to.z, 0),
           tr.flip ? "flip" : "feet", kLevel[tr.level], std::to_string(tr.jumps),
           a >= 0 ? "held" : "no", lands, jump_s, foot_s, grenade_s});
    }
  }
  // The map's gap tricks: each one's jump link at speed, against the way on foot to its landing.
  {
    const std::span<const bt::GapTrick> tricks = bt::gap_tricks(name);
    constexpr const char* kLevel[3] = {"amateur", "intermediate", "expert"};
    ge::ai::Search rs;
    out.speed_jumps_run = nav.gaps_made.run_makes;
    const auto why_of = [&](size_t k) {
      if (k >= nav.gaps_made.why.size()) return std::string("-");
      const sb::NavGapWhy w = nav.gaps_made.why[k];
      std::string s = sb::nav_gap_why_name(w);
      if (w == sb::NavGapWhy::short_of && nav.gaps_made.miss[k] >= 0) {
        const sh::Vec3f at = nav.gaps_made.miss_at[k];
        s += ": " + to_fixed(nav.gaps_made.miss[k], 0) + " off, at " + to_fixed(at.x, 0) + " " +
             to_fixed(at.y, 0) + " " + to_fixed(at.z, 0);
      }
      return s;
    };
    for (size_t k = 0; k < tricks.size() && k < nav.gaps_made.from.size(); k++) {
      const bt::GapTrick& tr = tricks[k];
      const int32_t a = nav.gaps_made.from[k];
      std::string jump_s = "-", foot_s = "-", lands = "-", speed = "-";
      if (a >= 0) {
        out.speed_jumps_held++;
        const sb::NavNode& n = g.nodes[static_cast<size_t>(a)];
        for (int32_t x = n.links - 1; x >= 0; x--) {
          const sb::NavLink& l =
              g.links[static_cast<size_t>(n.first_link) + static_cast<size_t>(x)];
          if (!l.gap) continue;
          jump_s = to_fixed(l.time_s, 2);
          speed = to_fixed(l.speed, 0);
          const sh::Vec3f o = g.nodes[static_cast<size_t>(l.to)].origin;
          lands = to_fixed(o.x, 0) + " " + to_fixed(o.y, 0) + " " + to_fixed(o.z, 0);
          const sb::NavRoute r = sb::nav_route(g, a, l.to, rs);
          foot_s = r.nodes.empty() ? "none" : to_fixed(r.time_s, 2);
          break;
        }
      }
      out.speed_jumps.push_back(
          {name, std::to_string(k + 1),
           to_fixed(tr.from.x, 0) + " " + to_fixed(tr.from.y, 0) + " " + to_fixed(tr.from.z, 0),
           to_fixed(tr.to.x, 0) + " " + to_fixed(tr.to.y, 0) + " " + to_fixed(tr.to.z, 0),
           to_fixed(tr.speed, 0), kLevel[tr.level], std::to_string(tr.jumps),
           tr.harm ? "yes" : "no", a >= 0 ? "held" : "no", lands, speed, jump_s, foot_s,
           why_of(k)});
    }
  }
  for (const sb::NavMover& m : g.movers) {
    MoverRow row{m.ent, m.model != nullptr, m.absmin, m.absmax, m.travel, 0};
    for (const sb::NavNode& nd : g.nodes)
      for (int32_t k = 0; k < nd.links; k++) {
        const sb::NavLink& l = g.links[static_cast<size_t>(nd.first_link) + static_cast<size_t>(k)];
        if (l.kind == sb::NavKind::ride && l.mover == m.ent) row.rides++;
      }
    out.movers.push_back(row);
  }
  std::vector<pg::EntityBlock> blocks;
  if (!pg::parse_entity_text(map->spec.entities.c_str(), blocks, out.error)) return;
  ge::ai::Search from, with_rockets;
  // What the main part reaches with rocket jumps too (from any one of its nodes).
  for (size_t k = 0; nav.main_n > 0 && k < g.nodes.size(); k++)
    if (nav.main.reached(static_cast<int32_t>(k))) {
      sb::nav_reach(g, static_cast<int32_t>(k), with_rockets, {.rockets = true});
      break;
    }
  for (const pg::EntityBlock& b : blocks) {
    const char* cname = pg::block_value(b, "classname");
    const char* flags = pg::block_value(b, "spawnflags");
    bt::ItemSpot it;
    sh::Vec3f at;
    if (!cname || !bt::item_of(cname, flags ? std::atoi(flags) : 0, it, bt::first_arsenal()) ||
        !vec_of(pg::block_value(b, "origin"), at))
      continue;
    out.items++;
    const int32_t n = sb::nav_nearest(g, {at.x, at.y, at.z + 24});
    const sh::Vec3f o = n >= 0 ? g.nodes[static_cast<size_t>(n)].origin : sh::Vec3f{};
    const float off =
        n >= 0 ? std::sqrt((o.x - at.x) * (o.x - at.x) + (o.y - at.y) * (o.y - at.y)) : 1e9f;
    const bool to = n >= 0 && nav.main_n > 0 && nav.main.reached(n);
    const bool rj = !to && n >= 0 && with_rockets.nodes() > 0 && with_rockets.reached(n);
    out.by_rocket += rj;
    bool back = false;
    if (n >= 0) {
      sb::nav_reach(g, n, from);
      for (size_t k = 0; k < g.nodes.size() && !back; k++)
        back = nav.main_n > 0 && nav.main.reached(static_cast<int32_t>(k)) && !nav.wet[k] &&
               from.reached(static_cast<int32_t>(k)) && static_cast<int32_t>(k) != n;
    }
    out.reached += to;
    out.back += back;
    if (!to || !back || off > 48)
      out.gaps.push_back({cname,
                          rj && back     ? "a rocket jump there"
                          : rj           ? "a rocket jump there, none back"
                          : !to && !back ? "no way there, none back"
                          : !to          ? "no way there"
                          : !back        ? "no way back"
                                         : "its node is far from it",
                          at, off});
  }
}

void run(gl::Lab& lab) {
  std::string error;
  ge::progs::Progs progs;
  if (!cl::baseline_progs(progs, error)) {
    lab.fail("se-reach: the baseline mod: " + error,
             "next: run tests/shooters_edge_baseline_mod_test.cpp");
    return;
  }
  std::vector<MapReach> maps;
  Probe probe;
  {
    std::istringstream r(lab.arg("route"));
    char sep = 0;
    probe.route = static_cast<bool>(r >> probe.from.x >> probe.from.y >> probe.from.z >> sep >>
                                    probe.to.x >> probe.to.y >> probe.to.z);
    std::istringstream fm(lab.arg("field-map"));
    probe.field_map =
        static_cast<bool>(fm >> probe.map_from.x >> probe.map_from.y >> probe.map_from.z);
    const std::string fj = lab.arg("field-jumper");
    probe.map_jumper = fj == "foot" ? 0 : fj == "rockets" ? 2 : 1;
    std::istringstream fd(lab.arg("field"));
    probe.field =
        static_cast<bool>(fd >> probe.field_from.x >> probe.field_from.y >> probe.field_from.z >>
                          sep >> probe.field_to.x >> probe.field_to.y >> probe.field_to.z);
    std::istringstream f(lab.arg("floors"));
    probe.floors = static_cast<bool>(f >> probe.fx0 >> probe.fy0 >> probe.fx1 >> probe.fy1);
    if (probe.floors && !(f >> probe.fstep)) probe.fstep = 8;
    probe.wet = std::stof(lab.arg("route-wet"));
    std::istringstream w(lab.arg("walk"));
    probe.walk =
        static_cast<bool>(w >> probe.walk_from.x >> probe.walk_from.y >> probe.walk_from.z >> sep >>
                          probe.walk_to.x >> probe.walk_to.y >> probe.walk_to.z);
    probe.walk_view_lag_s = std::stof(lab.arg("walk-view-lag"));
    probe.walk_from.z += 24;
    probe.walk_to.z += 24;
    // --walk-variants: "name:switches;name:switches" (se-team's --features on the full preset).
    std::istringstream vs(lab.arg("walk-variants"));
    std::string item;
    while (probe.walk && std::getline(vs, item, ';')) {
      const size_t colon = item.find(':');
      sb::Features feats = sb::features_all();
      const std::string sw = colon == std::string::npos ? "" : item.substr(colon + 1);
      if (!sw.empty() && !sb::apply_features(sw, feats, error)) {
        lab.fail("se-reach: --walk-variants: " + error,
                 "next: name switches as se-team's --features");
        return;
      }
      probe.walk_variants.emplace_back(item.substr(0, colon), feats);
    }
    // Each probe's point is a player's origin, its feet 24 under.
    probe.from.z += 24;
    probe.to.z += 24;
  }
  probe.rockets = lab.arg("rockets") == "fly";
  if (probe.rockets) {
    if (!sb::apply_features(lab.arg("gap-features"), probe.rocket_features, error)) {
      lab.fail("se-reach: --gap-features: " + error, "next: name switches as se-team's --features");
      return;
    }
    std::istringstream rn(lab.arg("rocket-near"));
    if (!(rn >> probe.rocket_x >> probe.rocket_y >> probe.rocket_r)) probe.rocket_r = 1e9f;
  }
  const std::string basedir = lab.arg("basedir");
  const bool fly = lab.arg("escapes") == "fly";
  sb::Features gap_features = sb::features_all();
  const sb::Features* fly_gaps = nullptr;
  if (lab.arg("gaps") == "fly") {
    if (!sb::apply_features(lab.arg("gap-features"), gap_features, error)) {
      lab.fail("se-reach: --gap-features: " + error, "next: name switches as se-team's --features");
      return;
    }
    fly_gaps = &gap_features;
  }
  std::ofstream gap_trace;
  if ((fly_gaps != nullptr || probe.rockets) && !lab.arg("gap-trace").empty()) {
    gap_trace.open(lab.arg("gap-trace"));
    gap_trace << "trick\tback\tt\tx\ty\tfeet\tspeed\tvz\tmode\tk\tyaw\tforward\tside\tbecause\n";
  }
  if (basedir.empty()) {
    std::vector<uint8_t> bytes;
    if (!ge::brush::write_bsp(ge::brush::sample_fixture(), ge::brush::Family::v29, &bytes,
                              &error)) {
      lab.fail("se-reach: the fixture halls: " + error, "next: run tests/brush_write_test.cpp");
      return;
    }
    maps.emplace_back();
    reach_map(bytes, "halls", cn::kTeamHallsEntities, progs, &probe, fly, fly_gaps,
              gap_trace.is_open() ? &gap_trace : nullptr, maps.back());
  } else {
    cl::SearchPath files;
    if (!cl::open_game_dir(basedir, lab.arg("game"), files, error)) {
      lab.fail(error, "Pass --basedir=<the folder that holds the game folder>. Skill: bots.");
      return;
    }
    std::istringstream list(lab.arg("maps"));
    std::string name;
    while (std::getline(list, name, ',')) {
      std::vector<uint8_t> store;
      const std::span<const uint8_t> f = cl::find_file(files, "maps/" + name + ".bsp", store);
      maps.emplace_back();
      if (f.empty()) {
        maps.back().name = name;
        maps.back().error = "no maps/" + name + ".bsp in the game folder";
        continue;
      }
      reach_map(f, name, "", progs, maps.size() == 1 ? &probe : nullptr, fly, fly_gaps,
                gap_trace.is_open() ? &gap_trace : nullptr, maps.back());
    }
  }

  gl::LabTable t{"each map: its items, those a route leads to from the spawns, and from back",
                 {"map", "nodes", "swim nodes", "swim links", "lava/slime", "rocket links", "flips",
                  "items", "reached", "by rocket", "back", "gaps"},
                 {}};
  gl::LabTable mv{"each map's movers: plats (ridden) and doors, their box at rest, travel, rides",
                  {"map", "ent", "kind", "box lo", "box hi", "travel", "ride links"},
                  {}};
  const auto v3 = [](sh::Vec3f v) {
    return to_fixed(v.x, 0) + " " + to_fixed(v.y, 0) + " " + to_fixed(v.z, 0);
  };
  gl::LabTable gaps{"each item a bot cannot get to or back from (its node across from it, units)",
                    {"map", "item", "at (x y z)", "node off", "why"},
                    {}};
  gl::LabTable tk{
      "each map's rocket-jump tricks (people's, bots/quake_maps.hpp) as the nav proved "
      "them: the link's landing, its flight and the way there on foot, s",
      {"map", "#", "people's from", "people's to", "kind", "level", "jumps", "proof",
       "lands (x y z)", "jump s", "on foot s", "grenade jump s"},
      {}};
  int items = 0, reached = 0, back = 0;
  for (const MapReach& m : maps) {
    if (!m.error.empty()) {
      lab.note("se-reach: " + m.name + ": " + m.error);
      continue;
    }
    items += m.items;
    reached += m.reached;
    back += m.back;
    for (const MoverRow& r : m.movers)
      if (r.plat || lab.arg("movers") == "all")
        mv.rows.push_back({m.name, std::to_string(r.ent), r.plat ? "plat" : "door", v3(r.lo),
                           v3(r.hi), v3(r.travel), std::to_string(r.rides)});
    t.rows.push_back(
        {m.name, std::to_string(m.nodes), std::to_string(m.swim_nodes),
         std::to_string(m.swim_links), std::to_string(m.harm_nodes), std::to_string(m.rocket_links),
         std::to_string(m.flips), std::to_string(m.items), std::to_string(m.reached),
         std::to_string(m.by_rocket), std::to_string(m.back), std::to_string(m.gaps.size())});
    lab.metric("reach." + m.name + ".items", m.items, "count");
    lab.metric("reach." + m.name + ".reached", m.reached, "count");
    lab.metric("reach." + m.name + ".back", m.back, "count");
    lab.metric("reach." + m.name + ".by-rocket", m.by_rocket, "count");
    lab.metric("reach." + m.name + ".rocket-links", static_cast<double>(m.rocket_links), "count");
    lab.metric("reach." + m.name + ".flips", static_cast<double>(m.flips), "count");
    if (!m.tricks.empty())
      lab.metric("reach." + m.name + ".tricks-held", static_cast<double>(m.tricks_held), "count");
    lab.metric("reach." + m.name + ".grenade-links", static_cast<double>(m.grenade_links), "count");
    lab.metric("reach." + m.name + ".risky-jumps", static_cast<double>(m.risky), "count");
    lab.metric("reach." + m.name + ".risky-removed", static_cast<double>(m.risky_removed), "count");
    lab.metric("reach." + m.name + ".escapes", static_cast<double>(m.escapes), "count");
    lab.metric("reach." + m.name + ".escape-links", static_cast<double>(m.escape_links), "count");
    lab.metric("reach." + m.name + ".escape-proofs", static_cast<double>(m.escape_proofs), "count");
    for (const auto& row : m.tricks) tk.rows.push_back(row);
    for (const Gap& gp : m.gaps)
      gaps.rows.push_back(
          {m.name, gp.item,
           to_fixed(gp.at.x, 0) + " " + to_fixed(gp.at.y, 0) + " " + to_fixed(gp.at.z, 0),
           to_fixed(gp.node_off, 0), gp.why});
  }
  lab.metric("reach.items", items, "count");
  lab.metric("reach.share", items > 0 ? static_cast<double>(reached) / items : 0, "share");
  lab.metric("reach.back-share", items > 0 ? static_cast<double>(back) / items : 0, "share");
  lab.table(t);
  lab.table(gaps);
  if (!tk.rows.empty()) lab.table(tk);
  gl::LabTable gt{
      "each map's gap tricks (people's speed jumps over a pit, bots/quake_maps.hpp) as the nav "
      "proved "
      "them: the link's landing and speed, its flight and the way there on foot, s; why none was "
      "kept (the nearest a proof came down to people's landing, across, and where)",
      {"map", "#", "people's from", "people's to", "speed", "level", "jumps", "harm", "proof",
       "lands (x y z)", "link u/s", "jump s", "on foot s", "why"},
      {}};
  for (const MapReach& m : maps) {
    for (const auto& row : m.speed_jumps) gt.rows.push_back(row);
    if (!m.speed_jumps.empty()) {
      lab.metric("reach." + m.name + ".gap-tricks-held", static_cast<double>(m.speed_jumps_held),
                 "count");
      lab.metric("reach." + m.name + ".gap-tricks-run", static_cast<double>(m.speed_jumps_run),
                 "count");
    }
  }
  if (!gt.rows.empty()) lab.table(gt);
  lab.table(mv);
  gl::LabTable st{
      "each map's scripted routes to key items (bots/quake_maps.hpp) as the move "
      "proved them: from the entry's node to the end's (feet), the time, the mover "
      "crossed",
      {"map", "route", "proof", "from", "to", "s", "mover"},
      {}};
  for (const MapReach& m : maps)
    for (const auto& row : m.script_rows) st.rows.push_back(row);
  if (!st.rows.empty()) lab.table(st);
  if (fly) {
    gl::LabTable et{
        "--escapes=fly: every rocket out of a pit's liquid (shooterbots/escape.hpp) flown by "
        "the pilot from the liquid node at its takeoff to its ledge: fired, landed",
        {"map", "#", "takeoff (feet)", "ledge (feet)", "pitch", "level", "fired", "landed", "s",
         "blast"},
        {}};
    int64_t flown = 0, fired = 0, landed = 0;
    for (const MapReach& m : maps) {
      for (const auto& row : m.escape_rows) et.rows.push_back(row);
      flown += m.escapes_flown, fired += m.escapes_fired, landed += m.escapes_landed;
      lab.metric("reach." + m.name + ".escapes-landed", static_cast<double>(m.escapes_landed),
                 "count");
    }
    lab.metric("reach.escapes-flown", static_cast<double>(flown), "count");
    lab.metric("reach.escapes-fired", static_cast<double>(fired), "count");
    lab.metric("reach.escapes-landed", static_cast<double>(landed), "count");
    lab.table(et);
  }
  if (fly_gaps != nullptr) {
    gl::LabTable gf{
        "--gaps=fly: each gap trick run up to by an expert's pilot from the node nearest each "
        "point behind its takeoff, walking in at 180, on 3 seeds: planned through the gap, circle "
        "jumps, jumped (and the takeoff's mean speed), landed, given up, arrived (and its time)",
        {"map", "#", "takeoff (feet)", "landing (feet)", "link u/s", "back", "start (feet)",
         "planned", "circles", "jumped", "takeoff u/s", "landed", "refused", "arrived", "s"},
        {}};
    int64_t runs = 0, planned = 0, jumped = 0, landed = 0, refused = 0;
    for (const MapReach& m : maps) {
      for (const auto& row : m.gap_rows) gf.rows.push_back(row);
      runs += m.gap_runs, planned += m.gap_planned, jumped += m.gap_jumped;
      landed += m.gap_landed, refused += m.gap_refused;
    }
    lab.metric("reach.gap-runs", static_cast<double>(runs), "count");
    lab.metric("reach.gap-planned", static_cast<double>(planned), "count");
    lab.metric("reach.gap-jumped", static_cast<double>(jumped), "count");
    lab.metric("reach.gap-landed", static_cast<double>(landed), "count");
    lab.metric("reach.gap-refused", static_cast<double>(refused), "count");
    lab.table(gf);
  }
  if (probe.rockets && !maps.empty()) {
    const MapReach& m = maps.front();
    lab.table({"--rockets=fly on " + m.name +
                   ": each rocket trick run up to by an expert's pilot with the launcher from the "
                   "node nearest each point behind its takeoff, walking in at 180, on 3 seeds: "
                   "planned through it, fired (and the mean speed at the shot), landed on its "
                   "node, short (sank 48 under both ends after the shot: into a pit)",
               {"map", "#", "takeoff (feet)", "landing (feet)", "way", "proven u/s", "pitch",
                "level", "back", "planned", "fired", "shot u/s", "landed", "short"},
               m.rocket_rows});
    lab.metric("reach.rocket-runs", static_cast<double>(m.rocket_runs), "count");
    lab.metric("reach.rocket-planned", static_cast<double>(m.rocket_planned), "count");
    lab.metric("reach.rocket-fired", static_cast<double>(m.rocket_fired), "count");
    lab.metric("reach.rocket-landed", static_cast<double>(m.rocket_landed), "count");
    lab.metric("reach.rocket-short", static_cast<double>(m.rocket_short), "count");
  }
  if (probe.walk && !maps.empty()) {
    lab.table({"--walk on " + maps.front().name +
                   ": an expert's pilot through an expert's keyboard under a view trailing its "
                   "heading (--walk-view-lag; no brink guard: no server) "
                   "walking the route, already running at the speed, headed 45, 20 and 0 degrees "
                   "either side of its first node, 4 seeds each: runs, those into lava or slime "
                   "(and where, the first few), arrived dry (and their mean time)",
               {"variant", "start u/s", "runs", "into lava", "arrived", "s", "where in"},
               maps.front().walk_rows});
    for (const auto& row : maps.front().walk_rows)
      lab.metric("reach.walk." + row[0] + ".fell-" + row[1], std::stod(row[3]), "count");
  }
  if (probe.route && !maps.empty())
    lab.table({"--route on " + maps.front().name + ": each node (feet) and the link onto it",
               {"#", "node", "at (x y feet)", "link", "s", "jump speed"},
               maps.front().route_rows});
  if (probe.field_map && !maps.empty() && maps.front().field_map.width > 0) {
    const std::string where = to_fixed(probe.map_from.x, 0) + " " + to_fixed(probe.map_from.y, 0) +
                              " " + to_fixed(probe.map_from.z, 0);
    lab.shot(
        {"field-map",
         "WAYS TO EVERY ITEM FROM " + where + " ON " + maps.front().name,
         "The map from above (height shaded, solid dark) and, from the node nearest the red "
         "cross, the way off each item's field (shooterbots/nav_field.hpp) for the jumper asked: "
         "key items thick in their colour (RA red, YA amber, RL brown, quad blue, mega "
         "green), the rest thin grey; a teleporter's two ends ringed; a magenta ring and "
         "B<n> where the way presses or shoots button n; each item's call and run time, a "
         "grey X where no way leads.",
         "An item crossed out that a player reaches from here; a way that runs round when a "
         "button or a jump is nearer; a press where no button is.",
         {{"field-map", lab.arg("field-map")}, {"field-jumper", lab.arg("field-jumper")}}},
        maps.front().field_map);
    lab.table({"--field-map on " + maps.front().name + ": every item's way from the start",
               {"item", "at", "s", "button", "nodes"},
               maps.front().field_items});
  }
  if (probe.field && !maps.empty()) {
    lab.table({"--field on " + maps.front().name +
                   ": the map's buttons as errands: the node pressed or shot from, the doors "
                   "fired and those that are a way, the time they stay open and the press, s",
               {"#", "button", "by", "from (feet)", "doors", "way doors", "open s", "press s",
                "press nodes"},
               maps.front().errand_rows});
    lab.table({"--field on " + maps.front().name +
                   ": the goal's field for each jumper: the time from the start and the errand "
                   "its way starts with",
               {"jumper", "from", "goal", "s", "errand at", "nodes"},
               maps.front().field_rows});
    lab.table({"--field on " + maps.front().name + ": the way off the field, node by node",
               {"jumper", "#", "node", "at (x y feet)", "press"},
               maps.front().field_ways});
  }
  if (probe.floors && !maps.empty())
    lab.table({"--floors on " + maps.front().name +
                   ": the floors (feet) straight down along the line, and the nav's nodes in "
                   "that grid column",
               {"x y", "floors (feet z)", "nodes (x,y,feet z)"},
               maps.front().floor_rows});
}

const gl::LabSpec kSpec{
    "se-reach",
    gl::LabKind::sim,
    "game:shooters_edge",
    "the bots' nav over every item of a map (games/shooters_edge/bots/map_nav.hpp, "
    "games/shooters_edge/labs/se_reach_lab.cpp)",
    "bots",
    "Can a bot get to every item of a map and back, by the routes its nav proves?",
    {
        {"basedir", "", "a folder holding the game folder; empty: the fixture halls"},
        {"game", "id1", "the game folder under --basedir"},
        {"maps", "e1m2,dm1,dm2,dm3,dm4,dm5,dm6", "the maps to ask, comma separated"},
        {"movers", "plats", "the movers tabled: plats (the ridden ones) or all"},
        {"route", "",
         "a route on the first map tabled node by node: 'x y z > x y z' (feet; nearest nodes)"},
        {"field", "",
         "the way off a goal's field on the first map: 'x y z > x y z' (feet; nearest nodes): the "
         "buttons as errands, the time for each jumper, the way node by node with its press"},
        {"field-map", "",
         "the ways to every item from a point on the first map drawn from above: 'x y z' "
         "(feet): the shot field-map, each item's time or no way, the buttons pressed"},
        {"field-jumper", "gaps",
         "--field-map's jumper: foot, gaps (a hopper's gap tricks) or rockets (rocket jumps too)"},
        {"route-wet", "1",
         "--route's search prices each link into a wet node this many times (1000: the dry way)"},
        {"floors", "",
         "the floors along a line on the first map: 'x0 y0 x1 y1 [step]' (the nav's misses)"},
        {"escapes", "", "fly: every rocket out of a pit's liquid flown by the pilot, tabled"},
        {"gaps", "", "fly: every gap trick run up to by an expert's pilot from behind, tabled"},
        {"gap-features", "+gapbuild",
         "--gaps=fly and --rockets=fly: the pilot's switches, as se-team's --features"},
        {"gap-trace", "",
         "--gaps=fly and --rockets=fly: every command of each first seed's run to this TSV"},
        {"rockets", "",
         "fly: every rocket trick of the first map run up to by an expert's pilot, tabled"},
        {"rocket-near", "", "--rockets=fly: only the tricks taking off within 'x y r' of a point"},
        {"walk", "",
         "a route on the first map walked by an expert's pilot from 'x y z > x y z' (feet), "
         "already running, at 250, 320 and 400 u/s: the runs into lava or slime tabled"},
        {"walk-view-lag", "0.3",
         "--walk: the view trails the run's heading with this time constant, s (the keys "
         "pressed under it, as under a fighter's calm view); 0: the view is the heading"},
        {"walk-variants",
         "base:", "--walk: 'name:switches;name:switches', each switches as se-team's --features"},
    },
    "",
    run,
};
GE_LAB(kSpec);

}  // namespace
