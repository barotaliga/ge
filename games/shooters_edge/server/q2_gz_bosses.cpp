// Ground Zero's bosses (gz-monsters.md § 7.5 to 7.7): what their roster rows cannot say. Each
// plays its attacks, pains and deaths by hand, a model frame a rules frame, and chooses its next
// move by its own table; between moves the base game's run carries it (q2_monsters.cpp).
//
// - **The carrier** (`monster_carrier`): a gunship that keeps 104 over its enemy. Two machine guns
//   (6 a bullet, the left gun aimed 0.2 s behind his motion, the right 0.2 s ahead), four grenades
//   a volley bent by its 0.4 s step, four rockets at once (half the time at a player each led by
//   its own time, else fanned; 2 s apart), a rail (50, 3 s apart) and three fliers launched in a
//   0.5 s sweep (the middle one a kamikaze where the pack has it) from its slots (3, 6, 6, 9 by
//   skill). In co-op it turns its rockets on a player behind or under it every 2 s. Its pains by
//   the blow (5 s apart, none at skill 3); its death the base game's boss wreck.
// - **The black widow** (`monster_widow`): walks; a blaster turret whose frame follows her enemy's
//   bearing from 100 degrees right to 70 left (each bolt held within 15 degrees of it), a charge
//   firing a bolt a frame, rails to the left, right or ahead, a kick within 100, and two stalkers
//   at a time from her slots (3, 3, 4, 6; more in co-op), their sweep of bolts first. She answers
//   a player's quad or double damage with her own multiplier, and his invulnerability with her
//   power shield or her own invulnerability, by skill. At her death she beams out and leaves her
//   legs, which burn down in blasts.
// - **Her second stage** (`monster_widow2`): a heat beam that follows where her enemy was a frame
//   before, a disruptor (a homing tracker when he has not moved off her aim), a tongue that pulls
//   him in and crunches, and stalkers at her sides after a sweep of beams. Every stalker in the
//   level dies with her; she burns down in a chain of blasts, searches the ground dead, and can
//   then be thrown apart.
//
// A boss's brood are spare bodies made with the map (q2_gz_monsters.cpp's `become`); each takes a
// slot, is not counted among the level's kills, and gives the slot back at its death.
//
// ge:summary Ground Zero's bosses: the carrier and the black widow's two stages, by hand
// ge:keywords q2 ground-zero rogue mission-pack boss carrier black-widow widow widow2 stalker
// ge:keywords heat-beam tracker tongue beam-out legs power-up-answer spawn-slots
// ge:skill shooter
// ge:test tests/shooters_edge_q2_gz_bosses_test.cpp
#include <algorithm>
#include <array>
#include <cmath>
#include <string_view>

#include "core/libm/libm.hpp"
#include "shooters_edge/server/q2_fight.hpp"
#include "shooters_edge/server/q2_fight_kit.hpp"
#include "shooters_edge/server/q2_gibs.hpp"
#include "shooters_edge/server/q2_map.hpp"
#include "shooters_edge/server/q2_monster_ai.hpp"
#include "shooters_edge/server/q2_monsters.hpp"
#include "shooters_edge/server/q2_server.hpp"

