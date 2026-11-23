// The Reckoning's monsters' own ways, beside the roster's rows (q2_monster_kinds.hpp; the facts are
// porting/shooters-edge/q2-reckoning-monsters.md; the timings between them are ours). Everything
// here answers only to a kind's own flags (`amphibian`, `repairs`, `heals`, `chant`), which only
// the pack's rows set, so the base game's monsters never reach it.
//
// - **The gekk** (`amphibian`): a walker on land, a swimmer once any part of it is in water (its
//   box's top at 16, the walker's 24 again out of it); swimming, every 0.4 s it leaves the water
//   3 times in 10 when its enemy is dry, with a leap of 300 forward and 250 up. Its choice: claws
//   in reach (A 34 times in 100, else B; in water always its water attack), else on land a leap
//   when the mutant's leap test passes, else nothing within 100 level unless it stands lower, else
//   a spit (half the time 80 or more away, else one time in five) or a leap. Its search and idle
//   heal it 10 to 20 (`heals`); with spawn flag 8 it chants and looks for no one until its first
//   blow.
// - **The repair bot** (`repairs`): with no spawn flags it stands and looks for players like any
//   monster. With any, it works: flag 16 lands (5.8 s), flag 8 takes off (1.6 s, ten thrust traces
//   of 2 straight down a frame), then it roams: to the farthest open point of twelve lines round
//   its facing at 50 units/s, a new one within 32 of it, 15 s on or after 10 frames without moving;
//   on the way it takes up any repair object within 1024 it sees with 100 health or more and welds
//   it from within 56: 1 s to start, then loops of 0.7 s (welding sparks the first six frames, 10
//   off at each loop's end while the object is at 0 or more), 0.7 s to finish. Stuck 10 frames
//   going to an object, it lets objects be for 15 s. Hurt, it fights the one who hurt it. It raises
//   the dead by its healing beam (q2_monster_medic.cpp).
//
// ge:summary The Reckoning's monsters' own ways: the gekk in and out of water, the repair bot's work
// ge:keywords q2 reckoning mission-pack gekk amphibian chant heal fixbot repair-bot weld roam
// ge:keywords takeoff landing thrust object-repair xatrix-monsters
// ge:skill shooter
// ge:test tests/shooters_edge_q2_reckoning_monsters_test.cpp
#include <algorithm>
#include <string_view>

#include "core/prng.hpp"
#include "shooters_edge/server/q2_fight.hpp"
#include "shooters_edge/server/q2_map.hpp"
#include "shooters_edge/server/q2_monster_ai.hpp"
#include "shooters_edge/server/q2_monsters.hpp"
#include "shooters_edge/server/q2_reckoning.hpp"
#include "shooters_edge/server/q2_server.hpp"

