// Ground Zero's three spheres (q2_gz.hpp § The spheres): the defender that halves its owner's
// blows and shoots back, the hunter that chases its owner's killer (and the camera his view rides),
// the vengeance that strikes whoever brings its owner low; and the spheres a doppleganger releases.
// Each is a row in the world's shots (`Q2Missile`), its kind one of the three sphere kinds.
//
// ge:summary Ground Zero's defender, hunter and vengeance spheres, and the hunter's camera
// ge:keywords q2 ground-zero rogue sphere defender-sphere hunter-sphere vengeance-sphere
// ge:keywords huntercam doppleganger only-one-sphere half-damage green-bolt
// ge:skill shooter
// ge:test tests/shooters_edge_q2_gz_spheres_test.cpp
#include <algorithm>
#include <cmath>
#include <string>
#include <utility>

#include "core/libm/libm.hpp"
#include "core/num.hpp"
#include "shooters_edge/server/messages.hpp"
#include "shooters_edge/server/q2_fight.hpp"
#include "shooters_edge/server/q2_fight_kit.hpp"
#include "shooters_edge/server/q2_gibs.hpp"
#include "shooters_edge/server/q2_gz.hpp"
#include "shooters_edge/server/q2_items.hpp"
#include "shooters_edge/server/q2_map.hpp"
#include "shooters_edge/server/q2_monsters.hpp"
#include "shooters_edge/server/q2_server.hpp"

