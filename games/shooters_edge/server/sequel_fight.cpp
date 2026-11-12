// The sequel runner's fight (sequel_fight.hpp; the rules in sequel_server.hpp and
// sequel_combat.hpp): shots as lines and as missiles, damage through the armour, radius damage
// and what it can reach, deaths, obituaries and frags, the weapon chosen, the world's rows.
//
// ge:summary The sequel runner's fight: shots, missiles, damage, deaths, the chosen weapon
// ge:keywords sequel fight shots hitscan pellets railgun missiles grenade-bounce radius-damage
// ge:keywords can-damage obituary death frags use-weapon weapnext rocket-jump
// ge:skill shooter
// ge:test tests/shooters_edge_sequel_fight_test.cpp
#include "shooters_edge/server/sequel_fight.hpp"

#include <algorithm>
#include <cstdint>
#include <string_view>
#include <vector>

#include "brush/trace.hpp"
#include "core/libm/libm.hpp"
#include "core/num.hpp"
#include "shooters_edge/server/messages.hpp"
#include "shooters_edge/server/sequel_map.hpp"

namespace shooters_edge::server {

namespace {

namespace num = ge::core::num;

constexpr float kMass = 200;    // a player's
constexpr float kRange = 8192;  // a line shot's reach
constexpr float kStep = 0.1f;   // a rules frame, s
constexpr int32_t kWorldMask = sh::kSqSolid | sh::kSqWindow;
constexpr int32_t kSurfSky = 0x4;  // a sky surface: a shot that reaches it is gone
constexpr uint32_t kEveryone = 0xffffffffu;
constexpr const char* kMissileModels[kSqShotKinds] = {"models/objects/laser/tris.md2",
                                                      "",
                                                      "",
                                                      "models/objects/grenade/tris.md2",
                                                      "models/objects/rocket/tris.md2",
                                                      "",
                                                      "sprites/s_bfg1.sp2",
                                                      "models/objects/grenade2/tris.md2"};
constexpr const char* kClick = "weapons/noammo.wav";
constexpr const char* kBoom = "weapons/rocklx1a.wav";
constexpr const char* kGrenadeBoom = "weapons/grenlx1a.wav";
constexpr const char* kBounce = "weapons/grenlb1b.wav";
constexpr const char* kHandBounce = "weapons/hgrenb1a.wav";
constexpr const char* kFuse = "weapons/hgrenc1b.wav";  // the hand grenade's fuse lit
constexpr const char* kGib = "misc/udeath.wav";
constexpr float kGibHealth = -40;  // a death under this throws the body apart
constexpr float kItemHalf = 15;    // a dropped item's box, ±15 each way

bool grenade(SqShot k) { return k == SqShot::grenade || k == SqShot::hand_grenade; }

bool deathmatch(const SequelServer& s) { return s.cvars.find("deathmatch")->on(); }
bool dmflag(const SequelServer& s, int bit) {
  return deathmatch(s) && (s.cvars.find("dmflags")->integer() & bit) != 0;
}
int skill(const SequelServer& s) {
  return std::clamp(static_cast<int>(s.cvars.find("skill")->number), 0, 3);
}

sh::Vec3f unpack(const int16_t v[3]) {
  return {float(v[0]) * 0.125f, float(v[1]) * 0.125f, float(v[2]) * 0.125f};
}
void pack(int16_t out[3], sh::Vec3f v) {
  const float in[3] = {v.x, v.y, v.z};
  for (int i = 0; i < 3; i++)
    out[i] = static_cast<int16_t>(std::clamp(num::to_int32(double(in[i]) * 8), -32768, 32767));
}

sh::Vec3f centre_of(const SequelPlayer& p) {
  return unpack(p.move.origin) + (p.mins + p.maxs) * 0.5f;
}

bool fightable(const SequelServer& s, int i) {
  return s.clients[i].active && !s.clients[i].spectator && s.players[i].alive;
}

float crandom(SequelServer& s) { return float(2 * (ge::core::next(s.rng) - 0.5)); }

const std::string& name_of(const SequelServer& s, int slot) {
  static const std::string kNobody = "someone";
  if (slot < 0 || slot >= sh::kMaxClients) return kNobody;
  for (const auto& [k, v] : s.clients[slot].userinfo)
    if (k == "name") return v;
  return kNobody;
}

void event(SequelServer& s, NetEvent ev) {
  ev.time = s.level_time;
  s.out.events.push_back(std::move(ev));
  s.out.events_total++;
}

void sound(SequelServer& s, int ent, sh::Vec3f at, const char* sample) {
  NetEvent ev;
  ev.kind = EventKind::sound;
  ev.to = kEveryone;
  ev.ent = ent;
  ev.level = 1;  // the weapon's channel
  ev.origin = at;
  ev.values[0] = 255;
  ev.values[1] = 1;
  ev.text = sample;
  event(s, std::move(ev));
}

void effect(SequelServer& s, const char* name, sh::Vec3f at) {
  NetEvent ev;
  ev.kind = EventKind::point_effect;
  ev.to = kEveryone;
  ev.origin = at;
  ev.values[1] = 1;
  ev.text = name;
  event(s, std::move(ev));
}

void print_to(SequelServer& s, int slot, std::string text) {
  NetEvent ev;
  ev.kind = EventKind::print;
  ev.to = 1u << slot;
  ev.reliable = true;
  ev.level = 2;
  ev.text = std::move(text);
  event(s, std::move(ev));
}

void print_all(SequelServer& s, std::string text) {
  NetEvent ev;
  ev.kind = EventKind::print;
  ev.to = kEveryone;
  ev.reliable = true;
  ev.level = 1;
  ev.text = std::move(text);
  event(s, std::move(ev));
}

// What a line met first: the world (a wall, the sky) or a player.
struct Line {
  float fraction = 1;
  sh::Vec3f end;
  sh::Vec3f normal;
  bool sky = false;
  int player = -1;
  int mover = -1;  // the map's entity it met (a door, a plat), when it met one
};

// The entry of the segment into a box, 0..1, or -1 when it misses (or starts inside: 0).
float enter_box(sh::Vec3f from, sh::Vec3f to, sh::Vec3f lo, sh::Vec3f hi) {
  const float a[3] = {from.x, from.y, from.z}, b[3] = {to.x, to.y, to.z};
  const float l[3] = {lo.x, lo.y, lo.z}, h[3] = {hi.x, hi.y, hi.z};
  float t0 = 0, t1 = 1;
  for (int k = 0; k < 3; k++) {
    const float d = b[k] - a[k];
    if (d == 0) {
      if (a[k] < l[k] || a[k] > h[k]) return -1;
      continue;
    }
    float near = (l[k] - a[k]) / d, far = (h[k] - a[k]) / d;
    if (near > far) std::swap(near, far);
    t0 = std::max(t0, near);
    t1 = std::min(t1, far);
    if (t0 > t1) return -1;
  }
  return t0;
}

// A line from `from` to `to` through the walls and the living players (not those in `skip`, a
// bit per slot).
Line shoot_line(SequelServer& s, sh::Vec3f from, sh::Vec3f to, uint32_t skip) {
  Line out;
  const sh::SequelTrace t = map_trace(s, from, {}, {}, to, kWorldMask);
  out.fraction = t.start_solid ? 0 : t.fraction;
  out.normal = t.normal;
  out.sky = t.fraction < 1 && (t.surface_flags & kSurfSky) != 0;
  if (t.fraction < 1) out.mover = map_entity_of_row(s, t.entity);
  for (int i = 0; i < sh::kMaxClients; i++) {
    if (((skip >> i) & 1) != 0 || !fightable(s, i)) continue;
    const SequelPlayer& p = s.players[i];
    const sh::Vec3f at = unpack(p.move.origin);
    const float f = enter_box(from, to, at + p.mins, at + p.maxs);
    if (f >= 0 && f < out.fraction) {
      out.fraction = f;
      out.player = i;
      out.sky = false;
    }
  }
  if (out.player >= 0) out.mover = -1;
  out.end = from + (to - from) * out.fraction;
  return out;
}

// Whether a blast at `at` reaches player `i`: a clear line to his origin, or to it moved 15 each
// way in x and y.
bool can_damage(SequelServer& s, sh::Vec3f at, int i) {
  const sh::Vec3f o = unpack(s.players[i].move.origin);
  const sh::Vec3f tries[5] = {o, o + sh::Vec3f{15, 15, 0}, o + sh::Vec3f{15, -15, 0},
                              o + sh::Vec3f{-15, 15, 0}, o + sh::Vec3f{-15, -15, 0}};
  for (const sh::Vec3f& t : tries)
    if (map_trace(s, at, {}, {}, t, kWorldMask).fraction >= 1) return true;
  return false;
}

// A blow on player `target` from `attacker` (a slot; -1 the world) along `dir`: pushed, shared
// with the armour, the rest off his health; at 0 he dies.
// Power armour's part of a blow along `dir` (the screen only from in front: the way back to where
// it came from within ~73° of his facing), its cells taken.
int power_save(SequelPlayer& p, sh::Vec3f dir, int damage) {
  const int screen = p.body.counts[sq_item("item_power_screen")];
  const int shield = p.body.counts[sq_item("item_power_shield")];
  int& cells = p.body.ammo[static_cast<int>(SqAmmo::cells)];
  if (!p.power_on || (screen == 0 && shield == 0) || cells <= 0) return 0;
  int part = 0, per_cell = 1;
  if (shield > 0) {
    part = damage * 2 / 3;
    per_cell = 2;
  } else {
    const sh::ViewAxes ax = sh::angle_vectors({0, p.view_angles.y, 0});
    if (sh::dot(ax.forward, dir * -1.0f) <= 0.3f) return 0;  // a hit from the side or behind
    part = damage / 3;
  }
  const int saved = std::min(part, cells * per_cell);
  cells -= saved / per_cell;
  return saved;
}

void hurt(SequelServer& s, int target, int attacker, sh::Vec3f dir, int damage, int knockback,
          bool energy, int weapon, bool no_armour = false, bool past_invulnerable = false) {
  if (!fightable(s, target) || damage <= 0) return;
  SequelPlayer& p = s.players[target];
  if (!deathmatch(s) && skill(s) == 0) damage = std::max(1, damage / 2);
  const bool along = sh::normalize(dir) > 0;
  if (knockback > 0 && along) {
    const float push = sq_knockback(knockback, kMass, attacker == target);
    pack(p.move.velocity, unpack(p.move.velocity) + dir * push);
  }
  if (p.invulnerable_until > s.level_time && !past_invulnerable) return;  // the push lands
  int saved = 0;
  if (!no_armour) {
    if (along) saved += power_save(p, dir, damage);
    saved += sq_armour_save(p.body, damage - saved, energy);
  }
  p.health -= float(damage - saved);
  if (p.health <= 0) {
    p.health = std::max(p.health, -999.0f);
    fight_kill(s, target, attacker, weapon);
  }
}

// Radius damage from `at`: each living player within `radius` of his box takes `damage` less half
// the distance to his box's centre (halved for the attacker), when the blast can reach him.
void blast(SequelServer& s, sh::Vec3f at, int attacker, int damage, float radius, int ignore,
           int weapon, bool energy = false) {
  for (int i = 0; i < sh::kMaxClients; i++) {
    if (i == ignore || !fightable(s, i)) continue;
    const SequelPlayer& p = s.players[i];
    const sh::Vec3f o = unpack(p.move.origin);
    const sh::Vec3f lo = o + p.mins, hi = o + p.maxs;
    const sh::Vec3f near{std::clamp(at.x, lo.x, hi.x), std::clamp(at.y, lo.y, hi.y),
                         std::clamp(at.z, lo.z, hi.z)};
    if (sh::length(near - at) > radius) continue;
    float points = float(damage) - 0.5f * sh::length(at - centre_of(p));
    if (i == attacker) points *= 0.5f;
    if (points <= 0 || !can_damage(s, at, i)) continue;
    hurt(s, i, attacker, o - at, static_cast<int>(points), static_cast<int>(points), energy,
         weapon);
  }
  // The map's shootable movers (a door or button with health): less half the distance to the
  // middle of its box, when within the radius.
  for (size_t e = 0; e < s.entities.size(); e++) {
    const SequelEntity& m = s.entities[e];
    if (m.gone || m.health <= 0) continue;
    const sh::Vec3f mid = m.origin + (m.mins + m.maxs) * 0.5f;
    const float d = sh::length(mid - at);
    const float points = float(damage) - 0.5f * d;
    if (points > 0) map_damage(s, static_cast<int>(e), attacker, static_cast<int>(points));
  }
}

SequelMissile* new_missile(SequelServer& s) {
  for (SequelMissile& m : s.missiles)
    if (!m.live) return &m;
  return nullptr;  // every row in flight: the shot is lost
}

size_t missile_row(const SequelServer& s, const SequelMissile& m) {
  return fight_first_row(s) + size_t(&m - s.missiles);
}

// A missile explodes at `at`: its radius damage (the one it hit directly left out), the sound and
// the flash; the BFG's ball also strikes everything within 1000 it and its owner can both reach.
void explode(SequelServer& s, SequelMissile& m, sh::Vec3f at, int ignore) {
  m.live = false;
  sound(s, int(missile_row(s, m)), at, grenade(m.kind) ? kGrenadeBoom : kBoom);
  effect(s, m.kind == SqShot::bfg ? "te_bfg_bigexplosion" : "te_explosion", at);
  blast(s, at, m.owner, m.radius_damage, m.radius, ignore, m.weapon, m.kind == SqShot::bfg);
  if (m.kind != SqShot::bfg) return;
  for (int i = 0; i < sh::kMaxClients; i++) {
    if (i == m.owner || !fightable(s, i)) continue;
    const float d = sh::length(unpack(s.players[i].move.origin) - at);
    if (d > 1000 || !can_damage(s, at, i)) continue;
    if (m.owner >= 0 && !can_damage(s, unpack(s.players[m.owner].move.origin), i)) continue;
    const float points = float(m.damage) * (1 - sh::root_of(d / 1000));
    hurt(s, i, m.owner, unpack(s.players[i].move.origin) - at, static_cast<int>(points), 0, true,
         m.weapon);
  }
}

// A bullet or a pellet from `start` along the axes, spread `h` right and `v` up.
void bullet(SequelServer& s, int slot, sh::Vec3f origin, sh::Vec3f start, const sh::ViewAxes& ax,
            float h, float v, int damage, int kick, int weapon) {
  const uint32_t me = 1u << slot;
  Line l = shoot_line(s, origin, start, me);  // the shooter's own reach to the start
  if (l.fraction >= 1) {
    // The draws in a fixed order: operands of one expression are evaluated in an order the
    // compiler picks, so two draws in one would differ from compiler to compiler.
    const float across = crandom(s) * h;
    const float up = crandom(s) * v;
    const sh::Vec3f end = start + ax.forward * kRange + ax.right * across + ax.up * up;
    l = shoot_line(s, start, end, me);
  }
  if (l.player >= 0) {
    hurt(s, l.player, slot, ax.forward, damage, kick, false, weapon);
    effect(s, "te_blood", l.end);
  } else if (l.fraction < 1 && !l.sky) {
    map_damage(s, l.mover, slot, damage);
    effect(s, "te_gunshot", l.end);
  }
}

// The railgun's line: through every player it meets to the first wall.
void rail(SequelServer& s, int slot, sh::Vec3f start, const sh::ViewAxes& ax, int damage, int kick,
          int weapon) {
  uint32_t skip = 1u << slot;
  const sh::Vec3f end = start + ax.forward * kRange;
  for (int n = 0; n <= sh::kMaxClients; n++) {
    const Line l = shoot_line(s, start, end, skip);
    if (l.player < 0) {
      map_damage(s, l.mover, slot, damage);
      effect(s, "te_railtrail", l.end);
      return;
    }
    hurt(s, l.player, slot, ax.forward, damage, kick, false, weapon);
    skip |= 1u << l.player;
  }
}

// A missile from `start` along the axes; the hand grenade's `speed` and `fuse` are its throw's.
void launch(SequelServer& s, int slot, SqShot kind, sh::Vec3f origin, sh::Vec3f start,
            const sh::ViewAxes& ax, int damage, int quad, int weapon, float speed = 600,
            double fuse = 2.5) {
  const Line reach = shoot_line(s, origin, start, 1u << slot);
  SequelMissile* m = new_missile(s);
  if (m == nullptr) return;
  *m = SequelMissile{};
  m->live = true;
  m->kind = kind;
  m->owner = slot;
  m->weapon = weapon;
  m->origin = start;
  m->damage = damage;
  switch (kind) {
    case SqShot::bolt:
      m->velocity = ax.forward * 1000;
      m->dies_at = s.level_time + 2;
      break;
    case SqShot::grenade:
    case SqShot::hand_grenade: {
      const float up = 200 + crandom(s) * 10;  // the draws in a fixed order (above)
      const float across = crandom(s) * 10;
      m->velocity = ax.forward * speed + ax.up * up + ax.right * across;
      m->dies_at = s.level_time + fuse;
      m->radius_damage = damage;
      m->radius = kind == SqShot::grenade ? 160 : 165;  // the quad does not widen it
      break;
    }
    case SqShot::rocket:
      m->velocity = ax.forward * 650;
      m->dies_at = s.level_time + 8000.0 / 650.0;
      m->damage = damage + ge::core::int_in(s.rng, 0, 20) * quad;
      m->radius_damage = 120 * quad;
      m->radius = 120;
      break;
    default:  // the BFG's ball
      m->velocity = ax.forward * 400;
      m->dies_at = s.level_time + 20;
      m->radius_damage = 200 * quad;
      m->radius = 100;
      break;
  }
  if (reach.fraction < 1) {  // the start is behind a wall: it meets it at once
    m->origin = reach.end - ax.forward * 10;
    if (reach.player >= 0) hurt(s, reach.player, slot, ax.forward, m->damage, 1, false, weapon);
    if (kind == SqShot::bolt)
      m->live = false;
    else if (!grenade(kind))
      explode(s, *m, m->origin, reach.player);
  }
}

// The hand grenade let go at `speed` with `fuse` left, or (no speed) held to the end of its fuse:
// it explodes in the hand.
void throw_grenade(SequelServer& s, int slot, float speed, double fuse) {
  SequelPlayer& p = s.players[slot];
  const int w = int(SqWeapon::hand_grenade);
  const SqWeaponRow& r = kSqWeapons[w];
  const sh::Vec3f origin = unpack(p.move.origin);
  const sh::ViewAxes ax = sh::angle_vectors(p.view_angles);
  const sh::Vec3f eye = origin + sh::Vec3f{0, 0, p.view_height - 8};
  const int quad = p.quad_until > s.level_time ? 4 : 1;
  const int damage = r.damage_dm * quad;
  if (speed <= 0) {
    SequelMissile* m = new_missile(s);
    if (m == nullptr) return;
    *m = SequelMissile{};
    m->live = true;
    m->kind = SqShot::hand_grenade;
    m->owner = slot;
    m->weapon = kSqByHeldGrenade;
    m->origin = origin;
    m->radius_damage = damage;
    m->radius = 165;
    explode(s, *m, origin, -1);
    return;
  }
  if (p.silencer_shots > 0)
    p.silencer_shots--;
  else
    sound(s, slot + 1, origin, r.sound);
  launch(s, slot, SqShot::hand_grenade, origin, eye + ax.forward * r.forward + ax.right * r.right,
         ax, damage, quad, w, speed, fuse);
}

void fire(SequelServer& s, int slot, int shots) {
  SequelPlayer& p = s.players[slot];
  const int w = p.hand.weapon;
  const SqWeaponRow& r = kSqWeapons[w];
  const sh::Vec3f origin = unpack(p.move.origin);
  const sh::ViewAxes ax = sh::angle_vectors(p.view_angles);
  const sh::Vec3f eye = origin + sh::Vec3f{0, 0, p.view_height - 8};
  const sh::Vec3f start = eye + ax.forward * r.forward + ax.right * r.right;
  const bool dm = deathmatch(s);
  const int quad = p.quad_until > s.level_time ? 4 : 1;  // ×4 damage and kick
  const int damage = (dm ? r.damage_dm : r.damage_sp) * quad;
  const int kick = r.kick * quad;
  if (p.silencer_shots > 0)
    p.silencer_shots--;
  else
    sound(s, slot + 1, origin, r.sound);
  switch (r.shot) {
    case SqShot::pellets:
      if (w == int(SqWeapon::super_shotgun)) {
        for (const float turn : {-5.0f, 5.0f}) {
          const sh::ViewAxes half = sh::angle_vectors(p.view_angles + sh::Vec3f{0, turn, 0});
          for (int i = 0; i < 10; i++)
            bullet(s, slot, origin, start, half, 1000, 500, damage, kick, w);
        }
      } else {
        for (int i = 0; i < 12; i++) bullet(s, slot, origin, start, ax, 500, 500, damage, kick, w);
      }
      return;
    case SqShot::bullet:
      for (int i = 0; i < shots; i++) {
        sh::Vec3f from = start;
        if (w == int(SqWeapon::chaingun)) {
          const float across = 7 + crandom(s) * 4;  // the draws in a fixed order (above)
          const float up = crandom(s) * 4;
          from = eye + ax.forward * r.forward + ax.right * across + ax.up * up;
        }
        bullet(s, slot, origin, from, ax, 300, 500, damage, kick, w);
      }
      return;
    case SqShot::rail:
      rail(s, slot, start, ax, damage, (dm ? 200 : 250) * quad, w);
      return;
    default:
      for (int i = 0; i < shots; i++) launch(s, slot, r.shot, origin, start, ax, damage, quad, w);
      return;
  }
}

// Item `item` thrown from player `slot`: 100 forward along his view turned `turn` degrees, 300
// up, from 24 ahead and 16 below his eye (nearer when a wall is closer); his to take back after
// 1 s, gone after 30 s in deathmatch. Null when every drop's row is in use.
SequelDrop* toss(SequelServer& s, int slot, int item, float turn) {
  SequelDrop* d = nullptr;
  for (SequelDrop& free : s.drops)
    if (!free.live) {
      d = &free;
      break;
    }
  if (d == nullptr) return nullptr;
  const SequelPlayer& p = s.players[slot];
  const sh::Vec3f origin = unpack(p.move.origin);
  const sh::ViewAxes ax = sh::angle_vectors(p.view_angles + sh::Vec3f{0, turn, 0});
  sh::Vec3f start = origin + sh::Vec3f{0, 0, p.view_height - 16} + ax.forward * 24;
  const float a[3] = {origin.x, origin.y, origin.z}, b[3] = {start.x, start.y, start.z};
  const float lo[3] = {-kItemHalf, -kItemHalf, -kItemHalf};
  const float hi[3] = {kItemHalf, kItemHalf, kItemHalf};
  const ge::brush::Trace t = s.tracer->trace(a, b, lo, hi, kWorldMask);
  if (t.fraction < 1) start = {t.end[0], t.end[1], t.end[2]};
  *d = SequelDrop{};
  d->live = true;
  d->item = item;
  d->owner = slot;
  d->origin = start;
  d->velocity = ax.forward * 100;
  d->velocity.z = 300;
  d->takeable_at = s.level_time + 1;
  d->dies_at = deathmatch(s) ? s.level_time + 30 : 0;
  return d;
}

const char* obituary(int weapon) {
  switch (static_cast<SqWeapon>(weapon)) {
    case SqWeapon::blaster:
      return " was zapped by %'s blaster.";
    case SqWeapon::shotgun:
      return " caught %'s buckshot.";
    case SqWeapon::super_shotgun:
      return " took both barrels from %.";
    case SqWeapon::machinegun:
      return " was stitched up by %'s machinegun.";
    case SqWeapon::chaingun:
      return " was shredded by %'s chaingun.";
    case SqWeapon::grenade_launcher:
      return " tripped over %'s grenade.";
    case SqWeapon::rocket_launcher:
      return " was caught in %'s rocket blast.";
    case SqWeapon::hyperblaster:
      return " was scorched by %'s hyperblaster.";
    case SqWeapon::railgun:
      return " was pierced by %'s rail.";
    case SqWeapon::bfg:
      return " vanished in %'s BFG blast.";
    case SqWeapon::hand_grenade:
      return " caught %'s hand grenade.";
  }
  if (weapon == kSqByHeldGrenade) return " was too close to %'s last grenade.";
  if (weapon == kSqByTelefrag) return " was in the way of %'s teleport.";
  if (weapon == kSqByExplosion) return " was caught in %'s explosion.";
  return " was killed by %.";
}

}  // namespace

size_t fight_first_row(const SequelServer& s) { return 1 + sh::kMaxClients + s.entities.size(); }

void fight_precache(SequelServer& s) {
  const auto model = [&s](const char* name) {
    if (name[0] == '\0') return 0;
    const auto found = std::find(s.models.begin(), s.models.end(), name);
    if (found != s.models.end()) return static_cast<int>(found - s.models.begin());
    s.models.emplace_back(name);
    return static_cast<int>(s.models.size()) - 1;
  };
  for (int w = 0; w < kSqWeaponCount; w++) s.weapon_models[w] = model(kSqWeapons[w].view_model);
  for (int k = 0; k < kSqShotKinds; k++) s.missile_models[k] = model(kMissileModels[k]);
  const auto add_sound = [&s](const char* name) {
    if (std::find(s.sounds.begin(), s.sounds.end(), name) == s.sounds.end())
      s.sounds.emplace_back(name);
  };
  for (const SqWeaponRow& r : kSqWeapons) add_sound(r.sound);
  for (const char* name : {kClick, kBoom, kGrenadeBoom, kBounce, kHandBounce, kFuse, kGib})
    add_sound(name);
}

void fight_spawn(SequelServer& s, int slot) {
  SequelPlayer& p = s.players[slot];
  p.hand = SqHand{};
  p.stepped = false;
  p.killer = -1;
  p.quad_until = p.invulnerable_until = p.breather_until = p.suit_until = 0;
  p.silencer_shots = 0;
  p.power_on = false;
  p.air_until = s.level_time + 12;
  p.drown_at = 0;
  p.drown_damage = 2;
  p.old_fall = 0;
  p.gibbed = false;
}

bool fight_use(SequelServer& s, int slot, int item) {
  if (item < 0 || item >= kSqItemCount || !fightable(s, slot)) return false;
  SequelPlayer& p = s.players[slot];
  const SqItem& it = kSqItems[item];
  if (p.body.counts[item] <= 0) return false;
  if (it.kind == SqKind::power_armour) {
    p.power_on = !p.power_on;
    return true;
  }
  if (it.kind != SqKind::powerup) return false;
  p.body.counts[item]--;
  const auto extend = [&s, &it](double& until) {
    until = std::max(until, s.level_time) + it.amount;
  };
  const std::string_view name = it.classname;
  if (name == "item_quad")
    extend(p.quad_until);
  else if (name == "item_invulnerability")
    extend(p.invulnerable_until);
  else if (name == "item_silencer")
    p.silencer_shots += it.amount;
  else if (name == "item_breather")
    extend(p.breather_until);
  else if (name == "item_enviro")
    extend(p.suit_until);
  return true;
}

void fight_body_frame(SequelServer& s, int slot) {
  if (!fightable(s, slot)) return;
  SequelPlayer& p = s.players[slot];
  const double now = s.level_time;
  const bool suit = p.suit_until > now;
  // Air: refilled by the rebreather or the suit; once it runs out under water, a blow each second
  // that grows by 2 from 2 to 15, no armour against it; surfacing resets both.
  if (p.breather_until > now || suit) p.air_until = std::max(p.air_until, now + 10);
  if (p.water_level < 3) {
    p.air_until = std::max(p.air_until, now + 12);
    p.drown_damage = 2;
  } else if (now >= p.air_until && now >= p.drown_at) {
    p.drown_at = now + 1;
    const int blow = p.drown_damage;
    p.drown_damage = std::min(p.drown_damage + 2, 15);
    hurt(s, slot, -1, {}, blow, 0, false, kSqByWater, true);
    if (!p.alive) return;
  }
  // Lava and slime each frame he is in them, by how deep.
  if (p.water_level > 0 && (p.water_type & sh::kSqLava) != 0) {
    hurt(s, slot, -1, {}, (suit ? 1 : 3) * p.water_level, 0, false, kSqByLava);
    if (!p.alive) return;
  }
  if (p.water_level > 0 && (p.water_type & sh::kSqSlime) != 0 && !suit) {
    hurt(s, slot, -1, {}, p.water_level, 0, false, kSqBySlime);
    if (!p.alive) return;
  }
  // Falling: the fall speed lost since the last frame (the last frame's when still falling but
  // slower, else the change on landing), squared × 0.0001; less in water.
  const float vz = unpack(p.move.velocity).z;
  float delta = 0;
  if (p.old_fall < 0 && vz > p.old_fall && !p.on_ground)
    delta = p.old_fall;
  else if (p.on_ground)
    delta = vz - p.old_fall;
  p.old_fall = vz;
  delta = delta * delta * 0.0001f;
  if (p.water_level == 3) return;
  if (p.water_level == 2) delta *= 0.25f;
  if (p.water_level == 1) delta *= 0.5f;
  const bool no_falling = deathmatch(s) && (s.cvars.find("dmflags")->integer() & 8) != 0;
  if (delta <= 30 || no_falling) return;
  hurt(s, slot, -1, {0, 0, -1}, std::max(1, static_cast<int>((delta - 30) / 2)), 0, false,
       kSqByFall);
}

bool fight_limits(SequelServer& s) {
  if (!s.pending_map.empty() || s.intermission_at > 0) return false;
  const int frags = static_cast<int>(s.cvars.find("fraglimit")->number);
  const double minutes = s.cvars.find("timelimit")->number;
  std::string why;
  if (minutes > 0 && s.level_time >= minutes * 60) why = "Time is up.";
  for (int i = 0; i < sh::kMaxClients && why.empty() && frags > 0; i++)
    if (s.clients[i].active && s.players[i].frags >= float(frags))
      why = name_of(s, i) + " reached the frag limit.";
  if (why.empty()) return false;
  print_all(s, why + "\n");
  // The next map of `sv_maplist` (spaces or commas, wrapping), else this one again.
  std::vector<std::string> list;
  std::string word;
  for (const char c : s.cvars.find("sv_maplist")->value + " ") {
    if (c == ' ' || c == ',') {
      if (!word.empty() && word != "0") list.push_back(word);  // an unset list reads "0"
      word.clear();
    } else {
      word += c;
    }
  }
  std::string next = list.empty() ? s.map_name : list[0];
  for (size_t i = 0; i < list.size(); i++)
    if (list[i] == s.map_name) next = list[(i + 1) % list.size()];
  s.next_map = dmflag(s, 32) ? s.map_name : next;  // the same level again
  return true;
}

void fight_clear(SequelServer& s) {
  for (SequelMissile& m : s.missiles) m = SequelMissile{};
  for (SequelDrop& d : s.drops) d = SequelDrop{};
  s.obituaries.clear();
}

void fight_step(SequelServer& s, int slot, bool attack) {
  SequelPlayer& p = s.players[slot];
  if (!fightable(s, slot)) return;
  const SqStep step = sq_hand_step(p.hand, p.body, attack, s.level_time, dmflag(s, 8192));
  if (step.click) sound(s, slot + 1, unpack(p.move.origin), kClick);
  if (step.lit) sound(s, slot + 1, unpack(p.move.origin), kFuse);
  if (step.shots > 0 && p.hand.weapon == int(SqWeapon::hand_grenade))
    throw_grenade(s, slot, step.throw_speed, step.fuse_left);
  else if (step.shots > 0)
    fire(s, slot, step.shots);
}

void fight_missiles(SequelServer& s) {
  const float gravity = static_cast<float>(s.cvars.find("sv_gravity")->number);
  for (SequelMissile& m : s.missiles) {
    if (!m.live) continue;
    if (s.level_time + 0.001 >= m.dies_at) {
      if (grenade(m.kind))
        explode(s, m, m.origin, -1);
      else
        m.live = false;
      continue;
    }
    if (m.resting) continue;
    if (grenade(m.kind)) m.velocity.z -= gravity * kStep;
    const sh::Vec3f to = m.origin + m.velocity * kStep;
    const uint32_t owner = m.owner >= 0 ? 1u << m.owner : 0u;
    const Line l = shoot_line(s, m.origin, to, owner);
    if (l.player >= 0) {
      const int hit = l.player;
      switch (m.kind) {
        case SqShot::bolt:
          hurt(s, hit, m.owner, m.velocity, m.damage, 1, true, m.weapon);
          m.live = false;
          break;
        case SqShot::grenade:
        case SqShot::hand_grenade: {
          const float d = sh::length(l.end - centre_of(s.players[hit]));
          hurt(s, hit, m.owner, m.velocity, static_cast<int>(float(m.damage) - 0.5f * d),
               static_cast<int>(float(m.damage) - 0.5f * d), false, m.weapon);
          explode(s, m, l.end, hit);
          break;
        }
        default:
          hurt(s, hit, m.owner, m.velocity, m.damage, 0, m.kind == SqShot::bfg, m.weapon);
          explode(s, m, l.end, hit);
          break;
      }
      continue;
    }
    if (l.fraction < 1) {
      if (l.sky) {
        m.live = false;
        continue;
      }
      switch (m.kind) {
        case SqShot::bolt:
          effect(s, "te_blaster", l.end);
          m.live = false;
          break;
        case SqShot::grenade:
        case SqShot::hand_grenade: {  // a bounce off the wall
          m.origin = l.end + l.normal * 0.1f;
          m.velocity = m.velocity - l.normal * (1.5f * sh::dot(m.velocity, l.normal));
          if (l.normal.z > 0.7f && sh::length(m.velocity) < 60) {
            m.velocity = {};
            m.resting = true;
          }
          sound(s, int(missile_row(s, m)), m.origin,
                m.kind == SqShot::hand_grenade ? kHandBounce : kBounce);
          break;
        }
        default:
          explode(s, m, l.end - m.velocity * (0.001f), -1);
          break;
      }
      continue;
    }
    m.origin = to;
    if (m.kind != SqShot::bfg) continue;
    // The ball's lasers: every player within 256 it can see takes a light blow.
    for (int i = 0; i < sh::kMaxClients; i++) {
      if (i == m.owner || !fightable(s, i)) continue;
      const sh::Vec3f c = centre_of(s.players[i]);
      if (sh::length(c - m.origin) > 256) continue;
      if (shoot_line(s, m.origin, c, owner).player != i) continue;
      hurt(s, i, m.owner, c - m.origin, deathmatch(s) ? 5 : 10, 0, true, m.weapon);
    }
  }
}

void fight_choose(SequelServer& s, int slot, int w) {
  SequelPlayer& p = s.players[slot];
  if (fightable(s, slot)) sq_choose(p.hand, p.body, w);
}

void fight_took(SequelServer& s, int slot, int item) {
  const int w = sq_weapon_of_item(item);
  SequelPlayer& p = s.players[slot];
  const SqItem& it = kSqItems[item];
  const bool dm = deathmatch(s);
  if (it.kind == SqKind::power_armour && dm && p.body.counts[item] == 1) p.power_on = true;
  if (it.kind == SqKind::powerup && dm && (s.cvars.find("dmflags")->integer() & 16) != 0)
    fight_use(s, slot, item);                     // instant items
  if (w < 0 || p.body.counts[item] != 1) return;  // only a newly owned weapon
  if (!deathmatch(s) || p.hand.weapon == int(SqWeapon::blaster)) sq_choose(p.hand, p.body, w);
}

bool fight_command(SequelServer& s, int slot, const std::string& cmd, const std::string& arg) {
  SequelPlayer& p = s.players[slot];
  if (cmd == "use") {
    const int w = sq_weapon_named(arg);
    if (w >= 0)
      fight_choose(s, slot, w);
    else
      fight_use(s, slot, sq_item_named(arg));
    return true;
  }
  if (cmd == "drop") {
    const int item = sq_item_named(arg);
    const int w = sq_weapon_named(arg);
    fight_drop(s, slot,
               w >= 0 && kSqWeapons[w].item != nullptr ? sq_item(kSqWeapons[w].item) : item);
    return true;
  }
  if (cmd != "weapnext" && cmd != "weapprev") return false;
  const int way = cmd == "weapnext" ? 1 : kSqWeaponCount - 1;
  const int from = p.hand.pending >= 0 ? p.hand.pending : p.hand.weapon;
  for (int k = 1; k < kSqWeaponCount; k++) {
    const int w = (from + way * k) % kSqWeaponCount;
    if (sq_can_fire(p.body, w)) {
      fight_choose(s, slot, w);
      break;
    }
  }
  return true;
}

void fight_kill(SequelServer& s, int slot, int killer, int weapon) {
  SequelPlayer& p = s.players[slot];
  if (!p.alive) return;
  p.alive = false;
  p.health = std::min(p.health, 0.0f);
  p.died_at = s.level_time;
  p.killer = killer;
  p.move.type = sh::SequelType::dead;
  const std::string& victim = name_of(s, slot);
  std::string line;
  if (killer >= 0 && killer != slot) {
    s.players[killer].frags += 1;
    std::string words = obituary(weapon);
    words.replace(words.find('%'), 1, name_of(s, killer));
    line = victim + words;
  } else {
    p.frags -= 1;
    if (killer == slot && weapon == kSqByHeldGrenade)
      line = victim + " held on to a grenade too long.";
    else if (killer == slot &&
             (weapon == int(SqWeapon::rocket_launcher) ||
              weapon == int(SqWeapon::grenade_launcher) || weapon == int(SqWeapon::bfg) ||
              weapon == int(SqWeapon::hand_grenade)))
      line = victim + " blew themselves up.";
    else if (killer == slot)
      line = victim + " gave up.";
    else if (weapon == kSqByFall)
      line = victim + " fell too far.";
    else if (weapon == kSqByLava)
      line = victim + " sank into the lava.";
    else if (weapon == kSqBySlime)
      line = victim + " melted in the slime.";
    else if (weapon == kSqByWater)
      line = victim + " ran out of air.";
    else if (weapon == kSqByCrush)
      line = victim + " was squashed.";
    else if (weapon == kSqByHurt)
      line = victim + " was in the wrong place.";
    else if (weapon == kSqByExplosion)
      line = victim + " was caught in an explosion.";
    else
      line = victim + " died.";
  }
  s.obituaries.push_back(line);
  print_all(s, line + "\n");
  const sh::Vec3f at = unpack(p.move.origin);
  if (p.health < kGibHealth) {  // thrown apart: the body is gone
    p.gibbed = true;
    sound(s, slot + 1, at, kGib);
    NetEvent ev;
    ev.kind = EventKind::point_effect;
    ev.to = kEveryone;
    ev.origin = at;
    ev.values[1] = 1;
    ev.values[3] = 32;
    ev.text = "te_blood";
    event(s, std::move(ev));
  }
  // In deathmatch the weapon in hand (not the blaster, only with its ammo) is dropped, and the quad
  // with more than 1 s left when `dmflags` 16384 says so, the two thrown 22.5 degrees apart.
  if (deathmatch(s)) {
    const SqWeaponRow& r = kSqWeapons[p.hand.weapon];
    const bool drop_weapon =
        r.item != nullptr && r.ammo != SqAmmo::none && p.body.ammo[static_cast<int>(r.ammo)] > 0;
    const bool quad = dmflag(s, 16384) && p.quad_until > s.level_time + 1;
    const float turn = drop_weapon && quad ? 11.25f : 0;
    if (drop_weapon) toss(s, slot, sq_item(r.item), -turn);
    if (quad) {
      SequelDrop* d = toss(s, slot, sq_item("item_quad"), turn);
      if (d != nullptr) d->quad_left = p.quad_until - s.level_time;
    }
  }
  p.quad_until = p.invulnerable_until = p.breather_until = p.suit_until = 0;
  p.silencer_shots = 0;
}

bool fight_dmflag(const SequelServer& s, int bit) { return dmflag(s, bit); }

void fight_hurt(SequelServer& s, int target, int attacker, sh::Vec3f dir, int damage, int knockback,
                int weapon, int flags) {
  hurt(s, target, attacker, dir, damage, knockback, false, weapon, (flags & 1) != 0,
       (flags & 2) != 0);
}

void fight_blast(SequelServer& s, sh::Vec3f at, int attacker, int damage, float radius,
                 int weapon) {
  blast(s, at, attacker, damage, radius, -1, weapon);
}

bool fight_drop(SequelServer& s, int slot, int item) {
  if (item < 0 || item >= kSqItemCount || !fightable(s, slot)) return false;
  SequelPlayer& p = s.players[slot];
  const SqItem& it = kSqItems[item];
  switch (it.kind) {
    case SqKind::weapon: {
      if (dmflag(s, 4)) {
        print_to(s, slot, "Weapons stay in this match, so none can be dropped.\n");
        return false;
      }
      if (p.body.counts[item] <= 0) return false;
      if (sq_weapon_of_item(item) == p.hand.weapon && p.body.counts[item] == 1) {
        print_to(s, slot, "You can't drop the weapon in your hand.\n");
        return false;
      }
      if (toss(s, slot, item, 0) == nullptr) return false;
      p.body.counts[item]--;
      return true;
    }
    case SqKind::ammo: {
      int& held = p.body.ammo[static_cast<int>(it.ammo)];
      const int n = std::min(held, it.amount);
      if (n <= 0) return false;
      if (p.hand.weapon == int(SqWeapon::hand_grenade) && it.ammo == SqAmmo::grenades &&
          held - n <= 0) {
        print_to(s, slot, "You can't drop the weapon in your hand.\n");
        return false;
      }
      SequelDrop* d = toss(s, slot, item, 0);
      if (d == nullptr) return false;
      d->count = n;
      held -= n;
      return true;
    }
    case SqKind::powerup:
      if (p.body.counts[item] <= 0 || toss(s, slot, item, 0) == nullptr) return false;
      p.body.counts[item]--;
      return true;
    default:
      return false;
  }
}

void fight_drops(SequelServer& s) {
  const float gravity = static_cast<float>(s.cvars.find("sv_gravity")->number);
  const float lo[3] = {-kItemHalf, -kItemHalf, -kItemHalf};
  const float hi[3] = {kItemHalf, kItemHalf, kItemHalf};
  for (SequelDrop& d : s.drops) {
    if (!d.live) continue;
    if (d.dies_at > 0 && s.level_time + 0.001 >= d.dies_at) {
      d.live = false;
      continue;
    }
    if (d.resting) continue;
    d.velocity.z -= gravity * kStep;
    const sh::Vec3f to = d.origin + d.velocity * kStep;
    const float a[3] = {d.origin.x, d.origin.y, d.origin.z}, b[3] = {to.x, to.y, to.z};
    const ge::brush::Trace t = s.tracer->trace(a, b, lo, hi, kWorldMask);
    if (t.start_solid || t.all_solid) {
      d.velocity = {};
      d.resting = true;
      continue;
    }
    d.origin = {t.end[0], t.end[1], t.end[2]};
    if (t.fraction >= 1) continue;
    const sh::Vec3f n{t.plane.n[0], t.plane.n[1], t.plane.n[2]};
    if (n.z > 0.7f) {  // a floor: it lies there
      d.velocity = {};
      d.resting = true;
    } else {  // a wall: it slides along
      d.velocity = d.velocity - n * sh::dot(d.velocity, n);
    }
  }
}

void fight_touch_drops(SequelServer& s, int slot) {
  if (!fightable(s, slot)) return;
  SequelPlayer& p = s.players[slot];
  const sh::Vec3f at = unpack(p.move.origin);
  const sh::Vec3f lo = at + p.mins, hi = at + p.maxs;
  for (SequelDrop& d : s.drops) {
    if (!d.live || (d.owner == slot && s.level_time + 0.001 < d.takeable_at)) continue;
    if (lo.x > d.origin.x + kItemHalf || hi.x < d.origin.x - kItemHalf ||
        lo.y > d.origin.y + kItemHalf || hi.y < d.origin.y - kItemHalf ||
        lo.z > d.origin.z + kItemHalf || hi.z < d.origin.z - kItemHalf)
      continue;
    const SqItem& it = kSqItems[d.item];
    if (d.quad_left > 0) {  // a dropped quad is used at once with the time it had left
      p.quad_until = std::max(p.quad_until, s.level_time) + d.quad_left;
    } else if (it.kind == SqKind::ammo && d.count > 0) {
      const int k = static_cast<int>(it.ammo);
      if (p.body.ammo[k] >= p.body.max_ammo[k]) continue;
      p.body.ammo[k] = std::min(p.body.ammo[k] + d.count, p.body.max_ammo[k]);
    } else {
      p.body.health = static_cast<int>(p.health);
      if (!sq_pick_up(d.item, p.body, deathmatch(s), skill(s), s.level_time)) continue;
      p.health = static_cast<float>(p.body.health);
    }
    d.live = false;
    p.taken.push_back(d.item);
    sound(s, slot + 1, at, it.sound);
    if (d.quad_left <= 0) fight_took(s, slot, d.item);
  }
}

void fight_rows(SequelServer& s) {
  RulesWorld& w = s.world;
  w.weapon_frames = true;
  for (int i = 0; i < sh::kMaxClients; i++) {
    if (!s.clients[i].active) continue;
    const SequelPlayer& p = s.players[i];
    PlayerNumbers& n = w.clients[i].numbers;
    const SqWeaponRow& r = kSqWeapons[p.hand.weapon];
    n.weapon = float(p.hand.weapon + 1);
    n.weapon_model = p.alive && s.intermission_at <= 0 ? s.weapon_models[p.hand.weapon] : 0;
    n.weapon_frame = float(p.hand.frame);
    n.current_ammo = r.ammo == SqAmmo::none ? 0 : float(p.body.ammo[static_cast<int>(r.ammo)]);
  }
  const size_t first = fight_first_row(s);
  if (w.entities.size() < first + kSequelMissiles + kSequelDrops) return;
  for (int k = 0; k < kSequelDrops; k++) {
    const SequelDrop& d = s.drops[k];
    WorldEntity& e = w.entities[first + kSequelMissiles + size_t(k)];
    e = WorldEntity{};
    if (!d.live) continue;
    e.free = false;
    e.origin = d.origin;
    e.velocity = d.velocity;
    e.modelindex = s.item_models[size_t(d.item)];
  }
  for (int k = 0; k < kSequelMissiles; k++) {
    const SequelMissile& m = s.missiles[k];
    WorldEntity& e = w.entities[first + size_t(k)];
    e = WorldEntity{};
    if (!m.live) continue;
    e.free = false;
    e.origin = m.origin;
    e.velocity = m.velocity;
    const double flat =
        double(sh::root_of(m.velocity.x * m.velocity.x + m.velocity.y * m.velocity.y));
    constexpr double kDegrees = 180.0 / 3.14159265358979323846;
    e.angles = {static_cast<float>(-ge::core::libm::atan2(m.velocity.z, flat) * kDegrees),
                static_cast<float>(ge::core::libm::atan2(m.velocity.y, m.velocity.x) * kDegrees),
                0};
    e.modelindex = s.missile_models[static_cast<int>(m.kind)];
  }
}

}  // namespace shooters_edge::server
