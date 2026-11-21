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
#include <cstring>
#include <memory>

#include "shooterbots/aim.hpp"
#include "shooterbots/bot_arsenal.hpp"
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
constexpr int kSampleEvery = kHz / 2;   // frames between the track's points

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

struct OnMap {
  std::unique_ptr<MapNav> nav;
  Fighter leader;
  WayThrough way;
  Wedge wedge;
  sh::MoveCmd cmds[3];
  double no_route_since = -1;  // the pilot with a goal and no way to it since, s
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
  Fighter& f = m.leader;
  f = Fighter();
  f.features = sb::features_of(opt.skill);
  fighter_init(f, nav.graph, s, 0, entities.c_str(), opt.skill,
               sb::traits_of(opt.skill, sb::Temper::balanced), kHz,
               opt.seed + 31u * static_cast<uint32_t>(entered), q2_arsenal());
  f.bot.yaw = s.world.entities[1].view_angles.y;
  f.bot.pilot.world = &nav.world;
  if (nav.swim.leading > 0) f.swim = &nav.swim;
  f.trap = &nav.trap;
  f.wet = &nav.wet;
  f.dry_pockets = false;                    // the way through leaves a pocket by its doors
  f.bot.pilot.pilot.set.under_plat = true;  // a raised lift is waited for out from under it
  // The way through keeps its own doors and buttons (way_through.hpp), not `locked`'s.
  f.features.set(sb::Feature::locked, false);
  hunt_init(f, s);
  sv::MapSpec spec = map;
  spec.entities = entities;
  m.way = WayThrough();
  m.way.been = been;
  way_init(m.way, f, nav, s, spec);
  if (opt.plan) m.way.log = &um.plan;
  m.wedge = Wedge();
  m.no_route_since = -1;
  for (const WayStep& st : m.way.steps) {
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
constexpr float kBreakReach = 192;
constexpr float kBodyReach = 80;
constexpr float kBodyAhead = 32;

bool way_break(const sv::Q2Server& q, const sv::RulesCommon& s, sh::MoveCmd& cmd) {
  const sh::Vec3f me = s.world.entities[1].origin;
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
  if (best < 0) return false;
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
  sv::RulesRunner rules = sv::q2_runner(sv::make_q2_server(so));
  rules.console("deathmatch 0");
  rules.console("coop 0");
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
    q->players[0].god = opt.god;
    const int helped = q->help_changed;
    const int frames = static_cast<int>(opt.map_seconds * kHz);
    int frame = 0;
    bool changed = false;
    int health =
        q->world.entities[1].health > 0 ? static_cast<int>(q->world.entities[1].health) : 0;
    std::vector<bool> took(m.way.steps.size(), false);
    for (; frame < frames && !changed; frame++) {
      const sv::RulesCommon& s = *rules;
      const double t = s.sv.time;
      way_plan(m.way, m.leader, *m.nav, s, 1, t);
      m.cmds[0] = m.cmds[1];
      m.cmds[1] = m.cmds[2];
      // A view the rules turned (a spawn, a teleporter) is his to start from.
      const bool forced = q->players[0].forced;
      if (forced) m.leader.bot.yaw = s.sv.edicts[1].v_angle.y;
      // A pilot with no way to the goal the way through found one to has given up on a link it
      // needs (a jump missed twice, out of the water or under a monster): after a while those
      // links are forgotten and tried again, as a person tries a jump once more.
      {
        sb::Pilot& pp = m.leader.bot.pilot.pilot;
        if (pp.mode == sb::PilotMode::idle && pp.route.empty() && !pp.failed.empty()) {
          if (m.no_route_since < 0) m.no_route_since = t;
          if (t - m.no_route_since >= kForgetFailed_s) {
            pp.failed.clear();
            pp.cut.clear();
            m.no_route_since = -1;
            um.forgot++;
          }
        } else {
          m.no_route_since = -1;
        }
      }
      m.cmds[2] = fighter_command(m.leader, s, 0, kPilotMsec);
      if (forced) m.cmds[2].angles = s.sv.edicts[1].v_angle;
      way_dive(m.way, *m.nav, s, 1, m.cmds[2]);
      if (way_break(*q, s, m.cmds[2])) {
        um.breaks++;
        m.leader.bot.yaw = m.cmds[2].angles.y;  // he turned to it: his keys follow his view
      } else if (way_touch(m.way, s, 1, m.cmds[2])) {
        um.touches++;
        m.leader.bot.yaw = m.cmds[2].angles.y;  // he turned to it: his keys follow his view
      } else {
        // A climber is held to the ladder by his pilot: a shove aside takes him off it.
        if (!m.leader.bot.pilot.pilot.climbing) unwedge(m.wedge, s, 1, t, m.cmds[2]);
        if (way_edge(*m.nav, m.leader, s, 1, m.cmds[2])) um.edges++;
      }
      // The intermission: a button now and then leaves for the next map.
      if (q->intermission_at > 0) m.cmds[2].buttons = (frame & 8) ? sh::kButtonAttack : 0;
      sh::Packet pk;
      pk.client = 0;
      pk.sequence = ++sequence;
      for (int k = 0; k < 3; k++) pk.cmds[k] = m.cmds[k];
      rules.frame(&pk, 1);
      sv::clear_frame(rules.common->out);
      h = fold_frame(h, s);
      const int now =
          s.world.entities[1].health > 0 ? static_cast<int>(s.world.entities[1].health) : 0;
      if (now < health) um.damage += health - now;
      health = now;
      for (size_t k = 0; k < m.way.steps.size(); k++)
        if (m.way.steps[k].kind == WayKind::key && m.way.steps[k].done && !took[k]) {
          took[k] = true;
          um.keys_taken++;
          um.took_keys.push_back(m.way.steps[k].name);
        }
      if (frame % kSampleEvery == 0) um.track.push_back(s.world.entities[1].origin);
      if (!s.pending_map.empty()) changed = true;
    }
    um.seconds = static_cast<double>(frame) / kHz;
    um.way = m.way.stats;
    um.shoves = m.wedge.shoves;
    um.helps = q->help_changed - helped;
    count(*q, um);
    if (!changed) {
      um.end = "the time ran out";
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