namespace shooters_edge::server::monster_ai {

namespace {

// The repair bot's work (`Q2Monster::job`).
enum Job : int { kStand = 0, kRoam, kPause, kWeldStart, kWelding, kWeldEnd, kTakeoff, kLanding };

constexpr double kWeldLoop = 0.7, kWeldStartS = 1.0, kTakeoffS = 1.6, kLandingS = 5.8;
constexpr float kRoamSpeed = 50;  // 5 units a frame
constexpr const char* kWelders[3] = {"misc/welder1.wav", "misc/welder2.wav", "misc/welder3.wav"};

// Any part of `m` in water.
bool in_water(const Q2Server& s, const Q2Monster& m) {
  const Q2Entity& e = s.entities[size_t(m.entity)];
  return wet(s, e.origin) || wet(s, foot(e, e.origin));
}

// The gekk's water: in, it swims; out of it (thrown, or its water gone), it walks; swimming, it
// leaves for a dry enemy now and then.
void water(Q2Server& s, Q2Monster& m) {
  Q2Entity& e = body(s, m);
  const bool in = in_water(s, m);
  if (!m.swimming) {
    if (in && !m.leaping) {
      m.swimming = true;
      e.maxs.z = 16;
    }
    return;
  }
  if (!in) {
    m.swimming = false;
    e.maxs.z = kind(m).maxs.z;
    return;
  }
  if (m.enemy == -1 || !foe_alive(s, m.enemy) || s.frame_number % 4 != 0) return;
  if (wet(s, foe_at(s, m.enemy)) || random01(s) >= 0.3) return;
  m.swimming = false;  // out after him: up and over the edge
  e.maxs.z = kind(m).maxs.z;
  sound(s, m.entity, kind(m).sight);
  e.origin.z += 1;
  m.velocity = forward_of(e.angles.y) * 300.0f + sh::Vec3f{0, 0, 250};
  m.on_ground = false;
  m.leaping = true;
  m.struck = false;
  m.busy_until = s.level_time + 3;
}

// A thing the repair bot welds: a repair object within 1024 it sees with 100 health or more.
int find_repair(const Q2Server& s, const Q2Monster& m) {
  const Q2Entity& e = s.entities[size_t(m.entity)];
  for (size_t i = 0; i < s.entities.size(); i++) {
    const Q2Entity& o = s.entities[i];
    if (o.gone || o.classname != "object_repair" || o.health < 100) continue;
    if (sh::length(o.origin - e.origin) > 1024 || !sees_point(s, m, o.origin)) continue;
    return static_cast<int>(i);
  }
  return -1;
}

// A roaming goal: the end of the longest of twelve lines round its facing (0 to 150 degrees either
// way, 30 apart), 32 short of what stopped it.
void roam_goal(Q2Server& s, Q2Monster& m) {
  const Q2Entity& e = body(s, m);
  float best = -1;
  for (int k = 0; k < 12; k++) {
    const float turn_by = float((k / 2) * 30) * (k % 2 == 0 ? 1.0f : -1.0f);
    const sh::Vec3f way = forward_of(e.angles.y + turn_by);
    const ge::classic::Q2Trace t =
        map_trace(s, e.origin, {}, {}, e.origin + way * 8192.0f, kOpaque, m.entity);
    const float d = sh::length(t.end - e.origin);
    if (d <= best) continue;
    best = d;
    m.job_goal = t.end - way * std::min(32.0f, d);
  }
  m.job_since = s.level_time;
  m.job_target = -1;
  m.still = 0;
}

// The welding sparks before the bot (10, palette 224 to 231) and, 2 times in 10, its welder.
void weld_sparks(Q2Server& s, Q2Monster& m) {
  const Q2Entity& e = body(s, m);
  const sh::ViewAxes ax = sh::angle_vectors({0, e.angles.y, 0});
  NetEvent ev;
  ev.kind = EventKind::point_effect;
  ev.to = 0xffffffffu;
  ev.origin = e.origin + ax.forward * 24.0f - ax.right * 0.8f - sh::Vec3f{0, 0, 10};
  ev.values[3] = 10;
  ev.level = 0xe0 + ge::core::int_in(s.rng, 0, 7);
  ev.text = "te_welding_sparks";
  event(s, std::move(ev));
  if (random01(s) < 0.2) sound(s, m.entity, kWelders[std::min(2, int(random01(s) * 3))], 2);
}

// The takeoff's thrust: ten traces of 2 straight down, spread wider each frame.
void thrust(Q2Server& s, Q2Monster& m) {
  const Q2Entity& e = body(s, m);
  const float wide = 1000 + float((s.level_time - m.job_since) * 10);
  for (int i = 0; i < 10; i++) {
    const sh::Vec3f end = e.origin + sh::Vec3f{crandom(s) * wide, crandom(s) * (wide - 500), -8192};
    fight_monster_bullet(s, m.entity, e.origin, end, 2, 1, kSqByMonster + m.kind);
  }
}

// One think of the repair bot's work with no enemy and no corpse to raise.
void work(Q2Server& s, Q2Monster& m) {
  Q2Entity& e = body(s, m);
  const Kind& k = kind(m);
  const double now = s.level_time + 0.001;
  switch (m.job) {
    case kTakeoff:
      thrust(s, m);
      if (now >= m.job_until) m.job = kRoam;
      return;
    case kLanding:
      if (now >= m.job_until) m.job = kRoam;
      return;
    case kPause:  // a stand's length between goals
      if (now >= m.job_until) {
        m.job = kRoam;
        roam_goal(s, m);
      }
      return;
    case kWeldStart:
    case kWelding:
    case kWeldEnd: {
      Q2Entity* o = m.job_target >= 0 ? &s.entities[size_t(m.job_target)] : nullptr;
      if (o != nullptr) turn(e, k, yaw_to(o->origin - e.origin));
      if (m.job == kWeldEnd) {
        step(s, m, forward_of(e.angles.y) * -2.0f);
        if (now >= m.job_until) {
          m.job = kPause;
          m.job_target = -1;
          m.job_until = s.level_time + 1.9;
        }
        return;
      }
      if (m.job == kWeldStart) {
        if (now >= m.job_until) {
          m.job = kWelding;
          m.job_until = s.level_time + kWeldLoop;
        }
        return;
      }
      if (s.level_time + kWeldLoop - 0.1 > m.job_until) weld_sparks(s, m);  // the first six
      if (now < m.job_until) return;
      if (o != nullptr && !o->gone && o->health >= 0) {
        reckoning_damage(s, m.job_target, 10);
        m.job_until = s.level_time + kWeldLoop;
        return;
      }
      m.job = kWeldEnd;
      m.job_until = s.level_time + kWeldLoop;
      return;
    }
    default:
      break;
  }
  // Roaming.
  if (m.job_target < 0 && s.level_time >= m.repair_rest) {
    const int found_one = find_repair(s, m);
    if (found_one >= 0) {
      m.job_target = found_one;
      m.job_since = s.level_time;
      m.still = 0;
    }
  }
  if (m.job_target < 0 && m.job_since == 0) roam_goal(s, m);
  const bool repair = m.job_target >= 0;
  const sh::Vec3f goal = repair ? s.entities[size_t(m.job_target)].origin : m.job_goal;
  const float d = sh::length(goal - e.origin);
  if (repair && d <= 56) {
    m.job = kWeldStart;
    m.job_until = s.level_time + kWeldStartS;
    return;
  }
  const bool timed_out = !repair && s.level_time - m.job_since > 15;
  if (sh::length(e.origin - m.still_at) < 0.01f) {
    m.still++;
  } else {
    m.still = 0;
    m.still_at = e.origin;
  }
  if ((!repair && d <= 32) || timed_out || m.still > 10) {
    if (repair) m.repair_rest = s.level_time + 15;
    m.job_target = -1;
    m.still = 0;
    m.job = kPause;
    m.job_until = s.level_time + 1.9;
    return;
  }
  turn(e, k, yaw_to(goal - e.origin));
  go_to(s, m, goal, kRoamSpeed);
}

}  // namespace

void reckoning_monster_spawn(Q2Server& s, Q2Monster& m) {
  const Kind& k = kind(m);
  const Q2Entity& e = body(s, m);
  if (k.chant[0] != nullptr && (e.spawnflags & 8) != 0) m.chant = true;
  if (!k.repairs || e.spawnflags == 0) return;  // none: it stands and looks, for good
  // Its first work by its flags (landing first, then takeoff, else roaming); after it the flags
  // are spent, the ambush with them.
  m.ambush = false;
  if ((e.spawnflags & 16) != 0) {
    m.job = kLanding;
    m.job_until = s.level_time + kLandingS;
  } else if ((e.spawnflags & 8) != 0) {
    m.job = kTakeoff;
    m.job_since = s.level_time;
    m.job_until = s.level_time + kTakeoffS;
  } else {
    m.job = kRoam;
  }
}

bool reckoning_monster_think(Q2Server& s, Q2Monster& m) {
  const Kind& k = kind(m);
  if (k.amphibian) water(s, m);
  if (!k.repairs || m.job == kStand) return false;
  if (tend(s, m)) return true;  // a corpse first
  if (m.enemy != -1) {          // hurt, it fights; its work waits
    if (m.job != kTakeoff && m.job != kLanding) m.job = kRoam;
    return false;
  }
  work(s, m);
  return true;
}

void reckoning_choose(Q2Server& s, Q2Monster& m, bool melee, float dist) {
  if (melee) {
    begin_attack(s, m, m.swimming || random01(s) >= 0.34 ? 1 : 0);
    return;
  }
  if (m.swimming || m.leaping) return;
  if (leap_test(s, m)) {
    begin_attack(s, m, 2);
    return;
  }
  const Q2Entity& e = body(s, m);
  const sh::Vec3f at = foe_at(s, m.enemy);
  const float level = sh::length(sh::Vec3f{at.x - e.origin.x, at.y - e.origin.y, 0});
  if (level < 100 && e.origin.z >= at.z) return;
  const bool spit = (random01(s) < 0.5 && dist >= kMelee) || random01(s) < 0.2;
  begin_attack(s, m, spit ? 3 : 2);
}

void reckoning_heal(Q2Server& s, Q2Monster& m) {
  Q2Entity& e = body(s, m);
  e.health = std::min(e.max_health, e.health + 10 + static_cast<int>(random01(s) * 10));
}

}  // namespace shooters_edge::server::monster_ai
