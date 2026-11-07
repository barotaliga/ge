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
// ge:summary Lab: every item of every map asked: a route to it from the spawns, and one back
// ge:keywords lab se-reach reachability coverage nav-gaps items pockets unreachable route maps
// ge:keywords dm1 dm2 dm3 dm4 dm5 dm6 e1m2
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
  int64_t nodes = 0, swim_nodes = 0, swim_links = 0;
  std::vector<Gap> gaps;
  std::vector<MoverRow> movers;
};

bool vec_of(const char* s, sh::Vec3f& out) {
  if (!s) return false;
  std::istringstream in(s);
  return static_cast<bool>(in >> out.x >> out.y >> out.z);
}

void reach_map(std::span<const uint8_t> bytes, const std::string& name, const std::string& entities,
               const ge::progs::Progs& progs, MapReach& out) {
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
  out.nodes = static_cast<int64_t>(g.nodes.size());
  out.swim_nodes = nav.swim_nav.cells;
  out.swim_links = g.counts.by_kind[static_cast<int>(sh::NavKind::swim)];
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
      sh::nav_reach(g, static_cast<int32_t>(k), with_rockets, true);
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
  const std::string basedir = lab.arg("basedir");
  if (basedir.empty()) {
    std::vector<uint8_t> bytes;
    if (!ge::brush::write_bsp(ge::brush::sample_fixture(), ge::brush::Family::v29, &bytes,
                              &error)) {
      lab.fail("se-reach: the fixture halls: " + error, "next: run tests/brush_write_test.cpp");
      return;
    }
    maps.emplace_back();
    reach_map(bytes, "halls", cn::kTeamHallsEntities, progs, maps.back());
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
      reach_map(f, name, "", progs, maps.back());
    }
  }

  gl::LabTable t{
      "each map: its items, those a route leads to from the spawns, and from back",
      {"map", "nodes", "swim nodes", "swim links", "items", "reached", "by rocket", "back", "gaps"},
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
    t.rows.push_back({m.name, std::to_string(m.nodes), std::to_string(m.swim_nodes),
                      std::to_string(m.swim_links), std::to_string(m.items),
                      std::to_string(m.reached), std::to_string(m.by_rocket),
                      std::to_string(m.back), std::to_string(m.gaps.size())});
    lab.metric("reach." + m.name + ".items", m.items, "count");
    lab.metric("reach." + m.name + ".reached", m.reached, "count");
    lab.metric("reach." + m.name + ".back", m.back, "count");
    lab.metric("reach." + m.name + ".by-rocket", m.by_rocket, "count");
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
  lab.table(mv);
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
    },
    "",
    run,
};
GE_LAB(kSpec);

}  // namespace
