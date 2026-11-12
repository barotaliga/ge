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
#include "core/libm/libm.hpp"
#include "core/prng.hpp"
#include "shooters_edge/server/sequel_fight.hpp"
#include "shooters_edge/server/sequel_map.hpp"
#include "shooters_edge/server/sequel_server.hpp"

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

// What an attack fires.
enum class Fire : uint8_t { none, bolt, pellets, burst, club, grenade, rockets, drain };

struct Attack {
  Fire fire = Fire::none;
  int damage = 0, damage_max = 0;  // a club's range of blows
  int kick = 0;
  float speed = 0;                     // a bolt's, a grenade's, a rocket's
  int count = 1;                       // pellets, bolts in a pair, rockets in a volley
  float h = 0, v = 0;                  // the spread at 8192, right and up
  float burst_min = 0, burst_max = 0;  // s
};

struct Kind {
  const char* classname;
  const char* name;  // the obituary's
  const char* model;
  int skin;
  int health, gib, mass;
  sh::Vec3f mins, maxs;
  bool flies;
  float walk, run;  // units/s
  Attack melee;
  Attack missile[3];
  bool soldier_aim;  // the soldiers' wild aim
  const char *sight, *pain, *death;
};

constexpr sh::Vec3f kLo{-16, -16, -24}, kHi{16, 16, 32};

