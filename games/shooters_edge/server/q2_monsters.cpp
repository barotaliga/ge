// Quake II's monsters (q2_monsters.hpp states the rules; the numbers are the clean-room
// note's, porting/shooters-edge/q2-monsters.md; speeds, timings and the step move are ours).
//
// ge:summary Quake II's monsters at work: noticing, chasing, attacking, the step move, death
// ge:keywords q2 monsters find-target attack-chance monster-step monster-damage corpse
// ge:skill shooter
// ge:test tests/shooters_edge_q2_monsters_test.cpp
#include "shooters_edge/server/q2_monsters.hpp"

#include <algorithm>
#include <cstdlib>
#include <string>
#include <string_view>

#include "brush/light_probe.hpp"
#include "brush/map.hpp"
#include "brush/trace.hpp"
#include "core/libm/libm.hpp"
#include "core/prng.hpp"
#include "shooters_edge/server/q2_fight.hpp"
#include "shooters_edge/server/q2_flash.hpp"
#include "shooters_edge/server/q2_lines.hpp"
#include "shooters_edge/server/q2_map.hpp"
#include "shooters_edge/server/q2_monster_kinds.hpp"
#include "shooters_edge/server/q2_monster_pain.hpp"
#include "shooters_edge/server/q2_monster_steps.hpp"
#include "shooters_edge/server/q2_server.hpp"
#include "shooters_edge/server/q2_targets.hpp"

