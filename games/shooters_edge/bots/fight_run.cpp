// The deathmatch of fighters (fight_run.hpp): the mod server over the arena with the baseline mod,
// the nav graph off its world and movers, and a fighter in each client slot.
//
// ge:summary The deathmatch of fighters: run_fight on the baseline mod over the arena
// ge:keywords run_fight fight-run deathmatch fighters packets digest deaths
// ge:skill shooter bots
// ge:test tests/shooters_edge_fight_test.cpp
#include "shooters_edge/bots/fight_run.hpp"

#include <cstring>

#include "shooter/nav.hpp"
#include "shooters_edge/bots/pilot_run.hpp"
#include "shooters_edge/server/baseline_mod.hpp"
#include "shooters_edge/server/commands.hpp"
#include "shooters_edge/server/mod_match.hpp"

namespace shooters_edge::bots {

namespace {

constexpr double kHz = 1000.0 / kPilotMsec;
constexpr float kPlatRise = 96, kPlatSpeed = 64;  // the arena's plat in the baseline mod's text

uint64_t fold(uint64_t h, uint32_t w) { return (h ^ w) * 1099511628211ull; }

uint64_t fold_f(uint64_t h, float f) {
  uint32_t b;
  std::memcpy(&b, &f, 4);
  return fold(h, b);
}

uint64_t frame_digest(const server::QcServer& s, int players) {
  uint64_t h = 1469598103934665603ull;
  uint64_t t;
  std::memcpy(&t, &s.sv.time, 8);
  h = fold(h, static_cast<uint32_t>(t));
  h = fold(h, static_cast<uint32_t>(t >> 32));
  for (int e = 1; e <= players; e++) {
    const sh::Edict& d = s.sv.edicts[static_cast<size_t>(e)];
    h = fold_f(fold_f(fold_f(h, d.origin.x), d.origin.y), d.origin.z);
    h = fold_f(fold_f(h, d.v_angle.x), d.v_angle.y);
    h = fold_f(h, d.health);
  }
  return h;
}

// The entity on inline model `name` ("*1"), or -1.
int entity_on(const server::QcServer& s, const char* name) {
  for (int e = 1; e < s.sv.num_edicts; e++) {
    const sh::Edict& d = s.sv.edicts[static_cast<size_t>(e)];
    if (d.free || d.modelindex <= 0 || d.modelindex >= static_cast<int>(s.models.size())) continue;
    if (s.models[static_cast<size_t>(d.modelindex)] == name) return e;
  }
  return -1;
}

}  // namespace

FightReport run_fight(const FightOptions& opt) {
  FightReport r;
  const int n = static_cast<int>(opt.skills.size());
  if (n < 1 || n > sh::kMaxClients) {
    r.error = "run_fight: 1..32 fighters; next: pass fewer skills";
    return r;
  }
  ge::progs::Progs progs;
  if (!server::compile_baseline_mod(opt.root, progs, r.error)) return r;
  server::QcServerOptions so;
  so.seed = opt.seed;
  so.loop.max_edicts = 1024;
  std::unique_ptr<server::QcServer> sp = server::make_qc_server(progs, so, r.error);
  if (!sp) return r;
  server::QcServer& s = *sp;
  server::server_command(s, "deathmatch 1");
  server::MapSpec map;
  map.name = "arena";
  map.models = server::arena_models(true);
  map.entities = server::kBaselineArenaEntities;
  if (!server::load_map(s, map, r.error)) {
    r.error += "; next: run tests/shooters_edge_baseline_mod_test.cpp";
    return r;
  }

  // The nav graph off the world, the door and the plat its movers.
  sh::MoveWorld world;
  sh::map_world(map.models[0], world);
  const int door = entity_on(s, "*1"), plat = entity_on(s, "*2");
  if (door < 0 || plat < 0) {
    r.error = "run_fight: the arena's door or plat is missing; next: read kBaselineArenaEntities";
    return r;
  }
  sh::NavMover movers[2];
  movers[0].ent = door;
  movers[0].absmin = s.sv.edicts[static_cast<size_t>(door)].absmin;
  movers[0].absmax = s.sv.edicts[static_cast<size_t>(door)].absmax;
  movers[1].ent = plat;
  movers[1].absmin = s.sv.edicts[static_cast<size_t>(plat)].absmin;
  movers[1].absmax = s.sv.edicts[static_cast<size_t>(plat)].absmax;
  movers[1].model = &map.models[2];
  movers[1].travel = {0, 0, kPlatRise};
  movers[1].ride_s = 1 + kPlatRise / kPlatSpeed;
  sh::NavGraph graph;
  sh::build_nav(world, movers, sh::MoveSettings{}, sh::NavSettings{}, graph);
  r.lo = graph.lo;
  r.hi = graph.hi;

  std::vector<Fighter> fighters(static_cast<size_t>(n));
  r.players.resize(static_cast<size_t>(n));
  for (int k = 0; k < n; k++) {
    FightPlayer& p = r.players[static_cast<size_t>(k)];
    p.name = "bot" + std::to_string(k + 1);
    p.skill = opt.skills[static_cast<size_t>(k)];
    server::connect(s, k, p.name, false, true);
  }
  for (int k = 0; k < n; k++) {
    Fighter& f = fighters[static_cast<size_t>(k)];
    fighter_init(f, graph, s, k, server::kBaselineArenaEntities, opt.skills[static_cast<size_t>(k)],
                 kHz, opt.seed + 31u * static_cast<uint32_t>(k));
    f.bot.yaw = s.sv.edicts[static_cast<size_t>(k) + 1].v_angle.y;
    f.bot.pilot.world = &world;  // the hop looks ahead through the map
    if (k == 0) f.bot.pilot.why = opt.why;
  }

  struct Recent {
    sh::MoveCmd cmds[3];
    uint32_t sequence = 0;
  };
  std::vector<Recent> recent(static_cast<size_t>(n));
  std::vector<sh::Packet> packets(static_cast<size_t>(n));
  std::vector<bool> alive(static_cast<size_t>(n), true);
  for (int frame = 0; frame < opt.frames; frame++) {
    for (int k = 0; k < n; k++) {
      Recent& rc = recent[static_cast<size_t>(k)];
      rc.cmds[0] = rc.cmds[1];
      rc.cmds[1] = rc.cmds[2];
      rc.cmds[2] = fighter_command(fighters[static_cast<size_t>(k)], s, k, kPilotMsec);
      sh::Packet& p = packets[static_cast<size_t>(k)];
      p.client = k;
      p.sequence = ++rc.sequence;
      for (int i = 0; i < 3; i++) p.cmds[i] = rc.cmds[i];
    }
    s.sv.sound_count = 0;
    server::qc_frame(s, packets.data(), n);
    server::clear_frame(s.out);
    r.frames = frame + 1;
    for (int k = 0; k < n; k++) {
      const sh::Edict& d = s.sv.edicts[static_cast<size_t>(k) + 1];
      FightPlayer& p = r.players[static_cast<size_t>(k)];
      p.path.push_back(d.origin);
      const bool live = d.health > 0;
      p.mode.push_back(live ? static_cast<uint8_t>(fighters[static_cast<size_t>(k)].mode) : 255);
      p.frags_at.push_back(ge::progs::as_float(
          s.q.vm.entities[static_cast<size_t>(k + 1) * static_cast<size_t>(s.q.vm.entityfields) +
                          static_cast<size_t>(ge::progs::kFFrags)]));
      if (alive[static_cast<size_t>(k)] && !live) r.deaths.push_back({frame, k, d.origin});
      alive[static_cast<size_t>(k)] = live;
    }
    r.digests.push_back(frame_digest(s, n));
    if (s.map_stopped) {
      r.error = "the mod stopped the map: " + s.stop_reason +
                "; next: read the console's last lines and the fault's QC stack";
      break;
    }
  }

  for (int k = 0; k < n; k++) {
    FightPlayer& p = r.players[static_cast<size_t>(k)];
    p.frags = p.frags_at.empty() ? 0 : p.frags_at.back();
    p.stats = fighters[static_cast<size_t>(k)].stats;
  }
  for (const FightDeath& d : r.deaths) r.players[static_cast<size_t>(d.victim)].deaths++;
  r.frag_log = s.frag_log;
  r.faults = s.q.faults;
  r.final_digest = r.digests.empty() ? 0 : r.digests.back();
  if (!fighters.empty())
    for (const ItemSpot& it : fighters[0].items) r.items.push_back({it.name, it.at});
  r.ok = r.error.empty();
  return r;
}

}  // namespace shooters_edge::bots
