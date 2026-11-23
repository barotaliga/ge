// Ground Zero's monsters beside Quake II's (gz-monsters.md § 7): what their roster rows cannot
// say. The medic commander's call for reinforcements: a strength rolled about the skill picks a
// group of one, three or five guards; five spots round it (in front, else behind it, turned
// round) each show a grow for 0.3 s and then a guard stands there, hunting its enemy. Each one out
// takes one of its slots (3, 4, 6, 6 by skill) and gives it back at its death; they are not
// counted among the level's kills. The guards are spare bodies made with the map (eight a
// commander), each taking the class its call names.
//
// The turret (`monster_turret`): a gun on a floor (angle -1), a ceiling (-2) or a wall, its
// weapon by spawn flag (8 blaster, the default, 16 machine gun, 32 rocket). It opens (frames 2 to
// 8) once it has an enemy, then turns at most 45 degrees a frame in yaw and pitch toward him,
// held within its arc (a wall's 85 degrees either way; a floor's or a ceiling's above or below
// the level by 5), and fires when he is in sight by a chance a frame and his bearing is within
// about 11 degrees of its aim. A wall unit (flag 128) waits in its wall untouchable until used,
// then slides 32 out at its `speed` (15) and wakes. It feels no pain; it dies in a blast with
// four pieces of debris.
//
// The stalker (`monster_stalker`): a spider that walks a ceiling as a floor (spawn flag 8 starts
// it there, gravity reversed). On the ground, a third of the times it fires it first jumps: at
// its enemy (a pounce, within 450 and 45 degrees, at 400.1 then 600.1 u/s) when he is more than
// 256 off (or half the time nearer), else straight up, to a ceiling in reach (180 above) or off
// one to a floor (384 below). In the air it falls at 1.3 gravity for at most 3 s and fires a bolt
// (0.3 + 0.1 x skill a frame, 0.3 s apart). A blow over 10 makes it jump or flinch (3 s apart);
// below a quarter of its health it may feign death (0.2 x skill), twitch back to full health (1,
// 1, 2, 3 a frame by skill) and rise; a blow while it twitches raises it at once.
//
// The hidden brain (`turret_invisible_brain`) drives the turret_breach its `target` names: it
// starts hunting what its `killtarget` names, or with a targetname waits for a use (the user its
// enemy, the first shot 3 s on; another use switches it off). It aims the breach at the middle of
// its enemy's box while it has a clear line, and fires once it has seen him for its `delay`
// (3 - skill s by default), then that long and a second more between shots.
//
// ge:summary Ground Zero's monsters: commander's call, turret and its brain, stalker's ceilings
// ge:keywords q2 ground-zero rogue mission-pack monsters medic-commander reinforcements
// ge:keywords spawn-grow monster-slots turret-invisible-brain turret-breach stalker ceiling
// ge:keywords pounce feign-death
// ge:skill shooter
// ge:test tests/shooters_edge_q2_gz_monsters_test.cpp
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <string>
#include <string_view>

#include "brush/map.hpp"
#include "core/libm/libm.hpp"
#include "core/prng.hpp"
#include "shooters_edge/server/q2_fight.hpp"
#include "shooters_edge/server/q2_gibs.hpp"
#include "shooters_edge/server/q2_map.hpp"
#include "shooters_edge/server/q2_monster_ai.hpp"
#include "shooters_edge/server/q2_monsters.hpp"
#include "shooters_edge/server/q2_server.hpp"
#include "shooters_edge/server/q2_targets.hpp"