namespace shooters_edge::server {

namespace {

constexpr float kStep = 0.1f;  // a rules frame, s
constexpr float kMelee = 80, kNear = 500, kMid = 1000;
constexpr float kStepUp = 18;     // the highest step a walker climbs
constexpr float kInFront = 0.3f;  // the cosine of in front
constexpr double kPainEvery = 3;  // s
constexpr float kHear = 1000;     // a noise's reach
constexpr int32_t kMonsterMask =
    sh::kSqSolid | sh::kSqWindow | sh::kSqMonster | 0x20000;  // + monster clip
constexpr int32_t kOpaque = sh::kSqSolid | sh::kSqSlime | sh::kSqLava;
constexpr int32_t kShotMask =
    sh::kSqSolid | sh::kSqWindow | sh::kSqSlime | sh::kSqLava | sh::kSqMonster | sh::kSqDeadMonster;
constexpr uint32_t kEveryone = 0xffffffffu;

enum class Range : uint8_t { melee, near, mid, far };

using roster::Attack;
using roster::End;
using roster::Fire;
using roster::Kind;
using roster::kKindCount;
using roster::kKinds;
using roster::Test;

int kind_of(std::string_view classname) {
  for (int k = 0; k < kKindCount; k++)
    if (classname == kKinds[k].classname) return k;
  return -1;
}

bool deathmatch(const Q2Server& s) { return s.cvars.find("deathmatch")->on(); }
int skill(const Q2Server& s) {
  return std::clamp(static_cast<int>(s.cvars.find("skill")->number), 0, 3);
}
double random01(Q2Server& s) { return ge::core::next(s.rng); }
float crandom(Q2Server& s) { return float(2 * (ge::core::next(s.rng) - 0.5)); }

sh::Vec3f unpack(const int16_t v[3]) {
  return {float(v[0]) * 0.125f, float(v[1]) * 0.125f, float(v[2]) * 0.125f};
}

void event(Q2Server& s, NetEvent ev) {
  ev.time = s.level_time;
  s.out.events.push_back(std::move(ev));
  s.out.events_total++;
}

void sound(Q2Server& s, int e, const char* sample, float reach = 1) {
  if (sample == nullptr) return;
  NetEvent ev;
  ev.kind = EventKind::sound;
  ev.to = kEveryone;
  ev.ent = map_row(e);
  ev.level = 2;  // the voice
  ev.origin = s.entities[size_t(e)].origin;
  ev.values[0] = 255;
  ev.values[1] = reach;
  ev.text = sample;
  event(s, std::move(ev));
}

sh::Vec3f eye(const Q2Entity& e, const Kind& k) {
  return e.origin + sh::Vec3f{0, 0, k.swims ? 10.0f : 25.0f};  // a swimmer's 10, else 25
}
sh::Vec3f player_eye(const Q2Player& p) {
  return unpack(p.move.origin) + sh::Vec3f{0, 0, p.view_height};
}
bool hunted(const Q2Server& s, int slot) {
  return slot >= 0 && slot < sh::kMaxClients && s.clients[slot].active &&
         !s.clients[slot].spectator && s.players[slot].alive;
}

// A player a monster may take up as its enemy by sight, by noise, by another's fight or by his
// trigger: hunted and not under `notarget` (q2_commands.hpp). A fight under way goes on.
bool targetable(const Q2Server& s, int slot) {
  return hunted(s, slot) && !s.players[slot].notarget;
}

// ---- Foes: a player's slot, or another monster as -2 - its entity (q2_server.hpp's `enemy`) ----

int foe_of_entity(int e) { return -2 - e; }
int foe_entity(int foe) { return foe <= -2 ? -2 - foe : -1; }
const Q2Monster* foe_monster(const Q2Server& s, int foe) {
  const int e = foe_entity(foe);
  const int i = e >= 0 ? monster_index(s, e) : -1;
  return i >= 0 ? &s.monsters[size_t(i)] : nullptr;
}
// A foe still there to fight: a player hunted, or a monster alive.
bool foe_alive(const Q2Server& s, int foe) {
  if (foe >= 0) return hunted(s, foe);
  const Q2Monster* o = foe_monster(s, foe);
  if (o == nullptr || o->dead) return false;
  const Q2Entity& b = s.entities[size_t(o->entity)];
  return !b.gone && b.health > 0;
}
sh::Vec3f foe_at(const Q2Server& s, int foe) {
  if (foe >= 0) return unpack(s.players[foe].move.origin);
  const Q2Monster* o = foe_monster(s, foe);
  return o != nullptr ? s.entities[size_t(o->entity)].origin : sh::Vec3f{};
}
sh::Vec3f foe_velocity(const Q2Server& s, int foe) {
  if (foe >= 0) return unpack(s.players[foe].move.velocity);
  const Q2Monster* o = foe_monster(s, foe);
  return o != nullptr ? o->velocity : sh::Vec3f{};
}
sh::Vec3f foe_eye(const Q2Server& s, int foe) {
  if (foe >= 0) return player_eye(s.players[foe]);
  const Q2Monster* o = foe_monster(s, foe);
  return o != nullptr ? eye(s.entities[size_t(o->entity)], kKinds[o->kind]) : sh::Vec3f{};
}
// The foe's box about its origin.
void foe_box(const Q2Server& s, int foe, sh::Vec3f& mins, sh::Vec3f& maxs) {
  if (foe >= 0) {
    mins = s.players[foe].mins;
    maxs = s.players[foe].maxs;
    return;
  }
  const Q2Monster* o = foe_monster(s, foe);
  mins = o != nullptr ? s.entities[size_t(o->entity)].mins : sh::Vec3f{};
  maxs = o != nullptr ? s.entities[size_t(o->entity)].maxs : sh::Vec3f{};
}

Range range_to(sh::Vec3f a, sh::Vec3f b) {
  const float d = sh::length(b - a);
  return d < kMelee ? Range::melee : d < kNear ? Range::near : d < kMid ? Range::mid : Range::far;
}

sh::Vec3f forward_of(float yaw) { return sh::angle_vectors({0, yaw, 0}).forward; }

bool in_front(const Q2Entity& e, sh::Vec3f at) {
  sh::Vec3f to = at - e.origin;
  to.z = 0;
  if (sh::normalize(to) == 0) return true;
  return sh::dot(to, forward_of(e.angles.y)) > kInFront;
}

float yaw_to(sh::Vec3f d) {
  constexpr double kDegrees = 180.0 / 3.14159265358979323846;
  if (d.x == 0 && d.y == 0) return 0;
  float y = static_cast<float>(ge::core::libm::atan2(double(d.y), double(d.x)) * kDegrees);
  return y < 0 ? y + 360 : y;
}

const Kind& kind(const Q2Monster& m) { return kKinds[m.kind]; }
Q2Entity& body(Q2Server& s, const Q2Monster& m) { return s.entities[size_t(m.entity)]; }

// Monster `m` thrown apart by a blow of `damage`: its pieces by its kind (q2_gibs.hpp).
void gib_monster(Q2Server& s, const Q2Monster& m, int damage) {
  const Q2Entity& e = s.entities[size_t(m.entity)];
  fight_gib(s, map_row(m.entity), e.origin,
            {e.origin + e.mins, e.origin + e.maxs, m.velocity, damage,
             sq_gib_set(kKinds[m.kind].classname)});
}
bool afloat(const Kind& k) { return k.flies || k.swims; }

// Water (or slime or lava) at `p`.
bool wet(const Q2Server& s, sh::Vec3f p) {
  if (!s.tracer) return false;
  const float q[3] = {p.x, p.y, p.z};
  return (s.tracer->contents(q) & sh::kSqMaskWater) != 0;
}
// The point 1 above the bottom of a box at `origin`.
sh::Vec3f foot(const Q2Entity& e, sh::Vec3f origin) {
  return origin + sh::Vec3f{0, 0, e.mins.z + 1};
}

// A box swept for monster `m` from `start` to `end`: the map, the movers, the other monsters and
// the players.
ge::classic::Q2Trace sweep(const Q2Server& s, const Q2Monster& m, sh::Vec3f start, sh::Vec3f end) {
  const Q2Entity& e = s.entities[size_t(m.entity)];
  ge::classic::Q2Trace t = map_trace(s, start, e.mins, e.maxs, end, kMonsterMask, m.entity);
  for (int i = 0; i < sh::kMaxClients; i++) {
    if (!hunted(s, i)) continue;
    const Q2Player& p = s.players[i];
    const sh::Vec3f at = unpack(p.move.origin);
    sh::Vec3f normal;
    const float f = sweep_box(start, end, e.mins, e.maxs, at + p.mins, at + p.maxs, normal);
    if (f >= 0 && f < t.fraction) {
      t.fraction = f;
      t.end = start + (end - start) * f;
      t.normal = normal;
      t.entity = i + 1;
    }
  }
  return t;
}

// A line from the monster's eye to the player's eye, through `mask`, reaches him.
bool reaches(const Q2Server& s, const Q2Monster& m, int foe, int32_t mask) {
  const Q2Entity& e = s.entities[size_t(m.entity)];
  ge::classic::Q2Trace t = map_trace(s, eye(e, kind(m)), {}, {}, foe_eye(s, foe), mask, m.entity);
  if (t.fraction < 1 && foe < 0) {  // a monster foe: its own box is no wall
    const Q2Monster* o = foe_monster(s, foe);
    if (o != nullptr && t.entity == map_row(o->entity)) return true;
  }
  return t.fraction >= 1;
}

bool on_floor(const Q2Server& s, const Q2Monster& m) {
  const Q2Entity& e = s.entities[size_t(m.entity)];
  const ge::classic::Q2Trace t = sweep(s, m, e.origin, e.origin - sh::Vec3f{0, 0, 0.25f});
  return t.fraction < 1 && t.normal.z > 0.7f;
}

// A walker's step of `move` along the floor: up a step of 18 when blocked, never off a ledge
// higher than a step, never into water from dry land. A flier never flies into water; a swimmer
// never swims out of it. False when it could not go.
bool step(Q2Server& s, Q2Monster& m, sh::Vec3f move) {
  Q2Entity& e = body(s, m);
  const Kind& k = kind(m);
  const bool was_wet = wet(s, foot(e, e.origin));
  if (afloat(k)) {
    const ge::classic::Q2Trace t = sweep(s, m, e.origin, e.origin + move);
    if (t.start_solid || t.fraction < 1) return false;
    const bool will_be_wet = wet(s, foot(e, t.end));
    if (k.flies && !was_wet && will_be_wet) return false;
    if (k.swims && was_wet && !will_be_wet) return false;
    e.origin = t.end;
    return true;
  }
  move.z = 0;
  const sh::Vec3f up{0, 0, kStepUp};
  sh::Vec3f to;
  ge::classic::Q2Trace t = sweep(s, m, e.origin, e.origin + move);
  if (t.start_solid) return false;
  if (t.fraction >= 1) {
    to = t.end;
  } else {  // up a step, along, then down onto it
    const ge::classic::Q2Trace rise = sweep(s, m, e.origin, e.origin + up);
    const ge::classic::Q2Trace along = sweep(s, m, rise.end, rise.end + move);
    if (along.fraction < 1 || along.start_solid) return false;
    const ge::classic::Q2Trace down = sweep(s, m, along.end, along.end - up);
    if (down.fraction >= 1 || down.normal.z <= 0.7f) return false;
    to = down.end;
  }
  if (m.on_ground) {  // never off a ledge
    const ge::classic::Q2Trace below = sweep(s, m, to, to - sh::Vec3f{0, 0, kStepUp + 1});
    if (below.fraction >= 1) return false;
    to = below.fraction > 0 ? below.end : to;
  }
  if (!was_wet && wet(s, foot(e, to))) return false;
  e.origin = to;
  return true;
}

// The monster's fall under gravity (a walker off the floor, a leaper, a dead flier); a leaper's
// strike on what it meets fast.
void fall(Q2Server& s, Q2Monster& m) {
  Q2Entity& e = body(s, m);
  const Kind& k = kind(m);
  if (k.flies && !m.dead) {  // no gravity: a blow's push, worn off by friction (a third upright)
    if (m.velocity.x == 0 && m.velocity.y == 0 && m.velocity.z == 0) return;
    const auto wear = [](float speed, float friction) {
      const float drop = std::max(speed, 100.0f) * friction * kStep;
      return speed > 0 ? std::max(0.0f, speed - drop) / speed : 0.0f;
    };
    const float flat = sh::length(sh::Vec3f{m.velocity.x, m.velocity.y, 0});
    const float hs = wear(flat, 6), vs = wear(std::abs(m.velocity.z), 2);
    m.velocity = {m.velocity.x * hs, m.velocity.y * hs, m.velocity.z * vs};
    const ge::classic::Q2Trace t = sweep(s, m, e.origin, e.origin + m.velocity * kStep);
    e.origin = t.end;
    if (t.fraction < 1) m.velocity = {};
    return;
  }
  if (k.swims && wet(s, foot(e, e.origin))) return;
  if (m.gravity == 0 && !m.dead && (k.good || k.turret)) {  // hung up, or riding a turret
    m.velocity = {};
    return;
  }
  m.on_ground = m.velocity.z <= 0 && on_floor(s, m);
  if (m.on_ground) {
    m.velocity = {};
    return;
  }
  q2_hold_speed(s, m.velocity);
  m.velocity.z -= static_cast<float>(s.cvars.find("sv_gravity")->number) * m.gravity * kStep;
  const ge::classic::Q2Trace t = sweep(s, m, e.origin, e.origin + m.velocity * kStep);
  if (m.leaping && !m.struck && t.fraction < 1 && t.entity >= 1 && t.entity <= sh::kMaxClients &&
      sh::length(m.velocity) > 400) {  // the leap lands on someone
    const Attack& a = k.missile[0];
    const int dmg = a.damage + static_cast<int>(random01(s) * (a.damage_max - a.damage + 1));
    fight_hurt(s, t.entity - 1, -1, m.velocity, dmg, dmg, kSqByMonster + m.kind);
    m.struck = true;
  }
  e.origin = t.end;
  if (t.fraction < 1 && t.normal.z > 0.7f) {
    m.on_ground = true;
    m.velocity = {};
    if (m.leaping) {  // down again: the thud, and free to attack
      m.leaping = false;
      m.busy_until = s.level_time + double(k.missile[0].recover);
      m.attack_finished = s.level_time;
    }
  }
}

// Turns toward the ideal yaw by the kind's turning speed.
void turn(Q2Entity& e, const Kind& k, float ideal) {
  const float most = afloat(k) ? 10.0f : 20.0f;
  float d = ideal - e.angles.y;
  while (d > 180) d -= 360;
  while (d < -180) d += 360;
  d = std::clamp(d, -most, most);
  e.angles.y += d;
  if (e.angles.y < 0) e.angles.y += 360;
  if (e.angles.y >= 360) e.angles.y -= 360;
}

// Goes toward `goal` at `speed` a second, turning to it; around an obstacle by a side step.
void go_to(Q2Server& s, Q2Monster& m, sh::Vec3f goal, float speed) {
  Q2Entity& e = body(s, m);
  sh::Vec3f d = goal - e.origin;
  if (!afloat(kind(m))) d.z = 0;
  const float dist = sh::length(d);
  if (dist < 1) return;
  m.ideal_yaw = yaw_to(d);
  turn(e, kind(m), m.ideal_yaw);
  sh::Vec3f way = d * (1 / dist);
  const float len = std::min(speed * kStep, dist);
  if (step(s, m, way * len)) return;
  // Blocked: a step to one side, then the other.
  const sh::Vec3f side = sh::angle_vectors({0, m.ideal_yaw, 0}).right;
  const float sign = random01(s) < 0.5 ? 1.0f : -1.0f;
  if (!step(s, m, side * (len * sign))) step(s, m, side * (-len * sign));
}

// ---- Noticing ----

// The monster turns on `foe` (seen when `saw`).
void found(Q2Server& s, Q2Monster& m, int foe, bool saw) {
  if (kind(m).good) return;  // a good guy hunts no one
  m.enemy = foe;
  if (!m.lost_combat.empty()) q2_lost_combat(s, m);
  m.seen_at = s.level_time;
  m.last_sight = foe_at(s, foe);
  if (!m.hold) m.attack_finished = s.level_time + 1;  // a second before its first shot
  m.sound_target = false;
  m.lost = m.on_trail = false;
  m.trail_time = s.level_time;
  m.search_time = 0;
  if (saw) {
    s.sight_entity = m.entity;
    s.sight_frame = s.frame_number;
    const bool soldier_kind =
        kind(m).sight != nullptr && std::string_view(kind(m).sight) == "soldier/solsght1.wav";
    sound(s, m.entity, soldier_kind && random01(s) < 0.5 ? "soldier/solsrch1.wav" : kind(m).sight);
    // A soldier 500 or more off, above the easiest skill: half the time it fires on the run.
    if (soldier_kind && skill(s) >= 1 &&
        sh::length(foe_at(s, foe) - s.entities[size_t(m.entity)].origin) >= kNear &&
        random01(s) < 0.5)
      m.run_shoot = 0;
  }
}

// Whether player `slot` stands in the dark: his light (150 times the brightest channel of the
// baked light on the floor under his eye) 5 or less, and no muzzle's flash on him (a shot in the
// last fifth of a second). A map with no lightmaps is lit.
bool in_dark(const Q2Server& s, int slot) {
  const Q2Player& p = s.players[slot];
  if (s.light == nullptr || s.map == nullptr || s.light->tris.empty()) return false;
  if (s.level_time < p.hostile_until - 0.8) return false;
  const sh::Vec3f eye = player_eye(p);
  const float at[3] = {eye.x, eye.y, eye.z};
  float rgb[3] = {1, 1, 1};
  if (!ge::brush::light_below(*s.light, *s.map, at, rgb)) return false;
  return 150.0f * std::max({rgb[0], rgb[1], rgb[2]}) <= 5.0f;
}

// Whether monster `m` sees `foe` now.
bool sees(const Q2Server& s, const Q2Monster& m, int foe) {
  const Q2Entity& e = s.entities[size_t(m.entity)];
  const sh::Vec3f at = foe_at(s, foe);
  const Range r = range_to(e.origin, at);
  if (r == Range::far) return false;
  if (!reaches(s, m, foe, kOpaque)) return false;
  if (r == Range::melee) return true;                   // close by: seen whichever way it faces
  if (r == Range::near && m.enemy == foe) return true;  // already found: no need to face him
  // Near, a player who fired in the last second is seen behind it too.
  if (r == Range::near && foe >= 0 && s.level_time < s.players[foe].hostile_until) return true;
  return in_front(e, at);
}

void notice(Q2Server& s, Q2Monster& m) {
  // 1. a monster that saw a player this frame or the last.
  if (!m.ambush && s.sight_entity >= 0 && s.sight_entity != m.entity &&
      s.sight_frame >= s.frame_number - 1) {
    const int other = monster_index(s, s.sight_entity);
    if (other >= 0) {
      const int slot = s.monsters[size_t(other)].enemy;
      if (targetable(s, slot) && slot != m.enemy) {
        found(s, m, slot, false);
        return;
      }
    }
  }
  // 2. a player's noise heard (hearable, through open doors; an ambusher must see where), within
  // 1000; 3. his shot's impact, by one with no enemy and no ambush. It goes to where it was.
  const Q2Entity& e = body(s, m);
  bool heard = false;
  for (int pass = 0; pass < 2 && !heard; pass++) {
    if (pass == 1 && (m.ambush || m.enemy != -1)) break;
    for (int i = 0; i < sh::kMaxClients; i++) {
      if (!targetable(s, i)) continue;
      const Q2Player& p = s.players[i];
      if ((pass == 0 ? p.noise_frame : p.impact_frame) < s.frame_number - 1) continue;
      const sh::Vec3f at = pass == 0 ? p.noise_at : p.impact_at;
      if (m.ambush ? map_trace(s, eye(e, kind(m)), {}, {}, at, kOpaque, m.entity).fraction < 1
                   : !map_hears(s, e.origin, at))
        continue;
      if (sh::length(at - e.origin) > kHear || !map_areas_connected(s, e.origin, at)) continue;
      if (m.enemy == -1) {
        m.sound_target = true;
        m.sound_at = at;
        m.sound_time = s.level_time;
        m.ideal_yaw = yaw_to(at - e.origin);  // turned to the sound
        if (!m.hold) m.attack_finished = s.level_time + 1;
      }
      heard = true;
      break;
    }
  }
  // 4. the frame's player in turn, seen (not in the dark).
  const int slot = s.sight_client;
  if (targetable(s, slot) && sees(s, m, slot) && !in_dark(s, slot)) found(s, m, slot, true);
}

// ---- Attacks ----

// The enemy's eye, `lead` s along his motion.
sh::Vec3f mark(const Q2Server& s, const Q2Monster& m, float lead) {
  return foe_eye(s, m.enemy) + foe_velocity(s, m.enemy) * lead;
}

// A shot from `from` at `target`, with the attack's spread.
sh::Vec3f aim_at(Q2Server& s, const Attack& a, sh::Vec3f from, sh::Vec3f target) {
  sh::Vec3f dir = target - from;
  sh::normalize(dir);
  if (a.h == 0 && a.v == 0) return dir;
  const sh::ViewAxes ax = sh::angle_vectors({0, yaw_to(dir), 0});
  const float across = crandom(s) * a.h;  // the draws in a fixed order: one draw a statement
  const float up = crandom(s) * a.v;
  const sh::Vec3f end = from + dir * 8192 + ax.right * across + ax.up * up;
  sh::Vec3f out = end - from;
  sh::normalize(out);
  return out;
}
// The point a shot is aimed at: the mark, spread first by the soldiers' own aim.
sh::Vec3f aim_point(Q2Server& s, const Q2Monster& m, const Attack& a, sh::Vec3f from) {
  const sh::Vec3f t = mark(s, m, a.lead);
  if (a.aim_h == 0 && a.aim_v == 0) return t;
  Attack wide;
  wide.h = a.aim_h;
  wide.v = a.aim_v;
  return from + aim_at(s, wide, from, t) * 8192;
}
sh::Vec3f aim(Q2Server& s, Q2Monster& m, const Attack& a, sh::Vec3f from) {
  return aim_at(s, a, from, aim_point(s, m, a, from));
}

int cause_of(const Q2Monster& m) { return kSqByMonster + m.kind; }

// A blow of `dmg` from `m` on its foe: a player hurt as by a monster, a monster as by this one.
void hurt_foe(Q2Server& s, const Q2Monster& m, int dmg, sh::Vec3f dir) {
  if (m.enemy >= 0) {
    fight_hurt(s, m.enemy, -1, {}, dmg, 0, cause_of(m));
    return;
  }
  if (const Q2Monster* o = foe_monster(s, m.enemy))
    monster_damage(s, o->entity, foe_of_entity(m.entity), dmg, dir);
}

const Attack& attack_at(const Q2Monster& m, int i) {
  return i < 2 ? kind(m).melee[i] : kind(m).missile[i - 2];
}
const Attack& attack_of(const Q2Monster& m) { return attack_at(m, m.attack); }

// A blow within 80 of the monster's origin, by a clear line: the damage and a kick added straight
// to the enemy's velocity (negative pulls). True when it landed.
bool blow(Q2Server& s, Q2Monster& m, const Attack& a) {
  const Q2Entity& e = body(s, m);
  const sh::Vec3f at = foe_at(s, m.enemy);
  if (sh::length(at - e.origin) > kMelee) return false;  // swung at the air
  if (!reaches(s, m, m.enemy, kOpaque)) return false;
  const int dmg = a.damage + static_cast<int>(random01(s) * (a.damage_max - a.damage + 1));
  sh::Vec3f dir = at - e.origin;
  sh::normalize(dir);
  hurt_foe(s, m, dmg, dir);
  if (a.kick != 0 && hunted(s, m.enemy)) fight_kick(s, m.enemy, dir * float(a.kick));
  return true;
}

// One frame's shots of attack `a` (`n` the shot's number in its loop).
void shoot(Q2Server& s, Q2Monster& m, const Attack& a, int n) {
  if (!foe_alive(s, m.enemy)) return;
  Q2Entity& e = body(s, m);
  const sh::Vec3f from = eye(e, kind(m));
  m.ideal_yaw = yaw_to(foe_at(s, m.enemy) - e.origin);
  if (a.fire != Fire::sweep) e.angles.y = m.ideal_yaw;
  // The shot leaving: its flash at the gun along the aim (a burst's every frame).
  const char* flash = nullptr;
  switch (a.fire) {
    case Fire::bolt:
    case Fire::sweep:
      flash = "te_monster_flash.blaster";
      break;
    case Fire::pellets:
    case Fire::burst:
    case Fire::guns:
      flash = "te_monster_flash.gun";
      break;
    case Fire::grenade:
    case Fire::rockets:
      flash = "te_monster_flash.rocket";
      break;
    case Fire::rail:
      flash = "te_monster_flash.rail";
      break;
    case Fire::bfg:
      flash = "te_monster_flash.bfg";
      break;
    default:
      break;
  }
  if (flash != nullptr) {
    const sh::Vec3f to = foe_at(s, m.enemy);
    sh::Vec3f d = to - from;
    sh::normalize(d);
    fight_monster_line(
        s, flash, m.entity, from, from + d * 16.0f,
        q2_monster_flash_value(kind(m).classname, std::string_view(flash).substr(17)));
  }
  switch (a.fire) {
    case Fire::bolt:
      for (int i = 0; i < a.count; i++)
        fight_monster_launch(s, m.entity, SqShot::bolt, from, aim(s, m, a, from), a.damage, a.speed,
                             cause_of(m));
      return;
    case Fire::grenade:
      fight_monster_launch(s, m.entity, SqShot::grenade, from, aim(s, m, a, from), a.damage,
                           a.speed, cause_of(m));
      return;
    case Fire::rockets: {
      const int volley = a.every > 0 ? a.count : 1;  // on a loop: all at once
      for (int i = 0; i < volley; i++)
        fight_monster_launch(s, m.entity, SqShot::rocket, from, aim(s, m, a, from), a.damage,
                             a.speed, cause_of(m));
      return;
    }
    case Fire::pellets: {
      const sh::Vec3f to = aim_point(s, m, a, from);  // one aim, each pellet spread about it
      for (int i = 0; i < a.count; i++) {
        const sh::Vec3f d = aim_at(s, a, from, to);
        fight_monster_bullet(s, m.entity, from, from + d * 8192, a.damage, a.kick, cause_of(m));
      }
      return;
    }
    case Fire::burst:
    case Fire::guns:
      for (int i = 0; i < (a.fire == Fire::guns ? a.count : 1); i++) {
        const sh::Vec3f d = aim(s, m, a, from);
        fight_monster_bullet(s, m.entity, from, from + d * 8192, a.damage, a.kick, cause_of(m));
      }
      return;
    case Fire::drain: {  // a tongue that holds while the line is clear, within 256, ±30° of pitch
      if (!reaches(s, m, m.enemy, kShotMask & ~(sh::kSqMonster | sh::kSqDeadMonster))) return;
      const sh::Vec3f d = foe_at(s, m.enemy) - from;
      const float flat = sh::length(sh::Vec3f{d.x, d.y, 0});
      if (sh::length(d) > 256 || std::abs(d.z) > 0.57735f * flat) return;  // tan 30°
      fight_monster_line(s, "te_parasite_attack", m.entity, from, foe_at(s, m.enemy));
      const int dmg = m.burst_frame == 0 ? a.damage : a.damage_max;
      hurt_foe(s, m, dmg, {});
      return;
    }
    case Fire::club:
      if (blow(s, m, a)) m.struck = true;
      return;
    case Fire::zap: {  // wherever he is
      const int dmg = a.damage + static_cast<int>(random01(s) * (a.damage_max - a.damage + 1));
      fight_effect(s, "te_sparks", e.origin + forward_of(e.angles.y) * 18.5f);
      hurt_foe(s, m, dmg, {});
      return;
    }
    case Fire::rail:
      if (!m.aimed) m.aim_at = mark(s, m, 0);
      fight_monster_rail(s, m.entity, from, m.aim_at, a.damage, a.kick, cause_of(m));
      return;
    case Fire::bfg:
      fight_monster_bfg(s, m.entity, from, aim(s, m, a, from), a.speed, a.damage, a.reach,
                        cause_of(m));
      return;
    case Fire::sweep: {  // from 80 to one side to 0, then from 70 to the other side to 0
      const float offset = n < 9 ? 80.0f - 10.0f * float(n) : -70.0f + 10.0f * float(n - 9);
      sh::Vec3f d = mark(s, m, 0) - from;
      const float level = sh::length(sh::Vec3f{d.x, d.y, 0});
      const sh::Vec3f across = forward_of(e.angles.y + offset) * level;
      d = sh::Vec3f{across.x, across.y, d.z};
      sh::normalize(d);
      fight_monster_launch(s, m.entity, SqShot::bolt, from, d, a.damage, a.speed, cause_of(m));
      return;
    }
    case Fire::leap: {  // off the ground at the enemy
      const sh::Vec3f fwd = forward_of(e.angles.y);
      e.origin.z += 1;
      m.velocity = fwd * a.speed + sh::Vec3f{0, 0, 250};
      m.on_ground = false;
      m.leaping = true;
      m.struck = false;
      sound(s, m.entity, kind(m).sight);
      return;
    }
    default:
      return;
  }
}

// Whole frames in `seconds` (the timings are whole tenths; the hundredth absorbs float error).
int frames(float seconds) { return std::max(1, static_cast<int>(seconds * 10 + 0.01f)); }

// Starts attack `i` (0 and 1 the blows, 2.. the ranged ones): its shots on the next frames.
void begin_attack(Q2Server& s, Q2Monster& m, int i) {
  const Attack& a = attack_at(m, i);
  m.attack = i;
  m.pose.cue_attack = i;
  m.slide = false;  // an attack ends a flier's slide
  m.burst_frame = 0;
  m.loops = 0;
  m.aimed = false;
  m.struck = false;
  m.fire_at = s.level_time + double(a.windup);
  double hold = 0.1;
  if (a.every > 0)
    hold = double(frames(a.loop) - 1) * kStep + 0.1;
  else if (a.fire == Fire::burst)
    hold = a.burst_min + random01(s) * double(a.burst_max - a.burst_min);
  else if (a.fire == Fire::rockets)
    hold = 0.2 * (a.count - 1) + 0.1;
  else if (a.fire == Fire::drain)
    hold = 1;
  m.burst_until = m.fire_at + hold;
  m.busy_until = m.burst_until + double(a.recover);
  if (a.fire == Fire::leap) m.busy_until = m.fire_at + 3;  // until it lands
  if (a.fire == Fire::rail && a.remember >= a.windup) {    // the aim taken now
    m.aim_at = mark(s, m, 0);
    m.aimed = true;
  }
}

// Whether the attack in hand goes on for another loop.
bool goes_on(Q2Server& s, Q2Monster& m, const Attack& a) {
  if (a.again <= 0 || (a.again_max > 0 && m.loops >= a.again_max)) return false;
  if (skill(s) < a.again_skill) return false;
  if (!foe_alive(s, m.enemy)) return false;
  const Q2Entity& e = body(s, m);
  const Range r = range_to(e.origin, foe_at(s, m.enemy));
  if (m.attack < 2) {  // a blow: while he is within reach (at skill 3 the mutant's, by chance)
    if (r != Range::melee && !(skill(s) == 3 && kind(m).test == Test::mutant)) return false;
  } else if (!reaches(s, m, m.enemy, kOpaque)) {
    return false;
  }
  return random01(s) < a.again;
}

// The attack in progress, this frame.
void attacking(Q2Server& s, Q2Monster& m) {
  const Attack& a = attack_of(m);
  if (a.fire == Fire::rail && !m.aimed && foe_alive(s, m.enemy) &&
      s.level_time + 0.001 >= m.fire_at - double(a.remember)) {
    m.aim_at = mark(s, m, 0);
    m.aimed = true;
  }
  if (s.level_time + 0.001 < m.fire_at || s.level_time > m.burst_until + 0.001) return;
  if (a.every > 0) {
    const bool later = m.loops > 0;
    const int every = frames(later && a.again_every > 0 ? a.again_every : a.every);
    const int n = m.burst_frame;
    if (n % every == 0 && (a.shots == 0 || n / every < a.shots)) shoot(s, m, a, n / every);
    m.burst_frame++;
    if (s.level_time + kStep > m.burst_until + 0.001 && goes_on(s, m, a)) {
      m.loops++;
      const int more = frames(a.again_loop > 0 ? a.again_loop : a.loop);
      m.burst_frame = 0;
      m.fire_at = m.burst_until + kStep;
      m.burst_until += double(more) * kStep;
      m.busy_until = m.burst_until + double(a.recover);
    }
  } else {
    const bool every_frame = a.fire == Fire::burst || a.fire == Fire::drain;
    const bool volley = a.fire == Fire::rockets && m.burst_frame % 2 == 0;
    if (m.burst_frame == 0 || every_frame || volley) shoot(s, m, a, m.burst_frame);
    m.burst_frame++;
  }
  // A blow that landed and is followed by another (the brain's grab, then its claws).
  if (m.attack < 2 && a.follow >= 0 && m.struck && skill(s) >= 1 &&
      s.level_time + kStep > m.burst_until + 0.001)
    begin_attack(s, m, a.follow);
}

int ranged_count(const Kind& k) {
  int n = 0;
  while (n < 3 && k.missile[n].fire != Fire::none) n++;
  return n;
}

// The ranged attack chosen by weight (the first one nearer than the kind's `close`).
int pick_ranged(Q2Server& s, const Q2Monster& m, float dist) {
  const Kind& k = kind(m);
  const int n = ranged_count(k);
  if (k.close > 0 && dist < k.close) return 2;
  if (k.by_range && n == 3) {  // the tank: blaster or gun within 125 (60/40) and 250 (even)
    const double roll = random01(s);
    if (dist <= 125) return roll < 0.4 ? 4 : 2;
    if (dist <= 250) return roll < 0.5 ? 4 : 2;
    return roll < 1.0 / 3 ? 2 : roll < 2.0 / 3 ? 3 : 4;  // a third each farther
  }
  float total = 0;
  for (int i = 0; i < n; i++) total += k.missile[i].weight;
  const float roll = float(random01(s)) * total;
  float sum = 0;
  for (int i = 0; i < n; i++) {
    sum += k.missile[i].weight;
    if (roll < sum) return 2 + i;
  }
  return 2 + n - 1;
}

// The mutant's leap test: the heights overlap and he is 100 or more away (then one time in ten).
bool leap_test(Q2Server& s, const Q2Monster& m) {
  const Q2Entity& e = body(s, m);
  const sh::Vec3f at = foe_at(s, m.enemy);
  sh::Vec3f lo, hi;
  foe_box(s, m.enemy, lo, hi);
  const float height = hi.z - lo.z;
  const float bottom = at.z + lo.z;
  if (e.origin.z + e.mins.z > bottom + 0.75f * height) return false;
  if (e.origin.z + e.maxs.z < bottom + 0.25f * height) return false;
  const sh::Vec3f d{at.x - e.origin.x, at.y - e.origin.y, 0};
  if (sh::length(d) < 100) return false;
  return random01(s) < 0.1;
}

// The attack chosen this frame, if any (q2-monsters.md § Choosing to attack;
// q2-monsters-full.md for the kinds with their own test).
void choose(Q2Server& s, Q2Monster& m) {
  if (s.level_time + 0.001 < m.busy_until) return;
  Q2Entity& e = body(s, m);
  const Kind& k = kind(m);
  const bool boss = k.test == Test::hover || k.test == Test::rider;
  if (!reaches(s, m, m.enemy, boss ? kShotMask & ~sh::kSqWindow : kShotMask)) return;
  const sh::Vec3f at = foe_at(s, m.enemy);
  // It turns to him first: an attack only within 45 degrees of facing him.
  const float want = yaw_to(at - e.origin);
  float off = e.angles.y - want;
  while (off > 180) off -= 360;
  while (off < -180) off += 360;
  if (std::abs(off) > 45) {
    turn(e, k, want);
    return;
  }
  const float dist = sh::length(at - e.origin);
  const Range r = range_to(e.origin, at);
  const bool has_missile = k.missile[0].fire != Fire::none;
  const bool has_melee = k.melee[0].fire != Fire::none;
  if (k.test == Test::mutant) {
    if (r == Range::melee)
      begin_attack(s, m, 0);
    else if (leap_test(s, m))
      begin_attack(s, m, 2);
    return;
  }
  if (r == Range::melee) {
    if (!boss && skill(s) == 0 && random01(s) < 0.75) return;
    if (has_melee) {
      const bool second = k.melee[1].fire != Fire::none && random01(s) < 0.5;
      begin_attack(s, m, second ? 1 : 0);
      return;
    }
    if (has_missile) {  // no blow: the ranged attack, always
      const int i = pick_ranged(s, m, dist);
      if (dist >= attack_at(m, i).closest) begin_attack(s, m, i);
      return;
    }
  }
  if (!has_missile || r == Range::far || s.level_time + 0.001 < m.attack_finished) return;
  double chance;
  if (k.test == Test::hover) {
    chance = m.hold ? 0.4 : 0.8;
  } else if (k.test == Test::rider) {
    chance = m.hold ? 0.4 : r == Range::melee ? 0.8 : r == Range::near ? 0.4 : 0.2;
  } else {
    chance = m.hold ? 0.4 : r == Range::melee ? 0.2 : r == Range::near ? 0.1 : 0.02;
    if (skill(s) == 0) chance *= 0.5;
    if (skill(s) >= 2) chance *= 2;
  }
  if (random01(s) >= chance) {
    // A flier that did not fire slides sideways three times in ten, each such frame afresh.
    if (k.flies && r != Range::melee) m.slide = random01(s) < 0.3;
    return;
  }
  const int i = pick_ranged(s, m, dist);
  m.attack_finished = s.level_time + 2 * random01(s);
  if (dist < attack_at(m, i).closest) return;  // too near for it: keeps closing
  begin_attack(s, m, i);
}

// ---- The medic ----

// Whether `medic` sees corpse `c` (eye to its middle).
bool sees_body(const Q2Server& s, const Q2Monster& medic, const Q2Monster& c) {
  const Q2Entity& e = s.entities[size_t(medic.entity)];
  const Q2Entity& b = s.entities[size_t(c.entity)];
  const sh::Vec3f mid = b.origin + (b.mins + b.maxs) * 0.5f;
  return map_trace(s, eye(e, kind(medic)), {}, {}, mid, kOpaque, medic.entity).fraction >= 1;
}

// The corpse within 1024 it sees with the most health to come back to; -1 none.
int find_patient(const Q2Server& s, const Q2Monster& medic) {
  const Q2Entity& e = s.entities[size_t(medic.entity)];
  int best = -1;
  for (size_t i = 0; i < s.monsters.size(); i++) {
    const Q2Monster& c = s.monsters[i];
    const Q2Entity& b = s.entities[size_t(c.entity)];
    if (!c.dead || c.claimed || b.gone || kind(c).end != End::corpse) continue;
    if (s.level_time + 0.001 < c.died_at + double(kind(c).revive_after)) continue;
    if (sh::length(b.origin - e.origin) > 1024 || !sees_body(s, medic, c)) continue;
    if (best < 0 || kind(c).health > kind(s.monsters[size_t(best)]).health)
      best = static_cast<int>(i);
  }
  return best;
}

// Corpse `c` comes back as its kind, with none of its map orders, full health, counted again.
void revive(Q2Server& s, Q2Monster& c, int enemy) {
  Q2Entity& b = body(s, c);
  const Kind& k = kind(c);
  Q2Monster fresh;
  fresh.entity = c.entity;
  fresh.kind = c.kind;
  fresh.ideal_yaw = b.angles.y;
  fresh.on_ground = c.on_ground;
  fresh.screen = k.screen ? 100 : 0;
  c = fresh;
  b.health = b.max_health = k.health;
  b.mins = k.mins;
  b.maxs = k.maxs;
  b.box = true;
  b.corpse = false;
  b.target.clear();
  b.targetname.clear();
  b.spawnflags = 0;
  s.monsters_total++;
  if (targetable(s, enemy)) found(s, c, enemy, false);
}

// A medic's frame with a corpse to raise: the cable within 256, else toward it. True when it was
// busy with one.
bool tend(Q2Server& s, Q2Monster& m) {
  if (m.patient < 0 && (s.frame_number % 10 == 0 || m.enemy == -1)) {
    m.patient = find_patient(s, m);
    if (m.patient >= 0) {
      s.monsters[size_t(m.patient)].claimed = true;
      m.old_enemy = m.enemy;
      m.revive_at = 0;
    }
  }
  if (m.patient < 0) return false;
  Q2Monster& c = s.monsters[size_t(m.patient)];
  Q2Entity& e = body(s, m);
  const Q2Entity& b = body(s, c);
  const auto release = [&] {
    c.claimed = false;
    m.patient = -1;
    m.revive_at = 0;
    m.enemy = foe_alive(s, m.old_enemy) ? m.old_enemy : -1;
  };
  if (!c.dead || b.gone) {
    release();
    return false;
  }
  const bool near = sh::length(b.origin - e.origin) <= 256 && sees_body(s, m, c);
  if (m.revive_at > 0) {  // the cable is out
    if (near) fight_monster_line(s, "te_medic_cable_attack", m.entity, eye(e, kind(m)), b.origin);
    if (s.level_time + 0.001 < m.revive_at) return true;
    if (near) {
      fight_effect(s, "te_medic_cable", b.origin);
      sound(s, m.entity, "medic/medatck4.wav");
      const int enemy = m.old_enemy;
      release();
      revive(s, c, enemy);
    } else {
      m.revive_at = 0;
    }
    return true;
  }
  if (s.level_time + 0.001 < m.busy_until) return true;
  turn(e, kind(m), yaw_to(b.origin - e.origin));
  if (near) {  // the cable: launched, hooked, and 1.7 s on the corpse rises
    sound(s, m.entity, "medic/medatck2.wav");
    m.revive_at = s.level_time + 1.7;
    m.busy_until = s.level_time + 2.8;
    return true;
  }
  go_to(s, m, b.origin, kind(m).run);
  return true;
}

// ---- Dodging (q2-monsters-full.md § Dodging and ducking) ----

using roster::Duck;
using roster::kDucks;
using roster::kDuckShoot;
using roster::kDuckShootLoop;
constexpr float kDuckDrop = 32;  // the box's top lowered while down

const Duck* duck_of(const Q2Monster& m) {
  for (const Duck& d : kDucks)
    if (std::string_view(d.classname) == kind(m).classname) return &d;
  return nullptr;
}
bool soldier(const Q2Monster& m) {
  return std::string_view(kind(m).classname).starts_with("monster_soldier");
}
bool named(const Q2Monster& m, std::string_view classname) {
  return kind(m).classname == classname;
}
bool stamps(const Q2Monster& m) {
  return named(m, "monster_tank") || named(m, "monster_tank_commander");
}

// One think of the duck in hand: the frame shown, what it does when new, then the next.
void ducking(Q2Server& s, Q2Monster& m) {
  Q2Entity& e = body(s, m);
  const Kind& k = kind(m);
  const Duck& d = m.duck_shoot ? kDuckShoot : *duck_of(m);
  const int f = m.duck_frame;
  m.duck_pose = d.first + f;
  if (m.duck_shoot && foe_alive(s, m.enemy)) turn(e, k, yaw_to(foe_at(s, m.enemy) - e.origin));
  if (m.duck_fresh) {
    const float walk = float(d.steps[f]) * d.scale;
    if (walk != 0) step(s, m, forward_of(e.angles.y) * walk);
    if (f == d.down) {
      const bool was = m.ducked;
      if (!was) {
        m.ducked = true;
        e.maxs.z -= kDuckDrop;
        if (!named(m, "monster_brain")) m.duck_hold = s.level_time + 1.0;  // the brain's is set
      }
      if (m.duck_shoot) shoot(s, m, attack_at(m, 2), 0);
      if (!was && named(m, "monster_gunner") && skill(s) >= 2 && random01(s) > 0.5) {
        const sh::Vec3f from = eye(e, k);  // a grenade straight ahead, unaimed
        fight_monster_launch(s, m.entity, SqShot::grenade, from, forward_of(e.angles.y), 50, 600,
                             cause_of(m));
      }
    }
    if (f == d.up && m.ducked) {
      m.ducked = false;
      e.maxs.z += kDuckDrop;
    }
  }
  m.duck_fresh = true;
  if (f == d.held && s.level_time + 0.001 < m.duck_hold) {  // held low: shown again, still
    m.duck_fresh = false;
    return;
  }
  int next = f + 1;
  if (m.duck_shoot && f == kDuckShootLoop && s.level_time + 0.4 + 0.001 < m.duck_hold)
    next = d.down;
  if (next >= d.frames) {  // done: the run takes over next think
    m.duck_frame = -1;
    m.duck_pose = -1;
    m.busy_until = s.level_time;
    return;
  }
  m.duck_frame = next;
}

// ---- The frame ----

// A monster's arrival at a `path_corner` or `point_combat` `e`: its `pathtarget` fired (no player
// for the activator). Returns the corner's `wait` (s; -1 for ever, 0 none).
float arrive(Q2Server& s, int e) {
  Q2Entity& c = s.entities[size_t(e)];
  if (c.block < 0 || size_t(c.block) >= s.map->entities.size()) return 0;
  const ge::brush::Entity& keys = s.map->entities[size_t(c.block)];
  const std::string path = keys.get_any_case("pathtarget");
  const std::string wait = keys.get_any_case("wait");
  if (!path.empty()) {
    const std::string saved = c.target;
    c.target = path;
    map_use_targets(s, e, -1);
    s.entities[size_t(e)].target = saved;
  }
  return wait.empty() ? 0.0f : std::strtof(wait.c_str(), nullptr);
}

// Monster `m` at its path_corner: the corner's pathtarget fired and its wait held (-1 for ever),
// then on to the next; a next corner with spawnflag 1 is reached at once, on to the one after.
void arrive_corner(Q2Server& s, Q2Monster& m) {
  const int here = m.corner;
  const float wait = arrive(s, here);
  if (m.dead || body(s, m).gone) return;
  if (wait < 0) {
    m.corner_wait = 1e30;
    return;
  }
  m.corner_wait = s.level_time + double(wait);
  m.corner = q2_find_named(s, s.entities[size_t(here)].target, "path_corner");
  if (m.corner >= 0 && (s.entities[size_t(m.corner)].spawnflags & 1) != 0) {
    const Q2Entity& next = s.entities[size_t(m.corner)];
    Q2Entity& e = body(s, m);
    e.origin = next.origin + sh::Vec3f{0, 0, -8 - e.mins.z};  // its feet at the corner's foot
    m.corner = q2_find_named(s, next.target, "path_corner");
  }
}

// A prisoner's pain: the cry of the band under his health (25, 50, 75, else 100), one of two.
const char* insane_pain(Q2Server& s, int health) {
  const int band = health < 25 ? 0 : health < 50 ? 1 : health < 75 ? 2 : 3;
  return kInsanePains[band * 2 + (random01(s) < 0.5 ? 0 : 1)];
}

// A soldier's shots as it falls: its first death's frames 293 and 296 fire one straight along its
// facing, with no aim and no spread of its own.
void dying_shot(Q2Server& s, Q2Monster& m) {
  const Kind& k = kind(m);
  if (std::string_view(k.classname).substr(0, 15) != "monster_soldier") return;
  if (m.pose.act != SqMonsterPose::Act::death || m.pose.span.first != 272) return;
  if (m.pose.frame != 293 && m.pose.frame != 296) return;
  const Q2Entity& e = body(s, m);
  const Attack& a = k.missile[0];
  const sh::Vec3f from = eye(e, k);
  const sh::Vec3f fwd = forward_of(e.angles.y);
  const sh::Vec3f to = from + fwd * 8192;
  const char* look = a.fire == Fire::bolt ? "blaster" : "gun";
  fight_monster_line(s, a.fire == Fire::bolt ? "te_monster_flash.blaster" : "te_monster_flash.gun",
                     m.entity, from, from + fwd * 16.0f, q2_monster_flash_value(k.classname, look));
  if (a.fire == Fire::bolt) {
    fight_monster_launch(s, m.entity, SqShot::bolt, from, fwd, a.damage, a.speed, cause_of(m));
    return;
  }
  for (int i = 0; i < (a.fire == Fire::pellets ? a.count : 1); i++) {
    const sh::Vec3f d = aim_at(s, a, from, to);  // the pellets' and bullets' own spread
    fight_monster_bullet(s, m.entity, from, from + d * 8192, a.damage, a.kick, cause_of(m));
  }
}

// A soldier's shot on the run at first sight, frames 109 to 122 a step each: it runs at him
// facing him, fires at the fourth, and at the last, at skill 3 while he lives 500 or more off,
// goes round again from the third; else it runs on as ever.
void run_and_shoot(Q2Server& s, Q2Monster& m) {
  const int at = m.run_shoot;
  if (!foe_alive(s, m.enemy)) {
    m.run_shoot = -1;
    return;
  }
  m.pose.shown = 109 + at;
  go_to(s, m, foe_at(s, m.enemy), kind(m).run);
  if (at == 3) shoot(s, m, kind(m).missile[0], 0);
  if (at < 13) {
    m.run_shoot = at + 1;
    return;
  }
  const bool again = skill(s) == 3 && foe_alive(s, m.enemy) &&
                     sh::length(foe_at(s, m.enemy) - body(s, m).origin) >= kNear;
  m.run_shoot = again ? 2 : -1;
}

// Whether kind `k` fidgets on its idle call (its poses' fidget started by no frame).
bool fidgets_on_call(const Kind& k) {
  const SqMonsterPoses* t = sq_monster_poses(k.classname);
  return t != nullptr && !t->fidget.none() && t->fidget_odds == 0;
}

// Flies on an infantryman's or a mutant's corpse: once it has fallen, out of water, half the time
// they come 5 to 15 s on (none if it lies in water then) and buzz round it for 60 s.
void flies(Q2Server& s, Q2Monster& m) {
  if (m.flies_at > 0 && s.level_time < m.flies_at + 60 && wet(s, body(s, m).origin))
    m.flies_at = -1;  // in water when they would come, or while they buzz: none
  if (m.flies_at != 0 || !(named(m, "monster_infantry") || named(m, "monster_mutant"))) return;
  if (m.pose.act != SqMonsterPose::Act::death || m.pose.frame < m.pose.span.last) return;
  m.flies_at = -1;
  if (wet(s, body(s, m).origin) || random01(s) >= 0.5) return;
  m.flies_at = s.level_time + 5 + random01(s) * 10;
}

// Monster `m`'s voice with no enemy: its idle standing (not an ambusher) or its search walking
// its path, on one timer: the first only sets it (0 to 15 s), each after plays and sets 15 to 30.
void voice(Q2Server& s, Q2Monster& m) {
  const Kind& k = kind(m);
  const bool walking = m.corner >= 0 && s.level_time + 0.001 >= m.corner_wait;
  const char* say = nullptr;
  float reach = 2;
  if (walking && k.search[0] != nullptr) {
    int n = 0;
    while (n < 3 && k.search[n] != nullptr) n++;
    say = k.search[n == 1 ? 0 : std::min(n - 1, static_cast<int>(random01(s) * n))];
    reach = k.search_reach;
  } else if (!walking && !m.ambush && (k.idle != nullptr || fidgets_on_call(k))) {
    say = k.idle != nullptr ? k.idle : "";  // the parasite's call is its fidget alone
  }
  if (say == nullptr) return;
  if (m.idle_at == 0) {
    m.idle_at = s.level_time + random01(s) * 15;
    return;
  }
  if (s.level_time < m.idle_at) return;
  if (walking && k.search_odds < 1 && random01(s) >= k.search_odds) say = nullptr;
  if (say != nullptr && *say != 0) sound(s, m.entity, say, reach);
  if (say != nullptr && !walking) m.pose.cue_fidget = true;
  m.idle_at = s.level_time + 15 + random01(s) * 15;
}

// Whether monster `m` sees point `p` (its eye to it, nothing opaque between).
bool sees_point(const Q2Server& s, const Q2Monster& m, sh::Vec3f p) {
  const Q2Entity& e = s.entities[size_t(m.entity)];
  return map_trace(s, eye(e, kind(m)), {}, {}, p, kOpaque, m.entity).fraction >= 1;
}

// The trail mark after monster `m`'s last look: the first newer than its trail time from the
// oldest on; from the last sighting (`first`), the one before it when only that one is in sight.
// -1 when there is none.
int next_mark(const Q2Server& s, const Q2Monster& m, bool first) {
  for (int n = 0; n < 8; n++) {
    const int i = (s.trail_head + n) % 8;
    if (s.trail_time[i] <= m.trail_time) continue;
    if (!first || sees_point(s, m, s.trail_at[i])) return i;
    const int before = (i + 7) % 8;
    if (n > 0 && s.trail_time[before] > 0 && sees_point(s, m, s.trail_at[before])) return before;
    return i;
  }
  return -1;
}

// A new goal blocked short of it (its box swept there): a side step first. Two points half way
// on to the block's far side and 16 to either side; the left when it gets at least as far as the
// way ahead and farther than the right, else the right when it does; held to half of what it
// reaches. The goal is kept for after (`real_goal`), and it faces the side point.
void correct(Q2Server& s, Q2Monster& m) {
  Q2Entity& e = body(s, m);
  sh::Vec3f to = m.goal;
  to.z = e.origin.z;
  const float f = sweep(s, m, e.origin, to).fraction;
  if (f >= 1) return;
  const float d1 = sh::length(to - e.origin);
  if (d1 < 1) return;
  const sh::ViewAxes ax = sh::angle_vectors({0, yaw_to(to - e.origin), 0});
  const float d2 = d1 * (f + 1) / 2;
  const float centre = d1 * f / d2;
  const sh::Vec3f ahead = e.origin + ax.forward * d2;
  const sh::Vec3f left = ahead - ax.right * 16.0f, right = ahead + ax.right * 16.0f;
  const float lf = sweep(s, m, e.origin, left).fraction;
  const float rf = sweep(s, m, e.origin, right).fraction;
  sh::Vec3f side;
  float got;
  if (lf >= centre && lf > rf) {
    side = left;
    got = lf;
  } else if (rf >= centre) {
    side = right;
    got = rf;
  } else {
    return;
  }
  if (got < 1) side = e.origin + (side - e.origin) * (got * 0.5f);
  m.real_goal = m.goal;
  m.side_step = true;
  m.goal = side;
  e.angles.y = m.ideal_yaw = yaw_to(side - e.origin);
}

// Monster `m` with its enemy out of sight: first to where it last saw him; there (and at each mark
// after) the search restarts (5 s) and it takes the next mark of his trail, facing the way he went.
// 20 s past the search's start it makes one move straight at him.
void pursue(Q2Server& s, Q2Monster& m) {
  Q2Entity& e = body(s, m);
  if (!m.lost) {
    m.lost = true;
    m.on_trail = m.side_step = false;
    m.goal = m.last_sight;
    correct(s, m);
    return;
  }
  if (m.search_time > 0 && s.level_time > m.search_time + 20) {
    m.goal = foe_at(s, m.enemy);
    m.side_step = false;
    m.search_time = 0;
    return;
  }
  sh::Vec3f d = m.goal - e.origin;
  d.z = 0;
  if (sh::length(d) > kind(m).run * kStep + 1) return;  // not there yet
  m.search_time = s.level_time + 5;
  if (m.side_step) {  // round the obstacle: on to the point it was making for
    m.side_step = false;
    m.goal = m.real_goal;
    correct(s, m);
    return;
  }
  const int i = next_mark(s, m, !m.on_trail);
  if (i < 0) return;
  m.on_trail = true;
  m.goal = s.trail_at[i];
  m.trail_time = s.trail_time[i];
  e.angles.y = m.ideal_yaw = s.trail_yaw[i];
  correct(s, m);
}

// The players' trail, at the monsters' frame: a mark where a player stood at the last one, once
// his eye no longer sees the newest mark (single player and coop).
void trail_frame(Q2Server& s) {
  if (deathmatch(s)) return;
  for (int slot = 0; slot < sh::kMaxClients; slot++) {
    Q2Player& p = s.players[slot];
    if (!hunted(s, slot)) continue;
    const sh::Vec3f at = unpack(p.move.origin);
    if (p.trail_last.x == 0 && p.trail_last.y == 0 && p.trail_last.z == 0) {  // new: no mark yet
      p.trail_last = at;
      continue;
    }
    const int newest = (s.trail_head + 7) % 8;
    if (map_trace(s, player_eye(p), {}, {}, s.trail_at[newest], kOpaque, -1).fraction < 1) {
      const int i = s.trail_head;
      s.trail_at[i] = p.trail_last;
      s.trail_time[i] = s.level_time;
      s.trail_yaw[i] = yaw_to(p.trail_last - s.trail_at[newest]);
      s.trail_head = (i + 1) % 8;
    }
    p.trail_last = at;
  }
}

void think(Q2Server& s, Q2Monster& m) {
  Q2Entity& e = body(s, m);
  const Kind& k = kind(m);
  if (!k.flies && !k.swims) targets_monster_touch(s, m);
  map_monster_touch(s, m.entity);
  fall(s, m);
  if (m.leaping) return;    // in the air
  if (m.duck_frame >= 0) {  // a dodge's duck: nothing else
    ducking(s, m);
    return;
  }
  // A tank's enemy fallen but not torn apart (health above -80) and in sight: it stamps on him
  // (frames 77 to 114, no harm done), then forgets him.
  if (m.stomp < 0 && m.enemy >= 0 && !foe_alive(s, m.enemy) && stamps(m) &&
      s.players[m.enemy].health > -80 && reaches(s, m, m.enemy, kOpaque))
    m.stomp = 0;
  if (m.stomp >= 0) {
    m.pose.shown = 77 + m.stomp;
    m.pose.shown_run = {77, 114};
    if (m.enemy >= 0) turn(e, k, yaw_to(foe_at(s, m.enemy) - e.origin));
    if (++m.stomp > 37) {
      m.stomp = -1;
      m.enemy = m.old_enemy = -1;
    }
    return;
  }
  if (m.enemy != -1 && !foe_alive(s, m.enemy)) {  // dead or gone: back to the one before
    m.enemy = m.old_enemy != -1 && foe_alive(s, m.old_enemy) ? m.old_enemy : -1;
    m.old_enemy = -1;
    if (m.enemy != -1) found(s, m, m.enemy, false);
  }
  if (k.turret) {  // sits its turret: aims it at whom it sees, fires once it has seen long enough
    if (m.enemy == -1) notice(s, m);
    targets_turret(s, m, m.enemy >= 0 && sees(s, m, m.enemy) ? m.enemy : -1);
    return;
  }
  if (k.medic && tend(s, m)) return;
  if (m.enemy == -1) notice(s, m);
  if (m.enemy == -1 && m.sound_target) {  // to the noise; within 64 it stands, 5 s on forgets it
    sh::Vec3f d = m.sound_at - e.origin;
    d.z = 0;
    if (sh::length(d) <= 64) {
      turn(e, k, yaw_to(d));
      if (s.level_time > m.sound_time + 5) m.sound_target = false;
      return;
    }
    go_to(s, m, m.sound_at, k.run);
    return;
  }
  if (m.enemy == -1) voice(s, m);
  if (m.enemy == -1) {  // walks its path, or stands
    if (m.corner >= 0) {
      const Q2Entity& c = s.entities[size_t(m.corner)];
      sh::Vec3f d = c.origin - e.origin;
      d.z = 0;
      if (s.level_time + 0.001 < m.corner_wait) return;  // held at its corner
      if (sh::length(d) < 16) {
        arrive_corner(s, m);
        return;
      }
      go_to(s, m, c.origin, k.walk);
    }
    return;
  }
  if (sees(s, m, m.enemy) || reaches(s, m, m.enemy, kOpaque)) {
    m.seen_at = s.level_time;
    m.last_sight = foe_at(s, m.enemy);
    m.trail_time = s.level_time;
    m.lost = m.on_trail = false;
  } else {
    pursue(s, m);  // out of sight: the last sighting, then his trail; it never gives him up
  }
  if (s.level_time + 0.001 < m.pain_hold) return;  // flinching
  if (m.run_shoot >= 0) {
    run_and_shoot(s, m);
    return;
  }
  attacking(s, m);
  if (s.level_time + 0.001 < m.busy_until) return;
  if (k.screen) m.screen = std::max(m.screen, 0);
  if (m.combat < 0) choose(s, m);  // none on the way to its combat point
  if (s.level_time + 0.001 < m.busy_until) return;
  // The run: to its combat point first, then at the enemy (or where it was last seen).
  if (m.combat >= 0) {
    const sh::Vec3f spot = s.entities[size_t(m.combat)].origin;
    sh::Vec3f d = spot - e.origin;
    d.z = 0;
    if (sh::length(d) < 16) {  // its pathtarget fired; on to the next, or held there
      const int at = m.combat;
      arrive(s, at);
      q2_combat_point_missing(s, at);
      const Q2Entity& point = s.entities[size_t(at)];
      m.combat = q2_find_named(s, point.target, "point_combat");
      if (m.combat < 0) m.hold = (point.spawnflags & 1) != 0 && !afloat(k);
      return;
    }
    go_to(s, m, spot, k.run);
    return;
  }
  if (m.hold) {
    turn(e, k, yaw_to(m.last_sight - e.origin));
    return;
  }
  if (m.lost) {
    go_to(s, m, m.goal, k.run);
    return;
  }
  if (m.slide && k.flies) {  // facing him, a run's step to one side; blocked, the other side
    const float to = yaw_to(m.last_sight - e.origin);
    turn(e, k, to);
    m.ideal_yaw = to;
    const float step_len = k.run * kStep;
    for (int tries = 0; tries < 2; tries++) {
      const sh::Vec3f side = forward_of(to + (m.slide_left ? 90.0f : -90.0f)) * step_len;
      if (step(s, m, side)) return;
      m.slide_left = !m.slide_left;
    }
    return;
  }
  sh::Vec3f goal = m.last_sight;
  if (afloat(k)) goal.z += 35;  // a flier keeps 30 to 40 above him
  if (range_to(e.origin, m.last_sight) == Range::melee && k.melee[0].fire == Fire::none) {
    turn(e, k, yaw_to(goal - e.origin));
    return;
  }
  sh::Vec3f d = goal - e.origin;
  d.z = 0;
  if (sh::length(d) < 40) {  // close enough
    turn(e, k, yaw_to(d));
    if (afloat(k)) go_to(s, m, goal, k.run);
    return;
  }
  go_to(s, m, goal, k.run);
}

// A boss's wreck thrown apart, or a flier's crash: the blast, and nothing left.
void blow_up(Q2Server& s, Q2Monster& m) {
  Q2Entity& e = body(s, m);
  fight_effect(s, "te_explosion", e.origin);
  e.gone = true;
}

// The spare Makron that waits inside boss `m` (Jorg) comes out and leaps at the frame's player.
void bring_out(Q2Server& s, const Q2Monster& m) {
  for (Q2Monster& o : s.monsters) {
    if (!o.spare || !o.waiting) continue;
    Q2Entity& e = body(s, o);
    if (e.block != body(s, m).block) continue;
    o.spare = false;
    o.waiting = false;
    e.hidden = false;
    e.box = true;
    e.origin = body(s, m).origin;
    s.monsters_total++;
    const int slot = s.sight_client;
    if (!hunted(s, slot)) return;
    sh::Vec3f d = unpack(s.players[slot].move.origin) - e.origin;
    d.z = 0;
    e.angles.y = o.ideal_yaw = yaw_to(d);
    sh::normalize(d);
    e.origin.z += 1;
    o.velocity = d * 400 + sh::Vec3f{0, 0, 200};
    o.on_ground = false;
    found(s, o, slot, false);
    return;
  }
}

// A dead monster's frame: its fall, and a boss's or a crashed flier's end.
void lie(Q2Server& s, Q2Monster& m) {
  fall(s, m);
  Q2Entity& e = body(s, m);
  if (e.gone) return;
  const End end = kind(m).end;
  if (end == End::crash) {
    if (m.on_ground || s.level_time + 0.001 >= m.end_at) blow_up(s, m);
    return;
  }
  if (end != End::boss && end != End::launch) return;
  if (s.level_time + 0.001 < m.end_at) return;
  m.boom_step++;  // eight blasts round the wreck, 0.1 s apart, then its pieces
  if (m.boom_step < 8) {
    const float k = m.boom_step < 4 ? 24.0f : 48.0f;
    const float sx = (m.boom_step & 1) != 0 ? k : -k, sy = (m.boom_step & 2) != 0 ? k : -k;
    fight_effect(s, "te_explosion", e.origin + sh::Vec3f{sx, sy, 24 + float(m.boom_step * 2)});
    if (end == End::launch && m.boom_step == 7) bring_out(s, m);
    return;
  }
  e.gone = true;
  gib_monster(s, m, 500);
}

void die(Q2Server& s, Q2Monster& m, int attacker, int damage) {
  Q2Entity& e = body(s, m);
  const Kind& k = kind(m);
  m.dead = true;
  m.died_at = s.level_time;
  m.duck_frame = m.duck_pose = -1;  // a duck ends at death (the corpse's box is set below)
  m.ducked = m.leaping = false;
  m.screen = 0;
  e.corpse = true;  // shots meet a corpse, moves pass it
  if (!k.good) s.monsters_killed++;
  // Co-op scores each kill to the player who made it.
  if (!k.good && attacker >= 0 && attacker < sh::kMaxClients && s.cvars.find("coop")->on() &&
      !s.cvars.find("deathmatch")->on())
    s.players[attacker].frags += 1;
  const DeathChoice dc = monster_death(k.classname, damage, s.rng);
  m.death_run = dc.run;
  if (named(m, "misc_insane"))  // a prisoner dies with a man's cry, one of four, close by
    sound(s, m.entity, kInsaneDeaths[std::min(3, static_cast<int>(random01(s) * 4))], 2);
  else
    sound(s, m.entity, dc.sound != nullptr ? dc.sound : k.death, dc.everywhere ? 0 : 1);
  if (m.patient >= 0) {  // a medic's patient is free for another
    s.monsters[size_t(m.patient)].claimed = false;
    m.patient = -1;
  }
  if (!m.item.empty()) fight_drop_at(s, e.origin, m.item);
  // Its `deathtarget` in place of its `target`, when the map gives one.
  if (e.block >= 0 && size_t(e.block) < s.map->entities.size()) {
    const std::string& deathtarget = s.map->entities[size_t(e.block)].get_any_case("deathtarget");
    if (!deathtarget.empty()) e.target = deathtarget;
  }
  map_use_targets(s, m.entity, m.enemy >= 0 ? m.enemy : attacker >= 0 ? attacker : -1);
  switch (k.end) {
    case End::explode:
      if (k.burst > 0) fight_blast(s, e.origin, -1, k.burst, float(k.burst) + 40, cause_of(m));
      blow_up(s, m);
      return;
    case End::boss:
    case End::launch:
      m.end_at = s.level_time + double(k.death_time);
      return;
    case End::crash:
      m.end_at = s.level_time + double(k.death_time) + 15;
      break;
    case End::corpse:
      break;
  }
  if (e.health <= k.gib) {
    e.gone = true;
    gib_monster(s, m, damage);
  }
  // The corpse's box: the kind's own where it has one, else its box with the top at -8.
  const std::string_view name = k.classname;
  if (name == "monster_tank" || name == "monster_tank_commander") {
    e.mins = {-16, -16, -16};
    e.maxs = {16, 16, 0};
  } else if (name == "monster_chick") {
    e.mins = {-16, -16, 0};
    e.maxs = {16, 16, 16};
  } else if (name == "monster_makron") {
    e.mins = {-60, -60, 0};
    e.maxs = {60, 60, 72};
  } else {
    e.maxs.z = std::min(e.maxs.z, -8.0f);
  }
}

}  // namespace

const char* monster_obituary(int cause) {
  const int k = cause - kSqByMonster;
  if (k < 0 || k >= kKindCount) return nullptr;
  return kKinds[k].name;
}

int monster_index(const Q2Server& s, int e) {
  for (size_t i = 0; i < s.monsters.size(); i++)
    if (s.monsters[i].entity == e) return static_cast<int>(i);
  return -1;
}

namespace {
bool named_jail5(std::string_view map) {
  constexpr std::string_view kJail5 = "jail5";
  if (map.size() != kJail5.size()) return false;
  for (size_t i = 0; i < map.size(); i++)
    if ((map[i] >= 'A' && map[i] <= 'Z' ? char(map[i] + 32) : map[i]) != kJail5[i]) return false;
  return true;
}
}  // namespace

void monster_spawn(Q2Server& s) {
  s.monsters.clear();
  for (int i = 0; i < 8; i++) {  // a new map: no trail yet
    s.trail_at[i] = {};
    s.trail_time[i] = 0;
    s.trail_yaw[i] = 0;
  }
  s.trail_head = 0;
  for (Q2Player& p : s.players) p.trail_last = {};
  s.monsters_total = s.monsters_killed = 0;
  s.sight_entity = -1;
  // A boss that brings out another at its death (Jorg) has it waiting inside, unseen: a map
  // entity of its own, so it has its world row from the start.
  const size_t mapped = s.entities.size();
  for (size_t i = 0; i < mapped; i++) {
    const int k = kind_of(s.entities[i].classname);
    if (k < 0 || s.entities[i].gone || kKinds[k].end != End::launch || deathmatch(s)) continue;
    Q2Entity spare = s.entities[i];
    spare.classname = "monster_makron";
    spare.targetname.clear();
    spare.spawnflags = 2;
    s.entities.push_back(spare);
  }
  monster_torsos(s);
  for (size_t i = 0; i < s.entities.size(); i++) {
    Q2Entity& e = s.entities[i];
    const int k = kind_of(e.classname);
    if (k < 0 || e.gone) continue;
    const Kind& kd = kKinds[k];
    if (deathmatch(s) && !kd.keep) {
      e.gone = true;
      continue;
    }
    Q2Monster m;
    m.entity = static_cast<int>(i);
    m.kind = k;
    m.spare = i >= mapped || e.block < -1;  // a boss's or a spawner's, waiting unseen
    e.health = kd.health;
    e.max_health = kd.health;
    e.mins = kd.mins;
    e.maxs = kd.maxs;
    e.box = true;
    if (m.spare) {
      const auto found = std::find(s.models.begin(), s.models.end(), kd.model);
      e.modelindex = static_cast<int>(found - s.models.begin());
    }
    // A monster waiting for a use that nothing can give (no name to be targeted by) is said, and
    // with g_fix_triggered 1 stands from the start like any other.
    if (!m.spare && !kd.prop && (e.spawnflags & 2) != 0 && e.targetname.empty()) {
      s.console.push_back("triggered " + e.classname + " at " + q2_pos(e.origin) +
                          " has no targetname");
      if (s.cvars.find("g_fix_triggered")->integer() != 0) e.spawnflags &= ~2;
    }
    // jail5's flyers placed at height -104 are waited on by their target's name: the map gives
    // them a target where it meant a name.
    if (kd.classname == std::string_view("monster_flyer") && e.origin.z == -104 &&
        named_jail5(s.map_name)) {
      e.targetname = e.target;
      e.target.clear();
    }
    m.screen = kd.screen ? 100 : 0;
    m.ambush = (e.spawnflags & 5) != 0;
    if (!m.spare) m.item = s.map->entities[size_t(e.block)].get_any_case("item");
    if (((e.spawnflags & 2) != 0 || m.spare) && !kd.prop) {  // waits hidden for a use
      e.hidden = true;
      e.box = false;
      m.waiting = true;
    }
    m.ideal_yaw = e.angles.y;
    m.corner = q2_find_named(s, e.target, "path_corner");
    const std::string combattarget =
        m.spare ? std::string() : s.map->entities[size_t(e.block)].get_any_case("combattarget");
    m.combat = q2_find_named(s, combattarget.empty() ? e.target : combattarget, "point_combat");
    if (m.combat < 0) m.lost_combat = combattarget;
    if (m.corner >= 0)
      e.angles.y = m.ideal_yaw = yaw_to(s.entities[size_t(m.corner)].origin - e.origin);
    if (kd.turret) m.gravity = 0;              // it rides its turret
    if ((e.spawnflags & 8) != 0 && kd.good) {  // crucified: hung in place, never falls
      e.mins = {-16, 0, 0};
      e.maxs = {16, 8, 32};
      m.gravity = 0;
    }
    s.monsters.push_back(m);
    if (!kd.prop && !kd.good && !m.spare) s.monsters_total++;
  }
  for (Q2Monster& m : s.monsters) {  // walkers to the floor
    if (afloat(kKinds[m.kind]) || m.waiting || m.gravity == 0) continue;
    Q2Entity& e = s.entities[size_t(m.entity)];
    const ge::classic::Q2Trace t =
        sweep(s, m, e.origin + sh::Vec3f{0, 0, 1}, e.origin - sh::Vec3f{0, 0, 256});
    m.start_solid = t.start_solid;
    if (t.start_solid) continue;
    e.origin = t.end;
    m.on_ground = t.fraction < 1;
  }
  for (Q2Monster& m : s.monsters) {  // standing, where they were placed
    m.pose.last = s.entities[size_t(m.entity)].origin;
    const SqMonsterPoses* t = sq_monster_poses(kKinds[m.kind].classname);
    if (t != nullptr && !t->stand.none()) {  // its stand (or either), on a frame at random
      const SqSpan stand = !t->stand2.none() && random01(s) > 0.5 ? t->stand2 : t->stand;
      m.pose.span = stand;
      const int frames = stand.last - stand.first + 1;
      m.pose.frame = stand.first + std::min(frames - 1, static_cast<int>(random01(s) * frames));
    }
  }
}

namespace {

// The contents at `p` (none without a map).
int contents_at(const Q2Server& s, sh::Vec3f p) {
  if (!s.tracer) return 0;
  const float q[3] = {p.x, p.y, p.z};
  return s.tracer->contents(q);
}

// Monster `m` in liquid this think: its level (1 above its feet, then 26 and 22 higher), the
// sounds going in and out, lava's 10 a level every 0.2 s and slime's 4 a level every second, and
// drowning: 12 s of air under, then 2 more a second from 2 up to 15, at most once a second (by the
// pain's wait, which it shares). A swimmer drowns out of the water instead, after 9 s. Hurt by the
// world, with no push.
void liquids(Q2Server& s, Q2Monster& m) {
  const Q2Entity& e = body(s, m);
  sh::Vec3f p = foot(e, e.origin);
  int level = 0;
  const int type = contents_at(s, p) & sh::kSqMaskWater;
  if (type != 0) {
    level = 1;
    p.z += 26;
    if ((contents_at(s, p) & sh::kSqMaskWater) != 0) {
      level = 2;
      p.z += 22;
      if ((contents_at(s, p) & sh::kSqMaskWater) != 0) level = 3;
    }
  }
  m.water_level = level;
  m.water_type = type;
  const Kind& k = kind(m);
  const double now = s.level_time;
  if (k.swims ? level > 0 : level < 3) m.air_until = now + (k.swims ? 9 : 12);
  if (now + 0.001 >= m.air_until && now + 0.001 >= m.pain_at + kPainEvery) {
    const int dmg = std::min(15, 2 + 2 * static_cast<int>(now - m.air_until));
    m.pain_at = now + 1 - kPainEvery;  // the next in a second, and no flinch for this one
    monster_damage(s, m.entity, -1, dmg, {}, true);
    if (m.dead || body(s, m).gone) return;
  }
  if (level == 0) {
    if (m.in_water) sound(s, m.entity, "player/watr_out.wav");
    m.in_water = false;
    return;
  }
  if (!m.in_water) {  // going in: the first burn on this think
    const bool lava = (type & sh::kSqLava) != 0;
    sound(s, m.entity,
          lava ? (random01(s) < 0.5 ? "player/lava1.wav" : "player/lava2.wav")
               : "player/watr_in.wav");
    m.in_water = true;
    m.liquid_hurt_at = 0;
  }
  if ((type & sh::kSqLava) != 0 && now + 0.001 >= m.liquid_hurt_at) {
    m.liquid_hurt_at = now + 0.2;
    monster_damage(s, m.entity, -1, 10 * level, {}, true);
  } else if ((type & sh::kSqSlime) != 0 && now + 0.001 >= m.liquid_hurt_at) {
    m.liquid_hurt_at = now + 1;
    monster_damage(s, m.entity, -1, 4 * level, {}, true);
  }
}

}  // namespace

void monster_frame(Q2Server& s) {
  if (s.monsters.empty()) return;
  trail_frame(s);
  // The frame's player in turn, for the monsters to look at.
  for (int k = 1; k <= sh::kMaxClients; k++) {
    const int slot = (s.sight_client + k) % sh::kMaxClients;
    if (targetable(s, slot)) {
      s.sight_client = slot;
      break;
    }
  }
  for (size_t i = 0; i < s.monsters.size(); i++) {
    Q2Monster& m = s.monsters[i];
    if (kind(m).prop) {  // the commander's body: falls, and after a use slumps, frame by frame
      if (kind(m).keep && m.used_at >= 0 && s.level_time + 0.001 >= m.used_at + 2.2 &&
          s.level_time < m.used_at + 2.25)
        sound(s, m.entity, "tank/thud.wav");
      if (kind(m).keep && s.level_time >= 0.5) fall(s, m);
      continue;
    }
    if (m.dead) {
      lie(s, m);
      const int was = m.pose.frame;
      pose_monster(s, m);
      if (m.pose.frame != was) dying_shot(s, m);
      monster_footstep(s, m);
      flies(s, m);
      continue;
    }
    if (m.waiting || body(s, m).gone) continue;
    think(s, m);
    if (!m.dead && !body(s, m).gone) liquids(s, m);
    pose_monster(s, m);
    monster_footstep(s, m);
  }
}

int monster_duck_frame(std::string_view classname) {
  for (const Duck& d : kDucks)
    if (classname == d.classname) return d.first + d.held;
  return -1;
}

void monster_dodge(Q2Server& s, int e, int shooter, float dist, float speed) {
  const int i = monster_index(s, e);
  if (i < 0 || !hunted(s, shooter)) return;
  Q2Monster& m = s.monsters[size_t(i)];
  const Q2Entity& ent = s.entities[size_t(e)];
  if (m.dead || m.waiting || ent.gone || ent.health <= 0 || duck_of(m) == nullptr) return;
  // Only from in front: the shooter within the cone its facing makes (0.3 the cosine).
  sh::Vec3f to = unpack(s.players[shooter].move.origin) - ent.origin;
  if (sh::normalize(to) <= 0 || sh::dot(to, forward_of(ent.angles.y)) <= 0.3f) return;
  const double eta = double(dist - ent.maxs.x) / double(speed);
  if (random01(s) > 0.25) return;                  // one shot in four
  if (m.enemy == -1) found(s, m, shooter, false);  // silently: no sight sound
  bool shoot = false;
  if (soldier(m) && skill(s) >= 1) {  // ducks, or ducks and shoots
    m.duck_hold = s.level_time + eta + 0.3;
    shoot = random01(s) <= (skill(s) == 1 ? 0.33 : 0.66);
  }
  if (named(m, "monster_brain")) m.duck_hold = s.level_time + eta + 0.5;
  if (m.duck_frame >= 0 && m.duck_shoot == shoot) return;  // already in it: goes on
  m.duck_frame = 0;
  m.duck_fresh = true;
  m.duck_shoot = shoot;
  m.burst_until = -1;  // an attack or a flinch cut short
  m.pain_hold = 0;
}

// Monster `m` hurt by `attacker` (a slot, or a monster as -2 - its entity) turns on it, as the
// game's monsters do: a player at once (the one it fought kept, if a player, to go back to); a
// monster that moves as it does (walks, flies or swims alike) and is another kind, unless one of
// the heavy kinds whose splash hits its own side; one that was aiming at it; else it joins that
// monster's fight against its foe.
void react(Q2Server& s, Q2Monster& m, int attacker) {
  if (hunted(s, attacker)) {
    if (m.patient >= 0 && m.enemy != -1) return;  // a medic at a corpse keeps to it
    if (attacker == m.enemy) return;
    if (m.enemy >= 0 && m.patient < 0) m.old_enemy = m.enemy;
    found(s, m, attacker, false);
    return;
  }
  const Q2Monster* a = foe_monster(s, attacker);
  if (a == nullptr || a == &m || a->dead || attacker == m.enemy || kind(m).good) return;
  const Kind& ak = kind(*a);
  const Kind& k = kind(m);
  const bool alike = ak.flies == k.flies && ak.swims == k.swims;
  const bool heavy = named(*a, "monster_tank") || named(*a, "monster_tank_commander") ||
                     named(*a, "monster_supertank") || named(*a, "monster_makron") ||
                     named(*a, "monster_jorg");
  int foe = -1;
  if ((alike && std::string_view(ak.classname) != k.classname && !heavy) ||
      a->enemy == foe_of_entity(m.entity))
    foe = attacker;
  else if (a->enemy != -1 && foe_alive(s, a->enemy))
    foe = a->enemy;
  if (foe == -1 || foe == m.enemy) return;
  if (m.enemy >= 0) m.old_enemy = m.enemy;
  found(s, m, foe, false);
}

bool monster_damage(Q2Server& s, int e, int attacker, int damage, sh::Vec3f dir, bool splash,
                    const sh::Vec3f* point) {
  const int i = monster_index(s, e);
  if (i < 0) return false;
  Q2Monster& m = s.monsters[size_t(i)];
  Q2Entity& ent = s.entities[size_t(e)];
  const Kind& k = kind(m);
  if (k.prop) {  // a figure: never hurt, but for the commander's body with its god mode off
    if (!named(m, "monster_commander_body") || ent.gone || damage <= 0 ||
        s.cvars.find("g_commanderbody_nogod")->integer() == 0)
      return true;
    sound(s, e, "tank/pain.wav");
    ent.gone = true;
    gib_monster(s, m, damage);  // the tank's pieces, the head a gear
    return true;
  }
  if (ent.gone || m.waiting || damage <= 0) return false;
  if (m.dead && k.end != End::corpse && k.end != End::crash) return true;  // a dying boss
  // A player's blow on a monster that has not yet noticed anyone: twice its points (not a blast's).
  if (!splash && !m.dead && m.enemy == -1 && hunted(s, attacker)) damage *= 2;
  // A power screen takes a third of a blow from in front (but not while its chest is open).
  const bool opened = m.attack == 1 && s.level_time + 0.001 < m.busy_until;
  if (m.screen > 0 && !opened && sh::normalize(dir) > 0 &&
      sh::dot(dir * -1.0f, forward_of(ent.angles.y)) > kInFront) {
    const int saved = std::min(damage / 3, m.screen);
    m.screen -= saved;
    damage -= saved;
    if (saved > 0)
      m.screen_flash = fight_power_hit(s, ent.origin + dir * -16.0f, dir * -1.0f, false);
    if (damage <= 0) return true;
  }
  fight_dealt(s, attacker, e, damage);
  ent.health -= damage;
  // The knockback: 500 × damage / mass along the blow (a flier's too; not a swimmer's).
  if (sh::normalize(dir) > 0 && !k.swims) {
    m.velocity = m.velocity + dir * (500.0f * float(damage) / float(std::max(50, k.mass)));
    m.on_ground = false;
  }
  if (m.dead) {  // a corpse thrown apart
    if (ent.health <= k.gib) {
      ent.gone = true;
      gib_monster(s, m, damage);
    }
    return true;
  }
  if (ent.health <= 0) {
    // A shot that lands within 4 of a soldier's eye: its head-shot death.
    m.head_shot = point != nullptr && soldier(m) && std::abs(point->z - (ent.origin.z + 25)) <= 4;
    die(s, m, attacker, damage);
    return true;
  }
  react(s, m, attacker);
  if (m.leaping || m.ducked) return true;  // ducked (in the air from a leap too): no pain
  const bool waiting = s.level_time + 0.001 < m.pain_at + kPainEvery;
  if (monster_pain_known(k.classname)) {  // its pains as data (q2_monster_pain.cpp)
    const PainChoice c =
        monster_pain(k.classname, damage, skill(s), m.velocity.z, m.pose.frame, m.pose.span.first,
                     m.pose.act == SqMonsterPose::Act::pain, waiting, s.rng);
    if (c.spent) m.pain_at = s.level_time;
    if (c.sound != nullptr) sound(s, e, c.sound, c.everywhere ? 0 : 1);
    if (!c.run.none()) {  // the flinch cuts what it was doing
      m.pose.cue_pain = true;
      m.pose.pain_run = c.run;
      m.pain_hold = s.level_time + 0.1 * (c.run.last - c.run.first + 1);
      m.run_shoot = -1;
      m.busy_until = m.burst_until = std::min(m.burst_until, s.level_time);
    }
  } else if (!waiting) {
    m.pain_at = s.level_time;
    // The flinch: held until its pain frames have played (pose_monster sets the end from the
    // run it picks). At skill 3 none, but the hornet's.
    const bool flinch = skill(s) < 3 || k.classname == std::string_view("monster_boss2");
    if (flinch) {
      m.pain_hold = s.level_time + 0.3;  // a kind with no pain frames
      m.pose.cue_pain = true;
      m.run_shoot = -1;
      if (named(m, "misc_insane"))  // a man's cry by his health, close by
        sound(s, e, insane_pain(s, ent.health), 2);
      else
        sound(s, e, k.pain);
    }
  }
  // At skill 3 every blow puts the next pain 5 s off.
  if (skill(s) == 3) m.pain_at = s.level_time + 2;
  return true;
}

bool monster_laser_proof(const Q2Server& s, int e) {
  const int i = monster_index(s, e);
  return i >= 0 && named(s.monsters[size_t(i)], "monster_boss2");
}

bool monster_use(Q2Server& s, int e, int activator) {
  const int i = monster_index(s, e);
  if (i < 0) return false;
  Q2Monster& m = s.monsters[size_t(i)];
  Q2Entity& ent = s.entities[size_t(e)];
  if (kind(m).prop) {
    if (kind(m).keep) {  // the commander's body slumps
      if (m.used_at < 0) {
        m.used_at = s.level_time;
        sound(s, e, "tank/pain.wav");
      }
    } else {  // the Makron's figure goes, in a flash
      fight_effect(s, "te_boss_teleport", ent.origin);
      ent.gone = true;
      ent.box = false;
    }
    return true;
  }
  if (m.spare) return true;
  if (!m.waiting) {  // awake: a use with no enemy angers it at a player who used it
    if (!m.dead && m.enemy == -1 && targetable(s, activator)) found(s, m, activator, false);
    return true;
  }
  m.waiting = false;
  m.air_until = s.level_time + 12;
  ent.hidden = false;
  ent.box = true;
  for (int p = 0; p < sh::kMaxClients; p++) {  // whoever stands there dies
    if (!hunted(s, p)) continue;
    const sh::Vec3f at = unpack(s.players[p].move.origin);
    const Q2Player& pl = s.players[p];
    if (at.x + pl.mins.x < ent.origin.x + ent.maxs.x &&
        at.x + pl.maxs.x > ent.origin.x + ent.mins.x &&
        at.y + pl.mins.y < ent.origin.y + ent.maxs.y &&
        at.y + pl.maxs.y > ent.origin.y + ent.mins.y &&
        at.z + pl.mins.z < ent.origin.z + ent.maxs.z &&
        at.z + pl.maxs.z > ent.origin.z + ent.mins.z)
      fight_hurt(s, p, -1, {}, 100000, 0, kSqByTelefrag, 3);
  }
  // An ambusher appears without its trigger's player for an enemy: it waits to see or be hurt.
  if (!m.ambush && targetable(s, activator)) found(s, m, activator, false);
  return true;
}

bool monster_class(std::string_view classname) { return kind_of(classname) >= 0; }

bool monster_bring_spare(Q2Server& s, int block, sh::Vec3f at, sh::Vec3f angles, sh::Vec3f velocity,
                         int activator) {
  for (Q2Monster& m : s.monsters) {
    if (!m.spare || !m.waiting) continue;
    Q2Entity& e = body(s, m);
    if (e.block != block) continue;
    m.spare = false;
    m.waiting = false;
    e.hidden = false;
    e.box = true;
    e.origin = at;
    e.angles = angles;
    m.ideal_yaw = angles.y;
    m.velocity = velocity;
    m.on_ground = velocity.z <= 0 && on_floor(s, m);
    s.monsters_total++;
    for (int p = 0; p < sh::kMaxClients; p++) {  // whoever stands there dies
      if (!hunted(s, p)) continue;
      const sh::Vec3f pa = unpack(s.players[p].move.origin);
      const Q2Player& pl = s.players[p];
      if (pa.x + pl.mins.x < at.x + e.maxs.x && pa.x + pl.maxs.x > at.x + e.mins.x &&
          pa.y + pl.mins.y < at.y + e.maxs.y && pa.y + pl.maxs.y > at.y + e.mins.y &&
          pa.z + pl.mins.z < at.z + e.maxs.z && pa.z + pl.maxs.z > at.z + e.mins.z)
        fight_hurt(s, p, -1, {}, 100000, 0, kSqByTelefrag, 3);
    }
    for (const Q2Monster& o : s.monsters) {  // and any monster there
      if (&o == &m || o.spare || o.dead) continue;
      const Q2Entity& b = s.entities[size_t(o.entity)];
      if (b.gone || b.hidden) continue;
      if (b.origin.x + b.mins.x < at.x + e.maxs.x && b.origin.x + b.maxs.x > at.x + e.mins.x &&
          b.origin.y + b.mins.y < at.y + e.maxs.y && b.origin.y + b.maxs.y > at.y + e.mins.y &&
          b.origin.z + b.mins.z < at.z + e.maxs.z && b.origin.z + b.maxs.z > at.z + e.mins.z)
        monster_damage(s, o.entity, -1, 100000, {});
    }
    (void)activator;  // it wakes as any monster does: by what it sees and hears
    return true;
  }
  return false;
}

}  // namespace shooters_edge::server
