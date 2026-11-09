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
// `--escapes=fly` flies every rocket out of a pit's liquid (shooter/escape.hpp) with the pilot
// (bots/pilot_run.hpp `run_route`) from the liquid node at its takeoff, and tables which fired and
// which landed: a staged run of a rare moment (a bot fallen into lava) a match seldom shows.
//
// ge:summary Lab: every item of every map asked: a route to it from the spawns, and one back
// ge:keywords lab se-reach reachability coverage nav-gaps items pockets unreachable route maps
// ge:keywords dm1 dm2 dm3 dm4 dm5 dm6 e1m2 route-probe floors-probe nav-debug escapes-fly lava
// ge:skill bots labs
// ge:test tests/shooters_edge_team_test.cpp
#include <cmath>
#include <cstdlib>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "brush/write.hpp"
#include "core/num_text.hpp"
#include "lab/lab.hpp"
#include "progs/entity_text.hpp"
#include "shooter/nav.hpp"
#include "shooters_edge/bots/knowledge.hpp"
#include "shooters_edge/bots/map_nav.hpp"
#include "shooters_edge/bots/pilot_run.hpp"
#include "shooters_edge/bots/rocket_tricks.hpp"
#include "shooters_edge/client/files.hpp"
#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/net/classic_team.hpp"
#include "shooters_edge/server/qc_server.hpp"
#include "shooters_edge/server/rules_runner.hpp"

namespace {

namespace gl = ge::lab;
namespace sh = ge::shooter;
namespace pg = ge::progs;
namespace cl = shooters_edge::client;
namespace sv = shooters_edge::server;
namespace bt = shooters_edge::bots;
namespace cn = shooters_edge::net;
using ge::core::num::to_fixed;

constexpr float kEscapeLimit_s = 8;  // --escapes: a flight given this long to land

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
  int64_t risky = 0, risky_removed = 0;  // jumps over lava or slime, and those removed
  int64_t escapes = 0, escape_links = 0, escape_proofs = 0;  // rockets out of a pit's liquid
  int64_t escapes_flown = 0, escapes_fired = 0, escapes_landed = 0;  // --escapes: the pilot's
  std::vector<std::vector<std::string>> escape_rows;
  std::vector<std::vector<std::string>> route_rows, floor_rows;  // --route's and --floors' rows
  std::vector<std::vector<std::string>> script_rows;  // the scripted routes, held or refused
};

