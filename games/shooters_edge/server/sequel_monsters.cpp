// The sequel's monsters (sequel_monsters.hpp states the rules; the numbers are the clean-room
// note's, porting/shooters-edge/sequel-monsters.md; speeds, timings and the step move are ours).
//
// ge:summary The sequel's monsters at work: noticing, chasing, attacking, the step move, death
// ge:keywords sequel monsters find-target attack-chance monster-step monster-damage corpse
// ge:skill shooter
// ge:test tests/shooters_edge_sequel_monsters_test.cpp
#include "shooters_edge/server/sequel_monsters.hpp"

#include <algorithm>
#include <cstdlib>
#include <string>
#include <string_view>

#include "brush/map.hpp"
#include "brush/trace.hpp"
#include "core/libm/libm.hpp"
#include "core/prng.hpp"
#include "shooters_edge/server/sequel_fight.hpp"
#include "shooters_edge/server/sequel_map.hpp"
#include "shooters_edge/server/sequel_monster_kinds.hpp"
#include "shooters_edge/server/sequel_server.hpp"
#include "shooters_edge/server/sequel_targets.hpp"

namespace shooters_edge::server {

namespace {

constexpr float kStep = 0.1f;  // a rules frame, s
constexpr float kMelee = 80, kNear = 500, kMid = 1000;
constexpr float kStepUp = 18;     // the highest step a walker climbs
constexpr float kInFront = 0.3f;  // the cosine of in front
constexpr double kGiveUp = 20;    // s after the last sighting
constexpr double kPainEvery = 3;  // s
constexpr float kHear = 1000;     // a noise's reach
constexpr int32_t kMonsterMask =
    sh::kSqSolid | sh::kSqWindow | sh::kSqMonster | 0x20000;  // + monster clip
constexpr int32_t kOpaque = sh::kSqSolid | sh::kSqSlime | sh::kSqLava;
constexpr int32_t kShotMask =
    sh::kSqSolid | sh::kSqWindow | sh::kSqSlime | sh::kSqLava | sh::kSqMonster;
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

bool deathmatch(const SequelServer& s) { return s.cvars.find("deathmatch")->on(); }
int skill(const SequelServer& s) {
  return std::clamp(static_cast<int>(s.cvars.find("skill")->number), 0, 3);
}
double random01(SequelServer& s) { return ge::core::next(s.rng); }
float crandom(SequelServer& s) { return float(2 * (ge::core::next(s.rng) - 0.5)); }

sh::Vec3f unpack(const int16_t v[3]) {
  return {float(v[0]) * 0.125f, float(v[1]) * 0.125f, float(v[2]) * 0.125f};
}

void event(SequelServer& s, NetEvent ev) {
  ev.time = s.level_time;
  s.out.events.push_back(std::move(ev));
  s.out.events_total++;
}

void sound(SequelServer& s, int e, const char* sample) {
  if (sample == nullptr) return;
  NetEvent ev;
  ev.kind = EventKind::sound;
  ev.to = kEveryone;
  ev.ent = map_row(e);
  ev.level = 2;  // the voice
  ev.origin = s.entities[size_t(e)].origin;
  ev.values[0] = 255;
  ev.values[1] = 1;
  ev.text = sample;
  event(s, std::move(ev));
}

sh::Vec3f eye(const SequelEntity& e, const Kind& k) {
  return e.origin + sh::Vec3f{0, 0, k.flies || k.swims ? 10.0f : 25.0f};
}
sh::Vec3f player_eye(const SequelPlayer& p) {
  return unpack(p.move.origin) + sh::Vec3f{0, 0, p.view_height};
}
bool hunted(const SequelServer& s, int slot) {
  return slot >= 0 && slot < sh::kMaxClients && s.clients[slot].active &&
         !s.clients[slot].spectator && s.players[slot].alive;
}

Range range_to(sh::Vec3f a, sh::Vec3f b) {
  const float d = sh::length(b - a);
  return d < kMelee ? Range::melee : d < kNear ? Range::near : d < kMid ? Range::mid : Range::far;
}

sh::Vec3f forward_of(float yaw) { return sh::angle_vectors({0, yaw, 0}).forward; }

bool in_front(const SequelEntity& e, sh::Vec3f at) {
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

const Kind& kind(const SequelMonster& m) { return kKinds[m.kind]; }
SequelEntity& body(SequelServer& s, const SequelMonster& m) { return s.entities[size_t(m.entity)]; }
bool afloat(const Kind& k) { return k.flies || k.swims; }

// Water (or slime or lava) at `p`.
bool wet(const SequelServer& s, sh::Vec3f p) {
  if (!s.tracer) return false;
  const float q[3] = {p.x, p.y, p.z};
  return (s.tracer->contents(q) & sh::kSqMaskWater) != 0;
}
// The point 1 above the bottom of a box at `origin`.
sh::Vec3f foot(const SequelEntity& e, sh::Vec3f origin) {
  return origin + sh::Vec3f{0, 0, e.mins.z + 1};
}

// A box swept for monster `m` from `start` to `end`: the map, the movers, the other monsters and
// the players.
sh::SequelTrace sweep(const SequelServer& s, const SequelMonster& m, sh::Vec3f start,
                      sh::Vec3f end) {
  const SequelEntity& e = s.entities[size_t(m.entity)];
  sh::SequelTrace t = map_trace(s, start, e.mins, e.maxs, end, kMonsterMask, m.entity);
  for (int i = 0; i < sh::kMaxClients; i++) {
    if (!hunted(s, i)) continue;
    const SequelPlayer& p = s.players[i];
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
bool reaches(const SequelServer& s, const SequelMonster& m, int slot, int32_t mask) {
  const SequelEntity& e = s.entities[size_t(m.entity)];
  const sh::SequelTrace t =
      map_trace(s, eye(e, kind(m)), {}, {}, player_eye(s.players[slot]), mask, m.entity);
  return t.fraction >= 1;
}

bool on_floor(const SequelServer& s, const SequelMonster& m) {
  const SequelEntity& e = s.entities[size_t(m.entity)];
  const sh::SequelTrace t = sweep(s, m, e.origin, e.origin - sh::Vec3f{0, 0, 0.25f});
  return t.fraction < 1 && t.normal.z > 0.7f;
}

// A walker's step of `move` along the floor: up a step of 18 when blocked, never off a ledge
// higher than a step, never into water from dry land. A flier never flies into water; a swimmer
// never swims out of it. False when it could not go.
bool step(SequelServer& s, SequelMonster& m, sh::Vec3f move) {
  SequelEntity& e = body(s, m);
  const Kind& k = kind(m);
  const bool was_wet = wet(s, foot(e, e.origin));
  if (afloat(k)) {
    const sh::SequelTrace t = sweep(s, m, e.origin, e.origin + move);
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
  sh::SequelTrace t = sweep(s, m, e.origin, e.origin + move);
  if (t.start_solid) return false;
  if (t.fraction >= 1) {
    to = t.end;
  } else {  // up a step, along, then down onto it
    const sh::SequelTrace rise = sweep(s, m, e.origin, e.origin + up);
    const sh::SequelTrace along = sweep(s, m, rise.end, rise.end + move);
    if (along.fraction < 1 || along.start_solid) return false;
    const sh::SequelTrace down = sweep(s, m, along.end, along.end - up);
    if (down.fraction >= 1 || down.normal.z <= 0.7f) return false;
    to = down.end;
  }
  if (m.on_ground) {  // never off a ledge
    const sh::SequelTrace below = sweep(s, m, to, to - sh::Vec3f{0, 0, kStepUp + 1});
    if (below.fraction >= 1) return false;
    to = below.fraction > 0 ? below.end : to;
  }
  if (!was_wet && wet(s, foot(e, to))) return false;
  e.origin = to;
  return true;
}

// The monster's fall under gravity (a walker off the floor, a leaper, a dead flier); a leaper's
// strike on what it meets fast.
void fall(SequelServer& s, SequelMonster& m) {
  SequelEntity& e = body(s, m);
  const Kind& k = kind(m);
  if (k.flies && !m.dead) return;
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
  m.velocity.z -= static_cast<float>(s.cvars.find("sv_gravity")->number) * m.gravity * kStep;
  const sh::SequelTrace t = sweep(s, m, e.origin, e.origin + m.velocity * kStep);
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
void turn(SequelEntity& e, const Kind& k, float ideal) {
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
void go_to(SequelServer& s, SequelMonster& m, sh::Vec3f goal, float speed) {
  SequelEntity& e = body(s, m);
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

int corner_named(const SequelServer& s, const std::string& name, const char* classname) {
  if (name.empty()) return -1;
  for (size_t i = 0; i < s.entities.size(); i++)
    if (s.entities[i].targetname == name && s.entities[i].classname == classname)
      return static_cast<int>(i);
  return -1;
}

// ---- Noticing ----

// The monster turns on `slot` (seen when `saw`).
void found(SequelServer& s, SequelMonster& m, int slot, bool saw) {
  if (kind(m).good) return;  // a good guy hunts no one
  m.enemy = slot;
  m.seen_at = s.level_time;
  m.last_sight = unpack(s.players[slot].move.origin);
  if (saw) {
    s.sight_entity = m.entity;
    s.sight_frame = s.frame_number;
    sound(s, m.entity, kind(m).sight);
  }
}

// Whether monster `m` sees player `slot` now.
bool sees(const SequelServer& s, const SequelMonster& m, int slot) {
  const SequelEntity& e = s.entities[size_t(m.entity)];
  const sh::Vec3f at = unpack(s.players[slot].move.origin);
  const Range r = range_to(e.origin, at);
  if (r == Range::far) return false;
  if (!reaches(s, m, slot, kOpaque)) return false;
  if (r == Range::near && m.enemy == slot) return true;  // already found: no need to face him
  return in_front(e, at);
}

void notice(SequelServer& s, SequelMonster& m) {
  // 1. a monster that saw a player this frame or the last.
  if (!m.ambush && s.sight_entity >= 0 && s.sight_entity != m.entity &&
      s.sight_frame >= s.frame_number - 1) {
    const int other = monster_index(s, s.sight_entity);
    if (other >= 0) {
      const int slot = s.monsters[size_t(other)].enemy;
      if (hunted(s, slot) && slot != m.enemy) {
        found(s, m, slot, false);
        return;
      }
    }
  }
  // 2. a noise a player made, heard within 1000.
  if (!m.ambush)
    for (int i = 0; i < sh::kMaxClients; i++) {
      if (!hunted(s, i) || s.players[i].noise_frame < s.frame_number - 1) continue;
      if (sh::length(s.players[i].noise_at - body(s, m).origin) > kHear) continue;
      m.enemy = i;
      m.seen_at = s.level_time;
      m.last_sight = s.players[i].noise_at;
      return;
    }
  // 3. the frame's player in turn, seen.
  const int slot = s.sight_client;
  if (hunted(s, slot) && sees(s, m, slot)) found(s, m, slot, true);
}

// ---- Attacks ----

// The enemy's eye, `lead` s along his motion.
sh::Vec3f mark(const SequelServer& s, const SequelMonster& m, float lead) {
  const SequelPlayer& p = s.players[m.enemy];
  return player_eye(p) + unpack(p.move.velocity) * lead;
}

// A shot from `from` at `target`, with the attack's spread.
sh::Vec3f aim_at(SequelServer& s, const Attack& a, sh::Vec3f from, sh::Vec3f target) {
  sh::Vec3f dir = target - from;
  sh::normalize(dir);
  if (a.h == 0 && a.v == 0) return dir;
  const sh::ViewAxes ax = sh::angle_vectors({0, yaw_to(dir), 0});
  sh::Vec3f end = from + dir * 8192 + ax.right * (crandom(s) * a.h) + ax.up * (crandom(s) * a.v);
  sh::Vec3f out = end - from;
  sh::normalize(out);
  return out;
}
sh::Vec3f aim(SequelServer& s, SequelMonster& m, const Attack& a, sh::Vec3f from) {
  return aim_at(s, a, from, mark(s, m, a.lead));
}

int cause_of(const SequelMonster& m) { return kSqByMonster + m.kind; }

const Attack& attack_at(const SequelMonster& m, int i) {
  return i < 2 ? kind(m).melee[i] : kind(m).missile[i - 2];
}
const Attack& attack_of(const SequelMonster& m) { return attack_at(m, m.attack); }

// A blow within 80 of the monster's origin, by a clear line: the damage and a kick added straight
// to the enemy's velocity (negative pulls). True when it landed.
bool blow(SequelServer& s, SequelMonster& m, const Attack& a) {
  const SequelEntity& e = body(s, m);
  const sh::Vec3f at = unpack(s.players[m.enemy].move.origin);
  if (sh::length(at - e.origin) > kMelee) return false;  // swung at the air
  if (!reaches(s, m, m.enemy, kOpaque)) return false;
  const int dmg = a.damage + static_cast<int>(random01(s) * (a.damage_max - a.damage + 1));
  sh::Vec3f dir = at - e.origin;
  sh::normalize(dir);
  fight_hurt(s, m.enemy, -1, {}, dmg, 0, cause_of(m));
  if (a.kick != 0 && hunted(s, m.enemy)) fight_kick(s, m.enemy, dir * float(a.kick));
  return true;
}

// One frame's shots of attack `a` (`n` the shot's number in its loop).
void shoot(SequelServer& s, SequelMonster& m, const Attack& a, int n) {
  if (!hunted(s, m.enemy)) return;
  SequelEntity& e = body(s, m);
  const sh::Vec3f from = eye(e, kind(m));
  m.ideal_yaw = yaw_to(unpack(s.players[m.enemy].move.origin) - e.origin);
  if (a.fire != Fire::sweep) e.angles.y = m.ideal_yaw;
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
    case Fire::pellets:
      for (int i = 0; i < a.count; i++) {
        const sh::Vec3f d = aim(s, m, a, from);
        fight_monster_bullet(s, m.entity, from, from + d * 8192, a.damage, a.kick, cause_of(m));
      }
      return;
    case Fire::burst:
    case Fire::guns:
      for (int i = 0; i < (a.fire == Fire::guns ? a.count : 1); i++) {
        const sh::Vec3f d = aim(s, m, a, from);
        fight_monster_bullet(s, m.entity, from, from + d * 8192, a.damage, a.kick, cause_of(m));
      }
      return;
    case Fire::drain: {  // a tongue that holds while the line is clear
      if (!reaches(s, m, m.enemy, kShotMask & ~sh::kSqMonster)) return;
      const int dmg = m.burst_frame == 0 ? a.damage : a.damage_max;
      fight_hurt(s, m.enemy, -1, {}, dmg, 0, cause_of(m));
      return;
    }
    case Fire::club:
      if (blow(s, m, a)) m.struck = true;
      return;
    case Fire::zap: {  // wherever he is
      const int dmg = a.damage + static_cast<int>(random01(s) * (a.damage_max - a.damage + 1));
      fight_effect(s, "te_sparks", e.origin + forward_of(e.angles.y) * 18.5f);
      fight_hurt(s, m.enemy, -1, {}, dmg, 0, cause_of(m));
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
void begin_attack(SequelServer& s, SequelMonster& m, int i) {
  const Attack& a = attack_at(m, i);
  m.attack = i;
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
bool goes_on(SequelServer& s, SequelMonster& m, const Attack& a) {
  if (a.again <= 0 || (a.again_max > 0 && m.loops >= a.again_max)) return false;
  if (!hunted(s, m.enemy)) return false;
  const SequelEntity& e = body(s, m);
  const Range r = range_to(e.origin, unpack(s.players[m.enemy].move.origin));
  if (m.attack < 2) {  // a blow: while he is within reach (at skill 3 the mutant's, by chance)
    if (r != Range::melee && !(skill(s) == 3 && kind(m).test == Test::mutant)) return false;
  } else if (!reaches(s, m, m.enemy, kOpaque)) {
    return false;
  }
  return random01(s) < a.again;
}

// The attack in progress, this frame.
void attacking(SequelServer& s, SequelMonster& m) {
  const Attack& a = attack_of(m);
  if (a.fire == Fire::rail && !m.aimed && hunted(s, m.enemy) &&
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
int pick_ranged(SequelServer& s, const SequelMonster& m, float dist) {
  const Kind& k = kind(m);
  const int n = ranged_count(k);
  if (k.close > 0 && dist < k.close) return 2;
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
bool leap_test(SequelServer& s, const SequelMonster& m) {
  const SequelEntity& e = body(s, m);
  const SequelPlayer& p = s.players[m.enemy];
  const sh::Vec3f at = unpack(p.move.origin);
  const float height = p.maxs.z - p.mins.z;
  const float bottom = at.z + p.mins.z;
  if (e.origin.z + e.mins.z > bottom + 0.75f * height) return false;
  if (e.origin.z + e.maxs.z < bottom + 0.25f * height) return false;
  const sh::Vec3f d{at.x - e.origin.x, at.y - e.origin.y, 0};
  if (sh::length(d) < 100) return false;
  return random01(s) < 0.1;
}

// The attack chosen this frame, if any (sequel-monsters.md § Choosing to attack;
// sequel-monsters-full.md for the kinds with their own test).
void choose(SequelServer& s, SequelMonster& m) {
  if (s.level_time + 0.001 < m.busy_until) return;
  SequelEntity& e = body(s, m);
  const Kind& k = kind(m);
  const bool boss = k.test == Test::hover || k.test == Test::rider;
  if (!reaches(s, m, m.enemy, boss ? kShotMask & ~sh::kSqWindow : kShotMask)) return;
  const sh::Vec3f at = unpack(s.players[m.enemy].move.origin);
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
  if (random01(s) >= chance) return;
  const int i = pick_ranged(s, m, dist);
  m.attack_finished = s.level_time + 2 * random01(s);
  if (dist < attack_at(m, i).closest) return;  // too near for it: keeps closing
  begin_attack(s, m, i);
}

// ---- The medic ----

// Whether `medic` sees corpse `c` (eye to its middle).
bool sees_body(const SequelServer& s, const SequelMonster& medic, const SequelMonster& c) {
  const SequelEntity& e = s.entities[size_t(medic.entity)];
  const SequelEntity& b = s.entities[size_t(c.entity)];
  const sh::Vec3f mid = b.origin + (b.mins + b.maxs) * 0.5f;
  return map_trace(s, eye(e, kind(medic)), {}, {}, mid, kOpaque, medic.entity).fraction >= 1;
}

// The corpse within 1024 it sees with the most health to come back to; -1 none.
int find_patient(const SequelServer& s, const SequelMonster& medic) {
  const SequelEntity& e = s.entities[size_t(medic.entity)];
  int best = -1;
  for (size_t i = 0; i < s.monsters.size(); i++) {
    const SequelMonster& c = s.monsters[i];
    const SequelEntity& b = s.entities[size_t(c.entity)];
    if (!c.dead || c.claimed || b.gone || kind(c).end != End::corpse) continue;
    if (s.level_time + 0.001 < c.died_at + double(kind(c).revive_after)) continue;
    if (sh::length(b.origin - e.origin) > 1024 || !sees_body(s, medic, c)) continue;
    if (best < 0 || kind(c).health > kind(s.monsters[size_t(best)]).health)
      best = static_cast<int>(i);
  }
  return best;
}

// Corpse `c` comes back as its kind, with none of its map orders, full health, counted again.
void revive(SequelServer& s, SequelMonster& c, int enemy) {
  SequelEntity& b = body(s, c);
  const Kind& k = kind(c);
  SequelMonster fresh;
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
  b.target.clear();
  b.targetname.clear();
  b.spawnflags = 0;
  s.monsters_total++;
  if (hunted(s, enemy)) found(s, c, enemy, false);
}

// A medic's frame with a corpse to raise: the cable within 256, else toward it. True when it was
// busy with one.
bool tend(SequelServer& s, SequelMonster& m) {
  if (m.patient < 0 && (s.frame_number % 10 == 0 || m.enemy < 0)) {
    m.patient = find_patient(s, m);
    if (m.patient >= 0) {
      s.monsters[size_t(m.patient)].claimed = true;
      m.old_enemy = m.enemy;
      m.revive_at = 0;
    }
  }
  if (m.patient < 0) return false;
  SequelMonster& c = s.monsters[size_t(m.patient)];
  SequelEntity& e = body(s, m);
  const SequelEntity& b = body(s, c);
  const auto release = [&] {
    c.claimed = false;
    m.patient = -1;
    m.revive_at = 0;
    m.enemy = hunted(s, m.old_enemy) ? m.old_enemy : -1;
  };
  if (!c.dead || b.gone) {
    release();
    return false;
  }
  const bool near = sh::length(b.origin - e.origin) <= 256 && sees_body(s, m, c);
  if (m.revive_at > 0) {  // the cable is out
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

// ---- The frame ----

void think(SequelServer& s, SequelMonster& m) {
  SequelEntity& e = body(s, m);
  const Kind& k = kind(m);
  if (!k.flies && !k.swims) targets_monster_touch(s, m);
  fall(s, m);
  if (m.leaping) return;                                  // in the air
  if (m.enemy >= 0 && !hunted(s, m.enemy)) m.enemy = -1;  // dead or gone
  if (k.turret) {  // sits its turret: aims it at whom it sees, fires once it has seen long enough
    if (m.enemy < 0) notice(s, m);
    targets_turret(s, m, m.enemy >= 0 && sees(s, m, m.enemy) ? m.enemy : -1);
    return;
  }
  if (k.medic && tend(s, m)) return;
  if (m.enemy < 0) notice(s, m);
  if (m.enemy < 0) {  // walks its path, or stands
    if (m.corner >= 0) {
      const SequelEntity& c = s.entities[size_t(m.corner)];
      sh::Vec3f d = c.origin - e.origin;
      d.z = 0;
      if (sh::length(d) < 16) {
        m.corner = corner_named(s, c.target, "path_corner");
        return;
      }
      go_to(s, m, c.origin, k.walk);
    }
    return;
  }
  if (sees(s, m, m.enemy) || reaches(s, m, m.enemy, kOpaque)) {
    m.seen_at = s.level_time;
    m.last_sight = unpack(s.players[m.enemy].move.origin);
  } else if (s.level_time > m.seen_at + kGiveUp) {
    m.enemy = -1;  // gives up
    return;
  }
  if (s.level_time + 0.001 < m.pain_hold) return;  // flinching
  attacking(s, m);
  if (s.level_time + 0.001 < m.busy_until) return;
  if (k.screen) m.screen = std::max(m.screen, 0);
  choose(s, m);
  if (s.level_time + 0.001 < m.busy_until) return;
  // The run: to its combat point first, then at the enemy (or where it was last seen).
  if (m.combat >= 0) {
    const sh::Vec3f spot = s.entities[size_t(m.combat)].origin;
    sh::Vec3f d = spot - e.origin;
    d.z = 0;
    if (sh::length(d) < 16) {
      m.combat = -1;
      m.hold = true;  // holds there
      return;
    }
    go_to(s, m, spot, k.run);
    return;
  }
  if (m.hold) {
    turn(e, k, yaw_to(m.last_sight - e.origin));
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
void blow_up(SequelServer& s, SequelMonster& m) {
  SequelEntity& e = body(s, m);
  fight_effect(s, "te_explosion", e.origin);
  e.gone = true;
}

// The spare Makron that waits inside boss `m` (Jorg) comes out and leaps at the frame's player.
void bring_out(SequelServer& s, const SequelMonster& m) {
  for (SequelMonster& o : s.monsters) {
    if (!o.spare || !o.waiting) continue;
    SequelEntity& e = body(s, o);
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
void lie(SequelServer& s, SequelMonster& m) {
  fall(s, m);
  SequelEntity& e = body(s, m);
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
  fight_gib(s, map_row(m.entity), e.origin);
}

void die(SequelServer& s, SequelMonster& m, int attacker) {
  SequelEntity& e = body(s, m);
  const Kind& k = kind(m);
  m.dead = true;
  m.died_at = s.level_time;
  m.leaping = false;
  m.screen = 0;
  e.box = false;  // shots pass a corpse
  if (!k.good) s.monsters_killed++;
  sound(s, m.entity, k.death);
  if (m.patient >= 0) {  // a medic's patient is free for another
    s.monsters[size_t(m.patient)].claimed = false;
    m.patient = -1;
  }
  if (!m.item.empty()) fight_drop_at(s, e.origin, m.item);
  map_use_targets(s, m.entity, m.enemy >= 0 ? m.enemy : attacker);
  switch (k.end) {
    case End::explode:
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
    fight_gib(s, map_row(m.entity), e.origin);
  }
  e.maxs.z = std::min(e.maxs.z, -8.0f);
}

}  // namespace

const char* monster_obituary(int cause) {
  const int k = cause - kSqByMonster;
  if (k < 0 || k >= kKindCount) return nullptr;
  return kKinds[k].name;
}

int monster_index(const SequelServer& s, int e) {
  for (size_t i = 0; i < s.monsters.size(); i++)
    if (s.monsters[i].entity == e) return static_cast<int>(i);
  return -1;
}

void monster_precache(SequelServer& s) {
  const auto model = [&s](const char* name) {
    const auto found = std::find(s.models.begin(), s.models.end(), name);
    if (found != s.models.end()) return static_cast<int>(found - s.models.begin());
    s.models.emplace_back(name);
    return static_cast<int>(s.models.size()) - 1;
  };
  const auto add_sound = [&s](const char* name) {
    if (name != nullptr && std::find(s.sounds.begin(), s.sounds.end(), name) == s.sounds.end())
      s.sounds.emplace_back(name);
  };
  const auto add_kind = [&](int k) {
    for (const char* name : {kKinds[k].sight, kKinds[k].pain, kKinds[k].death}) add_sound(name);
    return model(kKinds[k].model);
  };
  for (SequelEntity& e : s.entities) {
    const int k = kind_of(e.classname);
    if (k < 0 || e.gone || (deathmatch(s) && !kKinds[k].keep)) continue;
    e.modelindex = add_kind(k);
    if (kKinds[k].medic)
      for (const char* name : {"medic/medatck2.wav", "medic/medatck4.wav"}) add_sound(name);
    if (kKinds[k].end == End::launch) add_kind(kind_of("monster_makron"));
    if (kKinds[k].prop && kKinds[k].keep)
      for (const char* name : {"tank/pain.wav", "tank/thud.wav"}) add_sound(name);
  }
  for (const SequelEntity& e : s.entities) {  // what the spawners make
    if (e.classname != "target_spawner" || e.gone) continue;
    const int k = kind_of(e.target);
    if (k >= 0 && !(deathmatch(s) && !kKinds[k].keep)) add_kind(k);
  }
}

void monster_spawn(SequelServer& s) {
  s.monsters.clear();
  s.monsters_total = s.monsters_killed = 0;
  s.sight_entity = -1;
  // A boss that brings out another at its death (Jorg) has it waiting inside, unseen: a map
  // entity of its own, so it has its world row from the start.
  const size_t mapped = s.entities.size();
  for (size_t i = 0; i < mapped; i++) {
    const int k = kind_of(s.entities[i].classname);
    if (k < 0 || s.entities[i].gone || kKinds[k].end != End::launch || deathmatch(s)) continue;
    SequelEntity spare = s.entities[i];
    spare.classname = "monster_makron";
    spare.targetname.clear();
    spare.spawnflags = 2;
    s.entities.push_back(spare);
  }
  for (size_t i = 0; i < s.entities.size(); i++) {
    SequelEntity& e = s.entities[i];
    const int k = kind_of(e.classname);
    if (k < 0 || e.gone) continue;
    const Kind& kd = kKinds[k];
    if (deathmatch(s) && !kd.keep) {
      e.gone = true;
      continue;
    }
    SequelMonster m;
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
    m.screen = kd.screen ? 100 : 0;
    m.ambush = (e.spawnflags & 5) != 0;
    if (!m.spare) m.item = s.map->entities[size_t(e.block)].get("item");
    if (((e.spawnflags & 2) != 0 || m.spare) && !kd.prop) {  // waits hidden for a use
      e.hidden = true;
      e.box = false;
      m.waiting = true;
    }
    m.ideal_yaw = e.angles.y;
    m.corner = corner_named(s, e.target, "path_corner");
    m.combat = corner_named(s, e.target, "point_combat");
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
  for (SequelMonster& m : s.monsters) {  // walkers to the floor
    if (afloat(kKinds[m.kind]) || m.waiting || m.gravity == 0) continue;
    SequelEntity& e = s.entities[size_t(m.entity)];
    const sh::SequelTrace t =
        sweep(s, m, e.origin + sh::Vec3f{0, 0, 1}, e.origin - sh::Vec3f{0, 0, 256});
    if (t.start_solid) continue;
    e.origin = t.end;
    m.on_ground = t.fraction < 1;
  }
}

void monster_frame(SequelServer& s) {
  if (s.monsters.empty()) return;
  // The frame's player in turn, for the monsters to look at.
  for (int k = 1; k <= sh::kMaxClients; k++) {
    const int slot = (s.sight_client + k) % sh::kMaxClients;
    if (hunted(s, slot)) {
      s.sight_client = slot;
      break;
    }
  }
  for (size_t i = 0; i < s.monsters.size(); i++) {
    SequelMonster& m = s.monsters[i];
    if (kind(m).prop) {  // the commander's body: falls, and after a use slumps, frame by frame
      if (kind(m).keep && m.used_at >= 0 && s.level_time + 0.001 >= m.used_at + 2.2 &&
          s.level_time < m.used_at + 2.25)
        sound(s, m.entity, "tank/thud.wav");
      if (kind(m).keep && s.level_time >= 0.5) fall(s, m);
      continue;
    }
    if (m.dead) {
      lie(s, m);
      continue;
    }
    if (m.waiting || body(s, m).gone) continue;
    think(s, m);
  }
}

bool monster_damage(SequelServer& s, int e, int attacker, int damage, sh::Vec3f dir) {
  const int i = monster_index(s, e);
  if (i < 0) return false;
  SequelMonster& m = s.monsters[size_t(i)];
  SequelEntity& ent = s.entities[size_t(e)];
  const Kind& k = kind(m);
  if (k.prop) return true;  // a figure: never hurt
  if (ent.gone || m.waiting || damage <= 0) return false;
  if (m.dead && k.end != End::corpse && k.end != End::crash) return true;  // a dying boss
  // A power screen takes a third of a blow from in front (but not while its chest is open).
  const bool opened = m.attack == 1 && s.level_time + 0.001 < m.busy_until;
  if (m.screen > 0 && !opened && sh::normalize(dir) > 0 &&
      sh::dot(dir * -1.0f, forward_of(ent.angles.y)) > kInFront) {
    const int saved = std::min(damage / 3, m.screen);
    m.screen -= saved;
    damage -= saved;
    if (saved > 0) fight_effect(s, "te_screen_sparks", ent.origin + dir * -16.0f);
    if (damage <= 0) return true;
  }
  ent.health -= damage;
  // The knockback: 500 × damage / mass along the blow.
  if (sh::normalize(dir) > 0 && !afloat(k)) {
    m.velocity = m.velocity + dir * (500.0f * float(damage) / float(std::max(50, k.mass)));
    m.on_ground = false;
  }
  if (m.dead) {  // a corpse thrown apart
    if (ent.health <= k.gib) {
      ent.gone = true;
      fight_gib(s, map_row(e), ent.origin);
    }
    return true;
  }
  if (ent.health <= 0) {
    die(s, m, attacker);
    return true;
  }
  if (hunted(s, attacker) && attacker != m.enemy && m.patient < 0) found(s, m, attacker, false);
  if (hunted(s, attacker) && m.enemy < 0) found(s, m, attacker, false);
  if (s.level_time + 0.001 >= m.pain_at + kPainEvery && !m.leaping) {
    m.pain_at = s.level_time + (skill(s) == 3 ? 2.0 : 0.0);  // a 5 s wait at skill 3
    if (skill(s) < 3) m.pain_hold = s.level_time + 0.3;      // and no flinch
    sound(s, e, k.pain);
  }
  return true;
}

bool monster_use(SequelServer& s, int e, int activator) {
  const int i = monster_index(s, e);
  if (i < 0) return false;
  SequelMonster& m = s.monsters[size_t(i)];
  SequelEntity& ent = s.entities[size_t(e)];
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
  if (!m.waiting || m.spare) return true;
  m.waiting = false;
  ent.hidden = false;
  ent.box = true;
  for (int p = 0; p < sh::kMaxClients; p++) {  // whoever stands there dies
    if (!hunted(s, p)) continue;
    const sh::Vec3f at = unpack(s.players[p].move.origin);
    const SequelPlayer& pl = s.players[p];
    if (at.x + pl.mins.x < ent.origin.x + ent.maxs.x &&
        at.x + pl.maxs.x > ent.origin.x + ent.mins.x &&
        at.y + pl.mins.y < ent.origin.y + ent.maxs.y &&
        at.y + pl.maxs.y > ent.origin.y + ent.mins.y &&
        at.z + pl.mins.z < ent.origin.z + ent.maxs.z &&
        at.z + pl.maxs.z > ent.origin.z + ent.mins.z)
      fight_hurt(s, p, -1, {}, 100000, 0, kSqByTelefrag, 3);
  }
  if (hunted(s, activator)) found(s, m, activator, false);
  return true;
}

bool monster_class(std::string_view classname) { return kind_of(classname) >= 0; }

bool monster_bring_spare(SequelServer& s, int block, sh::Vec3f at, sh::Vec3f angles,
                         sh::Vec3f velocity, int activator) {
  for (SequelMonster& m : s.monsters) {
    if (!m.spare || !m.waiting) continue;
    SequelEntity& e = body(s, m);
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
      const SequelPlayer& pl = s.players[p];
      if (pa.x + pl.mins.x < at.x + e.maxs.x && pa.x + pl.maxs.x > at.x + e.mins.x &&
          pa.y + pl.mins.y < at.y + e.maxs.y && pa.y + pl.maxs.y > at.y + e.mins.y &&
          pa.z + pl.mins.z < at.z + e.maxs.z && pa.z + pl.maxs.z > at.z + e.mins.z)
        fight_hurt(s, p, -1, {}, 100000, 0, kSqByTelefrag, 3);
    }
    if (hunted(s, activator)) found(s, m, activator, false);
    return true;
  }
  return false;
}

void monster_noise(SequelServer& s, int slot) {
  SequelPlayer& p = s.players[slot];
  p.noise_frame = s.frame_number;
  p.noise_at = unpack(p.move.origin);
}

void monster_rows(SequelServer& s) {
  RulesWorld& w = s.world;
  for (const SequelMonster& m : s.monsters) {
    const SequelEntity& e = s.entities[size_t(m.entity)];
    const size_t row = size_t(map_row(m.entity));
    if (row >= w.entities.size()) continue;
    WorldEntity& r = w.entities[row];
    r.free = e.gone || e.hidden;
    r.origin = e.origin;
    r.angles = {0, e.angles.y, 0};
    r.velocity = m.velocity;
    r.mins = e.mins;
    r.maxs = e.maxs;
    r.absmin = e.origin + e.mins;
    r.absmax = e.origin + e.maxs;
    r.health = float(e.health);
    r.modelindex = e.modelindex;
    // The damaged skin below half health.
    r.skin = kind(m).skin + (e.health < e.max_health / 2 ? 1 : 0);
    // The commander's body slumps from frame 1 to 24, a frame every 0.1 s from 0.1 s after a use.
    if (kind(m).prop && m.used_at >= 0)
      r.frame = std::clamp(static_cast<int>((s.level_time - m.used_at) / 0.1), 0, 24);
  }
}

}  // namespace shooters_edge::server
