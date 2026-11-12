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

#include "brush/trace.hpp"
#include "core/libm/libm.hpp"
#include "core/num.hpp"
#include "shooters_edge/server/messages.hpp"

namespace shooters_edge::server {

namespace {

namespace num = ge::core::num;

constexpr float kMass = 200;    // a player's
constexpr float kRange = 8192;  // a line shot's reach
constexpr float kStep = 0.1f;   // a rules frame, s
constexpr int32_t kWorldMask = sh::kSqSolid | sh::kSqWindow;
constexpr int32_t kSurfSky = 0x4;  // a sky surface: a shot that reaches it is gone
constexpr uint32_t kEveryone = 0xffffffffu;
constexpr const char* kMissileModels[7] = {"models/objects/laser/tris.md2",
                                           "",
                                           "",
                                           "models/objects/grenade/tris.md2",
                                           "models/objects/rocket/tris.md2",
                                           "",
                                           "sprites/s_bfg1.sp2"};
constexpr const char* kClick = "weapons/noammo.wav";
constexpr const char* kBoom = "weapons/rocklx1a.wav";
constexpr const char* kGrenadeBoom = "weapons/grenlx1a.wav";
constexpr const char* kBounce = "weapons/grenlb1b.wav";

bool deathmatch(const SequelServer& s) { return s.cvars.find("deathmatch")->on(); }
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
  const float a[3] = {from.x, from.y, from.z}, b[3] = {to.x, to.y, to.z};
  const float zero[3] = {0, 0, 0};
  const ge::brush::Trace t = s.tracer->trace(a, b, zero, zero, kWorldMask);
  out.fraction = t.start_solid ? 0 : t.fraction;
  out.normal = {t.plane.n[0], t.plane.n[1], t.plane.n[2]};
  out.sky = t.fraction < 1 && (t.flags & kSurfSky) != 0;
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
  out.end = from + (to - from) * out.fraction;
  return out;
}

// Whether a blast at `at` reaches player `i`: a clear line to his origin, or to it moved 15 each
// way in x and y.
bool can_damage(SequelServer& s, sh::Vec3f at, int i) {
  const sh::Vec3f o = unpack(s.players[i].move.origin);
  const float zero[3] = {0, 0, 0};
  const float from[3] = {at.x, at.y, at.z};
  const sh::Vec3f tries[5] = {o, o + sh::Vec3f{15, 15, 0}, o + sh::Vec3f{15, -15, 0},
                              o + sh::Vec3f{-15, 15, 0}, o + sh::Vec3f{-15, -15, 0}};
  for (const sh::Vec3f& t : tries) {
    const float to[3] = {t.x, t.y, t.z};
    if (s.tracer->trace(from, to, zero, zero, kWorldMask).fraction >= 1) return true;
  }
  return false;
}

// A blow on player `target` from `attacker` (a slot; -1 the world) along `dir`: pushed, shared
// with the armour, the rest off his health; at 0 he dies.
void hurt(SequelServer& s, int target, int attacker, sh::Vec3f dir, int damage, int knockback,
          bool energy, int weapon) {
  if (!fightable(s, target) || damage <= 0) return;
  SequelPlayer& p = s.players[target];
  if (!deathmatch(s) && skill(s) == 0) damage = std::max(1, damage / 2);
  if (knockback > 0 && sh::normalize(dir) > 0) {
    const float push = sq_knockback(knockback, kMass, attacker == target);
    pack(p.move.velocity, unpack(p.move.velocity) + dir * push);
  }
  const int saved = sq_armour_save(p.body, damage, energy);
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
  sound(s, int(missile_row(s, m)), at, m.kind == SqShot::grenade ? kGrenadeBoom : kBoom);
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
    const sh::Vec3f end =
        start + ax.forward * kRange + ax.right * (crandom(s) * h) + ax.up * (crandom(s) * v);
    l = shoot_line(s, start, end, me);
  }
  if (l.player >= 0) {
    hurt(s, l.player, slot, ax.forward, damage, kick, false, weapon);
    effect(s, "te_blood", l.end);
  } else if (l.fraction < 1 && !l.sky) {
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
      effect(s, "te_railtrail", l.end);
      return;
    }
    hurt(s, l.player, slot, ax.forward, damage, kick, false, weapon);
    skip |= 1u << l.player;
  }
}