// The roster (sequel-monsters.md § The demo's roster).
const Kind kKinds[] = {
    {"monster_soldier_light",
     "a light guard",
     "models/monsters/soldier/tris.md2",
     0,
     20,
     -30,
     100,
     kLo,
     kHi,
     false,
     50,
     150,
     {},
     {{Fire::bolt, 5, 5, 0, 600}},
     true,
     "soldier/solsght1.wav",
     "soldier/solpain2.wav",
     "soldier/soldeth1.wav"},
    {"monster_soldier",
     "a guard",
     "models/monsters/soldier/tris.md2",
     2,
     30,
     -30,
     100,
     kLo,
     kHi,
     false,
     50,
     150,
     {},
     {{Fire::pellets, 2, 2, 1, 0, 12, 1000, 500}},
     true,
     "soldier/solsght1.wav",
     "soldier/solpain1.wav",
     "soldier/soldeth2.wav"},
    {"monster_soldier_ss",
     "a heavy guard",
     "models/monsters/soldier/tris.md2",
     4,
     40,
     -30,
     100,
     kLo,
     kHi,
     false,
     50,
     150,
     {},
     {{Fire::burst, 2, 2, 4, 0, 1, 1000, 500, 0.3f, 1}},
     true,
     "soldier/solsght1.wav",
     "soldier/solpain3.wav",
     "soldier/soldeth3.wav"},
    {"monster_infantry",
     "a trooper",
     "models/monsters/infantry/tris.md2",
     0,
     100,
     -40,
     200,
     kLo,
     kHi,
     false,
     50,
     150,
     {Fire::club, 5, 9, 50},
     {{Fire::burst, 3, 3, 4, 0, 1, 300, 500, 0.5f, 1}},
     false,
     "infantry/infsght1.wav",
     "infantry/infpain1.wav",
     "infantry/infdeth1.wav"},
    {"monster_gunner",
     "a gunner",
     "models/monsters/gunner/tris.md2",
     0,
     175,
     -70,
     200,
     kLo,
     kHi,
     false,
     50,
     150,
     {},
     {{Fire::burst, 3, 3, 4, 0, 1, 300, 500, 0.5f, 1.5f}, {Fire::grenade, 50, 50, 0, 600}},
     false,
     "gunner/sight1.wav",
     "gunner/gunpain1.wav",
     "gunner/death1.wav"},
    {"monster_berserk",
     "a berserker",
     "models/monsters/berserk/tris.md2",
     0,
     240,
     -60,
     250,
     kLo,
     kHi,
     false,
     50,
     200,
     {Fire::club, 15, 20, 400},
     {},
     false,
     "berserk/sight.wav",
     "berserk/berpain2.wav",
     "berserk/berdeth2.wav"},
    {"monster_parasite",
     "a parasite",
     "models/monsters/parasite/tris.md2",
     0,
     175,
     -50,
     250,
     kLo,
     sh::Vec3f{16, 16, 24},
     false,
     50,
     150,
     {},
     {{Fire::drain, 5, 2, 0}},
     false,
     "parasite/parsght1.wav",
     "parasite/parpain1.wav",
     "parasite/pardeth1.wav"},
    {"monster_flyer",
     "a flyer",
     "models/monsters/flyer/tris.md2",
     0,
     50,
     0,
     50,
     kLo,
     kHi,
     true,
     100,
     200,
     {Fire::club, 5, 5, 0},
     {{Fire::bolt, 1, 1, 0, 1000, 2}},
     false,
     "flyer/flysght1.wav",
     "flyer/flypain1.wav",
     "flyer/flydeth1.wav"},
    {"monster_tank",
     "a tank",
     "models/monsters/tank/tris.md2",
     0,
     750,
     -200,
     500,
     sh::Vec3f{-32, -32, -16},
     sh::Vec3f{32, 32, 72},
     false,
     40,
     80,
     {},
     {{Fire::bolt, 30, 30, 0, 800},
      {Fire::rockets, 50, 50, 0, 550, 3},
      {Fire::burst, 20, 20, 4, 0, 1, 300, 500, 0.5f, 1}},
     false,
     "tank/sight1.wav",
     "tank/tnkpain2.wav",
     "tank/death.wav"},
    {"monster_tank_commander",
     "a tank commander",
     "models/monsters/tank/tris.md2",
     2,
     1000,
     -225,
     500,
     sh::Vec3f{-32, -32, -16},
     sh::Vec3f{32, 32, 72},
     false,
     40,
     80,
     {},
     {{Fire::bolt, 30, 30, 0, 800},
      {Fire::rockets, 50, 50, 0, 550, 3},
      {Fire::burst, 20, 20, 4, 0, 1, 300, 500, 0.5f, 1}},
     false,
     "tank/sight1.wav",
     "tank/tnkpain2.wav",
     "tank/death.wav"},
};
constexpr int kKindCount = int(sizeof(kKinds) / sizeof(kKinds[0]));

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
  return e.origin + sh::Vec3f{0, 0, k.flies ? 10.0f : 25.0f};
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
// higher than a step. False when it could not go.
bool step(SequelServer& s, SequelMonster& m, sh::Vec3f move) {
  SequelEntity& e = body(s, m);
  if (kind(m).flies) {
    const sh::SequelTrace t = sweep(s, m, e.origin, e.origin + move);
    if (t.start_solid || t.fraction < 1) return false;
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
  e.origin = to;
  return true;
}

// The monster's fall under gravity (a walker off the floor).
void fall(SequelServer& s, SequelMonster& m) {
  SequelEntity& e = body(s, m);
  if (kind(m).flies) return;
  m.on_ground = on_floor(s, m);
  if (m.on_ground) {
    m.velocity = {};
    return;
  }
  m.velocity.z -= static_cast<float>(s.cvars.find("sv_gravity")->number) * kStep;
  const sh::SequelTrace t = sweep(s, m, e.origin, e.origin + m.velocity * kStep);
  e.origin = t.end;
  if (t.fraction < 1 && t.normal.z > 0.7f) {
    m.on_ground = true;
    m.velocity = {};
  }
}

// Turns toward the ideal yaw by the kind's turning speed.
void turn(SequelEntity& e, const Kind& k, float ideal) {
  const float most = k.flies ? 10.0f : 20.0f;
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
  if (!kind(m).flies) d.z = 0;
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

// A shot from the monster's eye at its enemy's, with the kind's spread.
sh::Vec3f aim(SequelServer& s, SequelMonster& m, const Attack& a, sh::Vec3f from) {
  const sh::Vec3f target = player_eye(s.players[m.enemy]);
  sh::Vec3f dir = target - from;
  sh::normalize(dir);
  if (a.h == 0 && a.v == 0) return dir;
  const sh::ViewAxes ax = sh::angle_vectors({0, yaw_to(dir), 0});
  sh::Vec3f end = from + dir * 8192 + ax.right * (crandom(s) * a.h) + ax.up * (crandom(s) * a.v);
  sh::Vec3f out = end - from;
  sh::normalize(out);
  return out;
}

int cause_of(const SequelMonster& m) { return kSqByMonster + m.kind; }

// One frame's shots of attack `a`.
void shoot(SequelServer& s, SequelMonster& m, const Attack& a) {
  if (!hunted(s, m.enemy)) return;
  SequelEntity& e = body(s, m);
  const sh::Vec3f from = eye(e, kind(m));
  m.ideal_yaw = yaw_to(unpack(s.players[m.enemy].move.origin) - e.origin);
  e.angles.y = m.ideal_yaw;
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
    case Fire::rockets:
      fight_monster_launch(s, m.entity, SqShot::rocket, from, aim(s, m, a, from), a.damage, a.speed,
                           cause_of(m));
      return;
    case Fire::pellets: {
      const sh::Vec3f dir = aim(s, m, Attack{}, from);
      for (int i = 0; i < a.count; i++) {
        Attack spread = a;
        const sh::Vec3f d = aim(s, m, spread, from);
        fight_monster_bullet(s, m.entity, from, from + d * 8192, a.damage, a.kick, cause_of(m));
      }
      (void)dir;
      return;
    }
    case Fire::burst: {
      const sh::Vec3f d = aim(s, m, a, from);
      fight_monster_bullet(s, m.entity, from, from + d * 8192, a.damage, a.kick, cause_of(m));
      return;
    }
    case Fire::drain: {  // a tongue that holds while the line is clear
      if (!reaches(s, m, m.enemy, kShotMask & ~sh::kSqMonster)) return;
      const int dmg = m.burst_frame == 0 ? a.damage : a.damage_max;
      fight_hurt(s, m.enemy, -1, {}, dmg, 0, cause_of(m));
      return;
    }
    case Fire::club: {
      const sh::Vec3f at = unpack(s.players[m.enemy].move.origin);
      if (sh::length(at - e.origin) > kMelee + 16) return;  // swung at the air
      const int dmg = a.damage + static_cast<int>(random01(s) * (a.damage_max - a.damage + 1));
      sh::Vec3f dir = at - e.origin;
      sh::normalize(dir);
      fight_hurt(s, m.enemy, -1, dir, dmg, a.kick, cause_of(m));
      return;
    }
    default:
      return;
  }
}

// Starts attack `a`: its shots on the next frames.
void begin_attack(SequelServer& s, SequelMonster& m, const Attack& a) {
  m.attack = &a - &kind(m).missile[0] >= 0 && &a - &kind(m).missile[0] < 3
                 ? static_cast<int>(&a - &kind(m).missile[0]) + 1
                 : 0;  // 0 the melee
  m.burst_frame = 0;
  m.fire_at = s.level_time + 0.3;  // the wind-up
  double hold = 0.1;
  if (a.fire == Fire::burst)
    hold = a.burst_min + random01(s) * double(a.burst_max - a.burst_min);
  else if (a.fire == Fire::rockets)
    hold = 0.2 * (a.count - 1) + 0.1;
  else if (a.fire == Fire::drain)
    hold = 1;
  m.burst_until = m.fire_at + hold;
  m.busy_until = m.burst_until + 0.3;  // the recovery
}

const Attack& attack_of(const SequelMonster& m) {
  return m.attack == 0 ? kind(m).melee : kind(m).missile[m.attack - 1];
}

// The attack in progress, this frame.
void attacking(SequelServer& s, SequelMonster& m) {
  if (s.level_time + 0.001 < m.fire_at || s.level_time > m.burst_until + 0.001) return;
  const Attack& a = attack_of(m);
  const bool every_frame = a.fire == Fire::burst || a.fire == Fire::drain;
  const bool volley = a.fire == Fire::rockets && m.burst_frame % 2 == 0;
  if (m.burst_frame == 0 || every_frame || volley) shoot(s, m, a);
  m.burst_frame++;
}

// The attack chosen this frame, if any (sequel-monsters.md § Choosing to attack).
void choose(SequelServer& s, SequelMonster& m) {
  if (s.level_time + 0.001 < m.busy_until) return;
  SequelEntity& e = body(s, m);
  if (!reaches(s, m, m.enemy, kShotMask)) return;
  const Kind& k = kind(m);
  const Range r = range_to(e.origin, unpack(s.players[m.enemy].move.origin));
  const bool has_missile = k.missile[0].fire != Fire::none;
  if (r == Range::melee) {
    if (skill(s) == 0 && random01(s) < 0.75) return;
    if (k.melee.fire != Fire::none) {
      begin_attack(s, m, k.melee);
      return;
    }
  }
  if (!has_missile || r == Range::far || s.level_time + 0.001 < m.attack_finished) return;
  double chance = m.hold ? 0.4 : r == Range::melee ? 0.2 : r == Range::near ? 0.1 : 0.02;
  if (skill(s) == 0) chance *= 0.5;
  if (skill(s) >= 2) chance *= 2;
  if (random01(s) >= chance) return;
  int n = 0;
  while (n < 3 && k.missile[n].fire != Fire::none) n++;
  begin_attack(s, m, k.missile[std::min(n - 1, static_cast<int>(random01(s) * n))]);
  m.attack_finished = s.level_time + 2 * random01(s);
}

// ---- The frame ----

void think(SequelServer& s, SequelMonster& m) {
  SequelEntity& e = body(s, m);
  fall(s, m);
  if (m.enemy >= 0 && !hunted(s, m.enemy)) m.enemy = -1;  // dead or gone
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
      go_to(s, m, c.origin, kind(m).walk);
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
    go_to(s, m, spot, kind(m).run);
    return;
  }
  if (m.hold) {
    turn(e, kind(m), yaw_to(m.last_sight - e.origin));
    return;
  }
  const sh::Vec3f goal = m.last_sight;
  if (range_to(e.origin, goal) == Range::melee && kind(m).melee.fire == Fire::none) {
    turn(e, kind(m), yaw_to(goal - e.origin));
    return;
  }
  sh::Vec3f d = goal - e.origin;
  d.z = 0;
  if (sh::length(d) < 40) {  // close enough
    turn(e, kind(m), yaw_to(d));
    return;
  }
  go_to(s, m, goal, kind(m).run);
}