namespace shooters_edge::server::monster_ai {

namespace {

constexpr std::string_view kCarrier = "monster_carrier";
constexpr std::string_view kWidow = "monster_widow";
constexpr std::string_view kWidow2 = "monster_widow2";
constexpr const char* kStalker = "monster_stalker";
constexpr const char* kFlyer = "monster_flyer";
constexpr const char* kKamikaze = "monster_kamikaze";
constexpr const char* kLegs = "models/monsters/legs/tris.md2";
constexpr const char* kSpawnSound = "medic_commander/monsterspawn1.wav";
constexpr const char* kRailSound = "gladiator/railgun.wav";
constexpr int kSpares = 12;  // bodies made with the map for each boss's brood
constexpr double kDegrees = 57.29577951308232;

enum class Who : uint8_t { none, carrier, widow, widow2 };

Who who(std::string_view classname) {
  if (classname == kCarrier) return Who::carrier;
  if (classname == kWidow) return Who::widow;
  if (classname == kWidow2) return Who::widow2;
  return Who::none;
}
Who who(const Q2Monster& m) { return who(kind(m).classname); }

// The moves a boss plays by hand: their first model frame and their frames.
enum Move : uint8_t {
  kNone,
  // The carrier's.
  kGunsIn,
  kGuns,
  kGunsOut,
  kGrenadesIn,
  kGrenades,
  kGrenadesOut,
  kRockets,
  kRail,
  kLaunch,
  kCarrierFlinch,
  kCarrierPain,
  // The widow's.
  kBlasterIn,
  kBlaster,
  kBlasterOut,
  kBlasterOutRight,
  kBlasterOutLeft,
  kRailIn,
  kRailAhead,
  kRailRight,
  kRailLeft,
  kCharge,
  kSpawn,
  kKick,
  kWidowFlinch,
  kWidowPain,
  // Her second stage's.
  kBeamIn,
  kBeam,
  kBeamOut,
  kDisruptor,
  kSpawn2,
  kTongue,
  kWidow2Pain,
};
struct Frames {
  int16_t first, count;
};
constexpr Frames kFrames[] = {
    {0, 0},   {13, 8}, {21, 3},  {24, 4},  {28, 6},  {34, 4},  {38, 6},  {28, 1},   {0, 9},
    {44, 18}, {44, 4}, {62, 10}, {59, 3},  {61, 19}, {80, 2},  {116, 5}, {121, 5},  {126, 4},
    {32, 9},  {41, 9}, {50, 9},  {24, 8},  {82, 18}, {161, 8}, {113, 3}, {100, 13}, {35, 4},
    {39, 5},  {40, 2}, {28, 7},  {10, 18}, {47, 8},  {55, 5},
};

// ---- Helpers ----

float wrap(float a) {
  while (a > 180) a -= 360;
  while (a < -180) a += 360;
  return a;
}

int row_of(const Q2Server& s, const Q2Monster& m) {
  return static_cast<int>(&m - s.monsters.data());
}

sh::ViewAxes axes(const Q2Entity& e) { return sh::angle_vectors(sh::Vec3f{0, e.angles.y, 0}); }

// A point on the boss's body: `f` forward, `r` right (negative left), `u` up from its origin.
sh::Vec3f at_body(const Q2Entity& e, float f, float r, float u) {
  const sh::ViewAxes ax = axes(e);
  return e.origin + ax.forward * f + ax.right * r + sh::Vec3f{0, 0, u};
}

// The angles (pitch positive down, yaw) of direction `d`.
sh::Vec3f angles_of(sh::Vec3f d) {
  const double flat = double(sh::length(sh::Vec3f{d.x, d.y, 0}));
  return {static_cast<float>(-ge::core::libm::atan2(double(d.z), flat) * kDegrees),
          static_cast<float>(ge::core::libm::atan2(double(d.y), double(d.x)) * kDegrees), 0};
}

sh::Vec3f unit(sh::Vec3f d) {
  sh::normalize(d);
  return d;
}

// Its enemy's bearing from its facing, degrees to the right.
float bearing_right(const Q2Server& s, const Q2Monster& m) {
  const Q2Entity& e = s.entities[size_t(m.entity)];
  return wrap(e.angles.y - yaw_to(foe_at(s, m.enemy) - e.origin));
}

bool enemy_player(const Q2Monster& m) { return m.enemy >= 0 && m.enemy < sh::kMaxClients; }

bool coop(const Q2Server& s) {
  return s.cvars.find("coop")->on() && !s.cvars.find("deathmatch")->on();
}

int players_in(const Q2Server& s) {
  int n = 0;
  for (int i = 0; i < sh::kMaxClients; i++)
    if (hunted(s, i)) n++;
  return n;
}

// Its slots for its brood: the carrier's by skill; a widow's by skill, more in co-op.
int slots(const Q2Server& s, Who w) {
  const int sk = std::clamp(skill(s), 0, 3);
  if (w == Who::carrier) {
    constexpr int kSlots[4] = {3, 6, 6, 9};
    return kSlots[sk];
  }
  constexpr int kSlots[4] = {3, 3, 4, 6};
  int n = kSlots[sk];
  if (coop(s)) n = std::min(6, n + sk * std::max(0, players_in(s) - 1));
  return n;
}
int left(const Q2Server& s, const Q2Monster& m) { return slots(s, who(m)) - m.called; }

void show(Q2Monster& m, int frame, Frames run) {
  m.pose.shown = frame;
  m.pose.shown_run = {static_cast<int16_t>(run.first),
                      static_cast<int16_t>(run.first + std::max<int16_t>(run.count, 1) - 1)};
}

void begin(Q2Monster& m, Move mv) {
  m.boss.move = mv;
  m.boss.frame = 0;
}

// A turn toward `ideal` of at most `most` degrees.
void turn_by(Q2Entity& e, float ideal, float most) {
  const float d = std::clamp(wrap(ideal - e.angles.y), -most, most);
  e.angles.y += d;
  if (e.angles.y < 0) e.angles.y += 360;
  if (e.angles.y >= 360) e.angles.y -= 360;
}

// A frame's move: toward its enemy (unless it steers by hand), then `dist` along its facing.
void charge(Q2Server& s, Q2Monster& m, float dist) {
  Q2Entity& e = body(s, m);
  if (!m.boss.manual && foe_alive(s, m.enemy)) {
    m.ideal_yaw = yaw_to(foe_at(s, m.enemy) - e.origin);
    turn(e, kind(m), m.ideal_yaw);
  }
  if (dist != 0) step(s, m, forward_of(e.angles.y) * dist);
}

// A gun's flash at `from` along `d`: the protocol's monster flash `value`.
void flash(Q2Server& s, const Q2Monster& m, sh::Vec3f from, sh::Vec3f d, int value) {
  fight_monster_line(s, "te_monster_flash.boss", m.entity, from, from + d * 16.0f, value);
}

// A bullet from `from` toward `to`, spread `h` across and `v` up at 8192.
void bullet(Q2Server& s, const Q2Monster& m, sh::Vec3f from, sh::Vec3f to, int damage, int kick,
            float h, float v) {
  const sh::ViewAxes ax = sh::angle_vectors(angles_of(to - from));
  const sh::Vec3f end =
      from + ax.forward * 8192.0f + ax.right * (crandom(s) * h) + ax.up * (crandom(s) * v);
  fight_monster_bullet(s, m.entity, from, end, damage, kick, cause_of(m));
}

// A blow to its enemy within `reach` of a point `reach` forward: `lo`..`hi`, a push of `kick`
// along its facing (half in the air for a player).
bool hit(Q2Server& s, Q2Monster& m, float reach, int lo, int hi, float kick) {
  if (!foe_alive(s, m.enemy)) return false;
  const Q2Entity& e = body(s, m);
  const sh::Vec3f at = foe_at(s, m.enemy);
  if (sh::length(at - e.origin) > reach + 32 || !reaches(s, m, m.enemy, kOpaque)) return false;
  const int dmg = lo + static_cast<int>(random01(s) * (hi - lo + 1));
  const sh::Vec3f dir = forward_of(e.angles.y);
  hurt_foe(s, m, dmg, dir);
  if (enemy_player(m)) {
    const bool grounded = s.players[m.enemy].on_ground;
    fight_kick(s, m.enemy, dir * (grounded ? kick : kick * 0.5f));
  }
  return true;
}

// A blast where something on its body bursts: the effect and its small pieces.
void debris(Q2Server& s, sh::Vec3f at, int meat, int metal, float factor) {
  for (int i = 0; i < meat; i++) gibs_debris(s, s.gib_models[0], at, {}, factor * 0.5f);
  for (int i = 0; i < metal; i++) gibs_debris(s, s.gib_models[3], at, {}, factor);
}
void boom(Q2Server& s, sh::Vec3f at, const char* name = "te_explosion1") {
  fight_effect(s, name, at);
}
// A large piece of the widow (`model` its file) thrown from `at`.
void widow_gib(Q2Server& s, const char* model, sh::Vec3f at, float factor) {
  const auto found = std::find(s.models.begin(), s.models.end(), model);
  if (found != s.models.end())
    gibs_debris(s, static_cast<int>(found - s.models.begin()), at, {}, factor);
}

// ---- The brood ----

// A spare grown at `at` as class `classname` for boss `m`: its row, or -1 when none is free or
// it does not fit there (a flier needs only room, a stalker ground within 256 too).
int grow_one(Q2Server& s, Q2Monster& m, const char* classname, sh::Vec3f at) {
  int k = kind_of(s, classname);
  if (k < 0) k = kind_of(s, kFlyer);
  if (k < 0) return -1;
  const Kind& kd = kKinds[k];
  sh::Vec3f spot = at;
  if (kd.flies) {
    ge::classic::Q2Trace t = map_trace(s, at, kd.mins, kd.maxs, at, kMonsterMask, m.entity);
    if (t.start_solid || t.all_solid) {
      spot = at + sh::Vec3f{0, 0, 32};
      t = map_trace(s, spot, kd.mins, kd.maxs, spot, kMonsterMask, m.entity);
      if (t.start_solid || t.all_solid) return -1;
    }
  } else {
    ge::classic::Q2Trace t = map_trace(s, at, kd.mins, kd.maxs, at, kMonsterMask, m.entity);
    for (float up = 0; (t.start_solid || t.all_solid) && up < 64; up += 16) {
      spot = at + sh::Vec3f{0, 0, up + 16};
      t = map_trace(s, spot, kd.mins, kd.maxs, spot, kMonsterMask, m.entity);
    }
    if (t.start_solid || t.all_solid) return -1;
    const ge::classic::Q2Trace down =
        map_trace(s, spot, kd.mins, kd.maxs, spot - sh::Vec3f{0, 0, 256}, kMonsterMask, m.entity);
    if (down.fraction >= 1) return -1;
    spot = down.end;
  }
  Q2Monster* spare = free_spare(s, m);
  if (spare == nullptr) return -1;
  become(s, *spare, k, row_of(s, m), spot);
  spare->grow_until = s.level_time + 0.3;
  spare->ideal_yaw = body(s, m).angles.y;
  return static_cast<int>(spare - s.monsters.data());
}

// Spare row `i` comes out, hunting `foe`.
Q2Monster* bring(Q2Server& s, Q2Monster& m, int i, int foe) {
  if (i < 0 || size_t(i) >= s.monsters.size()) return nullptr;
  Q2Monster& o = s.monsters[size_t(i)];
  if (!o.spare || o.commander != row_of(s, m)) return nullptr;
  Q2Entity& e = body(s, o);
  o.grow_until = 0;
  o.spare = o.waiting = false;
  e.angles = {0, o.ideal_yaw, 0};
  e.hidden = false;
  e.box = true;
  o.pose.last = e.origin;
  o.on_ground = !kind(o).flies && on_floor(s, o);
  m.called++;
  sound(s, o.entity, kSpawnSound, 0);
  if (foe_alive(s, foe)) found(s, o, foe, false);
  return &o;
}

// ---- Power-ups: the widows' answer ----

void shield_up(Q2Server& s, Q2Monster& m) {
  if (m.shield <= 0) m.shield += 250 * skill(s);
}

void answer(Q2Server& s, Q2Monster& m) {
  const double now = s.level_time;
  int slot = -1;
  if (coop(s)) {
    for (const int which : {0, 1, 2})
      for (int i = 0; i < sh::kMaxClients && slot < 0; i++) {
        if (!hunted(s, i)) continue;
        const Q2Player& p = s.players[i];
        const double until = which == 0   ? p.invulnerable_until
                             : which == 1 ? p.quad_until
                                          : p.double_until;
        if (now < until) slot = i;
      }
    if (slot < 0) return;  // none found: nothing changes
  } else if (enemy_player(m)) {
    slot = m.enemy;
  } else {
    return;
  }
  const Q2Player& p = s.players[slot];
  const int sk = std::clamp(skill(s), 0, 3);
  const bool quad = now < p.quad_until, twice = now < p.double_until;
  int& mult = m.boss.multiplier;
  m.boss.quad_until = quad && sk >= 1 ? p.quad_until : 0;  // the shell of what it took up
  m.boss.double_until = !quad && twice && sk >= 2 ? p.double_until : 0;
  if (quad) {
    if (sk == 1) mult = 2;
    if (sk >= 2) mult = 4;
    if (sk == 3) shield_up(s, m);
  } else if (twice) {
    if (sk >= 2) mult = 2;
    if (sk == 3) shield_up(s, m);
  } else {
    mult = 1;
  }
  if (now < p.invulnerable_until) {
    if (sk == 1 || sk == 3) shield_up(s, m);
    if (sk >= 2) m.boss.invulnerable_until = p.invulnerable_until;
  }
}

// ---- The carrier ----

constexpr float kGunLead = 0.2f;

void carrier_guns(Q2Server& s, Q2Monster& m) {
  if (!foe_alive(s, m.enemy)) return;
  const Q2Entity& e = body(s, m);
  const bool low = m.boss.manual;
  for (const float side : {-1.0f, 1.0f}) {
    const sh::Vec3f from = at_body(e, 56, 32 * side, low ? -24.0f : -4.0f);
    const sh::Vec3f to = foe_eye(s, m.enemy) + foe_velocity(s, m.enemy) * (kGunLead * side);
    flash(s, m, from, unit(to - from), side < 0 ? (low ? 152 : 138) : (low ? 153 : 139));
    bullet(s, m, from, to, 6, 4, 900, 500);
  }
}

void carrier_grenade(Q2Server& s, Q2Monster& m) {
  if (!foe_alive(s, m.enemy)) return;
  const Q2Entity& e = body(s, m);
  const sh::ViewAxes ax = axes(e);
  const sh::Vec3f from = at_body(e, 40, 0, -30);
  const float dir = random01(s) < 0.5 ? -1.0f : 1.0f;
  const int n = static_cast<int>((s.level_time - m.boss.timer) / 0.4 + 0.001);
  float right = 0, up = 0;
  switch (n) {
    case 0:
      right = 0.15f * dir;
      up = 0.1f - 0.1f * dir;
      break;
    case 1:
    case 3:
      up = 0.1f;
      break;
    case 2:
      right = -0.15f * dir;
      up = 0.1f + 0.1f * dir;
      break;
    default:
      break;
  }
  sh::Vec3f d = unit(foe_at(s, m.enemy) - from) + ax.right * right + ax.up * up;
  sh::normalize(d);
  d.z = std::clamp(d.z, -0.5f, 0.15f);
  sh::normalize(d);
  flash(s, m, from, d, 53);
  fight_monster_launch(s, m.entity, SqShot::grenade, from, d, 50, 600, cause_of(m));
}

// Four rockets at `foe`: half the time at a player each led by its own time (750 a second),
// else fanned (500 a second), the outer two 15 low.
void carrier_rockets(Q2Server& s, Q2Monster& m, int foe) {
  if (!foe_alive(s, foe)) return;
  const Q2Entity& e = body(s, m);
  const sh::ViewAxes ax = axes(e);
  const bool predict = foe >= 0 && random01(s) < 0.5;
  constexpr float kLead[4] = {-0.3f, -0.15f, 0, 0.15f};
  constexpr float kFan[4] = {-0.4f, -0.025f, 0.025f, 0.4f};
  for (int i = 0; i < 4; i++) {
    const sh::Vec3f from = at_body(e, 56, -42.0f + 28.0f * float(i), -10);
    sh::Vec3f d;
    float speed = 500;
    if (predict) {
      d = unit(foe_at(s, foe) + foe_velocity(s, foe) * kLead[i] - from);
      speed = 750;
    } else {
      sh::Vec3f to = foe_eye(s, foe);
      if (i == 0 || i == 3) to.z -= 15;
      d = unit(to - from) + ax.right * kFan[i];
      sh::normalize(d);
    }
    flash(s, m, from, d, 191 + i);
    fight_monster_launch(s, m.entity, SqShot::rocket, from, d, 50, speed, cause_of(m));
  }
  m.boss.rockets_at = s.level_time;
}

// In co-op, at most every 2 s: a player behind it or under it, in sight, takes a volley.
void coop_check(Q2Server& s, Q2Monster& m) {
  if (!coop(s) || s.level_time + 0.001 < m.boss.coop_at + 2) return;
  m.boss.coop_at = s.level_time;
  const Q2Entity& e = body(s, m);
  std::array<int, sh::kMaxClients> found_at{};
  int n = 0;
  for (int i = 0; i < sh::kMaxClients; i++) {
    if (!hunted(s, i) || !reaches(s, m, i, kOpaque)) continue;
    const sh::Vec3f to = unit(foe_at(s, i) - e.origin);
    const bool behind = sh::dot(to, forward_of(e.angles.y)) < -0.3f;
    const bool under = to.z < -0.951f;  // within 18 degrees of straight down
    if (behind || under) found_at[size_t(n++)] = i;
  }
  if (n == 0) return;
  carrier_rockets(s, m, found_at[size_t(std::min(n - 1, static_cast<int>(random01(s) * n)))]);
}

bool rail_ready(const Q2Server& s, const Q2Monster& m) {
  return s.level_time + 0.001 >= m.boss.rail_at + 3;
}

void start_rail(Q2Server& s, Q2Monster& m, Move mv) {
  sound(s, m.entity, kRailSound);
  begin(m, mv);
}

// The carrier's choice of attack (q2 gz-monsters § 7.5's table).
void carrier_choose(Q2Server& s, Q2Monster& m) {
  const Q2Entity& e = body(s, m);
  const sh::Vec3f to = foe_at(s, m.enemy) - e.origin;
  const float dist = sh::length(to);
  const sh::Vec3f d = unit(to);
  const float facing = sh::dot(d, forward_of(e.angles.y));
  const bool front = facing > kInFront, behind = facing < -kInFront, below = d.z < -0.951f;
  const int spare = left(s, m);
  const double roll = random01(s);
  if (m.boss.blind) {
    begin(m, kLaunch);
    return;
  }
  if (!front && (behind || below)) {
    begin(m, kRockets);
    return;
  }
  if (!front) {
    if (roll < 0.1 || !rail_ready(s, m))
      begin(m, kGunsIn);
    else
      start_rail(s, m, kRail);
    return;
  }
  if (dist <= 125) {
    if (roll < 0.8 || !rail_ready(s, m))
      begin(m, kGunsIn);
    else
      start_rail(s, m, kRail);
    return;
  }
  if (dist < 600) {
    if (spare > 2) {
      if (roll <= 0.2)
        begin(m, kGunsIn);
      else if (roll <= 0.4)
        begin(m, kGrenadesIn);
      else if (roll <= 0.7 && rail_ready(s, m))
        start_rail(s, m, kRail);
      else
        begin(m, kLaunch);
    } else {
      if (roll > 0.3 && roll <= 0.65)
        begin(m, kGrenadesIn);
      else if (roll > 0.65 && rail_ready(s, m))
        start_rail(s, m, kRail);
      else
        begin(m, kGunsIn);
    }
    return;
  }
  if (spare > 2) {
    if (roll < 0.3) {
      begin(m, kGunsIn);
    } else if (roll < 0.65 && rail_ready(s, m)) {
      start_rail(s, m, kRail);
      m.boss.saved = foe_eye(s, m.enemy);  // far, with slots: its aim taken at once
    } else {
      begin(m, kLaunch);
    }
  } else if (roll < 0.45 || !rail_ready(s, m)) {
    begin(m, kGunsIn);
  } else {
    start_rail(s, m, kRail);
  }
}

// The machine guns again, or a launch, or their end.
void carrier_guns_again(Q2Server& s, Q2Monster& m) {
  coop_check(s, m);
  const Q2Entity& e = body(s, m);
  if (foe_alive(s, m.enemy) && in_front(e, foe_at(s, m.enemy)) && random01(s) < 0.5) {
    if (left(s, m) <= 2 || random01(s) < 0.7)
      begin(m, kGuns);
    else
      begin(m, kLaunch);
    return;
  }
  begin(m, kGunsOut);
}

// Whether the carrier attacks this frame (and begins it); false leaves the frame to its run.
bool carrier_attacks(Q2Server& s, Q2Monster& m) {
  coop_check(s, m);
  if (!foe_alive(s, m.enemy)) return false;
  const Q2Entity& e = body(s, m);
  if (!reaches(s, m, m.enemy, kOpaque)) {
    if (enemy_player(m) && left(s, m) > 2) {
      m.boss.blind = true;
      carrier_choose(s, m);
      return true;
    }
    return false;
  }
  m.boss.blind = false;
  const sh::Vec3f d = unit(foe_at(s, m.enemy) - e.origin);
  const float facing = sh::dot(d, forward_of(e.angles.y));
  const bool front = facing > kInFront, behind = facing < -kInFront, below = d.z < -0.951f;
  if (behind || (below && !front)) {
    if (s.level_time + 0.001 >= m.boss.rockets_at + 2) {
      carrier_choose(s, m);
      return true;
    }
    m.slide = random01(s) < 0.6;
    return false;
  }
  const Range r = range_to(e.origin, foe_at(s, m.enemy));
  const double chance = r == Range::melee ? 1.0 : m.hold ? 0.4 : r == Range::far ? 0.5 : 0.8;
  if (random01(s) < chance) {
    carrier_choose(s, m);
    return true;
  }
  m.slide = random01(s) < 0.6;
  return false;
}

// One frame of a carrier's move: its events; false when the frame holds (the launch's aim).
bool carrier_frame(Q2Server& s, Q2Monster& m, int mv, int i) {
  Q2Entity& e = body(s, m);
  Q2Boss& b = m.boss;
  switch (mv) {
    case kGunsIn:
      charge(s, m, 4);
      if (i == 7) begin(m, kGuns);
      return true;
    case kGuns:
      charge(s, m, i < 2 ? -2.0f : 0.0f);
      carrier_guns(s, m);
      if (i == 2) carrier_guns_again(s, m);
      return true;
    case kGunsOut:
    case kGrenadesOut:
      charge(s, m, 4);
      return true;
    case kGrenadesIn:
      charge(s, m, 4);
      if (i == 5) {
        b.timer = s.level_time + kStep;
        begin(m, kGrenades);
      }
      return true;
    case kGrenades:
      if (i == 0) {
        charge(s, m, -15);
        carrier_grenade(s, m);
      } else if (i < 3) {
        charge(s, m, 4);
      } else {
        coop_check(s, m);
        const bool more = foe_alive(s, m.enemy) && in_front(e, foe_at(s, m.enemy)) &&
                          s.level_time - b.timer < 1.3;
        begin(m, more ? kGrenades : kGrenadesOut);
      }
      return true;
    case kRockets:
      charge(s, m, 15);
      carrier_rockets(s, m, m.enemy);
      return true;
    case kRail:
      charge(s, m, i == 3 ? -20.0f : 2.0f);
      if (i == 1 && foe_alive(s, m.enemy)) b.saved = foe_eye(s, m.enemy);
      if (i == 3) {
        const sh::Vec3f from = at_body(e, 56, 0, -4);
        flash(s, m, from, unit(b.saved - from), 147);
        fight_monster_rail(s, m.entity, from, b.saved, 50, 100, cause_of(m));
        b.rail_at = s.level_time;
      }
      return true;
    case kLaunch: {
      if (i != 8) charge(s, m, i == 11 ? -10.0f : -2.0f);
      if (i <= 6 || (i >= 9 && i <= 17)) carrier_guns(s, m);
      if (i == 6) {
        b.manual = true;
        b.timer = s.level_time;
      }
      if (i == 7) {
        b.step = std::clamp(static_cast<int>((s.level_time - b.timer) / 0.5 + 0.001), 0, 2);
        if (foe_alive(s, m.enemy))
          m.ideal_yaw = yaw_to(foe_at(s, m.enemy) - e.origin) - 30.0f + 30.0f * float(b.step);
      }
      if (i == 8) {
        turn_by(e, m.ideal_yaw, 10);
        if (std::abs(wrap(e.angles.y - m.ideal_yaw)) > 0.1f) {
          b.timer += kStep;  // the clock waits while it turns
          return false;
        }
        b.brood[0] = left(s, m) > 0
                         ? grow_one(s, m, b.step == 1 ? kKamikaze : kFlyer, at_body(e, 105, 0, -58))
                         : -1;
      }
      if (i == 11) {
        if (Q2Monster* f = bring(s, m, b.brood[0], m.enemy)) {
          if (b.step != 1) {  // the outer two circle their enemy, each its own way
            f->slide = true;
            f->slide_left = b.step == 0;
          }
        }
        b.brood[0] = -1;
        if (s.level_time - b.timer < 1.1) {
          b.frame = 7;  // round for the next
          return false;
        }
        b.manual = false;
      }
      if (i == 17) {
        coop_check(s, m);
        carrier_guns_again(s, m);
      }
      return true;
    }
    default:
      return true;  // its pains: frames only
  }
}

void carrier_pain(Q2Server& s, Q2Monster& m, int damage) {
  if (s.level_time + 0.001 < m.pain_at + 5) return;
  m.pain_at = s.level_time;
  const char* say = damage < 10   ? "carrier/pain_sm.wav"
                    : damage < 30 ? "carrier/pain_md.wav"
                                  : "carrier/pain_lg.wav";
  sound(s, m.entity, say, 0);
  if (damage < 10 || skill(s) == 3) return;
  Move mv = kNone;
  if (damage < 30) {
    if (random01(s) < 0.5) mv = kCarrierFlinch;
  } else {
    mv = kCarrierPain;
  }
  if (mv == kNone) return;
  if (Q2Monster* f = m.boss.brood[0] >= 0 ? &s.monsters[size_t(m.boss.brood[0])] : nullptr)
    f->grow_until = 0;  // a launch cut short
  m.boss.brood[0] = -1;
  m.boss.manual = false;
  begin(m, mv);
}

// ---- The black widow ----

int widow_damage(const Q2Monster& m, int base) { return base * m.boss.multiplier; }

sh::Vec3f widow_muzzle(const Q2Entity& e) { return at_body(e, 40, 0, 98); }

void widow_bolt(Q2Server& s, Q2Monster& m, sh::Vec3f from, sh::Vec3f d, int value) {
  flash(s, m, from, d, value);
  fight_monster_launch(s, m.entity, SqShot::bolt, from, d, widow_damage(m, 10), 1000, cause_of(m));
}

// The blaster's frame for a bearing (degrees right): 1 is 100 right, 11 straight ahead, 18 70 left.
int blaster_frame(float right) {
  const int k = 1 + static_cast<int>(std::floor((100.0f - right) / 10.0f + 0.5f));
  return std::clamp(k, 1, 18);
}
float blaster_angle(int k) { return k == 0 ? 0.0f : 100.0f - 10.0f * float(k - 1); }
int blaster_flash(int k) { return k == 0 || k == 11 ? 175 : k < 11 ? 164 + k : 175 + (k - 11); }

void blaster_fire(Q2Server& s, Q2Monster& m, int k) {
  if (!foe_alive(s, m.enemy)) return;
  const Q2Entity& e = body(s, m);
  const sh::Vec3f from = widow_muzzle(e);
  const sh::Vec3f eye_at = foe_eye(s, m.enemy);
  const float t = sh::length(eye_at - from) / 1000.0f + crandom(s) * 0.05f;
  sh::Vec3f a = angles_of(eye_at + foe_velocity(s, m.enemy) * t - from);
  const float frame_yaw = e.angles.y - blaster_angle(k);
  a.y = frame_yaw + std::clamp(wrap(a.y - frame_yaw), -15.0f, 15.0f);
  widow_bolt(s, m, from, sh::angle_vectors(a).forward, blaster_flash(k));
}

// Two stalkers grow at her sides (`side` out, `up` above, 30 forward), when slots allow.
void widow_grow(Q2Server& s, Q2Monster& m, float side, float up) {
  const Q2Entity& e = body(s, m);
  for (int i = 0; i < 2; i++) {
    m.boss.brood[i] = -1;
    if (left(s, m) - i < 1) continue;
    const sh::Vec3f p = at_body(e, 30, i == 0 ? side : -side, up);
    m.boss.brood[i] = grow_one(s, m, kStalker, p);
  }
}

// The grown stalkers come out; in co-op each of the second stage's hunts a player at random,
// another than hers when it can.
void widow_bring(Q2Server& s, Q2Monster& m, bool pick) {
  for (int& i : m.boss.brood) {
    int foe = m.enemy;
    if (pick && coop(s)) {
      for (int tries = 0; tries < 2; tries++) {
        const int slot =
            std::min(sh::kMaxClients - 1, static_cast<int>(random01(s) * sh::kMaxClients));
        if (!hunted(s, slot)) continue;
        foe = slot;
        if (slot != m.enemy) break;
      }
      if (!foe_alive(s, foe)) foe = m.enemy;
    }
    bring(s, m, i, foe);
    i = -1;
  }
}

bool blaster_ready(const Q2Server& s, const Q2Monster& m) {
  return s.level_time + 0.001 >= m.boss.blaster_end + 2;
}

// Where in her run she stands: the run's frame (0 to 12), -1 when not running.
int run_frame(const Q2Monster& m) {
  if (m.pose.act != SqMonsterPose::Act::run && m.pose.act != SqMonsterPose::Act::walk) return -1;
  if (m.pose.span.first != 11) return -1;
  return m.pose.frame - 11;
}

void widow_choose(Q2Server& s, Q2Monster& m, int run) {
  const Q2Entity& e = body(s, m);
  const float dist = sh::length(foe_at(s, m.enemy) - e.origin);
  const bool two = left(s, m) >= 2;
  Q2Boss& b = m.boss;
  const bool angered = b.angered;
  b.angered = false;
  if ((b.blind || b.bias) && two) {
    begin(m, kSpawn);
    return;
  }
  if (dist > 300 && !angered && !b.bias && random01(s) < 0.5) {
    begin(m, kCharge);
    return;
  }
  const bool blaster_frames = run >= 8 && run <= 11;
  const bool rail_frames = run == 12 || (run >= 0 && run <= 2);
  if (blaster_frames) {
    if (two)
      begin(m, kSpawn);
    else if (blaster_ready(s, m))
      begin(m, kBlasterIn);
    return;
  }
  if (rail_frames) {
    if (rail_ready(s, m)) begin(m, kRailIn);
    return;
  }
  const double roll = random01(s);
  if (two) {
    if (roll <= 0.4 && blaster_ready(s, m))
      begin(m, kBlasterIn);
    else if (roll <= 0.7 && rail_ready(s, m))
      begin(m, kRailIn);
    else
      begin(m, kSpawn);
    return;
  }
  if (rail_ready(s, m) && (roll <= 0.5 || s.level_time - b.blaster_end < 2))
    begin(m, kRailIn);
  else
    begin(m, kBlasterIn);
}

// The bias toward a spawn and the blind rule both stages share; true when the check ends here
// (`go` whether she attacks).
bool widow_first(Q2Server& s, Q2Monster& m, bool& go) {
  answer(s, m);
  const Q2Entity& e = body(s, m);
  Q2Boss& b = m.boss;
  b.bias = left(s, m) >= 2 && sh::length(foe_at(s, m.enemy) - e.origin) > 150 && random01(s) < 0.8;
  if (b.bias) {
    go = true;
    return true;
  }
  if (!reaches(s, m, m.enemy, kOpaque)) {
    b.blind = enemy_player(m) && left(s, m) >= 2;
    go = b.blind;
    return true;
  }
  b.blind = false;
  return false;
}

bool widow_attacks(Q2Server& s, Q2Monster& m) {
  if (!foe_alive(s, m.enemy)) return false;
  const int run = run_frame(m);
  if (run >= 3 && run <= 7) return false;  // her stride's middle: no check
  if (run == 11) return false;
  answer(s, m);
  bool go = false;
  if (widow_first(s, m, go)) {
    if (go) widow_choose(s, m, run);
    return go;
  }
  const Q2Entity& e = body(s, m);
  const float dist = sh::length(foe_at(s, m.enemy) - e.origin);
  if (dist <= 100) {
    if (skill(s) == 0 && random01(s) < 0.75) return false;
    begin(m, kKick);
    return true;
  }
  const Range r = range_to(e.origin, foe_at(s, m.enemy));
  const double chance = m.hold              ? 0.4
                        : r == Range::melee ? 0.8
                        : r == Range::near  ? 0.7
                        : r == Range::mid   ? 0.6
                                            : 0.5;
  if (random01(s) >= chance) return false;
  widow_choose(s, m, run);
  return m.boss.move != kNone;
}

constexpr float kChargeSteps[8] = {13, 11.72f, 18.04f, 14.58f, 13, 12.12f, 19.63f, 11.37f};
constexpr float kSpawnYaw[8] = {-26, -20, -10, 0, 6.5f, 13, 27, 41};  // left positive

bool widow_frame(Q2Server& s, Q2Monster& m, int mv, int i) {
  Q2Entity& e = body(s, m);
  Q2Boss& b = m.boss;
  switch (mv) {
    case kBlasterIn:
      b.manual = true;
      if (i == 2) {
        b.blaster_end = s.level_time + 1 + random01(s) * 2;
        b.step = 0;
        begin(m, kBlaster);
      }
      return true;
    case kBlaster: {
      blaster_fire(s, m, b.step);
      const float right = foe_alive(s, m.enemy) ? bearing_right(s, m) : 0.0f;
      if (s.level_time + 0.001 >= b.blaster_end) {
        b.manual = false;
        begin(m, kBlasterOut);
      } else if (right >= 105) {
        b.manual = false;
        begin(m, kBlasterOutRight);
      } else if (right <= -75) {
        b.manual = false;
        begin(m, kBlasterOutLeft);
      } else {
        b.step = blaster_frame(right);
        b.frame = b.step;
        return false;
      }
      return true;
    }
    case kBlasterOutRight: {
      constexpr float kSteps[5] = {-2, -10, -2, 0, 0};
      step(s, m, forward_of(e.angles.y) * kSteps[i]);
      return true;
    }
    case kBlasterOutLeft: {
      constexpr float kSteps[5] = {0, 14, -2, 10, 10};
      step(s, m, forward_of(e.angles.y) * kSteps[i]);
      return true;
    }
    case kRailIn:
      if (i == 0) b.manual = true;
      if (i == 3) {
        const float right = foe_alive(s, m.enemy) ? bearing_right(s, m) : 0.0f;
        sound(s, m.entity, kRailSound);
        begin(m, right < -15 ? kRailLeft : right > 15 ? kRailRight : kRailAhead);
      }
      return true;
    case kRailAhead:
    case kRailRight:
    case kRailLeft:
      if (i == 2 && foe_alive(s, m.enemy)) b.saved = foe_eye(s, m.enemy);
      if (i == 3) {
        step(s, m, forward_of(e.angles.y) * -10.0f);
        const sh::Vec3f from = widow_muzzle(e);
        flash(s, m, from, unit(b.saved - from),
              mv == kRailAhead  ? 150
              : mv == kRailLeft ? 154
                                : 155);
        fight_monster_rail(s, m.entity, from, b.saved, widow_damage(m, 50), 100, cause_of(m));
        b.rail_at = s.level_time;
      }
      if (i == 8) b.manual = false;
      return true;
    case kCharge: {
      if (i == 0 || i == 4) sound(s, m.entity, "widow/bwstep2.wav");
      const sh::Vec3f before = e.origin;
      charge(s, m, kChargeSteps[i]);
      if (foe_alive(s, m.enemy)) {
        const sh::Vec3f from = at_body(e, 40, 0, 98);
        widow_bolt(s, m, from, unit(foe_eye(s, m.enemy) - from), 183 + i);
      }
      if (sh::length(e.origin - before) < 1) {  // blocked: angered, and a check at once
        b.angered = true;
        begin(m, kNone);
        widow_attacks(s, m);
        return b.move == kNone;
      }
      return true;
    }
    case kSpawn:
      if (i == 3) b.manual = true;
      if (i >= 5 && i <= 12 && foe_alive(s, m.enemy)) {
        const sh::Vec3f from = widow_muzzle(e);
        sh::Vec3f a = angles_of(foe_eye(s, m.enemy) - from);
        a.y = e.angles.y + kSpawnYaw[i - 5];
        widow_bolt(s, m, from, sh::angle_vectors(a).forward, 157 + (i - 5));
      }
      if (i == 6) widow_grow(s, m, 100, 16);
      if (i == 9) widow_bring(s, m, false);
      if (i == 17) b.manual = false;
      return true;
    case kKick:
      if (i == 3) hit(s, m, 100, 50, 55, 500);
      return true;
    default:
      return true;  // the plain blaster's end, her pains: frames only
  }
}

// A pain of either stage: by the blow, 5 s apart, none at skill 3; `second` the second stage's.
void widow_pain(Q2Server& s, Q2Monster& m, int damage, bool second) {
  if (s.level_time + 0.001 < m.pain_at + 5) return;
  m.pain_at = s.level_time;
  const char* say[3] = {"widow/bw1pain1.wav", "widow/bw1pain2.wav", "widow/bw1pain3.wav"};
  const char* say2[3] = {"widow/bw2pain1.wav", "widow/bw2pain2.wav", "widow/bw2pain3.wav"};
  const int band = damage < 15 ? 0 : damage < 75 ? 1 : 2;
  sound(s, m.entity, second ? say2[band] : say[band], 0);
  if (band == 0 || skill(s) == 3) return;
  const double chance = band == 1 ? 0.6 - 0.2 * skill(s) : 0.75 - 0.1 * skill(s);
  if (random01(s) >= chance) return;
  for (int& i : m.boss.brood) {  // a spawn cut short: its grows go back
    if (i >= 0) s.monsters[size_t(i)].grow_until = 0;
    i = -1;
  }
  m.boss.manual = false;
  if (m.boss.move == kBlaster) m.boss.blaster_end = s.level_time;
  begin(m, second ? kWidow2Pain : band == 1 ? kWidowFlinch : kWidowPain);
}

// ---- The second stage ----

sh::Vec3f widow2_muzzle(const Q2Entity& e) { return at_body(e, 60, 0, 90); }

void heat_beam(Q2Server& s, Q2Monster& m, sh::Vec3f at, int value) {
  const Q2Entity& e = body(s, m);
  const sh::Vec3f from = widow2_muzzle(e);
  const sh::Vec3f d = unit(at - from);
  const Line l = fight_line(s, from, from + d * 8192.0f, 0, m.entity);
  if (l.player >= 0) {
    fight_hurt(s, l.player, -1, d, 10, 50, cause_of(m), 4);
  } else if (l.mover >= 0 && monster_index(s, l.mover) >= 0) {
    monster_damage(s, l.mover, foe_of_entity(m.entity), 10, d, false, &l.end);
  } else if (l.fraction < 1 && !l.sky) {
    fight_effect(s, "te_heatbeam_steam", l.end, l.normal);
  }
  flash(s, m, from, d, value);
  fight_monster_line(s, "te_monster_heatbeam", m.entity, from, l.end);
}

// Where the beam aims: her enemy's place a beam frame ago, at his eye less 10.
sh::Vec3f beam_mark(const Q2Server& s, const Q2Monster& m) {
  float height = 22;
  if (enemy_player(m)) height = s.players[m.enemy].view_height;
  return m.boss.beam_last + sh::Vec3f{0, 0, height - 10};
}

void disruptor(Q2Server& s, Q2Monster& m) {
  if (!foe_alive(s, m.enemy)) return;
  const Q2Entity& e = body(s, m);
  const sh::Vec3f from = widow2_muzzle(e);
  const bool homing = sh::length(foe_at(s, m.enemy) - m.boss.saved) <= 30;
  sh::Vec3f d;
  float speed;
  if (homing) {
    d = unit(m.boss.saved - from);
    speed = 500;
  } else {
    const sh::Vec3f eye_at = foe_eye(s, m.enemy);
    d = unit(eye_at + foe_velocity(s, m.enemy) * (sh::length(eye_at - from) / 1200.0f) - from);
    speed = 1200;
  }
  flash(s, m, from, d, 148);
  sound(s, m.entity, "weapons/disrupt.wav");
  fight_monster_launch(s, m.entity, SqShot::tracker, from, d, 20, speed, cause_of(m));
  if (!homing) return;
  for (Q2Missile& t : s.missiles) {  // the one just fired follows him
    if (!t.live || t.from != m.entity || t.kind != SqShot::tracker || t.prey != -1 ||
        t.stage != 0 || sh::length(t.origin - from) > 0.01f)
      continue;
    t.prey = m.enemy;
    t.dies_at = 1e18;
  }
}

sh::Vec3f tongue_start(const Q2Entity& e) { return at_body(e, 17, 0, 69); }

// Whether her tongue reaches point `p`: within 256, at most 30 degrees off level.
bool tongue_reaches(const Q2Entity& e, sh::Vec3f p) {
  const sh::Vec3f d = p - tongue_start(e);
  const float flat = sh::length(sh::Vec3f{d.x, d.y, 0});
  return sh::length(d) <= 256 && std::abs(d.z) <= 0.57735f * flat;
}

bool tongue_reaches_foe(const Q2Server& s, const Q2Monster& m) {
  const Q2Entity& e = s.entities[size_t(m.entity)];
  const sh::Vec3f at = foe_at(s, m.enemy);
  sh::Vec3f lo, hi;
  foe_box(s, m.enemy, lo, hi);
  return tongue_reaches(e, at) || tongue_reaches(e, at + sh::Vec3f{0, 0, hi.z - 8}) ||
         tongue_reaches(e, at + sh::Vec3f{0, 0, lo.z + 8});
}

// Her enemy pulled toward her: a player 1000 a second her way, a monster along her facing.
void pull(Q2Server& s, Q2Monster& m) {
  if (!foe_alive(s, m.enemy) || !tongue_reaches_foe(s, m)) return;
  const Q2Entity& e = body(s, m);
  if (enemy_player(m)) {
    fight_kick(s, m.enemy, unit(e.origin - foe_at(s, m.enemy)) * 1000.0f);
    return;
  }
  if (const Q2Monster* o = foe_monster(s, m.enemy)) {
    Q2Monster& prey = s.monsters[size_t(o - s.monsters.data())];
    Q2Entity& pe = body(s, prey);
    pe.angles.y = prey.ideal_yaw = yaw_to(e.origin - pe.origin);
    if (prey.on_ground) pe.origin.z += 1;
    prey.velocity = forward_of(e.angles.y) * 1000.0f;
    prey.on_ground = false;
  }
}

constexpr float kSweep2Yaw = 40;  // the sweep's first beam, left; 8 a beam to 40 right

void widow2_choose(Q2Server& s, Q2Monster& m) {
  const Q2Entity& e = body(s, m);
  const float dist = sh::length(foe_at(s, m.enemy) - e.origin);
  const bool two = left(s, m) >= 2;
  const Q2Boss& b = m.boss;
  if ((b.blind || b.bias) && two) {
    begin(m, kSpawn2);
    return;
  }
  const double roll = random01(s);
  if (dist < 600) {
    if (two)
      begin(m, roll <= 0.4 ? kBeamIn : roll <= 0.7 ? kDisruptor : kSpawn2);
    else
      begin(m, roll <= 0.5 ? kBeamIn : kDisruptor);
    return;
  }
  if (two)
    begin(m, roll < 0.3 ? kBeamIn : roll < 0.65 ? kSpawn2 : kDisruptor);
  else
    begin(m, roll < 0.45 ? kBeamIn : kDisruptor);
}

bool widow2_attacks(Q2Server& s, Q2Monster& m) {
  if (!foe_alive(s, m.enemy)) return false;
  bool go = false;
  if (widow_first(s, m, go)) {
    if (go) widow2_choose(s, m);
    return go;
  }
  const Q2Entity& e = body(s, m);
  const float dist = sh::length(foe_at(s, m.enemy) - e.origin);
  if (s.level_time + 0.001 >= m.boss.tongue_at && dist < 300 && tongue_reaches_foe(s, m) &&
      !(skill(s) == 0 && random01(s) < 0.75)) {
    begin(m, kTongue);
    return true;
  }
  const Range r = range_to(e.origin, foe_at(s, m.enemy));
  const double chance = m.hold ? 0.4 : r == Range::melee ? 0.0 : r == Range::far ? 0.5 : 0.8;
  if (random01(s) >= chance) return false;
  widow2_choose(s, m);
  return true;
}

// The beam again, or stalkers, or its end.
void beam_again(Q2Server& s, Q2Monster& m) {
  m.boss.manual = false;
  const Q2Entity& e = body(s, m);
  if (foe_alive(s, m.enemy) && in_front(e, foe_at(s, m.enemy)) && random01(s) < 0.5) {
    if (left(s, m) < 2 || random01(s) < 0.7) {
      m.boss.beam_last = foe_at(s, m.enemy);
      begin(m, kBeam);
    } else {
      begin(m, kSpawn2);
    }
    return;
  }
  begin(m, kBeamOut);
}

bool widow2_frame(Q2Server& s, Q2Monster& m, int mv, int i) {
  Q2Entity& e = body(s, m);
  Q2Boss& b = m.boss;
  switch (mv) {
    case kBeamIn:
      charge(s, m, 4);
      if (i == 3) {
        if (foe_alive(s, m.enemy)) b.beam_last = foe_at(s, m.enemy);
        begin(m, kBeam);
      }
      return true;
    case kBeam:
      if (i < 4 && foe_alive(s, m.enemy)) {
        heat_beam(s, m, beam_mark(s, m), 195 + i);
        b.beam_last = foe_at(s, m.enemy);
      }
      if (i == 4) beam_again(s, m);
      return true;
    case kBeamOut:
      charge(s, m, 4);
      return true;
    case kDisruptor:
      charge(s, m, i == 3 ? -20.0f : 2.0f);
      if (i == 2 && foe_alive(s, m.enemy)) b.saved = foe_eye(s, m.enemy);
      if (i == 3) disruptor(s, m);
      if (i == 6 && random01(s) < 0.25 + 0.15 * skill(s)) begin(m, kDisruptor);
      return true;
    case kSpawn2:
      if (i == 2) b.manual = true;
      if (i >= 3 && i <= 13 && foe_alive(s, m.enemy)) {
        const sh::Vec3f from = widow2_muzzle(e);
        sh::Vec3f a = angles_of(foe_eye(s, m.enemy) - from);
        a.y = e.angles.y + kSweep2Yaw - 8.0f * float(i - 3);
        heat_beam(s, m, from + sh::angle_vectors(a).forward * 512.0f, 200 + (i - 3));
      }
      if (i == 9) widow_grow(s, m, 135, 0);
      if (i == 13) widow_bring(s, m, true);
      if (i == 17) beam_again(s, m);
      return true;
    case kTongue:
      if (!foe_alive(s, m.enemy)) return true;
      if (i <= 2) {
        if (!tongue_reaches_foe(s, m)) return true;
        const Line l = fight_line(s, tongue_start(e), foe_at(s, m.enemy), 0, m.entity);
        const bool on_him = enemy_player(m) ? l.player == m.enemy : l.mover == foe_entity(m.enemy);
        if (!on_him) return true;
        sound(s, m.entity, "brain/brnatck3.wav");
        fight_monster_line(s, "te_parasite_attack", m.entity, tongue_start(e), foe_at(s, m.enemy));
        hurt_foe(s, m, 2, {});
      } else if (i <= 5) {
        pull(s, m);
      } else if (i == 6) {
        pull(s, m);
        hit(s, m, 150, 20, 25, 500);
      } else {
        b.tongue_at = s.level_time + 3;
      }
      return true;
    default:
      return true;  // her pain: frames only
  }
}

// ---- The deaths ----

// The widow's first stage: her beam-out on the 10th frame, her splash and her legs on the 31st;
// the legs' frames, their blasts, their end.
void widow_dies(Q2Server& s, Q2Monster& m) {
  Q2Entity& e = body(s, m);
  Q2Boss& b = m.boss;
  const sh::Vec3f p1 = at_body(e, 13, -44, 69), p2 = at_body(e, 3, 59, 68);
  if (b.legs < 0) {
    b.dying++;
    if (b.dying == 9) {
      for (const sh::Vec3f& p : {p1, p2}) {
        NetEvent ev;
        ev.kind = EventKind::trail_effect;
        ev.to = kEveryone;
        ev.ent = map_row(m.entity);
        ev.level = map_row(m.entity);
        ev.origin = p;
        ev.text = "te_widowbeamout";
        event(s, std::move(ev));
      }
      sound(s, m.entity, "misc/bwidowbeamout.wav", 0);
    }
    if (b.dying == 30) {
      fight_effect(s, "te_widowsplash", p1);
      fight_effect(s, "te_widowsplash", p2);
      fight_effect(s, "te_boss_teleport", e.origin + sh::Vec3f{0, 0, 36});
      b.legs = 0;
      b.legs_at = s.level_time;
      e.box = false;  // the legs are not solid
    }
    return;
  }
  const double t = s.level_time - b.legs_at;
  b.legs = std::min(23, static_cast<int>(t / kStep + 0.001));
  const int tick = static_cast<int>(t / kStep + 0.001);
  if (tick == 17) {
    boom(s, at_body(e, 11.77f, -7.24f, 23.31f));
    debris(s, at_body(e, 11.77f, -7.24f, 23.31f), 2, 2, 1);
  }
  if (tick == 28) {  // 0.5 s before the end
    boom(s, at_body(e, 31, -88.7f, 10.96f));
    boom(s, at_body(e, -12.67f, -4.39f, 15.68f));
  }
  if (tick >= 33) {  // 1 s after the last frame: the end
    const sh::Vec3f a = at_body(e, -65.6f, -8.44f, 28.59f), c = at_body(e, -1.04f, -51.18f, 7.04f);
    boom(s, a);
    boom(s, c);
    debris(s, a, 2, 2, 1);
    widow_gib(s, "models/monsters/blackwidow/gib1/tris.md2", a, 1);
    widow_gib(s, "models/monsters/blackwidow/gib2/tris.md2", a, 1);
    widow_gib(s, "models/monsters/blackwidow/gib1/tris.md2", c, 1);
    widow_gib(s, "models/monsters/blackwidow/gib2/tris.md2", c, 1);
    widow_gib(s, "models/monsters/blackwidow/gib3/tris.md2", c, 1);
    e.gone = true;
  }
}

// The dead search's frames after the chain: 104..118 once, 105..118 three times, then 119..125.
constexpr int kDeadFrames = 15 + 14 * 3 + 7;
int dead_frame(int n) {
  if (n < 15) return 104 + n;
  if (n < 15 + 14 * 3) return 105 + (n - 15) % 14;
  return std::min(125, 119 + (n - 15 - 14 * 3));
}

void blast2(Q2Server& s, sh::Vec3f at) {
  boom(s, at);
  debris(s, at, 1, 3, 2);
}

// The second stage: her blasts by death frame, the chain of thirteen, the dead search, the settle.
void widow2_dies(Q2Server& s, Q2Monster& m) {
  Q2Entity& e = body(s, m);
  Q2Boss& b = m.boss;
  const int n = ++b.dying;  // 1 on the death's first frame
  const sh::Vec3f p29 = at_body(e, -20.11f, -1.11f, 40.76f);
  switch (n) {
    case 3:
      blast2(s, at_body(e, 23.74f, -37.67f, 76.96f));
      break;
    case 6:
      blast2(s, at_body(e, -20.49f, 36.92f, 73.52f));
      break;
    case 18:
      blast2(s, at_body(e, 2.11f, -0.05f, 92.2f));
      break;
    case 25:
      blast2(s, at_body(e, -28.04f, -35.57f, -77.56f));
      break;
    case 29:
    case 34:
    case 38:
      blast2(s, p29);
      break;
    case 30: {  // the leg
      const sh::Vec3f a = at_body(e, -31.89f, -47.86f, 67.02f),
                      c = at_body(e, -44.9f, -82.14f, 54.72f);
      boom(s, a, "te_explosion1_big");
      widow_gib(s, "models/monsters/blackwidow2/gib2/tris.md2", a, 2);
      boom(s, c);
      widow_gib(s, "models/monsters/blackwidow2/gib1/tris.md2", c, 3);
      debris(s, a, 1, 1, 2);
      debris(s, c, 1, 1, 2);
      break;
    }
    default:
      break;
  }
  if (n >= 44 && n <= 56) {  // the chain: a step each 0.1 s
    const int k = n - 43;
    float u = 24 + float(random01(s)) * 15;
    if (k <= 8) u += 24 + float(random01(s)) * 31;
    constexpr float kAt[8][2] = {{-24, -24}, {24, 24}, {24, -24}, {-24, 24},
                                 {-48, -48}, {48, 48}, {-48, 48}, {48, -48}};
    if (k <= 8) {
      const sh::Vec3f at = e.origin + sh::Vec3f{kAt[k - 1][0], kAt[k - 1][1], u};
      boom(s, at, k % 2 == 1 ? "te_explosion1" : "te_explosion1_np");
      if (k == 2 || k == 8) debris(s, at, 2, 2, 2);
      if (k == 4) debris(s, at, 1, 5, 2);
      if (k == 6) {
        const sh::Vec3f arm = at_body(e, 65.76f, 17.52f, 7.56f);
        boom(s, arm, "te_explosion1_big");
        debris(s, arm, 0, 2, 2);
      }
      if (k == 7) {
        const sh::Vec3f arm = at_body(e, 65.76f, 17.52f, 7.56f);
        widow_gib(s, "models/monsters/blackwidow2/gib4/tris.md2", arm, 2);
        debris(s, arm, 1, 0, 2);
      }
    } else if (k <= 12) {
      constexpr float kDiag[4][2] = {{18, 18}, {-18, -18}, {18, -18}, {-18, 18}};
      const sh::Vec3f at = e.origin + sh::Vec3f{kDiag[k - 9][0], kDiag[k - 9][1], 48};
      boom(s, at, "te_explosion1_big");
      if (k == 9) debris(s, at, 1, 5, 2);
    } else {
      const sh::Vec3f at = e.origin + sh::Vec3f{0, 0, u};
      boom(s, at);
      debris(s, at, 1, 4, 3);
    }
  }
  if (n == 57 + kDeadFrames) {  // settled: a body that can be thrown apart
    e.mins = {-70, -70, 0};
    e.maxs = {70, 70, 80};
    e.box = true;
  }
}

// The spare bodies' first class for each boss.
const char* brood_class(Who w) { return w == Who::carrier ? kFlyer : kStalker; }

}  // namespace

bool gz_boss(std::string_view classname) { return who(classname) != Who::none; }
bool gz_boss(const Q2Monster& m) { return who(m) != Who::none; }

void gz_boss_spares(Q2Server& s) {
  const size_t mapped = s.entities.size();
  for (size_t i = 0; i < mapped; i++) {
    const Q2Entity& c = s.entities[i];
    const Who w = who(c.classname);
    if (c.gone || w == Who::none || kind_of(s, c.classname) < 0) continue;
    Q2Entity spare = c;
    spare.classname = brood_class(w);
    spare.targetname.clear();
    spare.target.clear();
    spare.spawnflags = 0;
    spare.block = -2 - static_cast<int>(i);
    for (int n = 0; n < kSpares; n++) s.entities.push_back(spare);  // `c` may move
  }
}

void gz_boss_spawned(Q2Server& s, Q2Monster& m) {
  Q2Entity& e = body(s, m);
  const int sk = std::clamp(skill(s), 0, 3);
  const Who w = who(m);
  int health = 0;
  if (w == Who::carrier) {
    constexpr int kHealth[4] = {2000, 2000, 3000, 4000};
    health = kHealth[sk];
  } else {
    health = (w == Who::widow ? 2000 : 2800) + 1000 * sk;
    if (sk == 3) m.shield = w == Who::widow ? 500 : 750;
  }
  if (coop(s)) health += 500 * sk;
  e.health = e.max_health = health;
  m.boss = Q2Boss{};
}

bool gz_boss_think(Q2Server& s, Q2Monster& m) {
  Q2Boss& b = m.boss;
  const Who w = who(m);
  if (b.move == kNone) {
    if (m.enemy == -1 || !foe_alive(s, m.enemy)) return false;
    const bool attacks = w == Who::carrier ? carrier_attacks(s, m)
                         : w == Who::widow ? widow_attacks(s, m)
                                           : widow2_attacks(s, m);
    if (!attacks || b.move == kNone) return false;
  }
  const int mv = b.move, i = b.frame;
  const Frames f = kFrames[mv];
  const int frame =
      mv == kBlaster ? f.first + b.step : f.first + std::min<int>(i, std::max(0, f.count - 1));
  show(m, frame, f);
  m.busy_until = s.level_time + kStep;
  const bool goes_on = w == Who::carrier ? carrier_frame(s, m, mv, i)
                       : w == Who::widow ? widow_frame(s, m, mv, i)
                                         : widow2_frame(s, m, mv, i);
  if (goes_on && b.move == mv && b.frame == i) {
    b.frame++;
    if (b.frame >= f.count) begin(m, kNone);
  }
  return true;
}

void gz_boss_pain(Q2Server& s, Q2Monster& m, int damage) {
  switch (who(m)) {
    case Who::carrier:
      carrier_pain(s, m, damage);
      return;
    case Who::widow:
      widow_pain(s, m, damage, false);
      return;
    case Who::widow2:
      widow_pain(s, m, damage, true);
      return;
    case Who::none:
      return;
  }
}

void gz_boss_died(Q2Server& s, Q2Monster& m) {
  const Who w = who(m);
  for (int& i : m.boss.brood) {  // a spawn cut short: its grows go back
    if (i >= 0) s.monsters[size_t(i)].grow_until = 0;
    i = -1;
  }
  m.boss.move = kNone;
  m.boss.manual = false;
  m.boss.multiplier = 1;
  m.boss.invulnerable_until = 0;
  m.boss.dying = 0;
  if (w == Who::carrier) sound(s, m.entity, "carrier/death.wav", 0);
  if (w == Who::widow2) sound(s, m.entity, "widow/death.wav", 0);
  if (w != Who::widow2) return;
  for (Q2Monster& o : s.monsters) {  // every stalker in the level dies with her
    if (&o == &m || o.dead || o.waiting || kind(o).classname != std::string_view(kStalker))
      continue;
    const Q2Entity& oe = body(s, o);
    if (oe.gone || oe.health <= 0) continue;
    monster_damage(s, o.entity, -1, oe.health + 1, {});
  }
}

bool gz_boss_lie(Q2Server& s, Q2Monster& m) {
  switch (who(m)) {
    case Who::widow:
      widow_dies(s, m);
      return true;
    case Who::widow2:
      widow2_dies(s, m);
      return true;
    default:
      return false;  // the carrier burns down as the base game's bosses do
  }
}

bool gz_boss_untouchable(const Q2Server& s, const Q2Monster& m) {
  const Who w = who(m);
  if (w == Who::none) return false;
  if (!m.dead) return s.level_time < m.boss.invulnerable_until;
  if (w == Who::widow) return true;  // beaming out, then her legs
  return w == Who::widow2 && m.boss.dying < 57 + kDeadFrames;
}

void gz_boss_row(const Q2Server& s, const Q2Monster& m, ge::shooter::WorldEntity& r) {
  const Who w = who(m);
  if (w == Who::widow && m.boss.legs >= 0) {  // her legs
    const auto at = std::find(s.models.begin(), s.models.end(), kLegs);
    if (at != s.models.end()) r.modelindex = static_cast<int>(at - s.models.begin());
    r.frame = m.boss.legs;
    r.skin = 0;
    return;
  }
  if (w == Who::widow2 && m.dead && m.boss.dying >= 56)
    r.frame = dead_frame(std::min(m.boss.dying - 56, kDeadFrames - 1));
}

BossContent gz_boss_content(std::string_view classname) {
  BossContent c;
  switch (who(classname)) {
    case Who::carrier:
      c = {.kinds = {kFlyer, kKamikaze},
           .models = {kSpawnGrow, "models/items/spawngro/tris.md2"},
           .sounds = {"carrier/pain_sm.wav", "carrier/pain_md.wav", "carrier/pain_lg.wav",
                      "carrier/death.wav", kRailSound, kSpawnSound, "infantry/infatck1.wav",
                      "gunner/gunatck3.wav", "weapons/grenlb1b.wav", "tank/rocket.wav",
                      "weapons/rockfly.wav"}};
      break;
    case Who::widow:
      c = {.kinds = {kStalker},
           .models = {kSpawnGrow, kLegs, "models/monsters/blackwidow/gib1/tris.md2",
                      "models/monsters/blackwidow/gib2/tris.md2",
                      "models/monsters/blackwidow/gib3/tris.md2"},
           .sounds = {"widow/bw1pain1.wav", "widow/bw1pain2.wav", "widow/bw1pain3.wav",
                      "misc/bwidowbeamout.wav", "widow/bwstep2.wav", "widow/bwstep3.wav",
                      "widow/laugh.wav", kRailSound, kSpawnSound, "tank/tnkatck3.wav",
                      "misc/bigtele.wav"}};
      break;
    case Who::widow2:
      c = {.kinds = {kStalker},
           .models = {kSpawnGrow, "models/monsters/blackwidow/gib3/tris.md2",
                      "models/monsters/blackwidow2/gib1/tris.md2",
                      "models/monsters/blackwidow2/gib2/tris.md2",
                      "models/monsters/blackwidow2/gib3/tris.md2",
                      "models/monsters/blackwidow2/gib4/tris.md2"},
           .sounds = {"widow/bw2pain1.wav", "widow/bw2pain2.wav", "widow/bw2pain3.wav",
                      "widow/death.wav", "brain/brnatck3.wav", "weapons/disrupt.wav",
                      "weapons/disint2.wav", "misc/fhit3.wav", "tank/tnkatck3.wav",
                      "infantry/melee2.wav", "bosshovr/bhvunqv1.wav", kSpawnSound}};
      break;
    case Who::none:
      break;
  }
  return c;
}

}  // namespace shooters_edge::server::monster_ai
