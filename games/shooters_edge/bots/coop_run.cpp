// The co-op run (coop_run.hpp): the campaign on the coop hall, the leader's tour, the companions.
//
// ge:summary The co-op run: run_coop on the coop hall, the leader's tour, the companions' record
// ge:keywords run_coop coop-hall tour leader companions digest kills button door
// ge:skill shooter bots
// ge:test tests/shooters_edge_companion_test.cpp
#include "shooters_edge/bots/coop_run.hpp"

#include <cmath>
#include <cstring>
#include <filesystem>
#include <memory>

#include "progs/qc_compile.hpp"
#include "shooter/nav.hpp"
#include "shooters_edge/server/campaign.hpp"
#include "shooters_edge/server/mod_match.hpp"
#include "shooters_edge/sim/arena.hpp"

namespace shooters_edge::bots {

namespace {

namespace pg = ge::progs;
namespace sv = shooters_edge::server;

constexpr float kPlatRise = 96, kPlatSpeed = 64;  // the arena's plat
constexpr float kArrive = 40;                     // a walk leg is done this near its point, units
constexpr int kMsec = 14;                         // a command's length on the 1/72 s host

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
  h = fold(h, static_cast<uint32_t>(s.sv.num_edicts));
  return h;
}

int entity_on(const sv::RulesCommon& s, const char* name) {
  for (int e = 1; e < s.sv.num_edicts; e++) {
    const sv::WorldEntity& d = s.world.entities[static_cast<size_t>(e)];
    if (d.free || d.modelindex <= 0 || d.modelindex >= static_cast<int>(s.models.size())) continue;
    if (s.models[static_cast<size_t>(d.modelindex)] == name) return e;
  }
  return -1;
}

// The button's brush: on the dividing wall's west face, north of the doorway, at chest height.
const sh::Vec3f kButtonMins{112, 200, 40}, kButtonMaxs{128, 264, 72};

const sh::ModelData& button_model() {
  static const sh::ModelData m = sh::build_brush_model({sim::box_brush(kButtonMins, kButtonMaxs)});
  return m;
}

const sh::Vec3f kExitMins{432, 336, 0}, kExitMaxs{496, 400, 72};

const sh::ModelData& exit_model() {
  static const sh::ModelData m = sh::build_brush_model({sim::box_brush(kExitMins, kExitMaxs)});
  return m;
}

bool load_hall(void* /*ctx*/, const std::string& /*name*/, sv::MapSpec& out,
               std::string& /*error*/) {
  out.models = sv::arena_models(true);
  out.models.push_back(button_model().model());
  out.entities = kCoopHallEntities;
  return true;
}

float dist(sh::Vec3f a, sh::Vec3f b) {
  const sh::Vec3f d = a - b;
  return std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
}

}  // namespace

// The arena (sim/arena.hpp): the west room x -512..128, the east room 144..512, the doorway at
// x 128..144, y -64..64 (its door "*1" opened only by the button "*3", and then for good), the
// plat "*2" in the east room's south-east. The riflemen and the hound face the doorway, asleep
// until a player shows.
const char* const kCoopHallEntities = R"({
"classname" "worldspawn"
"message" "the coop hall"
}
{
"classname" "info_player_start"
"origin" "-416 0 24"
"angle" "0"
}
{
"classname" "info_player_coop"
"origin" "-416 112 24"
"angle" "0"
}
{
"classname" "info_player_coop"
"origin" "-416 -112 24"
"angle" "0"
}
{
"classname" "info_player_coop"
"origin" "-320 0 24"
"angle" "0"
}
{
"classname" "func_door"
"model" "*1"
"targetname" "hall"
"angle" "-1"
"speed" "100"
"wait" "-1"
"lip" "8"
}
{
"classname" "func_plat"
"model" "*2"
"origin" "0 0 96"
"height" "96"
"speed" "64"
}
{
"classname" "func_button"
"model" "*3"
"target" "hall"
"angle" "0"
"wait" "-1"
}
{
"classname" "item_shells"
"origin" "-160 352 0"
}
{
"classname" "item_spikes"
"origin" "-352 -320 0"
}
{
"classname" "weapon_nailgun"
"origin" "-96 -224 0"
}
{
"classname" "item_health"
"origin" "-288 288 0"
}
{
"classname" "monster_army"
"origin" "432 288 24"
"angle" "180"
}
{
"classname" "monster_army"
"origin" "448 -96 24"
"angle" "180"
}
{
"classname" "monster_dog"
"origin" "400 416 24"
"angle" "225"
}
)";

