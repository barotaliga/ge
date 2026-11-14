// A co-op episode played through (coop_episode.hpp).
//
// voice: agent (a lab's harness: its errors land in the lab's report; docs/architecture.md § Player
// text).
//
// ge:summary The co-op episode: maps chained, each one's nav and players made, the record kept
// ge:keywords run_coop_episode episode intermission map-change leader companions kills digest
// ge:skill shooter bots
// ge:test tests/shooters_edge_companion_test.cpp
#include "shooters_edge/bots/coop_episode.hpp"

#include <cmath>
#include <cstring>
#include <filesystem>
#include <memory>

#include "shooters_edge/bots/map_nav.hpp"
#include "shooters_edge/server/baseline_mod.hpp"
#include "shooters_edge/server/mod_match.hpp"

namespace shooters_edge::bots {

namespace {

namespace pg = ge::progs;
namespace sv = shooters_edge::server;

constexpr int kMsec = 14;         // a command's length on the 1/72 s host
constexpr int kSampleEvery = 36;  // frames between the pictures' points (half a second)
constexpr double kHz = 1000.0 / kMsec;

uint64_t fold(uint64_t h, uint32_t w) { return (h ^ w) * 1099511628211ull; }

uint64_t fold_f(uint64_t h, float f) {
  uint32_t b;
  std::memcpy(&b, &f, 4);
  return fold(h, b);
}

uint64_t fold_frame(uint64_t h, const sv::RulesCommon& s, int players) {
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

float dist(sh::Vec3f a, sh::Vec3f b) {
  const sh::Vec3f d = a - b;
  return std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
}

bool sees(const Fighter& f, int ent) {
  for (const sh::Contact& c : f.bot.senses.contacts)
    if (c.ent == ent && c.was_visible) return true;
  return false;
}

struct Recent {
  sh::MoveCmd cmds[3];
  uint32_t sequence = 0;
};

// One map's players and plan, made afresh when it is entered.
struct OnMap {
  std::unique_ptr<MapNav> nav;
  std::vector<Fighter> players;  // the leader first, then the companions
  WayThrough way;
  std::vector<Wedge> wedges;     // everyone's shove at a corner
  std::vector<uint8_t> monster;  // entities that were monsters alive at the start
};

bool enter(OnMap& m, const sv::Campaign& c, const EpisodeOptions& opt, EpisodeMap& em,
           std::string& error) {
  const sv::QcServer* q = sv::qc_of(c.server);
  if (!q) {
    error = "the campaign runs no QC server";
    return false;
  }
  const sv::RulesCommon& s = *c.server;
  m.nav = std::make_unique<MapNav>();
  if (!build_map_nav(*q, c.map, "info_player_start", *m.nav, error)) return false;
  MapNav& nav = *m.nav;
  em.name = c.map.name;
  em.nav_nodes = static_cast<int64_t>(nav.graph.nodes.size());
  em.movers = static_cast<int64_t>(nav.movers.size());
  em.nav_ms = nav.build_ms;
  em.lo = nav.graph.lo;
  em.hi = nav.graph.hi;
  for (const sh::NavNode& n : nav.graph.nodes) em.nodes.push_back(n.origin);
  const int n = 1 + static_cast<int>(opt.skills.size());
  m.players.clear();
  m.players.resize(static_cast<size_t>(n));
  m.wedges.assign(static_cast<size_t>(n), {});
  for (int k = 0; k < n; k++) {
    Fighter& f = m.players[static_cast<size_t>(k)];
    const sh::AimSkill skill = k == 0 ? opt.leader : opt.skills[static_cast<size_t>(k) - 1];
    f.features = sh::features_of(skill);
    fighter_init(
        f, nav.graph, s, k, c.map.entities.c_str(), skill,
        sh::traits_of(skill, sh::Temper::balanced), kHz,
        opt.seed + 31u * static_cast<uint32_t>(k) + static_cast<uint32_t>(c.history.size()));
    f.bot.yaw = s.world.entities[static_cast<size_t>(k) + 1].view_angles.y;
    f.bot.pilot.world = &nav.world;
    if (nav.swim.leading > 0) f.swim = &nav.swim;
    f.trap = &nav.trap;
    if (k == 0) {
      // The way through keeps its own doors and buttons (way_through.hpp), not `locked`'s.
      f.features.set(sh::Feature::locked, false);
      hunt_init(f, s);
      way_init(m.way, f, nav, s, c.map);
      if (opt.plan) m.way.log = &em.plan;
    } else {
      escort_init(f, s, k, 0);
    }
  }
  for (const WayStep& st : m.way.steps)
    if (st.kind == WayKind::exit) em.exits.push_back((st.lo + st.hi) * 0.5f);
  m.monster.assign(s.sv.edicts.size(), 0);
  for (int e = s.sv.set.maxclients + 1; e < s.sv.num_edicts; e++) {
    const sh::Edict& d = s.sv.edicts[static_cast<size_t>(e)];
    if (!d.free && (d.flags & sh::kFlagMonster) != 0 && d.solid != sh::kSolidNot) {
      m.monster[static_cast<size_t>(e)] = 1;
      em.monsters++;
    }
  }
  em.mates.assign(opt.skills.size(), {});
  return true;
}

}  // namespace

EpisodeReport run_coop_episode(const EpisodeOptions& opt) {
  EpisodeReport r;
  const int mates = static_cast<int>(opt.skills.size());
  if (mates < 1 || mates > 3 || !opt.load) {
    r.error = "run_coop_episode: 1..3 companions and a map loader; next: check the options";
    return r;
  }
  pg::Progs own;
  const pg::Progs* progs = opt.progs;
  if (!progs) {
    std::string error;
    if (!sv::campaign_mod(own, error)) {
      r.error = "run_coop_episode: the campaign: " + error;
      return r;
    }
    progs = &own;
  }
  sv::Campaign c;
  sv::CampaignOptions co;
  co.server.seed = opt.seed;
  co.server.loop.max_edicts = 1024;
  co.load = opt.load;
  co.load_ctx = opt.load_ctx;
  co.settings = {"deathmatch 0", "coop 1", "teamplay 1", "skill " + std::to_string(opt.skill)};
  if (!sv::campaign_new_game(c, *progs, co, opt.first, r.error)) {
    r.error = "run_coop_episode: " + r.error;
    return r;
  }
  sv::campaign_join(c, 0, "leader");
  for (int k = 1; k <= mates; k++) sv::campaign_join(c, k, "mate" + std::to_string(k));
  const pg::Def* inter = pg::find_global(*progs, "intermission_running");

  uint64_t h = 1469598103934665603ull;
  std::vector<Recent> recent(static_cast<size_t>(mates) + 1);
  std::vector<sh::Packet> packets(static_cast<size_t>(mates) + 1);
  for (int mi = 0; mi < opt.maps; mi++) {
    OnMap m;
    EpisodeMap em;
    if (!enter(m, c, opt, em, r.error)) {
      r.error = "run_coop_episode: " + c.map.name + ": " + r.error;
      return r;
    }
    const size_t entered = c.history.size();
    std::vector<bool> alive(static_cast<size_t>(mates) + 1, true);
    int64_t near = 0, counted = 0;
    const int frames = static_cast<int>(opt.map_seconds * 72);
    int frame = 0;
    bool changed = false;
    for (; frame < frames && !changed; frame++) {
      const sv::RulesCommon& now = *c.server;
      const double t = now.sv.time;
      const sv::QcServer* q = sv::qc_of(c.server);
      const bool ending =
          inter && q && pg::global_float(q->q.vm, inter->offset) != 0;  // the intermission
      way_plan(m.way, m.players[0], *m.nav, now, 1, t);
      for (int k = 0; k <= mates; k++) {
        Recent& rc = recent[static_cast<size_t>(k)];
        rc.cmds[0] = rc.cmds[1];
        rc.cmds[1] = rc.cmds[2];
        rc.cmds[2] = fighter_command(m.players[static_cast<size_t>(k)], now, k, kMsec);
        if (k == 0) way_dive(m.way, *m.nav, now, 1, rc.cmds[2]);
        unwedge(m.wedges[static_cast<size_t>(k)], now, k + 1, t, rc.cmds[2]);
        if (ending) rc.cmds[2].buttons = (frame & 1) ? sh::kButtonAttack : 0;
        sh::Packet& p = packets[static_cast<size_t>(k)];
        p.client = k;
        p.sequence = ++rc.sequence;
        for (int i = 0; i < 3; i++) p.cmds[i] = rc.cmds[i];
      }
      if (!sv::campaign_frame(c, packets.data(), mates + 1, r.error)) {
        r.error = "run_coop_episode: " + em.name + ": " + r.error;
        return r;
      }
      if (c.history.size() != entered) {
        changed = true;
        em.finished = true;
        em.next = c.history.back();
        em.end = "the exit, to " + em.next;
        break;
      }
      const sv::RulesCommon& after = *c.server;
      sv::clear_frame(c.server.common->out);
      if (const sv::QcServer* qa = sv::qc_of(c.server)) em.faults = qa->q.faults;
      h = fold_frame(h, after, mates + 1);
      const sv::WorldEntity& L = after.world.entities[1];
      const bool leader_live = L.health > 0;
      if (alive[0] && !leader_live) em.leader_deaths++;
      alive[0] = leader_live;
      for (int k = 1; k <= mates; k++) {
        const sv::WorldEntity& d = after.world.entities[static_cast<size_t>(k) + 1];
        const bool live = d.health > 0;
        if (alive[static_cast<size_t>(k)] && !live) em.mate_deaths++;
        alive[static_cast<size_t>(k)] = live;
        if (live && leader_live && !ending) {
          const float dd = dist(d.origin, L.origin);
          near += dd < kEscortFar ? 1 : 0;
          counted++;
          em.farthest = dd > em.farthest ? dd : em.farthest;
          if (dd > kEscortCatchUp) {
            em.far_by_mode[static_cast<int>(m.players[static_cast<size_t>(k)].escort.mode)]++;
            if (d.waterlevel >= 2 || L.waterlevel >= 2) em.far_in_water++;
          }
        }
      }
      if (frame % kSampleEvery == 0) {
        em.leader.push_back(L.origin);
        for (int k = 1; k <= mates; k++)
          em.mates[static_cast<size_t>(k) - 1].push_back(
              after.world.entities[static_cast<size_t>(k) + 1].origin);
        em.way_kind.push_back(static_cast<uint8_t>(m.way.kind));
      }
      for (int e = after.sv.set.maxclients + 1;
           e < after.sv.num_edicts && static_cast<size_t>(e) < m.monster.size(); e++) {
        const sh::Edict& d = after.sv.edicts[static_cast<size_t>(e)];
        if (!m.monster[static_cast<size_t>(e)] || (!d.free && d.solid != sh::kSolidNot)) continue;
        m.monster[static_cast<size_t>(e)] = 0;
        em.kills.push_back(d.origin);
        // The nearest player whose sight it fell in.
        int by = -1;
        float best = 0;
        for (int k = 0; k <= mates; k++) {
          if (!sees(m.players[static_cast<size_t>(k)], e)) continue;
          const float dd = dist(after.world.entities[static_cast<size_t>(k) + 1].origin, d.origin);
          if (by < 0 || dd < best) {
            by = k;
            best = dd;
          }
        }
        if (by == 0)
          em.killed_by_leader++;
        else if (by > 0)
          em.killed_by_mates++;
        else
          em.killed_unseen++;
      }
    }
    em.seconds = static_cast<double>(frame) / 72;
    if (!changed) em.end = "the time ran out";
    em.near_share = counted > 0 ? static_cast<double>(near) / static_cast<double>(counted) : 0;
    em.way = m.way.stats;
    em.leader_shoves = m.wedges[0].shoves;
    for (int k = 1; k <= mates; k++) em.mate_shoves += m.wedges[static_cast<size_t>(k)].shoves;
    for (int k = 1; k <= mates; k++) {
      const EscortStats& es = m.players[static_cast<size_t>(k)].escort.stats;
      em.mate_buttons += es.buttons;
      em.mate_threats += es.threats;
      em.mate_idle += es.idle_breaks;
    }
    r.faults += em.faults;
    r.maps.push_back(std::move(em));
    if (!changed) break;
  }
  r.digest = h;
  r.ok = true;
  return r;
}

}  // namespace shooters_edge::bots
