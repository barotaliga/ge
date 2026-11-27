// A unit of Quake II's campaign played through (q2_unit.hpp).
//
// voice: agent (a lab's harness: its errors land in the lab's report; docs/architecture.md § Player
// text).
//
// ge:summary Quake II's unit played through: maps chained on one runner, the way, the record
// ge:keywords run_q2_unit q2 unit intermission map-change leader keys objectives digest
// ge:skill shooter bots
// ge:test tests/shooters_edge_q2_unit_test.cpp
#include "shooters_edge/bots/q2_unit.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>

#include "shooterbots/aim.hpp"
#include "shooterbots/bot_arsenal.hpp"
#include "shooterbots/buttons.hpp"
#include "shooterbots/companion.hpp"
#include "shooterbots/features.hpp"
#include "shooterbots/map_nav.hpp"
#include "shooterbots/traits.hpp"
#include "shooters_edge/bots/arsenals.hpp"
#include "shooters_edge/bots/pilot_run.hpp"
#include "shooters_edge/bots/quake_maps.hpp"
#include "shooters_edge/server/messages.hpp"
#include "shooters_edge/server/q2_map.hpp"
#include "shooters_edge/server/q2_server.hpp"

namespace sb = ge::shooterbots;

namespace shooters_edge::bots {

namespace {

namespace sv = shooters_edge::server;

constexpr int kHz = 1000 / kPilotMsec;  // the rules' frames a second
constexpr int kSampleEvery = kHz / 5;   // frames between the track's points (0.2 s)

uint64_t fold(uint64_t h, uint32_t w) { return (h ^ w) * 1099511628211ull; }

uint64_t fold_f(uint64_t h, float f) {
  uint32_t b;
  std::memcpy(&b, &f, 4);
  return fold(h, b);
}

uint64_t fold_frame(uint64_t h, const sv::RulesCommon& s) {
  uint64_t t;
  std::memcpy(&t, &s.sv.time, 8);
  h = fold(fold(h, static_cast<uint32_t>(t)), static_cast<uint32_t>(t >> 32));
  const sv::WorldEntity& d = s.world.entities[1];
  h = fold_f(fold_f(fold_f(h, d.origin.x), d.origin.y), d.origin.z);
  h = fold_f(fold_f(h, d.view_angles.x), d.view_angles.y);
  h = fold_f(h, d.health);
  return fold(h, static_cast<uint32_t>(s.sv.num_edicts));
}

// The map name a level change asks for, as the loader reads it: no unit mark, no spawn point.
std::string bare(const std::string& asked) {
  std::string m = asked;
  if (!m.empty() && m[0] == '*') m.erase(0, 1);
  if (const size_t d = m.find('$'); d != std::string::npos) m.resize(d);
  return m;
}

// A pilot with no way to his goal this long forgets the links he gave up on, s.
constexpr double kForgetFailed_s = 3;

// What he is shooting out of his way (way_break): its entity row and since when, and those he
// has given up on.
struct Breaking {
  int at = -1;
  double since = 0;
  std::vector<int> left;
};

// A bot of the party: the leader first, then his partners (UnitOptions::partners).
struct Bot {
  Fighter f;
  WayThrough way;
  Wedge wedge;
  Breaking breaking;
  sh::MoveCmd cmds[3];
  double no_route_since = -1;  // the pilot with a goal and no way to it since, s
};

struct OnMap {
  std::unique_ptr<MapNav> nav;
  std::vector<Bot> bots;
  sb::Party party;  // the map's work split among them (party.hpp)
  std::vector<sb::PartyMember> members;
};

bool enter(OnMap& m, const sv::RulesRunner& r, const sv::MapSpec& map, const UnitOptions& opt,
           int entered, const std::vector<std::string>& been, UnitMap& um, std::string& error) {
  const sv::RulesCommon& s = *r;
  m.nav = std::make_unique<MapNav>();
  m.nav->move = q2_nav_move(0);
  if (!build_map_nav(s, map, quake_maps(), "info_player_start", *m.nav, error)) return false;
  MapNav& nav = *m.nav;
  um.name = map.name;
  um.lo = nav.graph.lo;
  um.hi = nav.graph.hi;
  for (const sb::NavNode& n : nav.graph.nodes) um.nodes.push_back(n.origin);
  const std::string entities = map.entities.empty() ? map.brushes->entity_text : map.entities;
  sv::MapSpec spec = map;
  spec.entities = entities;
  const int n = 1 + static_cast<int>(opt.partners.size());
  m.bots.clear();
  m.bots.resize(static_cast<size_t>(n));
  m.party = {};
  m.members.clear();
  for (int k = 0; k < n; k++) {
    Bot& b = m.bots[static_cast<size_t>(k)];
    Fighter& f = b.f;
    const sb::AimSkill skill = k == 0 ? opt.skill : opt.partners[static_cast<size_t>(k) - 1];
    f.features = sb::features_of(skill);
    fighter_init(f, nav.graph, s, k, entities.c_str(), skill,
                 sb::traits_of(skill, sb::Temper::balanced), kHz,
                 opt.seed + 31u * static_cast<uint32_t>(entered) + 7919u * static_cast<uint32_t>(k),
                 q2_arsenal(opt.pack));
    sb::exits_init(f, nav.graph, s, entities.c_str(), &spec);  // the exits' boxes from the map's
    f.bot.yaw = s.world.entities[static_cast<size_t>(k) + 1].view_angles.y;
    f.bot.pilot.world = &nav.world;
    if (nav.swim.leading > 0) f.swim = &nav.swim;
    f.trap = &nav.trap;
    f.wet = &nav.wet;
    f.dry_pockets = false;                    // the way through leaves a pocket by its doors
    f.bot.pilot.pilot.set.under_plat = true;  // a raised lift is waited for out from under it
    // The way through keeps its own doors and buttons (way_through.hpp), not `locked`'s.
    f.features.set(sb::Feature::locked, false);
    f.features.set(sb::Feature::pressface, true);  // a button set in a wall, from its face
    f.features.set(sb::Feature::shotplace, true);  // a shot button, from where it is seen
    hunt_init(f, s);
    b.way.been = been;
    b.way.complete = opt.complete;
    b.way.complete_until = s.sv.time + opt.map_seconds * 0.75;
    if (opt.complete) way_complete_init(f, nav, s, spec);
    way_init(b.way, f, nav, s, spec);
    if (opt.plan && k == 0) b.way.log = &um.plan;
    if (!opt.beside || k > 0) m.members.push_back({&b.way, k + 1, true});
  }
  for (const WayStep& st : m.bots[0].way.steps) {
    if (way_exit(st)) um.exits.push_back((st.lo + st.hi) * 0.5f);
    if (st.kind == WayKind::key) {
      um.keys++;
      um.keys_at.push_back((st.lo + st.hi) * 0.5f);
    }
  }
  return true;
}

// GLASS IN HIS WAY: a pane that breaks (a `func_explosive` that takes blows) stops him as a wall
// does, though his nav, built of the world's brushes, runs through it. The nearest live one within
// `kBreakReach` that his eye sees is shot: the view on its nearest point, fire held. Barrels are
// left alone (their blast hurts him), and a crate a trigger blows up. A body lying as scenery
// (`misc_deadsoldier`) is solid to him too, and his nav runs over it: one within `kBodyReach` that
// his box, carried `kBodyAhead` along the move he means, would meet is shot to pieces (base2's by
// the south pool held him against it for good; base3's behind a wall beside his way, shot at from
// where he stood, held him there for good). True while he shoots.
// One shot at for kBreakGiveUp_s that still stands (a body some weapon will not mark) is let be
// for good, and he steps round it (base2's by the trench, the second time through).
constexpr float kBreakReach = 192;
constexpr float kBodyReach = 80;
constexpr float kBodyAhead = 32;
constexpr double kBreakGiveUp_s = 5;

bool way_break(const sv::Q2Server& q, const sv::RulesCommon& s, int ent, Breaking& br, double now,
               sh::MoveCmd& cmd) {
  const sh::Vec3f me = s.world.entities[static_cast<size_t>(ent)].origin;
  const sh::Vec3f eye = me + sh::Vec3f{0, 0, 22};
  // Where the move he means carries his box (the keys on his view's yaw).
  const sh::ViewAxes ax = sh::angle_vectors({0, cmd.angles.y, 0});
  sh::Vec3f wish = ax.forward * cmd.forward + ax.right * cmd.side;
  wish.z = 0;
  const bool moving = sh::normalize(wish) > 0;
  const sh::Vec3f ahead = me + wish * kBodyAhead;
  const sh::Vec3f box_lo{std::min(me.x, ahead.x) - 16, std::min(me.y, ahead.y) - 16, me.z - 24};
  const sh::Vec3f box_hi{std::max(me.x, ahead.x) + 16, std::max(me.y, ahead.y) + 16, me.z + 32};
  int best = -1;
  float nearest = kBreakReach;
  sh::Vec3f aim;
  for (size_t i = 0; i < q.entities.size(); i++) {
    const sv::Q2Entity& e = q.entities[i];
    const bool body = e.classname == "misc_deadsoldier" && e.box && !e.gone;
    // One only named (a crate a trigger blows up, base3's by its first hall) takes no blows.
    const bool named_only = (e.spawnflags & 1) == 0 && !e.targetname.empty();
    if (std::find(br.left.begin(), br.left.end(), static_cast<int>(i)) != br.left.end()) continue;
    if (!body && (e.classname != "func_explosive" || e.gone || e.hidden || !e.solid ||
                  e.health <= 0 || named_only))
      continue;
    const sh::Vec3f lo = e.origin + e.mins, hi = e.origin + e.maxs;
    if (body && (!moving || box_hi.x <= lo.x || box_lo.x >= hi.x || box_hi.y <= lo.y ||
                 box_lo.y >= hi.y || box_hi.z <= lo.z || box_lo.z >= hi.z))
      continue;  // not in the way he means to go
    // Its nearest point to his eye, kept 4 units inside its edges so the line meets its face.
    const auto in = [](float v, float a, float b) {
      return std::clamp(v, std::min(a + 4, (a + b) * 0.5f), std::max(b - 4, (a + b) * 0.5f));
    };
    const sh::Vec3f p{in(eye.x, lo.x, hi.x), in(eye.y, lo.y, hi.y), in(eye.z, lo.z, hi.z)};
    const float d = sh::length(p - eye);
    if (d >= nearest || (body && d >= kBodyReach)) continue;
    sh::Vec3f way = p - eye;
    sh::normalize(way);
    const ge::classic::Q2Trace t =
        sv::map_trace(q, eye, {}, {}, p + way * 8,
                      sh::kSqSolid | sh::kSqWindow | (body ? sh::kSqMonster : 0), -1);
    if (t.fraction >= 1 || t.entity != sv::map_row(static_cast<int>(i))) continue;  // not seen
    best = static_cast<int>(i);
    nearest = d;
    aim = p;
  }
  if (best < 0) {
    br.at = -1;
    return false;
  }
  if (best != br.at) {
    br.at = best;
    br.since = now;
  } else if (now - br.since > kBreakGiveUp_s) {
    br.left.push_back(best);
    br.at = -1;
    return false;
  }
  float yaw = 0, pitch = 0;
  sh::aim_angles(eye, aim, yaw, pitch);
  cmd.angles = {pitch, yaw, 0};
  cmd.forward = cmd.side = 0;
  cmd.buttons |= sh::kButtonAttack;
  return true;
}

// What the level counts: its objectives, secrets and monsters.
void count(const sv::Q2Server& q, UnitMap& um) {
  um.goals = q.goals;
  um.goals_found = q.goals_found;
  um.secrets = q.secrets;
  um.secrets_found = q.secrets_found;
  um.monsters = q.monsters_total;
  um.killed = q.monsters_killed;
}

}  // namespace

UnitReport run_q2_unit(const UnitOptions& opt) {
  UnitReport r;
  if (opt.load == nullptr) {
    r.error = "run_q2_unit: no map loader; next: pass UnitOptions::load";
    return r;
  }
  // He takes a turned view as his own (the start spot's angle, a teleport's), as the window's
  // player does: else the spot's turn stays added to every view he sends, and his moves and shots
  // go that much off the way he means.
  sv::Q2ServerOptions so;
  so.clients_take_view = true;
  so.pack = opt.pack;
  sv::RulesRunner rules = sv::q2_runner(sv::make_q2_server(so));
  rules.console("deathmatch 0");
  // A party plays co-op (the rules' `coop` 1), each bot a client of its own.
  rules.console(opt.partners.empty() ? "coop 0" : "coop 1");
  rules.console("skill " + std::to_string(opt.game_skill));
  sv::MapSpec map;
  if (!opt.load(opt.load_ctx, bare(opt.first), map, r.error)) {
    r.error = "run_q2_unit: " + opt.first + ": " + r.error;
    return r;
  }
  if (!rules.start(map, r.error)) {
    r.error = "run_q2_unit: " + opt.first + ": " + r.error;
    return r;
  }
  rules.connect(0, "player", false, false);
  for (size_t k = 0; k < opt.partners.size(); k++)
    rules.connect(static_cast<int>(k) + 1, "partner" + std::to_string(k + 1), false, false);
  const int n = 1 + static_cast<int>(opt.partners.size());
  std::vector<sh::Packet> packets(static_cast<size_t>(n));
  sv::Q2Server* q = sv::q2_of(rules);
  uint64_t h = 1469598103934665603ull;
  uint32_t sequence = 0;
  // The maps played before this one: on the first, the one `first` names after its `$` (a start
  // as if arrived from there, `base2$base1`), then each one he leaves.
  std::vector<std::string> been;
  if (const size_t d = opt.first.find('$'); d != std::string::npos)
    been.push_back(opt.first.substr(d + 1));
  for (int mi = 0; mi < opt.maps; mi++) {
    OnMap m;
    UnitMap um;
    if (!enter(m, rules, map, opt, mi, been, um, r.error)) {
      r.error = "run_q2_unit: " + map.name + ": " + r.error;
      return r;
    }
    for (int k = 0; k < n; k++) q->players[static_cast<size_t>(k)].god = opt.god;
    um.party.assign(static_cast<size_t>(n) - 1, {});
    const int helped = q->help_changed;
    const int frames = static_cast<int>(opt.map_seconds * kHz);
    int frame = 0;
    bool changed = false, died = false;
    int health =
        q->world.entities[1].health > 0 ? static_cast<int>(q->world.entities[1].health) : 0;
    std::vector<bool> took(m.bots[0].way.steps.size(), false);
    sb::PartyOptions people;
    int64_t helper_near = 0, helper_counted = 0;
    for (; frame < frames && !changed; frame++) {
      const sv::RulesCommon& s = *rules;
      const double t = s.sv.time;
      // The party splits the map's work between the bots (party.hpp).
      if (n > 1) {
        for (sb::PartyMember& pm : m.members)
          pm.alive = s.world.entities[static_cast<size_t>(pm.ent)].health > 0;
        if (opt.beside) {  // round him, a person's stand-in (party.hpp)
          people.humans.assign(1, s.world.entities[1].origin);
          people.facing.assign(1, s.world.entities[1].view_angles.y);
          people.leash = sb::kPartyLeash;
          people.exit_held = true;
        }
        sb::party_share(m.party, m.members, *m.nav, s, people);
      }
      for (int k = 0; k < n; k++) {
        Bot& b = m.bots[static_cast<size_t>(k)];
        const int me = k + 1;
        const int64_t plans = b.way.stats.plans;
        way_plan(b.way, b.f, *m.nav, s, me, t);
        if (opt.beside && k > 0 && sb::party_stay(b.f, *m.nav, s, me, people) &&
            b.way.stats.plans != plans)
          um.helper_stays++;
        b.cmds[0] = b.cmds[1];
        b.cmds[1] = b.cmds[2];
        // A view the rules turned (a spawn, a teleporter) is his to start from.
        const bool forced = q->players[static_cast<size_t>(k)].forced;
        if (forced) b.f.bot.yaw = s.sv.edicts[static_cast<size_t>(me)].v_angle.y;
        // A pilot with no way to the goal the way through found one to has given up on a link it
        // needs (a jump missed twice, out of the water or under a monster): after a while those
        // links are forgotten and tried again, as a person tries a jump once more.
        {
          sb::Pilot& pp = b.f.bot.pilot.pilot;
          if (pp.mode == sb::PilotMode::idle && pp.route.empty() && !pp.failed.empty()) {
            if (b.no_route_since < 0) b.no_route_since = t;
            if (t - b.no_route_since >= kForgetFailed_s) {
              pp.failed.clear();
              pp.cut.clear();
              b.no_route_since = -1;
              um.forgot++;
            }
          } else {
            b.no_route_since = -1;
          }
        }
        b.cmds[2] = fighter_command(b.f, s, k, kPilotMsec);
        way_dive(b.way, *m.nav, s, me, b.cmds[2]);
        way_reach_in(b.way, *m.nav, s, me, b.cmds[2]);
        if (way_break(*q, s, me, b.breaking, t, b.cmds[2])) {
          um.breaks++;
          b.f.bot.yaw = b.cmds[2].angles.y;  // he turned to it: his keys follow his view
        } else if (way_touch(b.way, s, me, b.cmds[2])) {
          um.touches++;
          b.f.bot.yaw = b.cmds[2].angles.y;  // he turned to it: his keys follow his view
        } else {
          // A climber is held to the ladder by his pilot: a shove aside takes him off it.
          if (!b.f.bot.pilot.pilot.climbing) unwedge(b.wedge, s, me, t, b.cmds[2]);
          if (way_edge(*m.nav, b.f, s, me, b.cmds[2])) um.edges++;
        }
        if (opt.beside && k > 0 && sb::party_give_way(s, me, people.humans, b.cmds[2]))
          um.helper_gave_way++;
        if (opt.beside && k > 0 && exits_keep_off(b.f, s, me, b.cmds[2])) um.helper_exits_turned++;
        // A turned view is taken as sent, whatever else turned him this step: the rules let go of
        // the turn only when the command's view is the one they set (a helper at a co-op spot
        // facing back, else ever turned half round from what he sends).
        if (forced) b.cmds[2].angles = s.sv.edicts[static_cast<size_t>(me)].v_angle;
        // The intermission: a button now and then leaves for the next map.
        if (q->intermission_at > 0) b.cmds[2].buttons = (frame & 8) ? sh::kButtonAttack : 0;
        sh::Packet& pk = packets[static_cast<size_t>(k)];
        pk.client = k;
        pk.sequence = ++sequence;
        for (int i = 0; i < 3; i++) pk.cmds[i] = b.cmds[i];
      }
      rules.frame(packets.data(), n);
      sv::clear_frame(rules.common->out);
      h = fold_frame(h, s);
      const int now =
          s.world.entities[1].health > 0 ? static_cast<int>(s.world.entities[1].health) : 0;
      if (now < health) um.damage += health - now;
      health = now;
      for (size_t k = 0; k < took.size(); k++) {
        bool done = false;  // taken by any bot of the party
        for (const Bot& b : m.bots) done = done || (k < b.way.steps.size() && b.way.steps[k].done);
        const WayStep& st = m.bots[0].way.steps[k];
        if (st.kind == WayKind::key && done && !took[k]) {
          took[k] = true;
          um.keys_taken++;
          um.took_keys.push_back(st.name);
        }
      }
      for (int k = 1; opt.beside && k < n && q->intermission_at <= 0; k++) {  // a helper's
        const sv::WorldEntity& d = s.world.entities[static_cast<size_t>(k) + 1];
        const sv::WorldEntity& lead = s.world.entities[1];
        if (d.health <= 0 || lead.health <= 0) continue;
        const sh::Vec3f dd = d.origin - lead.origin;
        const float apart = sh::length(dd);
        helper_near += apart <= sb::kPartyLeash ? 1 : 0;
        helper_counted++;
        um.helper_farthest = std::max(um.helper_farthest, apart);
      }
      if (frame % kSampleEvery == 0) {
        um.track.push_back(s.world.entities[1].origin);
        for (int k = 1; k < n; k++)
          um.party[static_cast<size_t>(k) - 1].push_back(
              s.world.entities[static_cast<size_t>(k) + 1].origin);
      }
      if (!s.pending_map.empty()) changed = true;
      // Dead alone (god mode keeps off all but a killbox; co-op brings him back), he would lie
      // there for the rest of the map's time, a gib with his box in the floor: the map ends with
      // his death, said as such (biggun's base blown up round him read as "the time ran out" ten
      // minutes later).
      if (!changed && n == 1 && now <= 0 && q->intermission_at <= 0) {
        died = true;
        frame++;
        break;
      }
    }
    um.seconds = static_cast<double>(frame) / kHz;
    um.helper_near_share =
        helper_counted > 0 ? static_cast<double>(helper_near) / static_cast<double>(helper_counted)
                           : 0;
    um.way = m.bots[0].way.stats;
    um.shoves = 0;
    for (const Bot& b : m.bots) um.shoves += b.wedge.shoves;
    um.shares = m.party.stats.shares;
    for (const Bot& b : m.bots) um.left_to_mates += b.way.mates_skipped;
    um.helps = q->help_changed - helped;
    count(*q, um);
    // (`plan`) The monsters still alive when the map ended, by where they stand and the model
    // they wear: what a 100% run left.
    if (opt.plan) {
      const sv::RulesCommon& s = *rules;
      for (int k = s.sv.set.maxclients + 1; k < s.sv.num_edicts; k++) {
        const sh::Edict& d = s.sv.edicts[static_cast<size_t>(k)];
        if (d.free || d.health <= 0 || d.solid != sh::kSolidSlidebox) continue;
        const int mdl = d.modelindex;
        const std::string model = mdl > 0 && static_cast<size_t>(mdl) < s.models.size()
                                      ? s.models[static_cast<size_t>(mdl)]
                                      : std::string("?");
        char line[192];
        std::snprintf(line, sizeof line, "left alive: %d %s at %.0f %.0f %.0f", k, model.c_str(),
                      d.origin.x, d.origin.y, d.origin.z);
        um.plan.emplace_back(line);
      }
    }
    if (!changed) {
      if (died) {
        const sh::Vec3f at = rules->world.entities[1].origin;
        char line[96];
        std::snprintf(line, sizeof line, "he died at %.0f %.0f %.0f", at.x, at.y, at.z);
        um.end = line;
      } else {
        um.end = "the time ran out";
      }
      r.maps.push_back(std::move(um));
      break;
    }
    const std::string asked = rules->pending_map;
    um.finished = true;
    um.next = asked;
    um.end = "the exit, to " + asked;
    r.maps.push_back(std::move(um));
    if (q->next_unit || (!asked.empty() && asked[0] == '*')) {
      r.unit_done = true;
      r.out_to = bare(asked);
      break;
    }
    been.push_back(map.name);
    if (!opt.load(opt.load_ctx, bare(asked), map, r.error)) {
      r.error = "run_q2_unit: " + asked + ": " + r.error;
      return r;
    }
    if (!rules.start(map, r.error)) {
      r.error = "run_q2_unit: " + asked + ": " + r.error;
      return r;
    }
  }
  r.digest = h;
  r.ok = true;
  return r;
}

}  // namespace shooters_edge::bots