void die(SequelServer& s, SequelMonster& m, int attacker) {
  SequelEntity& e = body(s, m);
  m.dead = true;
  e.box = false;  // shots pass a corpse
  s.monsters_killed++;
  sound(s, m.entity, kind(m).death);
  if (!m.item.empty()) fight_drop_at(s, e.origin, m.item);
  map_use_targets(s, m.entity, m.enemy >= 0 ? m.enemy : attacker);
  if (e.health <= kind(m).gib) {
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
    if (std::find(s.sounds.begin(), s.sounds.end(), name) == s.sounds.end())
      s.sounds.emplace_back(name);
  };
  for (SequelEntity& e : s.entities) {
    const int k = kind_of(e.classname);
    if (k < 0 || e.gone || deathmatch(s)) continue;
    e.modelindex = model(kKinds[k].model);
    for (const char* name : {kKinds[k].sight, kKinds[k].pain, kKinds[k].death}) add_sound(name);
  }
}

void monster_spawn(SequelServer& s) {
  s.monsters.clear();
  s.monsters_total = s.monsters_killed = 0;
  s.sight_entity = -1;
  for (size_t i = 0; i < s.entities.size(); i++) {
    SequelEntity& e = s.entities[i];
    const int k = kind_of(e.classname);
    if (k < 0 || e.gone) continue;
    if (deathmatch(s)) {
      e.gone = true;
      continue;
    }
    SequelMonster m;
    m.entity = static_cast<int>(i);
    m.kind = k;
    const Kind& kd = kKinds[k];
    e.health = kd.health;
    e.max_health = kd.health;
    e.mins = kd.mins;
    e.maxs = kd.maxs;
    e.box = true;
    m.ambush = (e.spawnflags & 5) != 0;
    m.item = s.map->entities[size_t(e.block)].get("item");
    if ((e.spawnflags & 2) != 0) {  // waits hidden for a use
      e.hidden = true;
      e.box = false;
      m.waiting = true;
    }
    m.ideal_yaw = e.angles.y;
    m.corner = corner_named(s, e.target, "path_corner");
    m.combat = corner_named(s, e.target, "point_combat");
    if (m.corner >= 0)
      e.angles.y = m.ideal_yaw = yaw_to(s.entities[size_t(m.corner)].origin - e.origin);
    s.monsters.push_back(m);
    s.monsters_total++;
  }
  for (SequelMonster& m : s.monsters) {  // walkers to the floor
    if (kKinds[m.kind].flies || m.waiting) continue;
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
  for (SequelMonster& m : s.monsters) {
    if (m.dead) {
      fall(s, m);
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
  if (ent.gone || m.waiting || damage <= 0) return false;
  ent.health -= damage;
  // The knockback: 500 × damage / mass along the blow.
  if (sh::normalize(dir) > 0 && !kind(m).flies) {
    m.velocity = m.velocity + dir * (500.0f * float(damage) / float(std::max(50, kind(m).mass)));
    m.on_ground = false;
  }
  if (m.dead) {  // a corpse thrown apart
    if (ent.health <= kind(m).gib) {
      ent.gone = true;
      fight_gib(s, map_row(e), ent.origin);
    }
    return true;
  }
  if (ent.health <= 0) {
    die(s, m, attacker);
    return true;
  }
  if (hunted(s, attacker) && attacker != m.enemy) found(s, m, attacker, false);
  if (hunted(s, attacker) && m.enemy < 0) found(s, m, attacker, false);
  if (s.level_time + 0.001 >= m.pain_at + kPainEvery) {
    m.pain_at = s.level_time;
    m.pain_hold = s.level_time + 0.3;
    sound(s, e, kind(m).pain);
  }
  return true;
}

bool monster_use(SequelServer& s, int e, int activator) {
  const int i = monster_index(s, e);
  if (i < 0) return false;
  SequelMonster& m = s.monsters[size_t(i)];
  SequelEntity& ent = s.entities[size_t(e)];
  if (!m.waiting) return true;
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
  }
}

}  // namespace shooters_edge::server