// The probes asked of the first map: a route between two points, each node and the link onto
// it; and the floors down the grid's columns along a line (where the nav samples a stair).
struct Probe {
  bool route = false, floors = false;
  sh::Vec3f from, to;                                   // --route's ends
  float fx0 = 0, fy0 = 0, fx1 = 0, fy1 = 0, fstep = 8;  // --floors' line and step
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

void probe_map(const bt::MapNav& nav, const Probe& pr, MapReach& out) {
  const sh::NavGraph& g = nav.graph;
  if (pr.route) {
    const int32_t a = sh::nav_nearest(g, pr.from), b = sh::nav_nearest(g, pr.to);
    ge::ai::Search rs;
    const sh::NavRoute r = a >= 0 && b >= 0 ? sh::nav_route(g, a, b, rs) : sh::NavRoute{};
    if (r.nodes.empty())
      out.route_rows.push_back(
          {"-", "no route", std::to_string(a) + " -> " + std::to_string(b), "", "", ""});
    for (size_t k = 0; k < r.nodes.size(); k++) {
      const sh::NavNode& n = g.nodes[static_cast<size_t>(r.nodes[k])];
      std::string kind = "start", t = "", speed = "";
      if (k > 0) {
        const sh::NavNode& p = g.nodes[static_cast<size_t>(r.nodes[k - 1])];
        for (int32_t x = 0; x < p.links; x++) {
          const sh::NavLink& l =
              g.links[static_cast<size_t>(p.first_link) + static_cast<size_t>(x)];
          if (l.to != r.nodes[k]) continue;
          kind = sh::nav_kind_name(l.kind);
          t = to_fixed(l.time_s, 2);
          speed = to_fixed(l.speed, 0);
          break;
        }
      }
      out.route_rows.push_back({std::to_string(k), std::to_string(r.nodes[k]),
                                to_fixed(n.origin.x, 0) + " " + to_fixed(n.origin.y, 0) + " " +
                                    to_fixed(n.origin.z - 24, 0),
                                kind, t, speed});
    }
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
          const sh::NavNode& nd =
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
               const ge::progs::Progs& progs, const Probe* probe, bool fly_escapes, MapReach& out) {
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
  if (!bt::build_map_nav(q, map->spec, "info_player_deathmatch", nav, out.error)) return;
  const sh::NavGraph& g = nav.graph;
  if (probe != nullptr) probe_map(nav, *probe, out);
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
  out.swim_links = g.counts.by_kind[static_cast<int>(sh::NavKind::swim)];
  for (const uint8_t h : nav.harm) out.harm_nodes += h;
  out.risky = nav.hardened.over;
  out.risky_removed = nav.hardened.removed;
  out.escapes = nav.escapes_made.held;
  out.escape_links = nav.escapes_made.links;
  out.escape_proofs = nav.escapes_made.proofs;
  // --escapes: each escape flown by the pilot from the liquid node at its takeoff to its ledge
  // (bots/pilot_run.hpp `run_route`: the move, the rocket's burst and blast as the proof's).
  for (size_t x = 0; fly_escapes && x < g.escapes.size(); x++) {
    const sh::NavEscape& e = g.escapes[x];
    sh::PilotSettings ps;
    ps.jumper.escapes = true;
    const sh::Vec3f to = g.nodes[static_cast<size_t>(e.to)].origin;
    const bt::RunResult r = bt::run_route(nav.world, g, e.from, to, sh::PilotHands::human,
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

  for (const sh::NavLink& l : g.links) {
    out.grenade_links += l.kind == sh::NavKind::grenade;
    if (l.kind == sh::NavKind::rocket) {
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
        const sh::NavNode& n = g.nodes[static_cast<size_t>(ga)];
        for (int32_t x = n.links - 1; x >= 0; x--) {
          const sh::NavLink& l =
              g.links[static_cast<size_t>(n.first_link) + static_cast<size_t>(x)];
          if (l.kind != sh::NavKind::grenade) continue;
          grenade_s = to_fixed(l.time_s, 2) + " (lead " + to_fixed(l.lead, 0) + ")";
          break;
        }
      }
      if (a >= 0) {
        out.tricks_held++;
        const sh::NavNode& n = g.nodes[static_cast<size_t>(a)];
        for (int32_t x = n.links - 1; x >= 0; x--) {
          const sh::NavLink& l =
              g.links[static_cast<size_t>(n.first_link) + static_cast<size_t>(x)];
          if (l.kind != sh::NavKind::rocket || l.level != tr.level) continue;
          jump_s = to_fixed(l.time_s, 2);
          const sh::Vec3f o = g.nodes[static_cast<size_t>(l.to)].origin;
          lands = to_fixed(o.x, 0) + " " + to_fixed(o.y, 0) + " " + to_fixed(o.z, 0);
          const sh::NavRoute r = sh::nav_route(g, a, l.to, rs);
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
  for (const sh::NavMover& m : g.movers) {
    MoverRow row{m.ent, m.model != nullptr, m.absmin, m.absmax, m.travel, 0};
    for (const sh::NavNode& nd : g.nodes)
      for (int32_t k = 0; k < nd.links; k++) {
        const sh::NavLink& l = g.links[static_cast<size_t>(nd.first_link) + static_cast<size_t>(k)];
        if (l.kind == sh::NavKind::ride && l.mover == m.ent) row.rides++;
      }
    out.movers.push_back(row);
  }
  std::vector<pg::EntityBlock> blocks;
  if (!pg::parse_entity_text(map->spec.entities.c_str(), blocks, out.error)) return;
  ge::ai::Search from, with_rockets;
  // What the main part reaches with rocket jumps too (from any one of its nodes).
  for (size_t k = 0; k < g.nodes.size(); k++)
    if (nav.main.reached(static_cast<int32_t>(k))) {
      sh::nav_reach(g, static_cast<int32_t>(k), with_rockets, {.rockets = true});
      break;
    }
  for (const pg::EntityBlock& b : blocks) {
    const char* cname = pg::block_value(b, "classname");
    const char* flags = pg::block_value(b, "spawnflags");
    bt::ItemSpot it;
    sh::Vec3f at;
    if (!cname || !bt::item_of(cname, flags ? std::atoi(flags) : 0, it) ||
        !vec_of(pg::block_value(b, "origin"), at))
      continue;
    out.items++;
    const int32_t n = sh::nav_nearest(g, {at.x, at.y, at.z + 24});
    const sh::Vec3f o = n >= 0 ? g.nodes[static_cast<size_t>(n)].origin : sh::Vec3f{};
    const float off =
        n >= 0 ? std::sqrt((o.x - at.x) * (o.x - at.x) + (o.y - at.y) * (o.y - at.y)) : 1e9f;
    const bool to = n >= 0 && nav.main_n > 0 && nav.main.reached(n);
    const bool rj = !to && n >= 0 && with_rockets.nodes() > 0 && with_rockets.reached(n);
    out.by_rocket += rj;
    bool back = false;
    if (n >= 0) {
      sh::nav_reach(g, n, from);
      for (size_t k = 0; k < g.nodes.size() && !back; k++)
        back = nav.main.reached(static_cast<int32_t>(k)) && !nav.wet[k] &&
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
    std::istringstream f(lab.arg("floors"));
    probe.floors = static_cast<bool>(f >> probe.fx0 >> probe.fy0 >> probe.fx1 >> probe.fy1);
    if (probe.floors && !(f >> probe.fstep)) probe.fstep = 8;
    // Each probe's point is a player's origin, its feet 24 under.
    probe.from.z += 24;
    probe.to.z += 24;
  }
  const std::string basedir = lab.arg("basedir");
  const bool fly = lab.arg("escapes") == "fly";
  if (basedir.empty()) {
    std::vector<uint8_t> bytes;
    if (!ge::brush::write_bsp(ge::brush::sample_fixture(), ge::brush::Family::v29, &bytes,
                              &error)) {
      lab.fail("se-reach: the fixture halls: " + error, "next: run tests/brush_write_test.cpp");
      return;
    }
    maps.emplace_back();
    reach_map(bytes, "halls", cn::kTeamHallsEntities, progs, &probe, fly, maps.back());
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
      reach_map(f, name, "", progs, maps.size() == 1 ? &probe : nullptr, fly, maps.back());
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
      "each map's rocket-jump tricks (people's, rocket_tricks.hpp) as the nav proved "
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
  lab.table(mv);
  gl::LabTable st{
      "each map's scripted routes to key items (bots/item_routes.hpp) as the move "
      "proved them: from the entry's node to the end's (feet), the time, the mover "
      "crossed",
      {"map", "route", "proof", "from", "to", "s", "mover"},
      {}};
  for (const MapReach& m : maps)
    for (const auto& row : m.script_rows) st.rows.push_back(row);
  if (!st.rows.empty()) lab.table(st);
  if (fly) {
    gl::LabTable et{
        "--escapes=fly: every rocket out of a pit's liquid (shooter/escape.hpp) flown by "
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
  if (probe.route && !maps.empty())
    lab.table({"--route on " + maps.front().name + ": each node (feet) and the link onto it",
               {"#", "node", "at (x y feet)", "link", "s", "jump speed"},
               maps.front().route_rows});
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
        {"floors", "",
         "the floors along a line on the first map: 'x0 y0 x1 y1 [step]' (the nav's misses)"},
        {"escapes", "", "fly: every rocket out of a pit's liquid flown by the pilot, tabled"},
    },
    "",
    run,
};
GE_LAB(kSpec);

}  // namespace