void launch(SequelServer& s, int slot, SqShot kind, sh::Vec3f origin, sh::Vec3f start,
            const sh::ViewAxes& ax, int damage, int weapon) {
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
      m->velocity =
          ax.forward * 600 + ax.up * (200 + crandom(s) * 10) + ax.right * (crandom(s) * 10);
      m->dies_at = s.level_time + 2.5;
      m->radius_damage = damage;
      m->radius = 160;
      break;
    case SqShot::rocket:
      m->velocity = ax.forward * 650;
      m->dies_at = s.level_time + 8000.0 / 650.0;
      m->damage = damage + ge::core::int_in(s.rng, 0, 20);
      m->radius_damage = 120;
      m->radius = 120;
      break;
    default:  // the BFG's ball
      m->velocity = ax.forward * 400;
      m->dies_at = s.level_time + 20;
      m->radius_damage = 200;
      m->radius = 100;
      break;
  }
  if (reach.fraction < 1) {  // the start is behind a wall: it meets it at once
    m->origin = reach.end - ax.forward * 10;
    if (reach.player >= 0) hurt(s, reach.player, slot, ax.forward, m->damage, 1, false, weapon);
    if (kind == SqShot::bolt)
      m->live = false;
    else if (kind != SqShot::grenade)
      explode(s, *m, m->origin, reach.player);
  }
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
  const int damage = dm ? r.damage_dm : r.damage_sp;
  sound(s, slot + 1, origin, r.sound);
  switch (r.shot) {
    case SqShot::pellets:
      if (w == int(SqWeapon::super_shotgun)) {
        for (const float turn : {-5.0f, 5.0f}) {
          const sh::ViewAxes half = sh::angle_vectors(p.view_angles + sh::Vec3f{0, turn, 0});
          for (int i = 0; i < 10; i++)
            bullet(s, slot, origin, start, half, 1000, 500, damage, r.kick, w);
        }
      } else {
        for (int i = 0; i < 12; i++)
          bullet(s, slot, origin, start, ax, 500, 500, damage, r.kick, w);
      }
      return;
    case SqShot::bullet:
      for (int i = 0; i < shots; i++) {
        sh::Vec3f from = start;
        if (w == int(SqWeapon::chaingun))
          from = eye + ax.forward * r.forward + ax.right * (7 + crandom(s) * 4) +
                 ax.up * (crandom(s) * 4);
        bullet(s, slot, origin, from, ax, 300, 500, damage, r.kick, w);
      }
      return;
    case SqShot::rail:
      rail(s, slot, start, ax, damage, dm ? 200 : 250, w);
      return;
    default:
      for (int i = 0; i < shots; i++) launch(s, slot, r.shot, origin, start, ax, damage, w);
      return;
  }
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
  }
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
  for (int k = 0; k < 7; k++) s.missile_models[k] = model(kMissileModels[k]);
  const auto add_sound = [&s](const char* name) {
    if (std::find(s.sounds.begin(), s.sounds.end(), name) == s.sounds.end())
      s.sounds.emplace_back(name);
  };
  for (const SqWeaponRow& r : kSqWeapons) add_sound(r.sound);
  for (const char* name : {kClick, kBoom, kGrenadeBoom, kBounce}) add_sound(name);
}

void fight_spawn(SequelServer& s, int slot) {
  SequelPlayer& p = s.players[slot];
  p.hand = SqHand{};
  p.stepped = false;
  p.killer = -1;
}

void fight_clear(SequelServer& s) {
  for (SequelMissile& m : s.missiles) m = SequelMissile{};
  s.obituaries.clear();
}

void fight_step(SequelServer& s, int slot, bool attack) {
  SequelPlayer& p = s.players[slot];
  if (!fightable(s, slot)) return;
  const bool infinite = deathmatch(s) && (s.cvars.find("dmflags")->integer() & 8192) != 0;
  const SqStep step = sq_hand_step(p.hand, p.body, attack, s.level_time, infinite);
  if (step.click) sound(s, slot + 1, unpack(p.move.origin), kClick);
  if (step.shots > 0) fire(s, slot, step.shots);
}

void fight_missiles(SequelServer& s) {
  const float gravity = static_cast<float>(s.cvars.find("sv_gravity")->number);
  for (SequelMissile& m : s.missiles) {
    if (!m.live) continue;
    if (s.level_time + 0.001 >= m.dies_at) {
      if (m.kind == SqShot::grenade)
        explode(s, m, m.origin, -1);
      else
        m.live = false;
      continue;
    }
    if (m.resting) continue;
    if (m.kind == SqShot::grenade) m.velocity.z -= gravity * kStep;
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
        case SqShot::grenade: {
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
        case SqShot::grenade: {  // a bounce off the wall
          m.origin = l.end + l.normal * 0.1f;
          m.velocity = m.velocity - l.normal * (1.5f * sh::dot(m.velocity, l.normal));
          if (l.normal.z > 0.7f && sh::length(m.velocity) < 60) {
            m.velocity = {};
            m.resting = true;
          }
          sound(s, int(missile_row(s, m)), m.origin, kBounce);
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
  if (w < 0 || p.body.counts[item] != 1) return;  // only a newly owned weapon
  if (!deathmatch(s) || p.hand.weapon == int(SqWeapon::blaster)) sq_choose(p.hand, p.body, w);
}

bool fight_command(SequelServer& s, int slot, const std::string& cmd, const std::string& arg) {
  SequelPlayer& p = s.players[slot];
  if (cmd == "use") {
    const int w = sq_weapon_named(arg);
    if (w >= 0) fight_choose(s, slot, w);
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
    if (killer == slot &&
        (weapon == int(SqWeapon::rocket_launcher) || weapon == int(SqWeapon::grenade_launcher) ||
         weapon == int(SqWeapon::bfg)))
      line = victim + " blew themselves up.";
    else if (killer == slot)
      line = victim + " gave up.";
    else
      line = victim + " died.";
  }
  s.obituaries.push_back(line);
  print_all(s, line + "\n");
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
    n.weapon_model = p.alive ? s.weapon_models[p.hand.weapon] : 0;
    n.weapon_frame = float(p.hand.frame);
    n.current_ammo = r.ammo == SqAmmo::none ? 0 : float(p.body.ammo[static_cast<int>(r.ammo)]);
  }
  const size_t first = fight_first_row(s);
  if (w.entities.size() < first + kSequelMissiles) return;
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