namespace shooters_edge::server {

namespace {

namespace num = ge::core::num;

constexpr double kLife = 30;            // a launched sphere's, s
constexpr double kHunt = 15;            // the least a hunting sphere has left, s
constexpr float kChase = 500;           // a hunter's or vengeance's speed, units/s
constexpr double kDefenderEvery = 0.4;  // the defender's shots at most this often, s
constexpr float kBolt = 1000;           // the defender's bolt's speed, units/s
constexpr int kStrike = 10000;          // a hunter's or vengeance's blow on a body
constexpr int kBurst = 512;             // its blast on a thing that takes no blows
constexpr float kBurstRadius = 256;
constexpr double kDegrees = 180.0 / 3.14159265358979323846;

sh::Vec3f unpack(const int32_t v[3]) {
  return {float(v[0]) * 0.125f, float(v[1]) * 0.125f, float(v[2]) * 0.125f};
}

void pack(int32_t out[3], sh::Vec3f v) {
  const float in[3] = {v.x, v.y, v.z};
  for (int i = 0; i < 3; i++)
    out[i] = static_cast<int16_t>(std::clamp(num::to_int32(double(in[i]) * 8), -32768, 32767));
}

bool living(const Q2Server& s, int i) {
  return i >= 0 && i < sh::kMaxClients && s.clients[i].active && !s.clients[i].spectator &&
         s.players[i].alive;
}

bool living_monster(const Q2Server& s, int e) {
  return e >= 0 && size_t(e) < s.entities.size() && !s.entities[size_t(e)].gone &&
         s.entities[size_t(e)].health > 0 && monster_index(s, e) >= 0;
}

// The sphere kind item row `item` launches; the bolt's kind (not a sphere) for any other.
SqShot kind_of(int item) {
  const std::string_view cn = kSqItems[item].classname;
  if (cn == "item_sphere_defender") return SqShot::defender_sphere;
  if (cn == "item_sphere_hunter") return SqShot::hunter_sphere;
  if (cn == "item_sphere_vengeance") return SqShot::vengeance_sphere;
  return SqShot::bolt;
}

int row_of(const Q2Server& s, const Q2Missile& m) { return int(&m - s.missiles); }

// Player `slot`'s own launched sphere, or nullptr.
Q2Missile* owned(Q2Server& s, int slot) {
  const int r = s.players[slot].sphere;
  if (r < 0 || r >= kQ2Missiles) return nullptr;
  Q2Missile& m = s.missiles[r];
  return m.live && gz_sphere_kind(m.kind) && m.owner == slot ? &m : nullptr;
}

// The end-of-life field the bar's timer reads for a sphere of kind `k`.
double Q2Player::*until_of(SqShot k) {
  return k == SqShot::defender_sphere ? &Q2Player::defender_until
         : k == SqShot::hunter_sphere ? &Q2Player::hunter_until
                                      : &Q2Player::vengeance_until;
}

// A doppleganger's sphere: its kills are the doppleganger's, and it is no one's own.
bool decoy(const Q2Missile& m) {
  return m.weapon == kSqByDoppleHunter || m.weapon == kSqByDoppleVengeance;
}

// Sphere `m` gone in a harmless burst (TE 5); its owner's link and timer let go.
void burst(Q2Server& s, Q2Missile& m) {
  fight_effect(s, "te_explosion1", m.origin);
  m.live = false;
  if (m.owner < 0 || m.owner >= sh::kMaxClients) return;
  Q2Player& p = s.players[m.owner];
  if (p.sphere == row_of(s, m)) {
    p.sphere = -1;
    p.*until_of(m.kind) = 0;
  }
  if (p.hunter_cam == row_of(s, m)) p.hunter_cam = -2;  // his view stops where it is
}

// Where a sphere aims at `prey` (a slot, at his eye; a monster, at its middle); false when it is
// no longer there to aim at.
bool aim_at(const Q2Server& s, int prey, sh::Vec3f& at) {
  if (prey >= 0) {
    if (!living(s, prey)) return false;
    const Q2Player& p = s.players[prey];
    at = unpack(p.move.origin) + sh::Vec3f{0, 0, p.view_height};
    return true;
  }
  const int e = -2 - prey;
  if (!living_monster(s, e)) return false;
  const Q2Entity& t = s.entities[size_t(e)];
  at = t.origin + (t.mins + t.maxs) * 0.5f;
  return true;
}

bool sees(const Q2Server& s, sh::Vec3f from, sh::Vec3f to) {
  return map_trace(s, from, {}, {}, to, 1).fraction >= 1;
}

// The yaw and pitch (degrees) that look from `from` to `to`.
sh::Vec3f looking(sh::Vec3f from, sh::Vec3f to) {
  const sh::Vec3f d = to - from;
  const double flat = double(sh::root_of(d.x * d.x + d.y * d.y));
  return {static_cast<float>(-ge::core::libm::atan2(d.z, flat) * kDegrees),
          static_cast<float>(ge::core::libm::atan2(d.y, d.x) * kDegrees), 0};
}

// A yaw turned toward `want` by at most `step` degrees.
float turned(float now, float want, float step) {
  float d = want - now;
  while (d > 180) d -= 360;
  while (d < -180) d += 360;
  return now + std::clamp(d, -step, step);
}

// An angle brought back into 0..360.
float wrapped(float a) {
  while (a >= 360) a -= 360;
  while (a < 0) a += 360;
  return a;
}

// The sphere's prey taken: hunting now, with at least the hunt's 15 s left.
void hunt(Q2Server& s, Q2Missile& m, int prey) {
  m.prey = prey;
  m.stage = 1;
  m.dies_at = std::max(m.dies_at, s.level_time + kHunt);
  m.effects = m.kind == SqShot::hunter_sphere ? 8 | 0x04000000 : 16;  // its glow while it hunts
  sh::Vec3f at;
  if (aim_at(s, prey, at)) m.last_seen = at;
}

// A hunting sphere's touch along `l`: a body takes the strike; a thing that takes no blows, or
// (for the vengeance) the world, a blast; then it bursts. False when it touched nothing it acts on
// (a hunter against the world).
bool strike(Q2Server& s, Q2Missile& m, const Line& l) {
  sh::Vec3f dir = m.velocity;
  sh::normalize(dir);
  const int owner = m.owner;
  if (l.player >= 0) {
    fight_hurt(s, l.player, owner, dir, kStrike, 1, m.weapon, 1);
  } else if (l.mover >= 0 && monster_index(s, l.mover) >= 0) {
    monster_damage(s, l.mover, owner, kStrike, dir, false, &l.end);
  } else if (l.mover >= 0 && map_damage(s, l.mover, owner, kStrike, dir, false, &l.end)) {
    // a thing that takes blows took it
  } else if (l.mover >= 0 || m.kind == SqShot::vengeance_sphere) {
    fight_blast(s, l.end, owner, kBurst, kBurstRadius, m.weapon, owner);
  } else {
    return false;  // a hunter ignores the world
  }
  m.origin = l.end;
  burst(s, m);
  return true;
}

// The sphere's flight this frame along its velocity, touching what it meets (never its owner).
void fly(Q2Server& s, Q2Missile& m) {
  const sh::Vec3f to = m.origin + m.velocity * s.step;
  const uint32_t skip = m.owner >= 0 && m.owner < sh::kMaxClients ? 1u << m.owner : 0u;
  const Line l = fight_line(s, m.origin, to, skip);
  if (l.fraction >= 1) {
    m.origin = to;
    return;
  }
  if (l.sky) {
    m.origin = l.end;
    return;
  }
  if (!strike(s, m, l)) m.origin = l.end + l.normal * 0.5f;  // held at the wall
}

// An idle sphere at its owner: 4 above his box's top, at 5 × the distance a second, leaping there
// on each whole second it cannot see him.
void follow(Q2Server& s, Q2Missile& m) {
  const Q2Player& p = s.players[m.owner];
  const sh::Vec3f spot = unpack(p.move.origin) + sh::Vec3f{0, 0, p.maxs.z + 4};
  const double now = s.level_time;
  const bool whole = std::floor(now + 1e-6) != std::floor(now - double(s.step) + 1e-6);
  if (whole && !sees(s, m.origin, spot)) {
    m.origin = spot;
    m.velocity = {};
    return;
  }
  m.velocity = (spot - m.origin) * 5.0f;
  m.origin = m.origin + m.velocity * s.step;
}

// The defender's shot at its owner's last attacker: a green bolt 2 above itself, 1000 units/s,
// gone after 2 s.
void defend(Q2Server& s, Q2Missile& m) {
  Q2Player& p = s.players[m.owner];
  sh::Vec3f at;
  if (p.hurt_by == -1 || p.hurt_by == m.owner || !aim_at(s, p.hurt_by, at)) {
    p.hurt_by = -1;  // its target dead or gone: it stops shooting
    return;
  }
  if (s.level_time + 1e-6 < m.think_at) return;
  const sh::Vec3f start = m.origin + sh::Vec3f{0, 0, 2};
  if (!sees(s, start, at)) return;
  sh::Vec3f dir = at - start;
  if (sh::normalize(dir) <= 0) return;
  Q2Missile* b = fight_new_missile(s);
  if (b == nullptr) return;
  *b = Q2Missile{};
  b->live = true;
  b->kind = SqShot::green_bolt;
  b->owner = m.owner;
  b->weapon = kSqByDefender;
  b->origin = start;
  b->velocity = dir * kBolt;
  b->damage = 10;
  b->radius_damage = 30;
  b->radius = 128;
  b->dies_at = s.level_time + 2;
  m.think_at = s.level_time + kDefenderEvery;
}

// The defender's bolt in flight: 10 on what it strikes and a 30 splash within 128 round it (never
// on its owner; the splash names no weapon), the green mark on a wall.
void defender_bolt(Q2Server& s, Q2Missile& m) {
  if (s.level_time + 1e-6 >= m.dies_at) {
    m.live = false;
    return;
  }
  const sh::Vec3f to = m.origin + m.velocity * s.step;
  const Line l = fight_line(s, m.origin, to, m.owner >= 0 ? 1u << m.owner : 0u);
  if (l.fraction >= 1) {
    m.origin = to;
    return;
  }
  m.live = false;
  if (l.sky) return;
  sh::Vec3f dir = m.velocity;
  sh::normalize(dir);
  uint32_t skip = 0;
  int skip_entity = -1;
  if (l.player >= 0) {
    fight_hurt(s, l.player, m.owner, dir, m.damage, 1, m.weapon, 4);
    skip = 1u << l.player;
  } else if (l.mover >= 0 && monster_index(s, l.mover) >= 0) {
    monster_damage(s, l.mover, m.owner, m.damage, dir, false, &l.end);
    skip_entity = l.mover;
  } else {
    if (l.mover >= 0) map_damage(s, l.mover, m.owner, m.damage, dir, false, &l.end);
    fight_effect(s, "te_blaster2", l.end, l.normal);
  }
  fight_blast(s, l.end, m.owner, m.radius_damage, m.radius, -1, m.owner, false, skip_entity, skip);
}

// The hunter on its prey: straight at him at 500 while it sees him; else to where it last saw
// him (500 from afar, the distance a second within 500, at once within 20), lurking there.
void chase(Q2Server& s, Q2Missile& m, sh::Vec3f at) {
  if (m.kind == SqShot::vengeance_sphere || sees(s, m.origin, at)) {
    m.last_seen = at;
    m.stage = 1;
    sh::Vec3f dir = at - m.origin;
    if (sh::normalize(dir) > 0) m.velocity = dir * kChase;
  } else {
    sh::Vec3f dir = m.last_seen - m.origin;
    const float d = sh::normalize(dir);
    if (d < 20) {
      m.velocity = (m.last_seen - m.origin) * (1.0f / s.step);
    } else {
      m.velocity = dir * (d > kChase ? kChase : d);
    }
    m.stage = d < 20 ? 2 : 1;  // lurking once it is there
  }
  m.facing = looking(m.origin, at);
  fly(s, m);
}

// The owner's view riding his hunter: at the sphere, turned on its prey.
void ride(Q2Server& s, const Q2Missile& m, sh::Vec3f at) {
  Q2Player& p = s.players[m.owner];
  if (p.alive || p.hunter_cam != row_of(s, m)) return;
  pack(p.move.origin, m.origin);
  pack(p.move.velocity, {});
  p.dead_angles = looking(m.origin, at);
  p.view_angles = p.dead_angles;
}

// The hunter takes its owner's killer; with `huntercam`, the killer far off and no forced
// respawn, the corpse is thrown apart and the owner's view rides the sphere.
void hunter_on(Q2Server& s, Q2Missile& m, int slot, int killer) {
  hunt(s, m, killer);
  Q2Player& p = s.players[slot];
  const auto* cam = s.cvars.find("huntercam");
  if (cam == nullptr || !cam->on() || fight_dmflag(s, 1024)) return;
  const sh::Vec3f at = unpack(p.move.origin);
  if (sh::length(unpack(s.players[killer].move.origin) - at) < 192) return;
  if (!p.gibbed) {
    p.gibbed = true;
    fight_gib(s, slot + 1, at,
              {at + p.mins, at + p.maxs, unpack(p.move.velocity), 0, SqGibSet::player});
  }
  p.hunter_cam = row_of(s, m);
}

void tell(Q2Server& s, int slot, std::string text) {
  NetEvent ev;
  ev.kind = EventKind::print;
  ev.to = 1u << slot;
  ev.reliable = true;
  ev.level = 2;  // high
  ev.text = std::move(text);
  ev.time = s.level_time;
  s.out.events.push_back(std::move(ev));
  s.out.events_total++;
}

}  // namespace

bool gz_use_sphere(Q2Server& s, int slot, int item) {
  const SqShot kind = kind_of(item);
  if (!gz_sphere_kind(kind)) return false;
  Q2Player& p = s.players[slot];
  if (owned(s, slot) != nullptr) {
    tell(s, slot, "Only one sphere at a time!\n");
    return true;
  }
  Q2Missile* m = fight_new_missile(s);
  if (m == nullptr) return true;  // every row in use: kept for later
  p.body.counts[item]--;
  *m = Q2Missile{};
  m->live = true;
  m->kind = kind;
  m->owner = slot;
  m->weapon = kind == SqShot::defender_sphere ? kSqByDefender
              : kind == SqShot::hunter_sphere ? kSqByHunter
                                              : kSqByVengeance;
  m->origin = unpack(p.move.origin) + sh::Vec3f{0, 0, p.maxs.z};
  m->dies_at = s.level_time + kLife;
  m->prey = -1;
  m->effects = 0;
  m->facing = {0, p.view_angles.y, 0};
  p.sphere = row_of(s, *m);
  p.hurt_by = -1;
  p.*until_of(kind) = m->dies_at;
  return true;
}

bool gz_sphere_refused(const Q2Server& s, int slot, int item) {
  if (!gz_sphere_kind(kind_of(item))) return false;
  const int r = s.players[slot].sphere;
  if (r < 0 || r >= kQ2Missiles) return false;
  const Q2Missile& m = s.missiles[r];
  return m.live && gz_sphere_kind(m.kind) && m.owner == slot;
}

int gz_sphere_hurt(Q2Server& s, int target, int by, int damage) {
  Q2Missile* m = owned(s, target);
  if (m == nullptr || m->kind != SqShot::defender_sphere) return damage;
  if (by != -1 && by != target) s.players[target].hurt_by = by;
  return std::max(1, damage / 2);
}

void gz_sphere_hit(Q2Server& s, int target, int by) {
  Q2Missile* m = owned(s, target);
  if (m == nullptr || m->kind != SqShot::vengeance_sphere || m->prey != -1) return;
  if (by == -1 || by == target || s.players[target].health >= 25) return;
  hunt(s, *m, by);
}

void gz_sphere_died(Q2Server& s, int slot, int killer) {
  Q2Missile* m = owned(s, slot);
  if (m == nullptr) return;
  if (m->kind == SqShot::hunter_sphere && m->prey == -1 && killer >= 0 && killer != slot &&
      living(s, killer)) {
    hunter_on(s, *m, slot, killer);
    return;
  }
  if (m->kind == SqShot::defender_sphere || m->prey == -1) burst(s, *m);
}

void gz_sphere_remove(Q2Server& s, int slot) {
  if (Q2Missile* m = owned(s, slot)) burst(s, *m);
}

void gz_release_sphere(Q2Server& s, int owner, int target, sh::Vec3f from, bool hunter) {
  Q2Missile* m = fight_new_missile(s);
  if (m == nullptr) return;
  *m = Q2Missile{};
  m->live = true;
  m->kind = hunter ? SqShot::hunter_sphere : SqShot::vengeance_sphere;
  m->owner = owner;
  m->weapon = hunter ? kSqByDoppleHunter : kSqByDoppleVengeance;
  m->origin = from;
  m->dies_at = s.level_time + kHunt;
  m->facing = {0, owner >= 0 ? s.players[owner].view_angles.y : 0, 0};
  hunt(s, *m, target);
}

bool gz_sphere_view(const Q2Server& s, int slot) {
  const Q2Player& p = s.players[slot];
  return !p.alive && p.hunter_cam != -1;
}

bool gz_sphere_missile(Q2Server& s, Q2Missile& m) {
  if (m.kind == SqShot::green_bolt) {
    if (m.owner < 0 || m.weapon != kSqByDefender) return false;  // a monster's: the base flight
    defender_bolt(s, m);
    return true;
  }
  if (!gz_sphere_kind(m.kind)) return false;
  const bool own = !decoy(m);
  const bool here = m.owner >= 0 && m.owner < sh::kMaxClients && s.clients[m.owner].active;
  // Its end: time run out, the intermission, its owner gone; an idle one's owner dead.
  if (s.level_time + 1e-6 >= m.dies_at || s.intermission_at > 0 || !here) {
    burst(s, m);
    return true;
  }
  Q2Player& p = s.players[m.owner];
  if (own && p.sphere == row_of(s, m) && p.alive) p.*until_of(m.kind) = m.dies_at;
  if (m.prey != -1) {  // hunting
    sh::Vec3f at;
    if (!aim_at(s, m.prey, at)) {  // its prey died or left
      burst(s, m);
      return true;
    }
    chase(s, m, at);
    if (m.live) ride(s, m, at);
    return true;
  }
  if (!p.alive) {  // idle with its owner dead (gz_sphere_died bursts it; a respawn races it)
    burst(s, m);
    return true;
  }
  follow(s, m);
  const float frame = s.step * 10;  // the turns are a frame's at 10 frames a second
  switch (m.kind) {
    case SqShot::defender_sphere:
      defend(s, m);
      break;
    case SqShot::hunter_sphere:
      m.facing.y = turned(m.facing.y, p.view_angles.y, 40 * frame);
      break;
    default:  // the vengeance spins on two axes, 30 degrees a second
      m.facing.x = wrapped(m.facing.x + 30 * s.step);
      m.facing.y = wrapped(m.facing.y + 30 * s.step);
      break;
  }
  return true;
}

}  // namespace shooters_edge::server
