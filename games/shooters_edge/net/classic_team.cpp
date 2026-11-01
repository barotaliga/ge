// A recorded team game of bots over our server (classic_team.hpp).
//
// ge:summary The team game of bots: the map, the nav graph and its movers, the clients, the record
// ge:keywords run_team_match team-match movers doors plats fighters clients mvd digest
// ge:skill shooter bots net
// ge:test tests/shooters_edge_team_test.cpp
#include "shooters_edge/net/classic_team.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <utility>

#include "net/conditions.hpp"
#include "net/sim_network.hpp"
#include "progs/entity_text.hpp"
#include "shooter/locs.hpp"
#include "shooter/nav.hpp"
#include "shooter/swim.hpp"
#include "shooters_edge/bots/fighter.hpp"
#include "shooters_edge/bots/knowledge.hpp"
#include "shooters_edge/bots/mind.hpp"
#include "shooters_edge/bots/pilot_run.hpp"
#include "shooters_edge/bots/team.hpp"
#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/net/classic_client.hpp"
#include "shooters_edge/net/classic_record.hpp"
#include "shooters_edge/net/classic_server.hpp"
#include "shooters_edge/net/classic_wire.hpp"
#include "shooters_edge/server/commands.hpp"

namespace shooters_edge::net {

namespace {

namespace bt = shooters_edge::bots;
namespace gn = ge::net;
namespace pg = ge::progs;
namespace sv = shooters_edge::server;
namespace cl = shooters_edge::client;

constexpr double kHz = 1000.0 / bt::kPilotMsec;
constexpr int kTickUs = 13000;
constexpr int32_t kItQuad = 4194304;

uint64_t fold(uint64_t h, uint32_t w) { return (h ^ w) * 1099511628211ull; }

uint64_t fold_f(uint64_t h, float f) {
  uint32_t b;
  std::memcpy(&b, &f, 4);
  return fold(h, b);
}

// A door that starts open (spawnflags 1) stays where it spawned until a button moves it: a plate
// over a pit, a wall drawn aside.
bool start_open(const pg::EntityBlock& b) {
  const char* sf = pg::block_value(b, "spawnflags");
  return sf && (std::atoi(sf) & 1) != 0;
}

// Those doors as solids of the nav's world, where they stand (the rest of the movers are left
// out of it and tagged on the links they close).
void open_doors_as_floor(const sv::QcServer& s, const sv::MapSpec& spec, sh::MoveWorld& world) {
  std::vector<pg::EntityBlock> blocks;
  std::string error;
  if (!pg::parse_entity_text(spec.entities.c_str(), blocks, error)) return;
  for (const pg::EntityBlock& b : blocks) {
    const char* cn = pg::block_value(b, "classname");
    const char* model = pg::block_value(b, "model");
    if (!cn || !model || model[0] != '*' || std::strcmp(cn, "func_door") != 0 || !start_open(b))
      continue;
    const int m = std::atoi(model + 1);
    if (m <= 0 || static_cast<size_t>(m) >= spec.models.size() || world.count >= sh::kMaxSolids)
      continue;
    for (int e = s.sv.set.maxclients + 1; e < s.sv.num_edicts; e++) {
      const sh::Edict& d = s.sv.edicts[static_cast<size_t>(e)];
      if (d.free || d.modelindex <= 0 || static_cast<size_t>(d.modelindex) >= s.models.size() ||
          s.models[static_cast<size_t>(d.modelindex)] != model)
        continue;
      const sh::BrushModel& bm = spec.models[static_cast<size_t>(m)];
      sh::Solid& so = world.solids[world.count++];
      so = sh::Solid{};
      so.kind = sh::SolidKind::brush;
      so.origin = d.origin;
      so.point_hull = bm.hulls[0];
      so.clip_hull = bm.hulls[1];
      so.mins = bm.mins;
      so.maxs = bm.maxs;
      break;
    }
  }
}

// The map's doors and plats as the nav's movers, off the entity text's brush entities and the
// edicts the mod made of them (each found by its inline model's name).
std::vector<sh::NavMover> map_movers(const sv::QcServer& s, const sv::MapSpec& spec) {
  std::vector<sh::NavMover> out;
  std::vector<pg::EntityBlock> blocks;
  std::string error;
  if (!pg::parse_entity_text(spec.entities.c_str(), blocks, error)) return out;
  for (const pg::EntityBlock& b : blocks) {
    const char* cn = pg::block_value(b, "classname");
    const char* model = pg::block_value(b, "model");
    if (!cn || !model || model[0] != '*') continue;
    const bool door = std::strcmp(cn, "func_door") == 0;
    const bool plat = std::strcmp(cn, "func_plat") == 0;
    if (!door && !plat) continue;
    if (door && start_open(b)) continue;  // a floor to the nav (open_doors_as_floor)
    const int m = std::atoi(model + 1);
    if (m <= 0 || static_cast<size_t>(m) >= spec.models.size()) continue;
    int ent = -1;
    for (int e = s.sv.set.maxclients + 1; e < s.sv.num_edicts; e++) {
      const sh::Edict& d = s.sv.edicts[static_cast<size_t>(e)];
      if (d.free || d.modelindex <= 0 || static_cast<size_t>(d.modelindex) >= s.models.size())
        continue;
      if (s.models[static_cast<size_t>(d.modelindex)] == model) {
        ent = e;
        break;
      }
    }
    if (ent < 0) continue;
    sh::NavMover mv;
    mv.ent = ent;
    mv.absmin = s.sv.edicts[static_cast<size_t>(ent)].absmin;
    mv.absmax = s.sv.edicts[static_cast<size_t>(ent)].absmax;
    if (plat) {
      const sh::BrushModel& bm = spec.models[static_cast<size_t>(m)];
      const char* h = pg::block_value(b, "height");
      const char* sp = pg::block_value(b, "speed");
      const float rise = h ? static_cast<float>(std::atof(h)) : (bm.maxs.z - bm.mins.z) - 8;
      const float speed = sp ? static_cast<float>(std::atof(sp)) : 150;
      mv.model = &bm;
      mv.rest = s.sv.edicts[static_cast<size_t>(ent)].origin;  // lowered from where it was built
      mv.travel = {0, 0, rise};
      mv.ride_s = 1 + rise / (speed > 0 ? speed : 150);
    }
    out.push_back(mv);
  }
  return out;
}

// The map's teleporters as links: from every node whose player box touches a teleporter's trigger
// to the node at its destination (a teleporter another entity switches on, by its name, is left
// out: nothing says when it works).
std::vector<sh::NavExtraLink> teleport_links(const sh::NavGraph& g, const sv::MapSpec& spec) {
  std::vector<sh::NavExtraLink> out;
  std::vector<pg::EntityBlock> blocks;
  std::string error;
  if (!pg::parse_entity_text(spec.entities.c_str(), blocks, error)) return out;
  for (const pg::EntityBlock& b : blocks) {
    const char* cn = pg::block_value(b, "classname");
    const char* model = pg::block_value(b, "model");
    const char* target = pg::block_value(b, "target");
    if (!cn || std::strcmp(cn, "trigger_teleport") != 0 || !model || model[0] != '*' || !target ||
        pg::block_value(b, "targetname"))
      continue;
    const int m = std::atoi(model + 1);
    if (m <= 0 || static_cast<size_t>(m) >= spec.models.size()) continue;
    const sh::BrushModel& box = spec.models[static_cast<size_t>(m)];
    sh::Vec3f dest;
    bool found = false;
    for (const pg::EntityBlock& d : blocks) {
      const char* dn = pg::block_value(d, "targetname");
      const char* dc = pg::block_value(d, "classname");
      const char* org = pg::block_value(d, "origin");
      if (!dn || !dc || !org || std::strcmp(dn, target) != 0 ||
          std::strcmp(dc, "info_teleport_destination") != 0)
        continue;
      char* end = nullptr;
      dest.x = std::strtof(org, &end);
      dest.y = std::strtof(end, &end);
      dest.z = std::strtof(end, &end) + 27;  // the mod lifts a destination 27 units
      found = true;
      break;
    }
    if (!found) continue;
    const int32_t to = sh::nav_nearest(g, dest);
    if (to < 0) continue;
    for (size_t a = 0; a < g.nodes.size(); a++) {
      const sh::Vec3f o = g.nodes[a].origin;
      if (o.x + 16 < box.mins.x || o.x - 16 > box.maxs.x || o.y + 16 < box.mins.y ||
          o.y - 16 > box.maxs.y || o.z + 32 < box.mins.z || o.z - 24 > box.maxs.z)
        continue;
      out.push_back({static_cast<int32_t>(a), to, sh::NavKind::walk, 0.1f});
    }
  }
  return out;
}

struct Seat {
  bt::Fighter fighter;
  bool ready = false;  // the fighter initialised on its slot
  bool alive = false;
  int team = 0, index = 0;
  sh::AimSkill skill = sh::AimSkill::average;
  bool had_quad = false;
  size_t heard = 0;  // its client's prints read so far
};

// Place names off the items, for a map with no place names file: each key item, weapon and mega
// health's spot by its callout ("ra", "rl", "mh").
void locs_from_items(const std::vector<bt::ItemSpot>& items, sh::Locs& out) {
  for (const bt::ItemSpot& it : items) {
    if (!bt::key_item(it) && it.kind != bt::ItemKind::weapon && it.kind != bt::ItemKind::mega)
      continue;
    out.points.push_back({bt::item_call(it), it.at});
  }
  out.from = "the map's items";
}

}  // namespace

TeamMatchReport run_team_match(const TeamMatchOptions& opt) {
  TeamMatchReport r;
  if (!opt.progs) {
    r.error = "run_team_match: no progs; next: compile the baseline mod (baseline_progs)";
    return r;
  }
  std::unique_ptr<cl::LoadedMap> map = cl::map_from_bytes(opt.map, opt.map_name, r.error);
  if (!map) return r;
  if (!opt.entities.empty()) map->spec.entities = opt.entities;
  sv::QcServerOptions so;
  so.seed = opt.seed;
  so.loop.max_edicts = 1024;
  std::unique_ptr<sv::QcServer> qc = sv::make_qc_server(*opt.progs, so, r.error);
  if (!qc) return r;
  sv::server_command(*qc, "deathmatch 1");
  sv::server_command(*qc, "teamplay 1");
  sv::server_command(*qc, "timelimit " + std::to_string(opt.timelimit));
  if (!sv::load_map(*qc, map->spec, r.error)) return r;
  sv::use_map_sets(*qc, map->bsp);

  // The nav graph off the world and the movers.
  sh::MoveWorld world;
  sh::map_world(map->spec.models[0], world);
  open_doors_as_floor(*qc, map->spec, world);
  const std::vector<sh::NavMover> movers = map_movers(*qc, map->spec);
  sh::NavGraph graph;
  sh::SwimField swim;
  ge::ai::Search best;  // what the map's main part reaches (the largest a spawn spot reaches)
  int32_t best_n = 0;
  std::vector<uint8_t> wet;               // the nodes standing in water
  std::vector<sh::NavExtraLink> entries;  // the links into the water the graph closed
  const auto t0 = std::chrono::steady_clock::now();
  sh::build_nav(world, movers, sh::MoveSettings{}, sh::NavSettings{}, graph);
  const std::vector<sh::NavExtraLink> teleports = teleport_links(graph, map->spec);
  sh::nav_add_links(graph, teleports);
  r.teleport_links = static_cast<int64_t>(teleports.size());
  // Water: the links prove no swim, so a pool is closed to entry (no route leads into it, and an
  // item under water is out of reach); a player knocked in finds his own way out (the fighter).
  {
    wet.assign(graph.nodes.size(), 0);
    for (size_t n = 0; n < graph.nodes.size(); n++) {
      const sh::Vec3f o = graph.nodes[n].origin;
      wet[n] = sh::water_contents(world, {o.x, o.y, o.z - 10}) <= sh::kContentsWater ? 1 : 0;
    }
    r.closed_links = sh::nav_close_entries(graph, wet, &entries);
    // The way out of the water: to any dry node of the map's main part (the largest a spawn
    // spot reaches).
    std::vector<pg::EntityBlock> blocks;
    std::string perr;
    ge::ai::Search sr;
    if (pg::parse_entity_text(map->spec.entities.c_str(), blocks, perr))
      for (const pg::EntityBlock& b : blocks) {
        const char* cn = pg::block_value(b, "classname");
        const char* org = pg::block_value(b, "origin");
        if (!cn || !org || std::strcmp(cn, "info_player_deathmatch") != 0) continue;
        char* end = nullptr;
        sh::Vec3f o;
        o.x = std::strtof(org, &end);
        o.y = std::strtof(end, &end);
        o.z = std::strtof(end, &end);
        const int32_t k = sh::nav_reach(graph, sh::nav_nearest(graph, o), sr);
        if (k > best_n) {
          best_n = k;
          std::swap(best, sr);
        }
      }
    std::vector<uint8_t> exits(graph.nodes.size(), 0);
    for (size_t n = 0; n < graph.nodes.size(); n++)
      exits[n] = best_n > 0 && best.reached(static_cast<int32_t>(n)) && !wet[n] ? 1 : 0;
    sh::build_swim(world, graph, exits, sh::SwimSettings{}, swim);
    r.swim_wet = swim.wet;
    r.swim_leading = swim.leading;
  }
  r.nav_build_ms =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
  r.lo = graph.lo;
  r.hi = graph.hi;
  r.nav_nodes = static_cast<int64_t>(graph.nodes.size());
  r.nav_links = static_cast<int64_t>(graph.links.size());
  r.movers = static_cast<int64_t>(movers.size());
  if (graph.nodes.empty()) {
    r.error = "run_team_match: the nav graph has no nodes; next: run the se-nav lab on the map";
    return r;
  }

  // The server over the simulated network, recording.
  gn::LinkConditions lan;
  gn::conditions_preset("lan", lan);
  gn::SimNetwork net = gn::make_sim_network(opt.seed, lan);
  gn::Transport st;
  gn::open_sim_transport(st, net, gn::sim_address(0, kPortServer));
  auto server = std::make_unique<ClassicServer>();
  ServerConfig cfg;
  cfg.hostname = "Shooter's Edge bots 4on4";
  if (!classic_start(*server, *qc, st, cfg, r.error)) return r;
  MultiviewRecorder mvd;
  if (opt.record) {
    mvd_start(mvd, false);
    server->mvd = &mvd;
  }

  // The players: a client each, a fighter at its controls once its slot is in the game.
  int n = 0;
  for (const TeamSide& side : opt.teams) n += static_cast<int>(side.skills.size());
  if (n < 2 || n > 16) {
    r.error = "run_team_match: 2..16 players; next: pass fewer skills";
    return r;
  }
  std::vector<Seat> seats(static_cast<size_t>(n));
  sh::Locs locs;  // the map's place names, every fighter's
  std::vector<gn::Transport> ct(static_cast<size_t>(n));
  auto clients = std::make_unique<ClassicClientEnd[]>(static_cast<size_t>(n));
  bt::Team teams[2];
  r.players.resize(static_cast<size_t>(n));
  sv::QcServer& s = *qc;
  {
    int k = 0;
    for (int t = 0; t < 2; t++) {
      const TeamSide& side = opt.teams[t];
      teams[t].name = side.name;
      for (size_t i = 0; i < side.skills.size(); i++, k++) {
        Seat& seat = seats[static_cast<size_t>(k)];
        seat.team = t;
        seat.index = k;
        seat.skill = side.skills[i];
        TeamPlayer& p = r.players[static_cast<size_t>(k)];
        p.name = i < side.names.size() ? side.names[i] : side.name + std::to_string(i + 1);
        p.team = t;
        p.skill = seat.skill;
        if (i < side.tempers.size()) p.temper = side.tempers[i];
        gn::open_sim_transport(ct[static_cast<size_t>(k)], net,
                               gn::sim_address(static_cast<uint32_t>(k + 1), kPortClient));
        ClientConfig cc;
        cc.name = p.name;
        cc.team = side.name;
        cc.topcolor = cc.bottomcolor = side.colour;
        cc.qport = static_cast<uint16_t>(200 + k);
        cc.seed = k + 1;
        ClassicClientEnd* c = &clients[static_cast<size_t>(k)];
        Seat* sp = &seat;
        cc.brain = [sp, c, &s](int) -> sh::MoveCmd {
          if (!sp->ready) return {};
          // The team lines its client was sent, read (or missed) by its own mind.
          for (; sp->heard < c->prints.size(); sp->heard++) {
            std::string who, words;
            if (bt::split_team_line(c->prints[sp->heard], who, words))
              bt::mind_hear(sp->fighter, who, words, s.sv.time);
          }
          const sh::MoveCmd cmd = bt::fighter_command(sp->fighter, s, c->playernum, bt::kPilotMsec);
          // The fighter's team messages, as a player types them.
          for (const std::string& line : sp->fighter.outbox)
            client_command(*c, "say_team \"" + line + "\"");
          sp->fighter.outbox.clear();
          return cmd;
        };
        client_start(*c, ct[static_cast<size_t>(k)], gn::sim_address(0, kPortServer), cc, 0);
      }
    }
  }

  // The dives: a field through the water into each pocket that holds an item no route reaches,
  // and the node of the main part it is entered from; made once the items are known.
  struct Dive {
    std::vector<uint8_t> pocket;  // the pocket's dry nodes
    sh::SwimField field;
    int32_t node = -1, entry = -1;  // the node it is entered from, and the wet node it drops to
    float swim_s = 0;
  };
  std::vector<std::unique_ptr<Dive>> dives;
  std::vector<int32_t> item_dive;  // each item's dive, -1 none
  bool dives_made = false;
  const std::vector<sh::Vec3f>* known_items = nullptr;
  int64_t now = 0;
  uint64_t h = 1469598103934665603ull;
  for (int frame = 0; frame < opt.frames; frame++) {
    now += kTickUs;
    gn::sim_advance(net, now);
    s.sv.sound_count = 0;
    classic_frame(*server, now);
    // Fighters take their slots once their players are in the game.
    for (int k = 0; k < n; k++) {
      Seat& seat = seats[static_cast<size_t>(k)];
      const int slot = clients[k].playernum;
      if (seat.ready || slot < 0 || !server->clients[slot].qc_active) continue;
      bt::fighter_init(seat.fighter, graph, s, slot, map->spec.entities.c_str(), seat.skill, kHz,
                       opt.seed + 31u * static_cast<uint32_t>(k));
      seat.fighter.bot.yaw = s.sv.edicts[static_cast<size_t>(slot) + 1].v_angle.y;
      seat.fighter.temper = r.players[static_cast<size_t>(k)].temper;
      seat.fighter.name = r.players[static_cast<size_t>(k)].name;
      if (locs.points.empty()) {
        if (!opt.locs.empty() && sh::parse_locs(opt.locs, locs) > 0) {
          locs.from = opt.locs_from;
        } else {
          locs_from_items(seat.fighter.items, locs);
        }
        r.locs_from = locs.from;
        r.locs = static_cast<int64_t>(locs.points.size());
      }
      seat.fighter.locs = &locs;
      seat.ready = true;
      if (!dives_made && swim.leading > 0 && best_n > 0) {
        dives_made = true;
        const std::vector<bt::ItemSpot>& items = seat.fighter.items;
        item_dive.assign(items.size(), -1);
        for (size_t i = 0; i < items.size(); i++) {
          const int32_t node = items[i].node;
          if (node < 0 || best.reached(node)) continue;
          for (size_t d = 0; d < dives.size() && item_dive[i] < 0; d++)
            if (dives[d]->pocket[static_cast<size_t>(node)]) item_dive[i] = static_cast<int32_t>(d);
          if (item_dive[i] >= 0) continue;
          auto dv = std::make_unique<Dive>();
          ge::ai::Search pr;
          sh::nav_reach(graph, node, pr);
          dv->pocket.assign(graph.nodes.size(), 0);
          for (size_t m = 0; m < graph.nodes.size(); m++)
            dv->pocket[m] = pr.reached(static_cast<int32_t>(m)) && !wet[m] &&
                                    !best.reached(static_cast<int32_t>(m))
                                ? 1
                                : 0;
          dv->pocket[static_cast<size_t>(node)] = 1;
          sh::build_swim(world, graph, dv->pocket, sh::SwimSettings{}, dv->field, &swim);
          // Entered by a way into the water the graph closed, from a node of the main part that
          // reaches most of it again (not a pit dropped into), the one nearest the pocket by water.
          int32_t steps = -1;
          ge::ai::Search back;
          for (const sh::NavExtraLink& e : entries) {
            if (!best.reached(e.from)) continue;
            const int32_t way =
                sh::swim_steps(dv->field, graph.nodes[static_cast<size_t>(e.to)].origin);
            if (way < 0 || (steps >= 0 && way >= steps)) continue;
            if (sh::nav_reach(graph, e.from, back) * 10 < best_n * 9) continue;
            steps = way;
            dv->node = e.from;
            dv->entry = e.to;
          }
          if (dv->node < 0) continue;
          dv->swim_s = static_cast<float>(steps) * dv->field.cell / 200.f;  // a swimmer's pace
          item_dive[i] = static_cast<int32_t>(dives.size());
          dives.push_back(std::move(dv));
        }
        r.dives = static_cast<int64_t>(dives.size());
      }
      for (size_t i = 0; i < item_dive.size() && i < seat.fighter.items.size(); i++) {
        if (item_dive[i] < 0) continue;
        const Dive& dv = *dives[static_cast<size_t>(item_dive[i])];
        seat.fighter.items[i].dive = &dv.field;
        seat.fighter.items[i].dive_node = dv.node;
        seat.fighter.items[i].dive_entry = dv.entry;
        seat.fighter.items[i].dive_s = dv.swim_s;
      }
      r.players[static_cast<size_t>(k)].slot = slot;
      if (r.nav_reached.empty()) {
        ge::ai::Search search;
        sh::nav_reach(graph,
                      sh::nav_nearest(graph, s.sv.edicts[static_cast<size_t>(slot) + 1].origin),
                      search);
        r.nav_reached.assign(graph.nodes.size(), 0);
        for (size_t i = 0; i < graph.nodes.size(); i++)
          r.nav_reached[i] = search.reached(static_cast<int32_t>(i)) ? 1 : 0;
      }
      bt::Team& tm = teams[seat.team];
      tm.members.push_back(&seat.fighter);
      seat.fighter.team = &tm;
      if (swim.leading > 0) seat.fighter.swim = &swim;
      bt::team_init(tm);
      for (bt::Fighter* f : tm.members) f->bot.senses.friends = tm.friends;
    }
    for (bt::Team& tm : teams) bt::team_count(tm);
    for (int k = 0; k < n; k++) client_tick(clients[k], now);
    r.frames = frame + 1;

    // The report's frame.
    for (int k = 0; k < n; k++) {
      Seat& seat = seats[static_cast<size_t>(k)];
      TeamPlayer& p = r.players[static_cast<size_t>(k)];
      if (!seat.ready) continue;
      const sh::Edict& d = s.sv.edicts[static_cast<size_t>(p.slot) + 1];
      const bool live = d.health > 0;
      if (seat.alive && !live) p.deaths++;
      seat.alive = live;
      const bool quad = (static_cast<int32_t>(bt::read_stats(s, p.slot + 1).items) & kItQuad) != 0;
      if (quad && !seat.had_quad) r.teams[seat.team].quad_pickups++;
      seat.had_quad = quad;
      if (frame % opt.path_every == 0) {
        p.path.push_back(d.origin);
        p.roles.push_back(live ? static_cast<uint8_t>(seat.fighter.role) : 255);
        p.known.push_back(static_cast<int16_t>(bt::items_known(seat.fighter)));
      }
      if (live) {
        p.role_commands[static_cast<int>(seat.fighter.role)]++;
        p.pilot_modes[static_cast<int>(seat.fighter.bot.pilot.pilot.mode)]++;
      }
      h = fold_f(fold_f(fold_f(fold_f(h, d.origin.x), d.origin.y), d.origin.z), d.health);
    }
    if (frame % opt.path_every == 0) {
      float tf[2] = {0, 0};
      for (const TeamPlayer& p : r.players)
        if (p.slot >= 0)
          tf[p.team] += pg::as_float(*pg::entity_field(s.q.vm, p.slot + 1, s.q.layout->f.frags));
      r.team_frags[0].push_back(tf[0]);
      r.team_frags[1].push_back(tf[1]);
    }
    if (!known_items)
      for (const Seat& seat : seats)
        if (seat.ready) {
          r.item_at.clear();
          r.item_name.clear();
          r.item_node_at.clear();
          r.item_routed.clear();
          for (const bt::ItemSpot& it : seat.fighter.items) {
            r.item_at.push_back(it.at);
            r.item_name.push_back(it.name);
            r.item_node_at.push_back(it.node >= 0 ? graph.nodes[static_cast<size_t>(it.node)].origin
                                                  : sh::Vec3f{});
            r.item_routed.push_back(it.node >= 0 && best_n > 0 && best.reached(it.node));
          }
          r.buttons.clear();
          for (const bt::ButtonSpot& b : seat.fighter.button_spots) {
            TeamButton tb;
            tb.at = b.at;
            tb.shoot = b.shoot;
            for (int d : b.doors) {
              const sh::Edict& de = s.sv.edicts[static_cast<size_t>(d)];
              tb.doors.push_back({(de.absmin.x + de.absmax.x) * 0.5f,
                                  (de.absmin.y + de.absmax.y) * 0.5f,
                                  (de.absmin.z + de.absmax.z) * 0.5f});
            }
            for (int32_t i : b.opens)
              tb.opens.push_back(seat.fighter.items[static_cast<size_t>(i)].name);
            if (b.node >= 0) tb.from = graph.nodes[static_cast<size_t>(b.node)].origin;
            r.buttons.push_back(std::move(tb));
          }
          known_items = &r.item_at;
          break;
        }
    if (s.map_stopped) {
      r.error = "the mod stopped the map: " + s.stop_reason +
                "; next: read the console's last lines and the fault's QC stack";
      break;
    }
    if (!s.pending_map.empty()) break;  // the match ended and the mod asked for the map again
  }
  if (opt.record) {
    mvd_stop(mvd);
    r.mvd = std::move(mvd.bytes);
  }
  r.seconds = s.sv.time;
  for (int k = 0; k < n; k++) {
    TeamPlayer& p = r.players[static_cast<size_t>(k)];
    const Seat& seat = seats[static_cast<size_t>(k)];
    if (p.slot >= 0)
      p.frags = pg::as_float(*pg::entity_field(s.q.vm, p.slot + 1, s.q.layout->f.frags));
    const bt::FighterStats& fs = seat.fighter.stats;
    p.shots = fs.shots;
    p.pickups = fs.pickups;
    p.switches = fs.switches;
    p.packs = fs.packs;
    p.held_fire = fs.held_fire;
    p.dives = seat.fighter.dives;
    p.dives_given_up = fs.dives_given_up;
    p.dive_wet_s = fs.dive_wet_s;
    p.off_graph = seat.fighter.lost;
    for (int m = 0; m < 4; m++) p.mode_commands[m] = fs.mode_commands[m];
    for (int w = 0; w < 9; w++) p.weapon_commands[w] = fs.weapon_commands[w];
    const sh::PilotStats& ps = seat.fighter.bot.pilot.pilot.stats;
    if (p.slot >= 0) {
      const bt::Fighter& f = seat.fighter;
      const sh::Vec3f o = s.sv.edicts[static_cast<size_t>(p.slot) + 1].origin;
      char b[256];
      const sh::Vec3f gn =
          f.goal_node >= 0 ? graph.nodes[static_cast<size_t>(f.goal_node)].origin : sh::Vec3f{};
      std::snprintf(b, sizeof b,
                    "at %.0f %.0f %.0f, %s, goal node %.0f %.0f %.0f, item %s%s, pilot %s, buttons "
                    "%lld, rockets held %lld commands",
                    o.x, o.y, o.z, bt::role_name(f.role), gn.x, gn.y, gn.z,
                    f.goal_item >= 0 ? f.items[static_cast<size_t>(f.goal_item)].name.c_str() : "-",
                    f.unlocking ? " (button first)" : "",
                    sh::pilot_mode_name(f.bot.pilot.pilot.mode), static_cast<long long>(f.buttons),
                    static_cast<long long>(fs.weapon_commands[7]));
      p.last = b;
    }
    p.plans = ps.plans;
    p.stuck = ps.stuck;
    p.lost = ps.lost;
    p.waits = ps.waits;
    p.barred = ps.barred;
    for (size_t i = 1; i < p.path.size(); i++) {
      const sh::Vec3f d = p.path[i] - p.path[i - 1];
      const float l = std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
      if (l < 160) p.travelled += l;
    }
    r.teams[p.team].frags += p.frags;
    r.teams[p.team].deaths += p.deaths;
  }
  for (int t = 0; t < 2; t++) {
    const bt::TeamStats& ts = teams[t].stats;
    r.teams[t].plans = ts.plans;
    r.teams[t].rushes = ts.rushes;
    r.teams[t].quads = ts.quads;
    r.teams[t].held_shots = ts.held_shots;
    r.teams[t].callouts = ts.callouts;
    r.teams[t].messages = ts.messages;
    r.teams[t].lost_seen = ts.lost_seen;
    r.teams[t].retakes = ts.retakes;
    r.teams[t].spam_shots = ts.spam_shots;
  }
  for (size_t k = 0; k < seats.size(); k++) {
    const bt::Mind& mn = seats[k].fighter.mind;
    r.players[k].read = mn.read;
    r.players[k].missed = mn.missed;
    const bt::Fighter& fi = seats[k].fighter;
    r.players[k].sounds = fi.hear_stats.sounds;
    r.players[k].heard = fi.hear_stats.taken;
    r.players[k].heard_taken = fi.stats.heard_taken;
    r.players[k].heard_spawns = fi.stats.heard_spawns;
    r.players[k].spam_shots = fi.stats.spam_shots;
    r.players[k].outmatched = fi.stats.outmatched;
    r.players[k].sound_chases = fi.stats.sound_chases;
    r.lines_read += mn.read;
    r.lines_missed += mn.missed;
  }
  // The teams' messages, merged in time order (a tie: the first team first).
  {
    std::vector<std::pair<double, std::string>> lines;
    for (int t = 0; t < 2; t++)
      for (const bt::TeamLine& l : teams[t].log) {
        std::string who = "?";
        for (const TeamPlayer& p : r.players)
          if (p.slot == l.slot) who = p.name;
        char at[32];
        std::snprintf(at, sizeof at, "%6.1f ", l.t);
        lines.push_back({l.t, at + teams[t].name + " " + who + ": " + l.text});
      }
    std::stable_sort(lines.begin(), lines.end(),
                     [](const auto& a, const auto& b) { return a.first < b.first; });
    for (auto& l : lines) r.chat.push_back(std::move(l.second));
    for (const bt::TeamLine& l : teams[0].log)
      for (TeamPlayer& p : r.players) p.said += p.slot == l.slot ? 1 : 0;
    for (const bt::TeamLine& l : teams[1].log)
      for (TeamPlayer& p : r.players) p.said += p.slot == l.slot ? 1 : 0;
  }
  r.hold_item = teams[0].hold_item;
  r.quad_item = teams[0].quad_item;
  r.frag_log = s.frag_log;
  r.faults = s.q.faults;
  r.digest = h;
  r.ok = r.error.empty();
  return r;
}

const char* const kTeamHallsEntities = R"({
"classname" "worldspawn"
"message" "The fixture halls"
}
{
"classname" "info_player_deathmatch"
"origin" "64 64 24"
"angle" "0"
}
{
"classname" "info_player_deathmatch"
"origin" "448 64 24"
"angle" "90"
}
{
"classname" "info_player_deathmatch"
"origin" "64 320 24"
"angle" "270"
}
{
"classname" "info_player_deathmatch"
"origin" "448 320 24"
"angle" "180"
}
{
"classname" "info_player_deathmatch"
"origin" "800 128 24"
"angle" "90"
}
{
"classname" "info_player_deathmatch"
"origin" "800 400 24"
"angle" "180"
}
{
"classname" "info_player_deathmatch"
"origin" "224 700 24"
"angle" "270"
}
{
"classname" "info_player_deathmatch"
"origin" "680 760 24"
"angle" "180"
}
{
"classname" "item_armor2"
"origin" "544 672 8"
}
{
"classname" "item_artifact_super_damage"
"origin" "800 256 32"
}
{
"classname" "weapon_rocketlauncher"
"origin" "256 96 8"
}
{
"classname" "item_health"
"origin" "224 520 8"
"spawnflags" "2"
}
{
"classname" "item_health"
"origin" "96 192 8"
}
{
"classname" "item_health"
"origin" "640 400 8"
}
{
"classname" "item_health"
"origin" "420 760 8"
}
{
"classname" "item_shells"
"origin" "400 300 8"
}
{
"classname" "item_rockets"
"origin" "160 300 8"
}
{
"classname" "item_rockets"
"origin" "700 600 8"
}
{
"classname" "weapon_supershotgun"
"origin" "600 120 8"
}
{
"classname" "weapon_grenadelauncher"
"origin" "450 600 8"
}
{
"classname" "item_spikes"
"origin" "260 640 8"
}
{
"classname" "weapon_supernailgun"
"origin" "700 300 8"
}
)";

}  // namespace shooters_edge::net
