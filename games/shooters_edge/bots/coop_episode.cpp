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

#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <memory>

#include "shooterbots/map_nav.hpp"
#include "shooters_edge/bots/arsenals.hpp"
#include "shooters_edge/bots/quake_maps.hpp"
#include "shooters_edge/server/baseline_mod.hpp"
#include "shooters_edge/server/mod_match.hpp"

namespace sb = ge::shooterbots;

namespace shooters_edge::bots {

namespace {

namespace pg = ge::progs;
namespace sv = shooters_edge::server;

constexpr int kMsec = 14;         // a command's length on the 1/72 s host
constexpr int kSampleEvery = 14;  // frames between the pictures' points (a fifth of a second)
constexpr double kHz = 1000.0 / kMsec;
constexpr int kFlagGodmode = 64;  // the program's `flags` bit for `god`

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
  for (const sb::Contact& c : f.bot.senses.contacts)
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
  m.nav->hatch_drops = true;  // the campaign's floor doors open drops (map_nav.hpp)
  if (!build_map_nav(*q, c.map, quake_maps(), "info_player_start", *m.nav, error)) return false;
  MapNav& nav = *m.nav;
  em.name = c.map.name;
  em.nav_nodes = static_cast<int64_t>(nav.graph.nodes.size());
  em.movers = static_cast<int64_t>(nav.movers.size());
  em.nav_ms = nav.build_ms;
  em.lo = nav.graph.lo;
  em.hi = nav.graph.hi;
  for (const sb::NavNode& n : nav.graph.nodes) em.nodes.push_back(n.origin);
  const int n = 1 + static_cast<int>(opt.skills.size());
  m.players.clear();
  m.players.resize(static_cast<size_t>(n));
  m.wedges.assign(static_cast<size_t>(n), {});
  for (int k = 0; k < n; k++) {
    Fighter& f = m.players[static_cast<size_t>(k)];
    const sb::AimSkill skill = k == 0 ? opt.leader : opt.skills[static_cast<size_t>(k) - 1];
    f.features = sb::features_of(skill);
    fighter_init(
        f, nav.graph, s, k, c.map.entities.c_str(), skill,
        sb::traits_of(skill, sb::Temper::balanced), kHz,
        opt.seed + 31u * static_cast<uint32_t>(k) + static_cast<uint32_t>(c.history.size()),
        first_arsenal());
    f.bot.yaw = s.world.entities[static_cast<size_t>(k) + 1].view_angles.y;
    f.bot.pilot.world = &nav.world;
    if (nav.swim.leading > 0) f.swim = &nav.swim;
    f.trap = &nav.trap;
    f.wet = &nav.wet;
    if (k == 0) {
      // The way through keeps its own doors and buttons (way_through.hpp), not `locked`'s.
      f.features.set(sb::Feature::locked, false);
      hunt_init(f, s);
      m.way.complete = opt.complete;
      m.way.toured = opt.tour;
      // The last quarter of the map's time is for the way out: a map he cannot clear still ends.
      m.way.complete_until = s.sv.time + opt.map_seconds * 0.75;
      m.way.avoid = opt.avoid;
      if (opt.complete) way_complete_init(f, nav, s, c.map);
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
  if (mates < (opt.single ? 0 : 1) || mates > (opt.single ? 0 : 3) || !opt.load) {
    r.error =
        "run_coop_episode: 1..3 companions (none in single player) and a map loader; next: check "
        "the options";
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
  co.wide_models = opt.wide_models;
  co.registered = opt.registered;
  co.settings = {"deathmatch 0", opt.single ? "coop 0" : "coop 1",
                 opt.single ? "teamplay 0" : "teamplay 1", "skill " + std::to_string(opt.skill)};
  if (!sv::campaign_new_game(c, *progs, co, opt.first, r.error)) {
    r.error = "run_coop_episode: " + r.error;
    return r;
  }
  sv::campaign_join(c, 0, "leader");
  for (int k = 1; k <= mates; k++) sv::campaign_join(c, k, "mate" + std::to_string(k));
  const pg::Def* inter = pg::find_global(*progs, "intermission_running");
  const pg::Def* counts[4] = {
      pg::find_global(*progs, "total_monsters"), pg::find_global(*progs, "killed_monsters"),
      pg::find_global(*progs, "total_secrets"), pg::find_global(*progs, "found_secrets")};

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
    std::vector<sh::Vec3f> blast;  // the leader's blast jump in the air (empty: none)
    int blast_frame = 0;
    bool in_grenade = false;
    for (; frame < frames && !changed; frame++) {
      const sv::RulesCommon& now = *c.server;
      const double t = now.sv.time;
      const sv::QcServer* q = sv::qc_of(c.server);
      const bool ending =
          inter && q && pg::global_float(q->q.vm, inter->offset) != 0;  // the intermission
      if (ending && em.exit_s < 0) em.exit_s = static_cast<double>(frame) / 72;
      if (q && !ending) {
        int* out[4] = {&em.total_monsters, &em.killed_monsters, &em.total_secrets,
                       &em.found_secrets};
        const int found_was = em.found_secrets;
        for (int i = 0; i < 4; i++)
          if (counts[i]) *out[i] = static_cast<int>(pg::global_float(q->q.vm, counts[i]->offset));
        for (int k = std::max(found_was, 0); k < em.found_secrets; k++)
          em.secrets.push_back(now.world.entities[1].origin);
      }
      if (opt.god && q) {
        // The leader's `god`: the program's own flag on his entity, as the cheat sets it.
        sv::QcServer* qs = sv::qc_of(c.server);
        uint32_t* fl = pg::entity_field(qs->q.vm, 1, pg::vm_layout(qs->q.vm).f.flags);
        const int had = static_cast<int>(pg::as_float(*fl));
        if ((had & kFlagGodmode) == 0) {
          *fl = pg::as_word(static_cast<float>(had | kFlagGodmode));
          qs->sv.edicts[1].flags = had | kFlagGodmode;
        }
      }
      way_plan(m.way, m.players[0], *m.nav, now, 1, t);
      for (int k = 0; k <= mates; k++) {
        Recent& rc = recent[static_cast<size_t>(k)];
        rc.cmds[0] = rc.cmds[1];
        rc.cmds[1] = rc.cmds[2];
        rc.cmds[2] = fighter_command(m.players[static_cast<size_t>(k)], now, k, kMsec);
        if (k == 0) {
          way_dive(m.way, *m.nav, now, 1, rc.cmds[2]);
          way_touch(m.way, now, 1, rc.cmds[2]);  // its keys turned or not, he goes on
        }
        if (!m.players[static_cast<size_t>(k)].bot.pilot.pilot.climbing)  // held to his ladder
          unwedge(m.wedges[static_cast<size_t>(k)], now, k + 1, t, rc.cmds[2]);
        if (ending) rc.cmds[2].buttons = (frame & 1) ? sh::kButtonAttack : 0;
        sh::Packet& p = packets[static_cast<size_t>(k)];
        p.client = k;
        p.sequence = ++rc.sequence;
        for (int i = 0; i < 3; i++) p.cmds[i] = rc.cmds[i];
      }
      // The leader's blast jump starts with the pilot's shot under him (a rocket jump) or the
      // first command of a grenade trick.
      {
        const sb::Pilot& lp = m.players[0].bot.pilot.pilot;
        const bool grenade = lp.mode == sb::PilotMode::grenade;
        if (blast.empty() && (lp.shot_now || (grenade && !in_grenade))) {
          blast.push_back(now.world.entities[1].origin);
          blast_frame = frame;
        }
        in_grenade = grenade;
      }
      if (!sv::campaign_frame(c, packets.data(), mates + 1, r.error)) {
        r.error = "run_coop_episode: " + em.name + ": " + r.error;
        return r;
      }
      if (c.history.size() != entered) {
        // The history's entry is "<how> <map>": a level changed to, or the same one restarted
        // (the leader died in single player).
        const std::string& h_last = c.history.back();
        const size_t sp = h_last.rfind(' ');
        em.next = sp == std::string::npos ? h_last : h_last.substr(sp + 1);
        changed = true;
        em.finished = h_last.rfind("restart", 0) != 0;
        em.end = em.finished ? "the exit, to " + em.next : "he died: the level restarted";
        break;
      }
      const sv::RulesCommon& after = *c.server;
      sv::clear_frame(c.server.common->out);
      if (const sv::QcServer* qa = sv::qc_of(c.server)) {
        em.faults = qa->q.faults;
        em.last_fault = qa->q.last_fault;
      }
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
      if (!blast.empty()) {
        // Followed until he stands again (a short hop's landing counts after 0.15 s in the air),
        // dies or 4 s have passed.
        if (frame % 6 == 0) blast.push_back(L.origin);
        const int flown = frame - blast_frame;
        if ((flown > 11 && (after.sv.edicts[1].flags & sh::kFlagOnGround) != 0) || !leader_live ||
            flown > 288) {
          blast.push_back(L.origin);
          em.blasts.push_back(std::move(blast));
          blast.clear();
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
    const std::string next = em.next;
    const bool died = changed && !em.finished;
    r.maps.push_back(std::move(em));
    if (!changed || died) break;
    bool stop = false;
    for (const std::string& x : opt.stop) stop = stop || x == next;
    // The 100% run: an exit back to a map played ends it (the coop hall's tour loops on itself).
    if (opt.complete)
      for (const EpisodeMap& done : r.maps) stop = stop || done.name == next;
    if (stop) break;
  }
  r.digest = h;
  r.ok = true;
  return r;
}

}  // namespace shooters_edge::bots