const char* const kCoopHallExitEntities = R"({
"classname" "trigger_changelevel"
"model" "*4"
"map" "hall"
}
)";

bool coop_hall_exit_map(void* /*ctx*/, const std::string& name, sv::MapSpec& out,
                        std::string& /*error*/) {
  out.name = name;
  out.models = sv::arena_models(true);
  out.models.push_back(button_model().model());
  out.models.push_back(exit_model().model());
  out.entities = std::string(kCoopHallEntities) + kCoopHallExitEntities;
  return true;
}

CoopReport run_coop(const CoopOptions& opt) {
  CoopReport r;
  const int mates = static_cast<int>(opt.skills.size());
  if (mates < 1 || mates > 3) {
    r.error = "run_coop: 1..3 companions; next: pass fewer skills";
    return r;
  }
  const std::filesystem::path src =
      std::filesystem::path(opt.root) / "games/shooters_edge/qc/campaign/progs.src";
  const pg::QcProject built = pg::compile_project(src);
  if (!built.result.ok) {
    r.error = "run_coop: " + src.generic_string() + " did not compile";
    for (const pg::QcDiagnostic& d : built.result.diagnostics)
      if (d.error) r.error += "; " + pg::diagnostic_text(d);
    r.error += "; next: `ge qc build games/shooters_edge/qc/campaign/progs.src`";
    return r;
  }
  const pg::Progs& progs = built.result.progs;

  sv::Campaign c;
  sv::CampaignOptions co;
  co.server.seed = opt.seed;
  co.server.loop.max_edicts = 512;
  co.load = opt.exit ? &coop_hall_exit_map : &load_hall;
  co.settings = {"deathmatch 0", "coop 1", "teamplay 1", "skill " + std::to_string(opt.skill)};
  if (!sv::campaign_new_game(c, progs, co, "hall", r.error)) {
    r.error = "run_coop: " + r.error;
    return r;
  }
  sv::campaign_join(c, 0, "leader");
  for (int k = 1; k <= mates; k++) sv::campaign_join(c, k, "mate" + std::to_string(k));
  const sv::RulesCommon& s = *c.server;

  // The nav graph off the hall's world, the door and the plat its movers.
  sh::MoveWorld world;
  sh::map_world(c.map.models[0], world);
  const int door = entity_on(s, "*1"), plat = entity_on(s, "*2"), button = entity_on(s, "*3");
  if (door < 0 || plat < 0 || button < 0) {
    r.error = "run_coop: the hall's door, plat or button is missing; next: read kCoopHallEntities";
    return r;
  }
  sh::NavMover movers[2];
  movers[0].ent = door;
  movers[0].absmin = s.world.entities[static_cast<size_t>(door)].absmin;
  movers[0].absmax = s.world.entities[static_cast<size_t>(door)].absmax;
  movers[1].ent = plat;
  movers[1].absmin = s.world.entities[static_cast<size_t>(plat)].absmin;
  movers[1].absmax = s.world.entities[static_cast<size_t>(plat)].absmax;
  movers[1].model = &c.map.models[2];
  movers[1].travel = {0, 0, kPlatRise};
  movers[1].ride_s = 1 + kPlatRise / kPlatSpeed;
  sh::NavGraph graph;
  sh::build_nav(world, movers, sh::MoveSettings{}, sh::NavSettings{}, graph);
  r.lo = graph.lo;
  r.hi = graph.hi;
  const sh::Vec3f door_rest = s.world.entities[static_cast<size_t>(door)].origin;
  const sh::Vec3f button_rest = s.world.entities[static_cast<size_t>(button)].origin;
  r.button_at = (kButtonMins + kButtonMaxs) * 0.5f;

  // The monsters at the start.
  std::vector<uint8_t> was_alive(s.sv.edicts.size(), 0);
  for (int e = s.sv.set.maxclients + 1; e < s.sv.num_edicts; e++) {
    const sh::Edict& d = s.sv.edicts[static_cast<size_t>(e)];
    if (!d.free && (d.flags & sh::kFlagMonster) != 0) {
      r.monsters.push_back(d.origin);
      was_alive[static_cast<size_t>(e)] = 1;
    }
  }
  r.monsters_total = static_cast<int>(r.monsters.size());

  // The leader: a pilot on plain hands, walking his tour.
  sh::PilotBot lead;
  {
    sh::PilotSettings ps;
    ps.strafe = false;
    pilot_init(lead.pilot, graph, ps, sh::pilot_hands(sh::PilotHands::none), 1000.0 / kMsec,
               opt.seed ^ 0x9e3779b9u);
    sh::pilot_face(lead.pilot, s.world.entities[1].view_angles.y);
  }
  std::vector<Fighter> fighters(static_cast<size_t>(mates));
  r.mates.resize(static_cast<size_t>(mates));
  for (int k = 0; k < mates; k++) {
    CoopMate& m = r.mates[static_cast<size_t>(k)];
    m.name = "mate" + std::to_string(k + 1);
    m.skill = opt.skills[static_cast<size_t>(k)];
    m.traits = sh::traits_of(m.skill, sh::Temper::balanced);
    if (static_cast<size_t>(k) < opt.traits.size() &&
        !sh::apply_traits(opt.traits[static_cast<size_t>(k)], m.traits, r.error)) {
      r.error = m.name + ": " + r.error;
      return r;
    }
    Fighter& f = fighters[static_cast<size_t>(k)];
    const std::string ents =
        std::string(kCoopHallEntities) + (opt.exit ? kCoopHallExitEntities : "");
    f.features = sh::features_of(m.skill);
    fighter_init(f, graph, s, k + 1, ents.c_str(), m.skill, m.traits, 1000.0 / kMsec,
                 opt.seed + 31u * static_cast<uint32_t>(k + 1));
    f.bot.yaw = s.world.entities[static_cast<size_t>(k) + 2].view_angles.y;
    f.bot.pilot.world = &world;
    escort_init(f, s, k + 1, 0);
    if (k == 0) f.bot.pilot.why = opt.why;
  }

  struct Recent {
    sh::MoveCmd cmds[3];
    uint32_t sequence = 0;
  };
  std::vector<Recent> recent(static_cast<size_t>(mates) + 1);
  std::vector<sh::Packet> packets(static_cast<size_t>(mates) + 1);
  std::vector<bool> alive(static_cast<size_t>(mates) + 1, true);
  size_t leg = 0;
  double leg_since = s.sv.time;
  bool leg_started = false;
  const double start = s.sv.time;
  const int frames = static_cast<int>(opt.seconds * 72);
  constexpr size_t kLegs = sizeof(kCoopTour) / sizeof(kCoopTour[0]);
  for (int frame = 0; frame < frames; frame++) {
    const sv::RulesCommon& now = *c.server;
    const double t = now.sv.time;
    // The leader's command: his leg walked or stood.
    sh::MoveCmd lc;
    lc.msec = kMsec;
    const sv::WorldEntity& le = now.world.entities[1];
    if (le.health <= 0) {
      lc.buttons = (frame & 1) ? sh::kButtonAttack : 0;  // back in at a coop start
      lc.angles = {0, lead.pilot.yaw, 0};
      leg_started = false;
    } else {
      if (leg < kLegs) {
        const TourLeg& tl = kCoopTour[leg];
        if (!leg_started) {
          leg_started = true;
          leg_since = t;
          if (tl.walk) {
            lead.goals.assign(1, sh::nav_nearest(graph, tl.to));
            lead.next_goal = 0;
            lead.pilot.mode = sh::PilotMode::idle;
          }
        }
        const bool done =
            tl.walk ? dist(le.origin, tl.to) < kArrive || lead.pilot.mode == sh::PilotMode::arrived
                    : t - leg_since >= tl.wait_s;
        if (done && leg + 1 < kLegs) {
          leg++;
          leg_started = false;
        }
      }
      const TourLeg& tl = kCoopTour[leg < kLegs ? leg : kLegs - 1];
      if (tl.walk) {
        lc = sh::pilot_source(lead).next(&lead, now.sv, 0, kMsec);
      } else {
        lc.angles = {0, sh::quantize_angle(lead.pilot.yaw), 0};
      }
    }
    Recent& lr = recent[0];
    lr.cmds[0] = lr.cmds[1];
    lr.cmds[1] = lr.cmds[2];
    lr.cmds[2] = lc;
    packets[0].client = 0;
    packets[0].sequence = ++lr.sequence;
    for (int i = 0; i < 3; i++) packets[0].cmds[i] = lr.cmds[i];
    for (int k = 0; k < mates; k++) {
      Recent& rc = recent[static_cast<size_t>(k) + 1];
      rc.cmds[0] = rc.cmds[1];
      rc.cmds[1] = rc.cmds[2];
      rc.cmds[2] = fighter_command(fighters[static_cast<size_t>(k)], now, k + 1, kMsec);
      sh::Packet& p = packets[static_cast<size_t>(k) + 1];
      p.client = k + 1;
      p.sequence = ++rc.sequence;
      for (int i = 0; i < 3; i++) p.cmds[i] = rc.cmds[i];
    }
    const size_t entered = c.history.size();
    if (!sv::campaign_frame(c, packets.data(), mates + 1, r.error)) {
      r.error = "run_coop: " + r.error;
      return r;
    }
    if (c.history.size() != entered) {  // someone took the exit: the hall is over
      r.exited = true;
      break;
    }
    const sv::RulesCommon& after = *c.server;
    sv::clear_frame(c.server.common->out);
    const double at = after.sv.time - start;
    r.t.push_back(static_cast<float>(at));
    const sv::WorldEntity& L = after.world.entities[1];
    r.leader.push_back(L.origin);
    r.leader_health.push_back(L.health);
    r.leg.push_back(static_cast<uint8_t>(leg));
    if (alive[0] && L.health <= 0) r.leader_deaths++;
    alive[0] = L.health > 0;
    for (int k = 0; k < mates; k++) {
      const Fighter& f = fighters[static_cast<size_t>(k)];
      const sv::WorldEntity& d = after.world.entities[static_cast<size_t>(k) + 2];
      CoopMate& m = r.mates[static_cast<size_t>(k)];
      const bool live = d.health > 0;
      m.path.push_back(d.origin);
      m.dist.push_back(dist(d.origin, L.origin));
      m.mode.push_back(live ? static_cast<uint8_t>(f.escort.mode) : 255);
      m.act.push_back(static_cast<uint8_t>(f.escort.idle.act));
      m.yaw.push_back(d.view_angles.y);
      if (alive[static_cast<size_t>(k) + 1] && !live) m.deaths++;
      alive[static_cast<size_t>(k) + 1] = live;
    }
    if (r.button_pressed_at < 0 &&
        dist(after.world.entities[static_cast<size_t>(button)].origin, button_rest) > 0.5f)
      r.button_pressed_at = at;
    if (r.door_open_at < 0 &&
        dist(after.world.entities[static_cast<size_t>(door)].origin, door_rest) > 0.5f)
      r.door_open_at = at;
    for (int e = after.sv.set.maxclients + 1;
         e < after.sv.num_edicts && static_cast<size_t>(e) < was_alive.size(); e++) {
      const sh::Edict& d = after.sv.edicts[static_cast<size_t>(e)];
      if (!was_alive[static_cast<size_t>(e)] || (!d.free && d.solid != sh::kSolidNot)) continue;
      was_alive[static_cast<size_t>(e)] = 0;
      CoopKill kill{at, e, d.origin, -1};
      for (int k = 0; k < mates && kill.by < 0; k++)
        for (const sh::Contact& ct : fighters[static_cast<size_t>(k)].bot.senses.contacts)
          if (ct.ent == e && ct.was_visible) kill.by = k + 1;
      if (kill.by > 0) r.mates[static_cast<size_t>(kill.by) - 1].kills++;
      r.kills.push_back(kill);
    }
    r.digests.push_back(frame_digest(after, mates + 1));
  }
  for (int k = 0; k < mates; k++) {
    CoopMate& m = r.mates[static_cast<size_t>(k)];
    m.escort = fighters[static_cast<size_t>(k)].escort.stats;
    m.fighter = fighters[static_cast<size_t>(k)].stats;
    m.idle = fighters[static_cast<size_t>(k)].escort.idle;
  }
  if (const sv::QcServer* q = sv::qc_of(c.server)) r.faults = q->q.faults;
  r.final_digest = r.digests.empty() ? 0 : r.digests.back();
  r.ok = true;
  return r;
}

}  // namespace shooters_edge::bots
