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
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <memory>

#include "shooterbots/map_nav.hpp"
#include "shooters_edge/bots/arsenals.hpp"
#include "shooters_edge/bots/quake_maps.hpp"
#include "shooters_edge/server/baseline_mod.hpp"
#include "shooters_edge/server/mod_match.hpp"
#include "shooters_edge/server/rules_world.hpp"

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
  std::vector<Fighter> players;  // the leader first, his partners, then the companions
  std::vector<WayThrough> ways;  // the leader's way through and each partner's (party.hpp)
  sb::Party party;
  std::vector<sb::PartyMember> members;
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
  m.nav->hatch_drops = true;     // the campaign's floor doors open drops (map_nav.hpp)
  m.nav->one_way_tricks = true;  // the record runs' blast jumps lead on (map_nav.hpp)
  if (!build_map_nav(*q, c.map, quake_maps(), "info_player_start", *m.nav, error,
                     campaign_nav_settings()))
    return false;
  MapNav& nav = *m.nav;
  em.name = c.map.name;
  em.nav_nodes = static_cast<int64_t>(nav.graph.nodes.size());
  em.nav_rockets = nav.tricks_made.held;
  em.nav_grenades = nav.tricks_made.grenades;
  em.nav_combos = nav.tricks_made.combos;
  em.movers = static_cast<int64_t>(nav.movers.size());
  em.nav_ms = nav.build_ms;
  em.lo = nav.graph.lo;
  em.hi = nav.graph.hi;
  for (const sb::NavNode& n : nav.graph.nodes) em.nodes.push_back(n.origin);
  const int partners = static_cast<int>(opt.partners.size());
  const int n = 1 + partners + static_cast<int>(opt.skills.size());
  m.players.clear();
  m.players.resize(static_cast<size_t>(n));
  m.wedges.assign(static_cast<size_t>(n), {});
  m.ways.clear();
  m.ways.resize(static_cast<size_t>(partners) + 1);
  m.party = {};
  m.members.clear();
  for (int k = 0; k < n; k++) {
    Fighter& f = m.players[static_cast<size_t>(k)];
    const sb::AimSkill skill = k == 0          ? opt.leader
                               : k <= partners ? opt.partners[static_cast<size_t>(k) - 1]
                                               : opt.skills[static_cast<size_t>(k - partners) - 1];
    f.features = sb::features_of(skill);
    fighter_init(
        f, nav.graph, s, k, c.map.entities.c_str(), skill,
        sb::traits_of(skill, sb::Temper::balanced), kHz,
        opt.seed + 31u * static_cast<uint32_t>(k) + static_cast<uint32_t>(c.history.size()),
        opt.arsenal != nullptr ? *opt.arsenal : first_arsenal());
    f.bot.yaw = s.world.entities[static_cast<size_t>(k) + 1].view_angles.y;
    f.bot.pilot.world = &nav.world;
    if (nav.swim.leading > 0) f.swim = &nav.swim;
    f.trap = &nav.trap;
    f.wet = &nav.wet;
    if (k <= partners) {
      // The way through keeps its own doors and buttons (way_through.hpp), not `locked`'s.
      WayThrough& way = m.ways[static_cast<size_t>(k)];
      f.features.set(sb::Feature::locked, false);
      hunt_init(f, s);
      f.ledge_guard = true;  // a fall into a pit with no way out ends his map
      way.complete = opt.complete;
      way.toured = opt.tour;
      // The last quarter of the map's time is for the way out: a map he cannot clear still ends.
      way.complete_until = s.sv.time + opt.map_seconds * 0.75;
      way.avoid = opt.avoid;
      // A shot button whose place he does not reach is shot from one he has (r1m4's across the
      // lava, from the end of the corridor), as Quake II's campaign does.
      if (opt.complete) f.features.set(sb::Feature::shotplace, true);
      if (opt.complete) way_complete_init(f, nav, s, c.map);
      way_init(way, f, nav, s, c.map);
      if (opt.plan && k == 0) way.log = &em.plan;
      if (!opt.beside || k > 0) m.members.push_back({&way, k + 1, true});
    } else {
      escort_init(f, s, k, 0);
    }
  }
  for (const WayStep& st : m.ways[0].steps)
    if (st.kind == WayKind::exit) em.exits.push_back((st.lo + st.hi) * 0.5f);
  m.monster.assign(s.sv.edicts.size(), 0);
  for (int e = s.sv.set.maxclients + 1; e < s.sv.num_edicts; e++) {
    const sh::Edict& d = s.sv.edicts[static_cast<size_t>(e)];
    if (!d.free && (d.flags & sh::kFlagMonster) != 0 && d.solid != sh::kSolidNot) {
      m.monster[static_cast<size_t>(e)] = 1;
      em.monsters++;
    }
  }
  em.mates.assign(static_cast<size_t>(n) - 1, {});
  return true;
}