namespace shooters_edge::server::monster_ai {

namespace {

constexpr std::string_view kCommander = "monster_medic_commander";
constexpr int kSparesEach = 8;
constexpr const char* kCallSound = "medic_commander/monsterspawn1.wav";
constexpr uint32_t kSphereTrans = 1u << 28;  // the grow drawn as a see-through sphere
// The call's frames, a step a rules frame: 209..218, then 224..236 (219..223 skipped).
constexpr int kCallSteps = 23;
constexpr int kCallSoundAt = 9, kDecide = 10, kGrow = 11, kAppear = 14;
// The five spots, forward and right of its facing, 10 up.
constexpr float kSpots[5][2] = {{80, 0}, {40, 60}, {40, -60}, {0, 80}, {0, -80}};
// The i-th guard of a group is class (strength - kBack[i]) of kCommanderGroup.
constexpr int kBack[5] = {0, 2, 2, 4, 4};

bool commander(const Q2Monster& m) { return kind(m).classname == kCommander; }

int slots(const Q2Server& s) {
  constexpr int kSlots[4] = {3, 4, 6, 6};
  return kSlots[std::clamp(skill(s), 0, 3)];
}

int left(const Q2Server& s, const Q2Monster& m) { return slots(s) - m.called; }

int frame_of(int step) { return step < 10 ? 209 + step : 224 + (step - 10); }

int row_of(const Q2Server& s, const Q2Monster& m) {
  return static_cast<int>(&m - s.monsters.data());
}

// Whether a body of `kd`'s box fits at `p` (or dropped onto it from 32 above), over solid, dry,
// nearly flat ground within 256 below whose four corners are within 18 of the middle; the place
// it stands in `out`.
bool fits(const Q2Server& s, int skip, const Kind& kd, sh::Vec3f p, sh::Vec3f& out) {
  ge::classic::Q2Trace t = map_trace(s, p, kd.mins, kd.maxs, p, kMonsterMask, skip);
  if (t.start_solid || t.all_solid) {
    t = map_trace(s, p + sh::Vec3f{0, 0, 32}, kd.mins, kd.maxs, p, kMonsterMask, skip);
    if (t.start_solid || t.all_solid) return false;
    p = t.end;
  }
  const ge::classic::Q2Trace down =
      map_trace(s, p, kd.mins, kd.maxs, p - sh::Vec3f{0, 0, 256}, kMonsterMask, skip);
  if (down.fraction >= 1 || down.normal.z < 0.7f) return false;
  const sh::Vec3f foot{down.end.x, down.end.y, down.end.z + kd.mins.z - 1};
  if ((map_contents(s, foot) & (sh::kSqSolid | sh::kSqWater | sh::kSqSlime | sh::kSqLava)) != 0 &&
      (map_contents(s, foot) & sh::kSqSolid) == 0)
    return false;
  const float ground = down.end.z + kd.mins.z;
  for (const float cx : {kd.mins.x, kd.maxs.x})
    for (const float cy : {kd.mins.y, kd.maxs.y}) {
      const sh::Vec3f top{down.end.x + cx, down.end.y + cy, ground + 1};
      const ge::classic::Q2Trace c =
          map_trace(s, top, {}, {}, top - sh::Vec3f{0, 0, 256}, kMonsterMask, skip);
      if (c.fraction >= 1 || std::abs(c.end.z - ground) > 18) return false;
    }
  out = down.end;
  return true;
}

// The spot of the i-th position round commander `e`, behind it when `back`.
sh::Vec3f spot(const Q2Entity& e, int i, bool back) {
  const float sign = back ? -1.0f : 1.0f;
  const sh::Vec3f f = forward_of(e.angles.y) * (kSpots[i][0] * sign);
  const sh::Vec3f r = forward_of(e.angles.y - 90) * (kSpots[i][1] * sign);
  return e.origin + f + r + sh::Vec3f{0, 0, 10};
}

// The enemy the guards take: the commander's (the one it had while it heals).
int their_enemy(const Q2Server& s, const Q2Monster& c) {
  const int foe = c.patient >= 0 ? c.old_enemy : c.enemy;
  return foe_alive(s, foe) ? foe : -1;
}

// The decision at frame 224: the group by strength, the spots in front (else behind); true when
// a spot fits at least one.
bool decide(Q2Server& s, Q2Monster& c) {
  int strength = skill(s);
  const double roll = random01(s);
  constexpr double kOdds[7] = {0.05, 0.10, 0.15, 0.40, 0.15, 0.10, 0.05};
  double sum = 0;
  for (int i = 0; i < 7; i++) {
    sum += kOdds[i];
    if (roll < sum || i == 6) {
      strength += i - 3;
      break;
    }
  }
  strength = std::clamp(strength, 0, 6);
  const int count = strength <= 2 ? 1 : strength <= 4 ? 3 : 5;
  const Q2Entity& e = body(s, c);
  int spots_ok = 0;
  for (const bool back : {false, true}) {
    spots_ok = 0;
    for (int i = 0; i < count; i++) {
      const int k = kind_of(s, kCommanderGroup[std::max(0, strength - kBack[i])]);
      sh::Vec3f at;
      if (k >= 0 && fits(s, c.entity, kKinds[k], spot(e, i, back), at)) spots_ok++;
    }
    if (spots_ok > 0) {
      c.call_back = back;
      if (back) c.ideal_yaw = e.angles.y + 180;
      // The group, kept as the spares' classes: one a spot that fits, shown at the grow.
      for (int i = 0; i < count; i++) {
        const int k = kind_of(s, kCommanderGroup[std::max(0, strength - kBack[i])]);
        sh::Vec3f at;
        if (k < 0 || !fits(s, c.entity, kKinds[k], spot(e, i, back), at)) continue;
        Q2Monster* m = free_spare(s, c);
        if (m == nullptr) break;
        become(s, *m, k, row_of(s, c), at);
        m->grow_until = -1;  // chosen; it grows once the commander faces the spots
        m->ideal_yaw = e.angles.y + (back ? 180.0f : 0.0f);
      }
      return true;
    }
  }
  return false;
}

// The chosen guards show their grow now.
void grow(Q2Server& s, const Q2Monster& c) {
  const int row = row_of(s, c);
  for (Q2Monster& m : s.monsters) {
    if (m.commander != row || m.grow_until >= 0 || !m.spare) continue;
    m.grow_until = s.level_time + 0.3;
    Q2Entity& e = body(s, m);
    e.angles = {0, float(random01(s) * 360.0), 0};
  }
}

// The grown guards come, where they still fit; the rest go back to waiting.
void appear(Q2Server& s, Q2Monster& c) {
  const int row = row_of(s, c);
  const int foe = their_enemy(s, c);
  for (Q2Monster& m : s.monsters) {
    if (m.commander != row || m.grow_until == 0 || !m.spare) continue;
    Q2Entity& e = body(s, m);
    m.grow_until = 0;
    sh::Vec3f at;
    if (!fits(s, c.entity, kind(m), e.origin, at)) continue;
    m.spare = m.waiting = false;
    e.origin = at;
    e.angles = {0, m.ideal_yaw, 0};
    e.hidden = false;
    e.box = true;
    m.pose.last = at;
    m.on_ground = on_floor(s, m);
    c.called++;
    if (foe != -1) found(s, m, foe, false);
  }
}

// A call cut short: the chosen and growing guards back to waiting.
void call_off(Q2Server& s, Q2Monster& c) {
  const int row = row_of(s, c);
  for (Q2Monster& m : s.monsters)
    if (m.commander == row && m.spare) m.grow_until = 0;
  c.call = -1;
}

// ---- The turret ----

constexpr std::string_view kTurret = "monster_turret";
constexpr int kBlasterFlag = 8, kGunFlag = 16, kRocketFlag = 32, kBeamFlag = 64, kWallFlag = 128;
constexpr double kDegrees = 57.29577951308232;
constexpr const char* kDebris = "models/objects/debris1/tris.md2";

enum Stage : int {
  kStand = 0,     // no enemy: frames 0 and 1
  kOpen = 2,      // opening: frames 2 to 8, one a stage
  kReady = 9,     // aiming: frames 8 and 9
  kFire = 10,     // the fire run: 10 to 13, firing on 10
  kInWall = -2,   // a wall unit waiting for its use
  kSliding = -1,  // a wall unit coming out
};

enum class Gun : uint8_t { blaster, machine_gun, rocket };

bool turret(const Q2Monster& m) { return kind(m).classname == kTurret; }

// Its weapon: blaster before machine gun before rocket; none (or the heat beam) the blaster.
Gun gun_of(int flags) {
  if ((flags & (kBlasterFlag | kBeamFlag)) != 0) return Gun::blaster;
  if ((flags & kGunFlag) != 0) return Gun::machine_gun;
  if ((flags & kRocketFlag) != 0) return Gun::rocket;
  return Gun::blaster;
}

float wrap(float a) {
  while (a > 180) a -= 360;
  while (a < -180) a += 360;
  return a;
}

// The pitch and yaw from `from` toward `at` (pitch positive down).
sh::Vec3f bearing(sh::Vec3f from, sh::Vec3f at) {
  const sh::Vec3f d = at - from;
  const double flat = double(sh::length(sh::Vec3f{d.x, d.y, 0}));
  return {static_cast<float>(-ge::core::libm::atan2(double(d.z), flat) * kDegrees),
          static_cast<float>(ge::core::libm::atan2(double(d.y), double(d.x)) * kDegrees), 0};
}

// Angles `a` held within the turret's arc: on a wall within 85 of its facing in yaw and of the
// level in pitch; on a floor 5 or more above the level, on a ceiling 5 or more below it.
sh::Vec3f in_arc(const Q2Monster& m, sh::Vec3f a) {
  if (m.mount.x < -45) {  // a floor's, facing up
    a.x = std::min(a.x, -5.0f);
  } else if (m.mount.x > 45) {  // a ceiling's, facing down
    a.x = std::max(a.x, 5.0f);
  } else {
    a.x = std::clamp(wrap(a.x), -85.0f, 85.0f);
    a.y = m.mount.y + std::clamp(wrap(a.y - m.mount.y), -85.0f, 85.0f);
  }
  return a;
}

// Where it aims at foe `foe`: his eye (a player's), else his origin; led by his motion within
// 512 for the blaster and the rocket, with a chance by skill.
sh::Vec3f turret_mark(Q2Server& s, const Q2Monster& m, int foe, bool lead) {
  const Q2Entity& e = body(s, m);
  sh::Vec3f at = foe >= 0 ? foe_eye(s, foe) : foe_at(s, foe);
  const float dist = sh::length(at - e.origin);
  if (lead && dist < 512 && gun_of(e.spawnflags) != Gun::machine_gun) {
    constexpr double kLead[4] = {0.5, 0.6, 0.7, 0.8};
    if (random01(s) < kLead[std::clamp(skill(s), 0, 3)])
      at = at + foe_velocity(s, foe) * (dist / 1000.0f);
  }
  return at;
}

// A step of the aim toward its enemy: at most 45 in yaw and in pitch, within its arc.
void turret_aim(Q2Server& s, Q2Monster& m) {
  Q2Entity& e = body(s, m);
  if (m.enemy == -1) return;
  const sh::Vec3f want = in_arc(m, bearing(e.origin, turret_mark(s, m, m.enemy, false)));
  e.angles.x += std::clamp(wrap(want.x - e.angles.x), -45.0f, 45.0f);
  e.angles.y += std::clamp(wrap(want.y - e.angles.y), -45.0f, 45.0f);
  e.angles = in_arc(m, e.angles);
}

// Its shot, when he is near its aim and the line is clear.
void turret_fire(Q2Server& s, Q2Monster& m) {
  Q2Entity& e = body(s, m);
  if (!foe_alive(s, m.enemy)) return;
  const Gun gun = gun_of(e.spawnflags);
  const auto ax = sh::angle_vectors(e.angles);
  sh::Vec3f to_him = foe_at(s, m.enemy) - e.origin;
  if (sh::normalize(to_him) <= 0 || sh::dot(to_him, ax.forward) < 0.98f) return;
  const sh::Vec3f start = e.origin + ax.forward * 12.0f;
  const sh::Vec3f at = turret_mark(s, m, m.enemy, true);
  const ge::classic::Q2Trace t = map_trace(s, start, {}, {}, at, kShotMask, m.entity);
  const int foe_row = m.enemy >= 0 ? m.enemy + 1 : map_row(foe_entity(m.enemy));
  if (t.fraction < 1 && t.entity != 0 && t.entity != foe_row) return;  // something in the way
  sh::Vec3f dir = at - start;
  const float dist = sh::normalize(dir);
  if (gun == Gun::rocket && t.fraction < 1 && t.fraction * dist < 72) return;
  const int sk = std::clamp(skill(s), 0, 3);
  switch (gun) {
    case Gun::blaster: {
      sound(s, m.entity, "soldier/solatck2.wav");
      const float speed = sk == 0 ? 600.0f : sk == 1 ? 800.0f : 1000.0f;
      fight_monster_launch(s, m.entity, SqShot::bolt, start, dir, 20, speed, cause_of(m));
      break;
    }
    case Gun::machine_gun: {
      sound(s, m.entity, "infantry/infatck1.wav");
      const auto d = sh::angle_vectors(bearing(start, at));
      const sh::Vec3f end =
          start + d.forward * 8192.0f + d.right * (crandom(s) * 300) + d.up * (crandom(s) * 500);
      fight_monster_bullet(s, m.entity, start, end, 4, 0, cause_of(m));
      break;
    }
    case Gun::rocket: {
      sound(s, m.entity, "chick/chkatck2.wav");
      float speed = 550;
      if (sk == 2) speed = 550 + float(random01(s)) * 200;
      if (sk == 3) speed = 650 + float(random01(s)) * 200;
      fight_monster_launch(s, m.entity, SqShot::rocket, start, dir, 50, speed, cause_of(m));
      break;
    }
  }
}

bool turret_think(Q2Server& s, Q2Monster& m) {
  Q2Entity& e = body(s, m);
  if (m.stage == kInWall) return true;
  if (m.stage == kSliding) {  // out of its wall at its speed, then awake
    sh::Vec3f d = m.goal - e.origin;
    const float left_to_go = sh::normalize(d);
    float speed = 15;
    if (e.block >= 0 && size_t(e.block) < s.map->entities.size()) {
      const std::string& key = s.map->entities[size_t(e.block)].get_any_case("speed");
      if (!key.empty()) speed = std::strtof(key.c_str(), nullptr);
    }
    const float step_len = speed * kStep;
    e.origin = left_to_go <= step_len ? m.goal : e.origin + d * step_len;
    if (left_to_go <= step_len) m.stage = kStand;
    m.turret_frame = 0;
    return true;
  }
  const int sk = std::clamp(skill(s), 0, 3);
  if (m.enemy != -1 && !foe_alive(s, m.enemy)) m.enemy = -1;
  if (m.enemy == -1) notice(s, m);
  if (m.enemy == -1 && m.stage < kReady) {  // standing closed
    m.stage = kStand;
    m.turret_frame = int(s.frame_number & 1);
    return true;
  }
  if (m.stage < kReady) {  // opening, a frame a step; it neither aims nor fires till it is open
    m.stage = m.stage < kOpen ? kOpen : m.stage + 1;
    m.turret_frame = std::min(m.stage, 8);
    if (m.stage > 8) m.stage = kReady;
    return true;
  }
  turret_aim(s, m);
  if (m.stage >= kFire) {
    if (m.stage == kFire) turret_fire(s, m);
    m.turret_frame = m.stage;
    if (++m.stage > 13) m.stage = kReady;
    return true;
  }
  m.turret_frame = 8 + int(s.frame_number & 1);
  if (m.enemy == -1 || s.level_time + 0.001 < m.attack_finished) return true;
  const bool in_sight = m.enemy >= 0 ? reaches(s, m, m.enemy, kOpaque) : true;
  if (!in_sight) return true;
  const Gun gun = gun_of(e.spawnflags);
  double chance = gun == Gun::rocket ? 0.1 : gun == Gun::blaster ? 0.35 : 0.5;
  if (sk == 0) chance *= 0.5;
  if (sk >= 2) chance *= 2;
  if (random01(s) >= chance) return true;
  m.stage = kFire;
  const double wait = gun == Gun::rocket    ? 1.8 - 0.2 * sk
                      : gun == Gun::blaster ? 1.2 - 0.2 * sk
                                            : 0.8 - 0.1 * sk;
  m.attack_finished = s.level_time + wait;
  return true;
}

// ---- The stalker ----

constexpr std::string_view kStalker = "monster_stalker";
constexpr int kOnRoof = 8;

enum Stalk : int {
  kStalking = 0,
  kAir = 1,        // in a jump: 1.3 gravity, till it lands or `stage_until`
  kFeignFall = 2,  // falling into a false death: frames 70 to 78
  kTwitch = 3,     // healing where it lies: frames 79 to 88, over and over
  kRise = 4,       // getting up: frames 89 to 92
};

bool stalker(const Q2Monster& m) { return kind(m).classname == kStalker; }

bool on_ceiling(const Q2Monster& m) { return m.gravity < 0; }

// Whether a flat world surface facing it is within `reach` straight up (from a floor) or down
// (from a ceiling): its box swept there meets solid facing back, and its four corners meet it
// within 8 of the same height.
bool surface_in_reach(const Q2Server& s, const Q2Monster& m, float reach) {
  const Q2Entity& e = s.entities[size_t(m.entity)];
  const float way = on_ceiling(m) ? -1.0f : 1.0f;
  const ge::classic::Q2Trace t = map_trace(
      s, e.origin, e.mins, e.maxs, e.origin + sh::Vec3f{0, 0, reach * way}, sh::kSqSolid, m.entity);
  if (t.fraction >= 1 || t.entity != 0 || t.normal.z * way > -0.9f) return false;
  const float face = way > 0 ? t.end.z + e.maxs.z : t.end.z + e.mins.z;
  for (const float cx : {e.mins.x, e.maxs.x})
    for (const float cy : {e.mins.y, e.maxs.y}) {
      const sh::Vec3f from{e.origin.x + cx, e.origin.y + cy, e.origin.z};
      const ge::classic::Q2Trace c = map_trace(
          s, from, {}, {}, sh::Vec3f{from.x, from.y, face + 8 * way}, sh::kSqSolid, m.entity);
      if (c.fraction >= 1 || c.end.z - face > 8 || face - c.end.z > 8) return false;
    }
  return true;
}

void airborne(Q2Server& s, Q2Monster& m, sh::Vec3f velocity) {
  m.velocity = velocity;
  m.on_ground = false;
  m.gravity = on_ceiling(m) ? -1.3f : 1.3f;
  m.stage = kAir;
  m.stage_until = s.level_time + 3;
}

// The jump straight up: off a floor with a ceiling in reach it turns to the ceiling; off a
// ceiling with a floor in reach it lets go.
void jump_up(Q2Server& s, Q2Monster& m) {
  if (on_ceiling(m)) {
    if (!surface_in_reach(s, m, 384)) return;
    m.gravity = 1;
    airborne(s, m, {});
    return;
  }
  const sh::Vec3f v{crandom(s) * 5, crandom(s) * 5, 400};
  if (surface_in_reach(s, m, 180)) m.gravity = -1;
  airborne(s, m, v);
}

// The pounce at its enemy: not from a ceiling, him on dry ground, within 450 and 45 degrees of
// its facing; 32 above him when he stands 32 higher, the high arc then (or when the straight line
// is blocked by more than him), at 400.1 or else 600.1 u/s under a gravity of 800. False when
// no arc reaches.
bool pounce(Q2Server& s, Q2Monster& m) {
  if (on_ceiling(m) || m.enemy < 0 || m.enemy >= sh::kMaxClients) return false;
  Q2Entity& e = body(s, m);
  const Q2Player& p = s.players[m.enemy];
  if (!p.on_ground || p.water_level > 0) return false;
  sh::Vec3f at = foe_at(s, m.enemy);
  if (wet(s, at)) return false;
  sh::Vec3f to = at - e.origin;
  const float flat = sh::length(sh::Vec3f{to.x, to.y, 0});
  if (sh::length(to) > 450 || flat <= 0) return false;
  float off = yaw_to(to) - e.angles.y;
  while (off > 180) off -= 360;
  while (off < -180) off += 360;
  if (off > 45 || off < -45) return false;
  e.angles.y = m.ideal_yaw = yaw_to(to);
  bool high = false;
  if (at.z >= e.origin.z + 32) {
    at.z += 32;
    high = true;
  }
  const ge::classic::Q2Trace line = map_trace(s, e.origin, {}, {}, at, kShotMask, m.entity);
  if (line.fraction < 1 && line.entity != m.enemy + 1) high = true;
  to = at - e.origin;
  constexpr float kG = 800;
  for (const float v : {400.1f, 600.1f}) {
    const float v2 = v * v;
    const float disc = v2 * v2 - kG * (kG * flat * flat + 2 * to.z * v2);
    if (disc < 0) continue;
    const float root = std::sqrt(disc);
    const float tan_a = (v2 + (high ? root : -root)) / (kG * flat);
    const float cos_a = 1 / std::sqrt(1 + tan_a * tan_a);
    const sh::Vec3f fwd = sh::Vec3f{to.x, to.y, 0} * (1 / flat);
    sh::Vec3f vel = fwd * (v * cos_a);
    vel.z = v * tan_a * cos_a + 0.5f * kG * kStep;
    airborne(s, m, vel);
    return true;
  }
  return false;
}

// Its pose by hand while it lies or rises.
void show(Q2Monster& m, int frame, SqSpan run) {
  m.pose.shown = frame;
  m.pose.shown_run = run;
}

bool stalker_think(Q2Server& s, Q2Monster& m) {
  Q2Entity& e = body(s, m);
  const Kind& k = kind(m);
  const int sk = std::clamp(skill(s), 0, 3);
  switch (m.stage) {
    case kAir: {
      if (m.on_ground || s.level_time >= m.stage_until) {  // down: its gravity its own again
        m.stage = kStalking;
        m.gravity = m.gravity < 0 ? -1.0f : 1.0f;
        return false;
      }
      if (m.enemy != -1 && foe_alive(s, m.enemy) && s.level_time + 0.001 >= m.attack_finished &&
          random01(s) < 0.3 + 0.1 * sk) {
        shoot(s, m, k.missile[0], 0);
        m.attack_finished = s.level_time + 0.3;
      }
      show(m, 47, {42, 48});
      return true;
    }
    case kFeignFall: {
      const int n = static_cast<int>((s.level_time - m.stage_until) / kStep + 0.001);
      show(m, std::min(70 + n, 78), {70, 78});
      if (n >= 8) m.stage = kTwitch;
      return true;
    }
    case kTwitch: {
      constexpr int kHeal[4] = {1, 1, 2, 3};
      e.health = std::min(e.max_health, e.health + kHeal[sk]);
      show(m, 79 + int(s.frame_number % 10), {79, 88});
      if (e.health >= e.max_health) {
        m.stage = kRise;
        m.stage_until = s.level_time;
      }
      return true;
    }
    case kRise: {
      const int n = static_cast<int>((s.level_time - m.stage_until) / kStep + 0.001);
      show(m, std::min(89 + n, 92), {89, 92});
      if (n >= 3) m.stage = kStalking;
      return true;
    }
    default:
      return false;
  }
}

bool stalker_pain(Q2Server& s, Q2Monster& m, int damage) {
  Q2Entity& e = body(s, m);
  if (m.stage == kTwitch) {  // hit while it twitches: up at once
    m.stage = kRise;
    m.stage_until = s.level_time;
    return true;
  }
  if (m.stage == kFeignFall || m.stage == kRise || m.stage == kAir || skill(s) == 3) return true;
  if (e.health < e.max_health / 4 && random01(s) < 0.2 * skill(s) &&
      (!on_ceiling(m) || surface_in_reach(s, m, 384))) {  // a false death, off its ceiling first
    m.gravity = 1;
    m.on_ground = false;
    m.hold = true;
    m.stage = kFeignFall;
    m.stage_until = s.level_time;
    return true;
  }
  if (damage <= 10 || s.level_time + 0.001 < m.pain_at + kPainEvery) return true;
  m.pain_at = s.level_time;
  sound(s, m.entity, kind(m).pain);
  if (m.on_ground && random01(s) < 0.5) {
    jump_up(s, m);
    return true;
  }
  m.pose.cue_pain = true;
  m.pose.pain_run = {66, 69};
  m.pain_hold = s.level_time + 0.4;
  m.busy_until = m.burst_until = std::min(m.burst_until, s.level_time);
  return true;
}

}  // namespace

Q2Monster* free_spare(Q2Server& s, const Q2Monster& c) {
  const int block = -2 - c.entity;
  for (Q2Monster& m : s.monsters) {
    const Q2Entity& e = s.entities[size_t(m.entity)];
    if (e.block != block) continue;
    if ((m.spare && m.waiting && m.grow_until == 0) || (m.dead && e.gone)) return &m;
  }
  return nullptr;
}

void become(Q2Server& s, Q2Monster& m, int k, int from, sh::Vec3f at) {
  Q2Entity& e = body(s, m);
  const Kind& kd = kKinds[k];
  Q2Monster fresh;
  fresh.entity = m.entity;
  fresh.kind = k;
  fresh.spare = fresh.waiting = true;
  fresh.commander = from;
  fresh.screen = kd.screen ? 100 : 0;
  fresh.shield = kd.shield_flag == 0 ? kd.shield : 0;
  m = fresh;
  e.classname = kd.classname;
  e.health = e.max_health = kd.health;
  e.mins = kd.mins;
  e.maxs = kd.maxs;
  e.origin = at;
  e.gone = e.corpse = false;
  e.hidden = true;
  e.box = false;
  const auto found_model = std::find(s.models.begin(), s.models.end(), kd.model);
  e.modelindex = static_cast<int>(found_model - s.models.begin());
  m.pose.last = at;
}

void gz_spares(Q2Server& s) {
  gz_boss_spares(s);
  const size_t mapped = s.entities.size();
  for (size_t i = 0; i < mapped; i++) {
    const Q2Entity& c = s.entities[i];
    if (c.gone || c.classname != kCommander || kind_of(s, c.classname) < 0) continue;
    Q2Entity spare = c;
    spare.classname = kCommanderGroup[0];
    spare.targetname.clear();
    spare.target.clear();
    spare.spawnflags = 0;
    spare.block = -2 - static_cast<int>(i);
    for (int n = 0; n < kSparesEach; n++) s.entities.push_back(spare);  // `c` may move
  }
}

void gz_begin_call(Q2Server& s, Q2Monster& m) {
  m.call = 0;
  m.call_back = false;
  m.busy_until = s.level_time + double(kCallSteps) * kStep;
}

bool gz_call_instead(Q2Server& s, Q2Monster& m, bool melee) {
  if (stalker(m)) {  // a third of the time on the ground it jumps first, and fires all the same
    if (m.on_ground && random01(s) < 0.33) {
      const float dist = sh::length(foe_at(s, m.enemy) - body(s, m).origin);
      if (!((dist > 256 || random01(s) < 0.5) && pounce(s, m))) jump_up(s, m);
    }
    return false;
  }
  if (!commander(m) || melee || left(s, m) <= 2 || random01(s) >= 0.8) return false;
  gz_begin_call(s, m);
  return true;
}

bool gz_cable_instead(Q2Server& s, Q2Monster& m) {
  if (!commander(m) || left(s, m) <= 2 || random01(s) >= 0.2) return false;
  gz_begin_call(s, m);
  return true;
}

bool gz_think(Q2Server& s, Q2Monster& m) {
  if (m.dead) return false;
  if (gz_boss(m)) return gz_boss_think(s, m);
  if (turret(m)) return turret_think(s, m);
  if (stalker(m)) return stalker_think(s, m);
  if (!commander(m)) return false;
  Q2Entity& e = body(s, m);
  const Kind& k = kind(m);
  if (m.call < 0) {
    // An enemy (a player) it cannot see: it calls, while it has more than two slots.
    if (m.enemy >= 0 && m.enemy < sh::kMaxClients && m.patient < 0 && left(s, m) > 2 &&
        s.level_time + 0.001 >= m.busy_until && s.level_time + 0.001 >= m.pain_hold &&
        !sees(s, m, m.enemy))
      gz_begin_call(s, m);
    if (m.call < 0) return false;
  }
  if (s.level_time + 0.001 < m.pain_hold) {  // a flinch cuts the call
    call_off(s, m);
    m.busy_until = s.level_time;
    return false;
  }
  const int foe = their_enemy(s, m);
  if (m.call < kDecide && foe != -1) turn(e, k, yaw_to(foe_at(s, foe) - e.origin));
  if (m.call == kCallSoundAt) sound(s, m.entity, kCallSound);
  if (m.call == kDecide && !decide(s, m)) m.call = kAppear;  // nowhere: straight on to 229
  if (m.call == kGrow) {
    if (m.call_back) {  // the spots are behind it: it turns round first, holding this frame
      turn(e, k, m.ideal_yaw);
      float off = e.angles.y - m.ideal_yaw;
      while (off > 180) off -= 360;
      while (off < -180) off += 360;
      if (off > 0.1f || off < -0.1f) {
        m.pose.shown = frame_of(m.call);
        m.pose.shown_run = {209, 236};
        m.busy_until = s.level_time + float(kCallSteps - m.call) * kStep;
        return true;
      }
      m.call_back = false;
    }
    grow(s, m);
  }
  if (m.call == kAppear) appear(s, m);
  m.pose.shown = frame_of(m.call);
  m.pose.shown_run = {209, 236};
  m.busy_until = s.level_time + float(kCallSteps - m.call) * kStep;
  if (++m.call >= kCallSteps) call_off(s, m);
  return true;
}

void gz_spawned(Q2Server& s, Q2Monster& m) {
  if (stalker(m) && (body(s, m).spawnflags & kOnRoof) != 0) m.gravity = -1;  // on its ceiling
  if (gz_boss(m)) gz_boss_spawned(s, m);
  if (!turret(m)) return;
  Q2Entity& e = body(s, m);
  m.gravity = 0;
  // Its mounting by its angle: -1 the floor (facing up), -2 the ceiling (down), else a wall.
  if (e.angles.y == -1)
    m.mount = {-90, 0, 0};
  else if (e.angles.y == -2)
    m.mount = {90, 0, 0};
  else
    m.mount = {0, e.angles.y, 0};
  e.angles = m.mount;
  e.origin = e.origin + sh::angle_vectors(m.mount).forward * 2.0f;
  m.ideal_yaw = m.mount.y;
  if ((e.spawnflags & kWallFlag) != 0) {
    if (e.targetname.empty()) {  // a wall unit nothing can use: gone
      e.gone = true;
      return;
    }
    m.stage = kInWall;
    m.goal = e.origin + sh::angle_vectors(m.mount).forward * 32.0f;
  }
}

bool gz_use(Q2Server& s, Q2Monster& m) {
  if (!turret(m) || m.stage != kInWall) return false;
  m.stage = kSliding;
  sound(s, m.entity, "world/dr_short.wav");
  return true;
}

bool gz_untouchable(const Q2Server& s, const Q2Monster& m) {
  return (turret(m) && m.stage < 0) || gz_boss_untouchable(s, m);
}

bool gz_pain(Q2Server& s, Q2Monster& m, int damage) {
  if (turret(m)) return true;
  if (gz_boss(m)) {
    gz_boss_pain(s, m, damage);
    return true;
  }
  if (stalker(m)) return stalker_pain(s, m, damage);
  return false;
}

void gz_died(Q2Server& s, Q2Monster& m) {
  if (gz_boss(m)) gz_boss_died(s, m);
  if (stalker(m)) {  // it always falls, off its ceiling too
    m.gravity = 1;
    m.on_ground = false;
    m.stage = kStalking;
  }
  if (turret(m)) {  // four pieces of debris from just in front of it
    const Q2Entity& e = body(s, m);
    const auto at = std::find(s.models.begin(), s.models.end(), kDebris);
    const sh::Vec3f from = e.origin + sh::angle_vectors(e.angles).forward;
    if (at != s.models.end())
      for (const float factor : {1.0f, 2.0f, 1.0f, 2.0f})
        gibs_debris(s, static_cast<int>(at - s.models.begin()), from, {}, factor);
  }
  if (m.commander < 0 || size_t(m.commander) >= s.monsters.size()) return;
  Q2Monster& c = s.monsters[size_t(m.commander)];
  c.called = std::max(0, c.called - 1);
}

bool gz_counted(const Q2Monster& m) { return m.commander < 0 && !m.revived; }

void gz_row(const Q2Server& s, const Q2Monster& m, ge::shooter::WorldEntity& r) {
  if (gz_boss(m)) {
    gz_boss_row(s, m, r);
    return;
  }
  if (turret(m)) {  // its aim in pitch too, its frame, its weapon's skin
    const Q2Entity& e = s.entities[size_t(m.entity)];
    r.angles = e.angles;
    r.frame = m.turret_frame;
    r.skin = (e.spawnflags & kGunFlag) != 0 ? 1 : (e.spawnflags & kRocketFlag) != 0 ? 2 : 0;
    return;
  }
  if (stalker(m) && on_ceiling(m)) r.angles.z = 180;  // upside down on its ceiling
  if (m.grow_until <= 0 || s.level_time >= m.grow_until) return;
  const auto at = std::find(s.models.begin(), s.models.end(), kSpawnGrow);
  r.free = false;
  r.modelindex = static_cast<int>(at - s.models.begin());  // the grow (q2_monster_rows.cpp)
  r.frame = 0;
  r.effects = kSphereTrans;
  r.mins = r.maxs = {};
}

}  // namespace shooters_edge::server::monster_ai

