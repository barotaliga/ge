// A deathmatch on a game's own map and program (pack_fight.hpp).
//
// voice: agent (a lab's harness: its errors land in the lab's report; docs/architecture.md § Player
// text).
//
// ge:summary The pack deathmatch: the campaign's server in deathmatch, fighters on the map's nav
// ge:keywords run_pack_fight deathmatch teamplay fighters map-nav items-taken intermission
// ge:skill shooter bots
// ge:test tests/shooters_edge_fight_test.cpp
#include "shooters_edge/bots/pack_fight.hpp"

#include <cstring>
#include <memory>

#include "progs/vm.hpp"
#include "shooterbots/map_nav.hpp"
#include "shooterbots/way_through.hpp"
#include "shooters_edge/bots/quake_maps.hpp"
#include "shooters_edge/server/lobby.hpp"

namespace shooters_edge::bots {

namespace {

namespace pg = ge::progs;
namespace sv = shooters_edge::server;

constexpr int kMsec = 14;  // a command's length on the 1/72 s host
constexpr double kHz = 1000.0 / kMsec;

uint64_t fold(uint64_t h, uint32_t w) { return (h ^ w) * 1099511628211ull; }

uint64_t fold_f(uint64_t h, float f) {
  uint32_t b;
  std::memcpy(&b, &f, 4);
  return fold(h, b);
}

uint64_t frame_digest(const sv::RulesCommon& s, int players) {
  uint64_t h = 1469598103934665603ull;
  uint64_t t;
  std::memcpy(&t, &s.sv.time, 8);
  h = fold(fold(h, static_cast<uint32_t>(t)), static_cast<uint32_t>(t >> 32));
  for (int e = 1; e <= players; e++) {
    const sv::WorldEntity& d = s.world.entities[static_cast<size_t>(e)];
    h = fold_f(fold_f(fold_f(h, d.origin.x), d.origin.y), d.origin.z);
    h = fold_f(fold_f(h, d.view_angles.x), d.view_angles.y);
    h = fold_f(h, d.health);
  }
  return fold(h, static_cast<uint32_t>(s.sv.num_edicts));
}

struct Recent {
  sh::MoveCmd cmds[3];
  uint32_t sequence = 0;
};

}  // namespace

PackFightReport run_pack_fight(const PackFightOptions& opt) {
  PackFightReport out;
  FightReport& r = out.fight;
  const int n = static_cast<int>(opt.skills.size());
  if (n < 1 || n > 16 || !opt.load || !opt.progs) {
    r.error = "run_pack_fight: 1..16 fighters, a map loader and a program; next: check the options";
    return out;
  }
  sv::Campaign c;
  sv::CampaignOptions co;
  co.server.seed = opt.seed;
  co.server.loop.max_edicts = 1024;
  co.load = opt.load;
  co.load_ctx = opt.load_ctx;
  co.wide_models = opt.wide_models;
  co.registered = opt.registered;
  co.settings = {"coop 0",
                 "deathmatch " + std::to_string(opt.deathmatch),
                 "teamplay " + std::to_string(opt.teamplay),
                 "timelimit " + std::to_string(opt.timelimit),
                 "fraglimit " + std::to_string(opt.fraglimit),
                 "maxplayers " + std::to_string(n)};
  if (!sv::campaign_new_game(c, *opt.progs, co, opt.map, r.error)) {
    r.error = "run_pack_fight: " + r.error;
    return out;
  }
  sv::Lobby lobby;
  for (int k = 0; k < n; k++) {
    sv::campaign_join(c, k, "bot" + std::to_string(k + 1));
    if (opt.teamplay != 0) {
      lobby.seats[static_cast<size_t>(k)].used = true;
      lobby.seats[static_cast<size_t>(k)].team = k % 2 == 0 ? sv::Team::red : sv::Team::blue;
      sv::lobby_apply(c.server, lobby, k);
    }
  }
  const sv::QcServer* q = sv::qc_of(c.server);
  if (!q) {
    r.error = "run_pack_fight: the campaign runs no QC server; next: pass a progs program";
    return out;
  }
  MapNav nav;
  if (!build_map_nav(*q, c.map, quake_maps(), "info_player_deathmatch", nav, r.error)) {
    r.error = "run_pack_fight: " + c.map.name + "'s nav: " + r.error;
    return out;
  }
  r.lo = nav.graph.lo;
  r.hi = nav.graph.hi;
  for (const sb::NavNode& nd : nav.graph.nodes) out.nodes.push_back(nd.origin);
  const sv::RulesCommon& s0 = *c.server;
  std::vector<Fighter> fighters(static_cast<size_t>(n));
  std::vector<sb::Wedge> wedges(static_cast<size_t>(n));
  r.players.resize(static_cast<size_t>(n));
  for (int k = 0; k < n; k++) {
    FightPlayer& p = r.players[static_cast<size_t>(k)];
    p.name = "bot" + std::to_string(k + 1);
    p.skill = opt.skills[static_cast<size_t>(k)];
    p.traits = sb::traits_of(p.skill, p.temper);
    Fighter& f = fighters[static_cast<size_t>(k)];
    f.features = sb::features_of(p.skill);
    fighter_init(f, nav.graph, s0, k, c.map.entities.c_str(), p.skill, p.traits, kHz,
                 opt.seed + 31u * static_cast<uint32_t>(k), first_arsenal());
    f.bot.yaw = s0.world.entities[static_cast<size_t>(k) + 1].view_angles.y;
    f.bot.pilot.world = &nav.world;
    if (nav.swim.leading > 0) f.swim = &nav.swim;
    f.trap = &nav.trap;
    f.wet = &nav.wet;
  }
  const pg::Def* inter = pg::find_global(*opt.progs, "intermission_running");

  std::vector<Recent> recent(static_cast<size_t>(n));
  std::vector<sh::Packet> packets(static_cast<size_t>(n));
  std::vector<bool> alive(static_cast<size_t>(n), true);
  // Every item the map spawned (a weapon_ or item_ class, the pack's own too), by the server's
  // entities: taken when its model goes, put down to the bot nearest it within 96 units.
  struct Item {
    int ent;
    std::string name;
    sh::Vec3f at;
    int model;
  };
  std::vector<Item> items;
  for (int e = n + 1; e < s0.sv.num_edicts && static_cast<size_t>(e) < q->spawned_as.size(); e++) {
    const std::string& cls = q->spawned_as[static_cast<size_t>(e)];
    const sv::WorldEntity& d = s0.world.entities[static_cast<size_t>(e)];
    if (d.free || d.modelindex <= 0) continue;
    if (cls.rfind("weapon_", 0) != 0 && cls.rfind("item_", 0) != 0) continue;
    items.push_back({e, cls, (d.absmin + d.absmax) * 0.5f, d.modelindex});
    r.items.push_back({cls, items.back().at});
  }
  const size_t entered = c.history.size();
  for (int frame = 0; frame < opt.frames; frame++) {
    const sv::RulesCommon& s = *c.server;
    const sv::QcServer* qs = sv::qc_of(c.server);
    if (inter && qs && pg::global_float(qs->q.vm, inter->offset) != 0) {
      out.intermission = true;
      break;
    }
    for (int k = 0; k < n; k++) {
      Recent& rc = recent[static_cast<size_t>(k)];
      rc.cmds[0] = rc.cmds[1];
      rc.cmds[1] = rc.cmds[2];
      rc.cmds[2] = fighter_command(fighters[static_cast<size_t>(k)], s, k, kMsec);
      if (!fighters[static_cast<size_t>(k)].bot.pilot.pilot.climbing)
        sb::unwedge(wedges[static_cast<size_t>(k)], s, k + 1, s.sv.time, rc.cmds[2]);
      // A dead fighter presses attack every other command: Quake's rules stand a player again
      // only on a press after every button is let go.
      if (s.world.entities[static_cast<size_t>(k) + 1].health <= 0)
        rc.cmds[2].buttons = (frame & 1) ? sh::kButtonAttack : 0;
      sh::Packet& p = packets[static_cast<size_t>(k)];
      p.client = k;
      p.sequence = ++rc.sequence;
      for (int i = 0; i < 3; i++) p.cmds[i] = rc.cmds[i];
    }
    if (!sv::campaign_frame(c, packets.data(), n, r.error)) {
      r.error = "run_pack_fight: " + c.map.name + ": " + r.error;
      return out;
    }
    if (c.history.size() != entered) {  // the program changed the level: the match is over
      out.intermission = true;
      break;
    }
    r.frames = frame + 1;
    const sv::RulesCommon& now = *c.server;
    for (int k = 0; k < n; k++) {
      const sv::WorldEntity& d = now.world.entities[static_cast<size_t>(k) + 1];
      FightPlayer& p = r.players[static_cast<size_t>(k)];
      p.path.push_back(d.origin);
      const bool live = d.health > 0;
      p.mode.push_back(live ? static_cast<uint8_t>(fighters[static_cast<size_t>(k)].mode) : 255);
      p.frags_at.push_back(now.world.clients[static_cast<size_t>(k)].numbers.frags);
      if (alive[static_cast<size_t>(k)] && !live) r.deaths.push_back({frame, k, d.origin});
      alive[static_cast<size_t>(k)] = live;
    }
    for (Item& it : items) {  // the server's pickups
      const int model = now.world.entities[static_cast<size_t>(it.ent)].modelindex;
      if (it.model > 0 && model <= 0) {
        int best = -1;
        float best_d = 96 * 96;
        for (int k = 0; k < n; k++) {
          const sh::Vec3f d = now.world.entities[static_cast<size_t>(k) + 1].origin - it.at;
          if (d.x * d.x + d.y * d.y < best_d) {
            best_d = d.x * d.x + d.y * d.y;
            best = k;
          }
        }
        if (best >= 0) {
          r.players[static_cast<size_t>(best)].took++;
          out.taken[it.name]++;
        }
      }
      it.model = model;
    }
    r.digests.push_back(frame_digest(now, n));
  }

  for (int k = 0; k < n; k++) {
    FightPlayer& p = r.players[static_cast<size_t>(k)];
    p.frags = p.frags_at.empty() ? 0 : p.frags_at.back();
    p.stats = fighters[static_cast<size_t>(k)].stats;
  }
  for (const FightDeath& d : r.deaths) r.players[static_cast<size_t>(d.victim)].deaths++;
  r.frag_log = c.server->frag_log;
  if (const sv::QcServer* qs = sv::qc_of(c.server)) {
    r.faults = qs->q.faults;
    out.last_fault = qs->q.last_fault;
  }
  r.final_digest = r.digests.empty() ? 0 : r.digests.back();
  r.ok = r.error.empty();
  return out;
}

}  // namespace shooters_edge::bots