// THE CENSUS: each entity slot's class name and first place, so a removal, a reuse of the slot
// and a move are seen; the counts kept by class name in the map's row.
struct Census {
  struct Slot {
    int cls = -1;  // index into the map's census, -1 a free slot
    sh::Vec3f at;
    bool moved = false;
    bool shown = false;  // its model drawn at the last look
  };
  std::vector<Slot> slots;
};

int census_class(EpisodeMap& em, const char* name) {
  // An entity its program gave no class name (a body left behind, a thrown gib) counts as one
  // class.
  const std::string_view n = name != nullptr && name[0] != 0 ? name : "(no class name)";
  for (size_t i = 0; i < em.census.size(); i++)
    if (em.census[i].name == n) return static_cast<int>(i);
  em.census.push_back({std::string(n)});
  return static_cast<int>(em.census.size()) - 1;
}

// One look at the server's entities: `first` at the map's start (each counted as spawned).
void census_look(Census& cs, EpisodeMap& em, const sv::RulesRunner& run, bool first) {
  const sv::RulesCommon& now = *run;
  const sv::QcServer* q = sv::qc_of(run);
  if (q == nullptr) return;
  cs.slots.resize(std::max(cs.slots.size(), now.sv.edicts.size()));
  std::vector<int> alive(em.census.size(), 0);
  for (size_t e = static_cast<size_t>(now.sv.set.maxclients) + 1; e < cs.slots.size(); e++) {
    Census::Slot& slot = cs.slots[e];
    const bool live = e < static_cast<size_t>(now.sv.num_edicts) && !now.sv.edicts[e].free;
    int cls = -1;
    const bool shown = live && sv::model_shown(*q, static_cast<int>(e));
    if (live) {
      const char* name =
          pg::vm_string(q->q.vm, static_cast<int32_t>(*pg::entity_field(
                                     q->q.vm, static_cast<int>(e), q->q.layout->f.classname)));
      cls = census_class(em, name);
      alive.resize(em.census.size(), 0);
      alive[static_cast<size_t>(cls)]++;
    }
    if (slot.cls >= 0 && slot.cls != cls) em.census[static_cast<size_t>(slot.cls)].gone++;
    if (cls >= 0 && slot.cls != cls) {
      ClassCensus& c = em.census[static_cast<size_t>(cls)];
      (first ? c.spawned : c.made)++;
      slot.at = now.sv.edicts[e].origin;
      slot.moved = false;
    } else if (cls >= 0 && slot.shown && !shown) {  // its model taken away: an item picked up
      em.census[static_cast<size_t>(cls)].taken++;
    }
    if (cls >= 0 && slot.cls == cls && !slot.moved && dist(now.sv.edicts[e].origin, slot.at) > 1) {
      slot.moved = true;
      em.census[static_cast<size_t>(cls)].moved++;
    }
    slot.shown = shown;
    slot.cls = cls;
  }
  for (size_t i = 0; i < alive.size(); i++)
    em.census[i].most = std::max(em.census[i].most, alive[i]);
}

}  // namespace