namespace shooters_edge::server {

using namespace monster_ai;

namespace {

constexpr std::string_view kBrain = "turret_invisible_brain";

// Where foe `foe` (a slot, or any entity as -2 - its index) is: its box's middle; false when it is
// gone.
bool brain_mark(const Q2Server& s, int foe, sh::Vec3f& at) {
  if (foe >= 0) {
    if (!hunted(s, foe)) return false;
    const Q2Player& p = s.players[foe];
    at = unpack(p.move.origin) + (p.mins + p.maxs) * 0.5f;
    return true;
  }
  const int e = foe_entity(foe);
  if (e < 0 || size_t(e) >= s.entities.size()) return false;
  const Q2Entity& b = s.entities[size_t(e)];
  if (b.gone) return false;
  if (const Q2Monster* m = foe_monster(s, foe); m != nullptr && m->dead) return false;
  at = b.origin + (b.mins + b.maxs) * 0.5f;
  return true;
}

// Whether the line from `from` to `at` is clear to foe `foe` (it meets nothing, or him), entity
// `skip` (the breach it fires from) passed through.
bool clear_to(const Q2Server& s, sh::Vec3f from, sh::Vec3f at, int foe, int skip) {
  const ge::classic::Q2Trace t = map_trace(s, from, {}, {}, at, kShotMask, skip);
  if (t.fraction >= 1) return true;
  return foe >= 0 ? t.entity == foe + 1 : false;
}

}  // namespace

bool gz_brain_spawn(Q2Server& s, int i) {
  Q2Entity& e = s.entities[size_t(i)];
  if (e.classname != kBrain || s.opt.pack != Q2Pack::ground_zero) return false;
  e.hidden = true;
  e.box = false;
  e.aim = -1;     // its enemy (a slot, or an entity as -2 - its index)
  e.until = -1;   // since when it has seen him
  e.tick_at = 0;  // its next shot's earliest time
  e.state = 0;    // 1 once it has looked for its first enemy
  e.on = e.targetname.empty();
  e.delay = 0;
  if (e.block >= 0 && size_t(e.block) < s.map->entities.size()) {
    const ge::brush::Entity& keys = s.map->entities[size_t(e.block)];
    e.killtarget = keys.get_any_case("killtarget");
    const std::string& delay = keys.get_any_case("delay");
    if (!delay.empty()) e.delay = std::strtof(delay.c_str(), nullptr);
  }
  if (e.delay <= 0) e.delay = float(3 - std::clamp(skill(s), 0, 3));
  return true;
}

bool gz_brain_use(Q2Server& s, int i, int activator) {
  Q2Entity& e = s.entities[size_t(i)];
  if (e.classname != kBrain || s.opt.pack != Q2Pack::ground_zero) return false;
  if (e.on) {  // a second use switches it off
    e.on = false;
    return true;
  }
  e.on = true;
  if (e.aim == -1 && activator >= 0) e.aim = activator;
  e.tick_at = s.level_time + 3;
  return true;
}

void gz_brains(Q2Server& s) {
  if (s.opt.pack != Q2Pack::ground_zero) return;
  for (size_t n = 0; n < s.entities.size(); n++) {
    Q2Entity& e = s.entities[n];
    if (e.gone || !e.on || e.classname != kBrain) continue;
    const int i = static_cast<int>(n);
    if (e.state == 0) {  // linked: its first enemy what its killtarget names
      e.state = 1;
      for (size_t t = 0; t < s.entities.size() && e.aim == -1; t++)
        if (!e.killtarget.empty() && s.entities[t].targetname == e.killtarget)
          e.aim = foe_of_entity(static_cast<int>(t));
    }
    const int breach = targets_breach_of(s, e);
    if (breach < 0) continue;
    sh::Vec3f at;
    if (e.aim != -1 && !brain_mark(s, e.aim, at)) e.aim = -1;
    if (e.aim == -1) {  // one it sees
      for (int p = 0; p < sh::kMaxClients && e.aim == -1; p++)
        if (targetable(s, p) && brain_mark(s, p, at) &&
            clear_to(s, targets_breach_muzzle(s, breach), at, p, breach))
          e.aim = p;
    }
    if (e.aim == -1 || !brain_mark(s, e.aim, at) ||
        !clear_to(s, targets_breach_muzzle(s, breach), at, e.aim, breach)) {
      e.until = -1;
      continue;
    }
    targets_breach_aim(s, breach, at);
    if (e.until < 0) e.until = s.level_time;
    if (s.level_time + 0.001 < e.until + e.delay || s.level_time + 0.001 < e.tick_at) continue;
    e.tick_at = s.level_time + e.delay + 1;
    const int driver = kind_of(s, "turret_driver");
    targets_breach_fire(s, breach, i, kSqByMonster + std::max(driver, 0));
  }
}

}  // namespace shooters_edge::server