EpisodeReport run_coop_episode(const EpisodeOptions& opt) {
  EpisodeReport r;
  const int partners = static_cast<int>(opt.partners.size());
  const int mates = partners + static_cast<int>(opt.skills.size());
  if (mates > (opt.single ? 0 : 7) || !opt.load) {
    r.error =
        "run_coop_episode: at most 7 partners and companions (none in single player) and a map "
        "loader; next: check the options";
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
  if (!sv::campaign_new_game(c, *progs, co, opt.before.empty() ? opt.first : opt.before[0],
                             r.error)) {
    r.error = "run_coop_episode: " + r.error;
    return r;
  }
  sv::campaign_join(c, 0, "leader");
  // Gear on arrival: each earlier map's pickups taken and its monsters paid for, then the level
  // change on, as a playthrough arrives.
  for (size_t b = 0; b < opt.before.size(); b++) {
    r.gear.push_back(q1_take_map_gear(c, 0));
    const std::string& to = b + 1 < opt.before.size() ? opt.before[b + 1] : opt.first;
    if (!sv::campaign_change(c, sv::MapChange::change_level, to, r.error)) {
      r.error = "run_coop_episode: gear on arrival, " + to + ": " + r.error;
      return r;
    }
  }
  if (!opt.before.empty()) r.arrived = q1_gear_text(c, 0);
  for (int k = 1; k <= mates; k++) sv::campaign_join(c, k, "mate" + std::to_string(k));
  const pg::Def* inter = pg::find_global(*progs, "intermission_running");
  const pg::Def* counts[4] = {
      pg::find_global(*progs, "total_monsters"), pg::find_global(*progs, "killed_monsters"),
      pg::find_global(*progs, "total_secrets"), pg::find_global(*progs, "found_secrets")};

  uint64_t h = 1469598103934665603ull;
  // Every frame's line (EpisodeOptions::frames), closed when the episode returns.
  std::unique_ptr<std::FILE, int (*)(std::FILE*)> frames_file(
      opt.frames.empty() ? nullptr : std::fopen(opt.frames.c_str(), "w"), &std::fclose);
  std::FILE* frames_out = frames_file.get();
  if (!opt.frames.empty() && frames_out == nullptr) {
    r.error = "run_coop_episode: cannot write the frames file " + opt.frames +
              "; next: pass --frames=<a path in a folder that exists>";
    return r;
  }
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
    Census census;
    if (opt.census) census_look(census, em, c.server, true);
    std::vector<bool> alive(static_cast<size_t>(mates) + 1, true);
    std::vector<sb::PartyHeld> held(static_cast<size_t>(partners) + 1), held_after = held;
    sb::PartyHealTally heals;
    int64_t near = 0, counted = 0;
    int64_t helper_near = 0, helper_counted = 0;
    sb::PartyOptions people;
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
      for (int k = 0; opt.god && q && k <= partners; k++) {
        // The leader's and his partners' `god`: the program's own flag on the entity, as the
        // cheat sets it.
        sv::QcServer* qs = sv::qc_of(c.server);
        uint32_t* fl = pg::entity_field(qs->q.vm, k + 1, pg::vm_layout(qs->q.vm).f.flags);
        const int had = static_cast<int>(pg::as_float(*fl));
        if ((had & kFlagGodmode) == 0) {
          *fl = pg::as_word(static_cast<float>(had | kFlagGodmode));
          qs->sv.edicts[static_cast<size_t>(k) + 1].flags = had | kFlagGodmode;
        }
      }
      // The party splits the map's work between the leader and his partners (party.hpp).
      if (partners > 0) {
        for (sb::PartyMember& pm : m.members)
          pm.alive = now.world.entities[static_cast<size_t>(pm.ent)].health > 0;
        if (opt.beside) {  // round the leader, a person's stand-in (party.hpp)
          people.humans.assign(1, now.world.entities[1].origin);
          people.facing.assign(1, now.world.entities[1].view_angles.y);
          people.leash = sb::kPartyLeash;
          people.exit_held = true;
          const sb::PartyHeld lh = sb::party_held(now, 1);
          people.health.assign(1, lh.health);
          people.armour.assign(1, lh.armour);
        }
        sb::party_share(m.party, m.members, *m.nav, now, people);
      }
      for (int k = 0; k <= partners; k++) {
        WayThrough& way = m.ways[static_cast<size_t>(k)];
        const int64_t plans = way.stats.plans;
        const int left =
            opt.beside && opt.share_heals && k > 0
                ? sb::party_leave_items(m.players[static_cast<size_t>(k)], now, k + 1, people)
                : 0;
        way_plan(way, m.players[static_cast<size_t>(k)], *m.nav, now, k + 1, t);
        if (left > 0 && way.stats.plans != plans) em.helper_left_items++;
        if (opt.beside && k > 0 &&
            sb::party_stay(m.players[static_cast<size_t>(k)], *m.nav, now, k + 1, people) &&
            way.stats.plans != plans)
          em.helper_stays++;
      }
      for (int k = 0; k <= mates; k++) {
        Recent& rc = recent[static_cast<size_t>(k)];
        rc.cmds[0] = rc.cmds[1];
        rc.cmds[1] = rc.cmds[2];
        rc.cmds[2] = fighter_command(m.players[static_cast<size_t>(k)], now, k, kMsec);
        if (k <= partners) {
          WayThrough& way = m.ways[static_cast<size_t>(k)];
          way_dive(way, *m.nav, now, k + 1, rc.cmds[2]);
          way_touch(way, now, k + 1, rc.cmds[2]);  // its keys turned or not, he goes on
          if (opt.beside && k > 0 && sb::party_give_way(now, k + 1, people.humans, rc.cmds[2]))
            em.helper_gave_way++;
          if (opt.beside && k > 0 &&
              exits_keep_off(m.players[static_cast<size_t>(k)], now, k + 1, rc.cmds[2]))
            em.helper_exits_turned++;
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
      // What the leader and his partners hold before the frame (the sharing's count, below).
      for (int k = 0; opt.beside && k <= partners; k++)
        held[static_cast<size_t>(k)] = sb::party_held(now, k + 1);
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
      if (frames_out != nullptr) {
        const sv::WorldEntity& f = after.world.entities[1];
        const sh::MoveCmd& lc = recent[0].cmds[2];           // the leader's command this frame
        const sb::Pilot& lp = m.players[0].bot.pilot.pilot;  // and his pilot's why
        std::fprintf(
            frames_out,
            "%s %d %016llx %a %a %a %a %a %a %d v %a %a %a fl %d cmd %d %a %a %d %d %d %d fight %s "
            "finger %s weapon %d target %d at %.0f pilot %s %zu/%zu reach %d %d swim %d goal %d | "
            "%s\n",
            c.map.name.c_str(), frame, static_cast<unsigned long long>(h),
            static_cast<double>(f.origin.x), static_cast<double>(f.origin.y),
            static_cast<double>(f.origin.z), static_cast<double>(f.view_angles.x),
            static_cast<double>(f.view_angles.y), static_cast<double>(f.health),
            after.sv.num_edicts, static_cast<double>(f.velocity.x),
            static_cast<double>(f.velocity.y), static_cast<double>(f.velocity.z), f.flags, lc.msec,
            static_cast<double>(lc.angles.x), static_cast<double>(lc.angles.y), lc.forward, lc.side,
            lc.up, lc.buttons, sb::fight_mode_name(m.players[0].mode),
            m.players[0].finger_now >= 0
                ? sb::kFingerWhyNames[static_cast<size_t>(m.players[0].finger_now)]
                : "-",
            m.players[0].held_number, m.players[0].bot.target,
            static_cast<double>(m.players[0].bot.aim_dist), sb::pilot_mode_name(lp.mode), lp.k,
            lp.route.size(), m.players[0].reach_from, m.players[0].reach_n,
            m.players[0].swimming ? 1 : 0, m.players[0].goal_node,
            lp.because != nullptr ? lp.because : "");
      }
      if (opt.census && frame % 6 == 0) census_look(census, em, c.server, false);
      const sv::WorldEntity& L = after.world.entities[1];
      const bool leader_live = L.health > 0;
      if (opt.beside) {  // what heals, taken by whom (party.hpp)
        for (int k = 0; k <= partners; k++)
          held_after[static_cast<size_t>(k)] = sb::party_held(after, k + 1);
        sb::party_heal_tally(heals, held, held_after);
      }
      if (alive[0] && !leader_live) em.leader_deaths++;
      alive[0] = leader_live;
      for (int k = 1; k <= mates; k++) {
        const sv::WorldEntity& d = after.world.entities[static_cast<size_t>(k) + 1];
        const bool live = d.health > 0;
        if (alive[static_cast<size_t>(k)] && !live) em.mate_deaths++;
        alive[static_cast<size_t>(k)] = live;
        if (opt.beside && live && leader_live && !ending && k <= partners) {  // a helper's
          const float dd = dist(d.origin, L.origin);
          helper_near += dd <= sb::kPartyLeash ? 1 : 0;
          helper_counted++;
          em.helper_farthest = std::max(em.helper_farthest, dd);
        }
        if (live && leader_live && !ending && k > partners) {  // a companion's keeping near
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
        em.way_kind.push_back(static_cast<uint8_t>(m.ways[0].kind));
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
        else if (by > 0 && by <= partners)
          em.killed_by_partners++;
        else if (by > 0)
          em.killed_by_mates++;
        else
          em.killed_unseen++;
      }
    }
    em.seconds = static_cast<double>(frame) / 72;
    // An intermission that leads to no map is the game's end (Scourge's hipend: Armagon dead, the
    // finale): the map is finished.
    if (!changed && em.exit_s >= 0) {
      em.finished = true;
      em.end = "the game's end";
    } else if (!changed) {
      em.end = "the time ran out";
    }
    em.near_share = counted > 0 ? static_cast<double>(near) / static_cast<double>(counted) : 0;
    em.helper_near_share =
        helper_counted > 0 ? static_cast<double>(helper_near) / static_cast<double>(helper_counted)
                           : 0;
    em.leader_healed = heals.person_healed;
    em.helper_heal_needed = heals.health_in_need;
    em.helper_armour_needed = heals.armour_in_need;
    em.way = m.ways[0].stats;
    em.party = m.party.stats;
    for (int k = 1; k <= partners; k++) {
      const WayStats& ws = m.ways[static_cast<size_t>(k)].stats;
      em.partner_steps += ws.steps_done;
    }
    for (const WayThrough& w : m.ways) em.left_to_mates += w.mates_skipped;
    em.leader_shoves = m.wedges[0].shoves;
    for (int k = 1; k <= mates; k++) em.mate_shoves += m.wedges[static_cast<size_t>(k)].shoves;
    for (int k = partners + 1; k <= mates; k++) {
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
