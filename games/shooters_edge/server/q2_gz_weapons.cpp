// Ground Zero's weapons fired and their shots in flight (q2_gz.hpp): the chainfist's reach, the
// plasma beam's line, the flechette, the disruptor's bolt and its hurting, the prox mine, the tesla
// and the A-M bomb.
//
// ge:summary Ground Zero's shots: chainfist, plasma beam, flechette, disruptor, mines, A-M bomb
// ge:keywords q2 ground-zero shots chainfist plasma-beam heat-beam flechette disruptor tracker
// ge:keywords prox-mine tesla zap am-bomb nuke quake
// ge:skill shooter
// ge:test tests/shooters_edge_q2_gz_test.cpp
#include <algorithm>
#include <cmath>
#include <string_view>
#include <utility>

#include "core/prng.hpp"
#include "shooters_edge/server/messages.hpp"
#include "shooters_edge/server/q2_fight.hpp"
#include "shooters_edge/server/q2_fight_kit.hpp"
#include "shooters_edge/server/q2_flash.hpp"
#include "shooters_edge/server/q2_gz.hpp"
#include "shooters_edge/server/q2_map.hpp"
#include "shooters_edge/server/q2_monsters.hpp"
#include "shooters_edge/server/q2_server.hpp"

namespace shooters_edge::server {

namespace {

constexpr uint32_t kEveryone = 0xffffffffu;             // a message to every client
constexpr int32_t kLava = 8, kSlime = 16, kWater = 32;  // what a point holds (map_contents)
constexpr float kBeamRange = 8192;
constexpr const char* kBlast = "weapons/grenlx1a.wav";
constexpr const char* kWarn = "weapons/proxwarn.wav";
constexpr const char* kBeep = "weapons/nukewarn2.wav";

bool deathmatch(const Q2Server& s) { return s.cvars.find("deathmatch")->on(); }

sh::Vec3f unpack(const int32_t v[3]) {
  return {float(v[0]) * 0.125f, float(v[1]) * 0.125f, float(v[2]) * 0.125f};
}

bool living(const Q2Server& s, int i) {
  return s.clients[i].active && !s.clients[i].spectator && s.players[i].alive;
}

bool living_monster(const Q2Server& s, int e) {
  return e >= 0 && size_t(e) < s.entities.size() && !s.entities[size_t(e)].gone &&
         s.entities[size_t(e)].health > 0 && monster_index(s, e) >= 0;
}

sh::Vec3f middle(const Q2Entity& e) { return e.origin + (e.mins + e.maxs) * 0.5f; }

void event(Q2Server& s, NetEvent ev) {
  ev.time = s.level_time;
  s.out.events.push_back(std::move(ev));
  s.out.events_total++;
}

// A point effect `name` at `at` facing `way`.
void effect(Q2Server& s, const char* name, sh::Vec3f at, sh::Vec3f way = {}) {
  fight_effect(s, name, at, way);
}

// A sound heard everywhere at its full strength (`attenuation` 0) or carried farther than most.
void sound_far(Q2Server& s, int ent, sh::Vec3f at, const char* sample, float volume,
               float attenuation) {
  NetEvent ev;
  ev.kind = EventKind::sound;
  ev.to = kEveryone;
  ev.ent = ent;
  ev.level = 0;
  ev.origin = at;
  ev.values[0] = 255 * volume;
  ev.values[1] = attenuation;
  ev.text = sample;
  event(s, std::move(ev));
}

// The tesla's bolt from `from` (its own entity) to `to` (entity `struck`'s).
void lightning(Q2Server& s, int tesla, int struck, sh::Vec3f from, sh::Vec3f to) {
  NetEvent ev;
  ev.kind = EventKind::trail_effect;
  ev.to = kEveryone;
  ev.ent = struck;
  ev.level = tesla;
  ev.origin = from;
  ev.values[0] = to.x;
  ev.values[1] = to.y;
  ev.values[2] = to.z;
  ev.text = "te_lightning";
  event(s, std::move(ev));
}

// The view's kick: back `back` units along the aim, the pitch up `up` degrees.
void kick(Q2Player& p, sh::Vec3f forward, float back, float up) {
  p.view.weapon_angles = {-up, 0, 0};
  p.view.weapon_origin = forward * -back;
}

// Where a shot starts: the eye (the origin and the view height) moved along the view's axes.
sh::Vec3f start_at(const Q2Player& p, const sh::ViewAxes& ax, float forward, float right,
                   float up) {
  return unpack(p.move.origin) + sh::Vec3f{0, 0, p.view_height} + ax.forward * forward +
         ax.right * right + ax.up * up;
}

// A blow to what a line met: a player, a monster, a mover that takes blows. True when it took it.
bool strike(Q2Server& s, const Line& l, int attacker, sh::Vec3f dir, int damage, int push,
            int weapon, int flags) {
  if (l.player >= 0) {
    fight_hurt(s, l.player, attacker, dir, damage, push, weapon, flags);
    return true;
  }
  if (l.mover < 0) return false;
  if (monster_index(s, l.mover) >= 0)
    return monster_damage(s, l.mover, attacker, damage, dir, false, &l.end);
  return map_damage(s, l.mover, attacker, damage, dir, false, &l.end);
}

// --- the weapons fired --------------------------------------------------------------------------

void chainfist(Q2Server& s, int slot, const sh::ViewAxes& ax, int multiplier) {
  Q2Player& p = s.players[slot];
  const sh::Vec3f start = start_at(p, ax, 0, 8, -4);
  const Line l = fight_line(s, start, start + ax.forward * 64, 1u << slot);
  const int damage = (deathmatch(s) ? 30 : 15) * multiplier;
  kick(p, ax.forward, 2, 1);
  const bool body = l.player >= 0 || (l.mover >= 0 && monster_index(s, l.mover) >= 0);
  if (strike(s, l, slot, ax.forward, damage, 0, int(SqWeapon::chainfist), 1)) {
    if (body) effect(s, "te_moreblood", l.end, l.normal);
    fight_kick(s, slot, ax.forward * 75 + ax.up * 75);  // the saw pulls its wielder on
  } else if (l.fraction < 1 && !l.sky) {
    effect(s, "te_gunshot", l.end, l.normal);
  }
}

void plasma_beam(Q2Server& s, int slot, const sh::ViewAxes& ax, int multiplier) {
  Q2Player& p = s.players[slot];
  const sh::Vec3f start = start_at(p, ax, 7, 2, -3);
  const sh::Vec3f aim = fight_aim(s, slot, start, ax.forward);
  const sh::Vec3f far = start + aim * kBeamRange;
  const Line l = fight_line(s, start, far, 1u << slot);
  int damage = 15 * multiplier;
  const int push = (deathmatch(s) ? 75 : 30) * multiplier;
  // Water on the way: from out of it, the entry's sparks and half the damage; bubbles either way.
  const bool under = (map_contents(s, start) & (kWater | kSlime | kLava)) != 0;
  bool crossed = under;
  if (under) {
    fight_line_effect(s, "te_bubbletrail2", start, l.end, 0);
  } else {
    const ge::classic::Q2Trace w = map_trace(s, start, {}, {}, l.end, kWater | kSlime | kLava);
    if (w.fraction < 1) {
      crossed = true;
      damage /= 2;
      const sh::Vec3f in = w.end;
      effect(s, "te_heatbeam_sparks", in, w.normal);
      fight_line_effect(s, "te_bubbletrail2", in, l.end, 0);
    }
  }
  if (!strike(s, l, slot, aim, damage, push, int(SqWeapon::plasma_beam), 4) && l.fraction < 1 &&
      !l.sky && !crossed)
    effect(s, "te_heatbeam_steam", l.end, l.normal);
  fight_line_effect(s, "te_heatbeam", start, l.end, slot + 1);
}

// A shot in flight of `kind` from `slot` at `start` with `velocity`.
Q2Missile* fly(Q2Server& s, int slot, SqShot kind, int weapon, sh::Vec3f start, sh::Vec3f velocity,
               int multiplier) {
  Q2Missile* m = fight_new_missile(s);
  if (m == nullptr) return nullptr;
  *m = Q2Missile{};
  m->live = true;
  m->kind = kind;
  m->owner = slot;
  m->weapon = weapon;
  m->origin = start;
  m->velocity = velocity;
  m->multiplier = multiplier;
  m->fired_z = start.z;
  return m;
}

// The thrown kinds' speed off the view: along the aim, 200 ± 10 up and ± 10 across.
sh::Vec3f thrown(Q2Server& s, const sh::ViewAxes& ax, sh::Vec3f aim, float speed) {
  const float up = 200 + fight_crandom(s) * 10;  // the draws in a fixed order
  const float across = fight_crandom(s) * 10;
  return aim * speed + ax.up * up + ax.right * across;
}

// A mine's life by its multiplier: 45, 30, 15 or 10 s.
double mine_life(int multiplier) {
  return multiplier >= 8 ? 10 : multiplier >= 4 ? 15 : multiplier >= 2 ? 30 : 45;
}

// --- the shots in flight ------------------------------------------------------------------------

// A blast at `at` with the multiplier's sound and a burst drawn by where it lies.
void burst(Q2Server& s, Q2Missile& m, sh::Vec3f at, int damage, float radius, int weapon) {
  const bool wet = (map_contents(s, at) & kWater) != 0;
  const bool ground = m.stage == 0 ? m.resting : m.stuck.z > 0.7f;
  effect(s,
         wet ? (ground ? "te_grenade_explosion_water" : "te_rocket_explosion_water")
             : (ground ? "te_grenade_explosion" : "te_rocket_explosion"),
         at);
  fight_sound(s, fight_missile_entity(s, m), at, kBlast);
  if (const char* boost = fight_multiplier_sound(m.multiplier))
    fight_sound(s, fight_missile_entity(s, m), at, boost);
  if (damage > 0 && radius > 0) fight_blast(s, at, m.owner, damage, radius, weapon);
  m.live = false;
}

// Whether a body (a living player, or a living monster; when `bodies`) stands within `reach` of
// `at` in sight; a spawn or a teleporter's exit counts too when `spawns`.
bool in_sight(Q2Server& s, sh::Vec3f at, float reach, bool bodies, bool spawns) {
  for (int i = 0; bodies && i < sh::kMaxClients; i++) {
    if (!living(s, i)) continue;
    const sh::Vec3f o = unpack(s.players[i].move.origin);
    if (sh::length(o - at) <= reach && map_trace(s, at, {}, {}, o, 1).fraction >= 1) return true;
  }
  for (size_t e = 0; e < s.entities.size(); e++) {
    const Q2Entity& t = s.entities[e];
    if (t.gone) continue;
    const bool body = bodies && living_monster(s, int(e));
    const std::string_view cn = t.classname;
    const bool spot = spawns && (cn == "info_player_start" || cn == "info_player_deathmatch" ||
                                 cn == "info_player_coop" || cn == "misc_teleporter_dest");
    if (!body && !spot) continue;
    if (sh::length(t.origin - at) <= reach && map_trace(s, at, {}, {}, t.origin, 1).fraction >= 1)
      return true;
  }
  return false;
}

// Whether a body's box meets the box `lo`..`hi`.
bool body_in(const Q2Server& s, sh::Vec3f lo, sh::Vec3f hi) {
  const auto meets = [&](sh::Vec3f a, sh::Vec3f b) {
    return a.x <= hi.x && b.x >= lo.x && a.y <= hi.y && b.y >= lo.y && a.z <= hi.z && b.z >= lo.z;
  };
  for (int i = 0; i < sh::kMaxClients; i++) {
    if (!living(s, i)) continue;
    const Q2Player& p = s.players[i];
    const sh::Vec3f o = unpack(p.move.origin);
    if (meets(o + p.mins, o + p.maxs)) return true;
  }
  for (size_t e = 0; e < s.entities.size(); e++) {
    if (!living_monster(s, int(e))) continue;
    const Q2Entity& t = s.entities[e];
    if (meets(t.origin + t.mins, t.origin + t.maxs)) return true;
  }
  return false;
}

// A thrown thing's fall and bounce (the tesla and the bomb): false when it went into the sky.
bool tumble(Q2Server& s, Q2Missile& m, float gravity, uint32_t skip) {
  if (m.resting) return true;
  m.velocity.z -= gravity * s.step;
  const sh::Vec3f to = m.origin + m.velocity * s.step;
  const Line l = fight_line(s, m.origin, to, skip);
  if (l.fraction >= 1) {
    m.origin = to;
    return true;
  }
  if (l.sky) return false;
  m.origin = l.end + l.normal * 0.1f;
  m.velocity = m.velocity - l.normal * (1.5f * sh::dot(m.velocity, l.normal));
  fight_sound(s, fight_missile_entity(s, m), m.origin,
              ge::core::next(s.rng) < 0.5 ? "weapons/hgrenb1a.wav" : "weapons/hgrenb2a.wav");
  if (l.normal.z > 0.7f && sh::length(m.velocity) < 60) {
    m.velocity = {};
    m.resting = true;
    m.stuck = l.normal;
  }
  return true;
}

void flechette(Q2Server& s, Q2Missile& m) {
  if (s.level_time + 0.001 >= m.dies_at) {
    m.live = false;
    return;
  }
  const sh::Vec3f to = m.origin + m.velocity * s.step;
  const Line l = fight_line(s, m.origin, to, m.owner >= 0 ? 1u << m.owner : 0u, m.from);
  if (l.fraction >= 1) {
    m.origin = to;
    return;
  }
  m.live = false;
  if (l.sky) return;
  sh::Vec3f dir = m.velocity;
  sh::normalize(dir);
  // Past armour: the flechette's damage meets only the body (the power armour still saves).
  if (!strike(s, l, m.owner, dir, m.damage, m.burst, int(SqWeapon::etf_rifle), 1))
    effect(s, "te_flechette", l.end, l.normal);
}

// Where the disruptor's bolt aims: its target's eye or middle; false when it is gone.
bool tracked(const Q2Server& s, int prey, sh::Vec3f& at) {
  if (prey >= 0 && prey < sh::kMaxClients) {
    if (!living(s, prey)) return false;
    at = unpack(s.players[prey].move.origin) + sh::Vec3f{0, 0, s.players[prey].view_height};
    return true;
  }
  const int e = -2 - prey;
  if (!living_monster(s, e)) return false;
  at = middle(s.entities[size_t(e)]);
  return true;
}

void tracker(Q2Server& s, Q2Missile& m) {
  if (m.stage == 4) {  // hurting what it struck: a fifth of its damage a tenth, 0.5 s long
    sh::Vec3f at;
    if (s.level_time > m.stage_at + 0.001 || !tracked(s, m.prey, at)) {
      m.live = false;
      return;
    }
    const int tick = m.damage / 5;
    if (m.prey >= 0 && m.prey < sh::kMaxClients)  // a player; a monster's prey is -2 - entity
      fight_hurt(s, m.prey, m.owner, {}, tick, 0, m.weapon, 4);
    else if (m.prey <= -2)
      monster_damage(s, -2 - m.prey, m.owner, tick, {}, false);
    return;
  }
  sh::Vec3f aim;
  if (m.prey != -1) {
    if (!tracked(s, m.prey, aim)) {  // its target died or went: it bursts where it is
      effect(s, "te_tracker_explosion", m.origin);
      m.live = false;
      return;
    }
    sh::Vec3f way = aim - m.origin;
    const float speed = sh::length(m.velocity);  // the speed it was fired at (a widow's 500)
    if (sh::normalize(way) > 0) m.velocity = way * (speed > 0 ? speed : 1000.0f);
  } else if (s.level_time + 0.001 >= m.dies_at) {
    m.live = false;
    return;
  }
  const sh::Vec3f to = m.origin + m.velocity * s.step;
  const Line l = fight_line(s, m.origin, to, m.owner >= 0 ? 1u << m.owner : 0u, m.from);
  if (l.fraction >= 1) {
    m.origin = to;
    return;
  }
  if (l.sky) {
    m.live = false;
    return;
  }
  effect(s, "te_tracker_explosion", l.end);
  sh::Vec3f dir = m.velocity;
  sh::normalize(dir);
  const int struck = l.player >= 0 ? l.player : living_monster(s, l.mover) ? -2 - l.mover : -1;
  if (struck == -1) {  // anything else that takes blows takes the whole at once
    if (l.mover >= 0) map_damage(s, l.mover, m.owner, m.damage, dir, false, &l.end);
    m.live = false;
    return;
  }
  // On a body: a push of three times its damage and 140 up, then it hurts it for 0.5 s.
  if (l.player >= 0)
    fight_kick(s, l.player, dir * sq_knockback(3 * m.damage, 200, false) + sh::Vec3f{0, 0, 140});
  m.stage = 4;
  m.prey = struck;
  m.stage_at = s.level_time + 0.5;
  m.velocity = {};
  m.origin = l.end;
}

void prox(Q2Server& s, Q2Missile& m, float gravity) {
  const double now = s.level_time;
  const int weapon = int(SqWeapon::prox_launcher);
  const int damage = 90 * m.multiplier;
  if (m.stage == 0) {  // in flight: it sticks to the world, goes off on a body
    if (now + 0.001 >= m.dies_at) return burst(s, m, m.origin, damage, 192, weapon);
    m.velocity.z -= gravity * s.step;
    const sh::Vec3f to = m.origin + m.velocity * s.step;
    const Line l = fight_line(s, m.origin, to, m.owner >= 0 ? 1u << m.owner : 0u);
    if (l.fraction >= 1) {
      m.origin = to;
      return;
    }
    if (l.sky) {
      m.live = false;
      return;
    }
    if (l.player >= 0 || living_monster(s, l.mover)) return burst(s, m, l.end, damage, 192, weapon);
    sh::Vec3f dir = m.velocity;
    sh::normalize(dir);
    if ((map_contents(s, l.end + dir * 10) & (kSlime | kLava)) != 0)
      return burst(s, m, l.end, damage, 192, weapon);
    if (l.mover >= 0 && l.normal.z <= 0.7f) {  // a mover's side: it bounces on
      m.origin = l.end + l.normal * 0.1f;
      m.velocity = m.velocity - l.normal * (1.5f * sh::dot(m.velocity, l.normal));
      if (m.velocity.z > 60) m.velocity.z = 60;
      return;
    }
    m.origin = l.end + l.normal * 0.1f;  // stuck: it opens over the next second
    m.velocity = {};
    m.stuck = l.normal;
    m.stage = 1;
    m.frame = 0;
    m.stage_at = now + 1.0;
    fight_sound(s, fight_missile_entity(s, m), m.origin, "weapons/proxopen.wav");
    return;
  }
  if (m.stage == 1) {
    if (m.frame < 9) m.frame++;
    if (now + 0.001 < m.stage_at) return;
    if (in_sight(s, m.origin, 202, true, deathmatch(s))) {  // armed in sight of a body: at once
      fight_sound(s, fight_missile_entity(s, m), m.origin, kWarn);
      return burst(s, m, m.origin, damage, 192, weapon);
    }
    m.stage = 2;
    const auto* strong = s.cvars.find("strong_mines");
    m.dies_at = now + (strong != nullptr && strong->on() ? 45.0 : mine_life(m.multiplier));
    return;
  }
  if (m.stage == 2) {
    m.frame = m.frame >= 13 ? 9 : m.frame + 1;
    if (now + 0.001 >= m.dies_at) return burst(s, m, m.origin, damage, 192, weapon);
    if (body_in(s, m.origin - sh::Vec3f{96, 96, 96}, m.origin + sh::Vec3f{96, 96, 96})) {
      fight_sound(s, fight_missile_entity(s, m), m.origin, kWarn);
      m.stage = 3;
      m.stage_at = now + 0.5;
    }
    return;
  }
  if (now + 0.001 >= m.stage_at) burst(s, m, m.origin, damage, 192, weapon);
}

void tesla(Q2Server& s, Q2Missile& m, float gravity) {
  const double now = s.level_time;
  const int weapon = int(SqWeapon::tesla);
  const auto big = [&] {  // water, slime or lava: a grenade's blast, 150 within 200
    burst(s, m, m.origin, 150 * m.multiplier, 200, int(SqWeapon::hand_grenade));
  };
  const auto small = [&] { burst(s, m, m.origin, 0, 0, weapon); };
  if (m.stage == 0) {
    if (!tumble(s, m, gravity, m.owner >= 0 ? 1u << m.owner : 0u)) {
      m.live = false;
      return;
    }
    if ((map_contents(s, m.origin) & (kSlime | kLava)) != 0) return big();
    if (now + 0.001 < m.stage_at) return;
    m.stage = 1;  // 3 s on: it opens, a frame a tenth
    m.frame = 0;
    fight_sound(s, fight_missile_entity(s, m), m.origin, "weapons/teslaopen.wav");
    return;
  }
  if (m.stage == 1) {
    if (m.frame < 14) {
      m.frame++;
      if (m.frame == 10 && m.owner >= 0) monster_noise(s, m.owner, m.origin, false);
      return;
    }
    if ((map_contents(s, m.origin) & (kWater | kSlime | kLava)) != 0) return big();
    if (deathmatch(s) && in_sight(s, m.origin, 192, false, true)) return small();
    m.stage = 2;
    m.dies_at = now + 30;
    return;
  }
  if (now + 0.001 >= m.dies_at) return small();
  // Zapping: every body in its field it sees, each tenth.
  const bool dm = deathmatch(s);
  const sh::Vec3f from = m.origin + sh::Vec3f{0, 0, 16};
  const sh::Vec3f lo = m.origin + sh::Vec3f{-128, -128, 0},
                  hi = m.origin + sh::Vec3f{128, 128, 128};
  const int self = fight_missile_entity(s, m);
  const int damage = 3 * m.multiplier;
  const auto inside = [&](sh::Vec3f a, sh::Vec3f b) {
    return a.x <= hi.x && b.x >= lo.x && a.y <= hi.y && b.y >= lo.y && a.z <= hi.z && b.z >= lo.z;
  };
  for (int i = 0; dm && i < sh::kMaxClients; i++) {
    if (!living(s, i)) continue;
    const Q2Player& p = s.players[i];
    const sh::Vec3f o = unpack(p.move.origin);
    if (!inside(o + p.mins, o + p.maxs)) continue;
    const Line l = fight_line(s, from, o, 0);
    if (l.player != i && l.fraction < 1) continue;
    sh::Vec3f dir = o - from;
    sh::normalize(dir);
    fight_hurt(s, i, m.owner, dir, damage, 8, weapon);
    lightning(s, self, i + 1, from, l.end);
    if (const char* boost = fight_multiplier_sound(m.multiplier)) fight_sound(s, self, from, boost);
  }
  for (size_t e = 0; e < s.entities.size(); e++) {
    if (!living_monster(s, int(e))) continue;
    const Q2Entity& t = s.entities[e];
    if (!inside(t.origin + t.mins, t.origin + t.maxs)) continue;
    if (map_trace(s, from, {}, {}, t.origin, 1).fraction < 1) continue;
    sh::Vec3f dir = t.origin - from;
    sh::normalize(dir);
    monster_damage(s, int(e), m.owner, damage, dir, false);
    gz_tesla_bad_area(s, int(&m - s.missiles), int(e), lo, hi);  // its ground, and at it
    lightning(s, self, map_row(int(e)), from, t.origin);
    if (const char* boost = fight_multiplier_sound(m.multiplier)) fight_sound(s, self, from, boost);
  }
}

// The A-M bomb's blast: 10 000 within its kill radius, falling off to nothing at twice it.
void nuke_blast(Q2Server& s, Q2Missile& m) {
  const sh::Vec3f at = m.origin;
  const float kill = m.multiplier <= 1 ? 512.0f : 512.0f + 128.0f * float(m.multiplier);
  const int full = 400 * m.multiplier;
  const auto points = [&](float d) {
    if (d <= kill) return 10000;
    if (d >= 2 * kill) return 0;
    return static_cast<int>(float(full) * (1 - (d - kill) / kill));
  };
  if (const char* boost = fight_multiplier_sound(m.multiplier)) sound_far(s, 0, at, boost, 1, 0);
  sound_far(s, 0, at, kBlast, 1, 0);
  effect(s, "te_explosion1_big", at);
  effect(s, "te_nukeblast", at);
  for (int i = 0; i < sh::kMaxClients; i++) {
    if (!living(s, i)) continue;
    const Q2Player& p = s.players[i];
    const sh::Vec3f c = unpack(p.move.origin) + (p.mins + p.maxs) * 0.5f;
    const int d = points(sh::length(c - at));
    if (d > 0) fight_hurt(s, i, m.owner, c - at, d, d, kSqByAmBomb);
  }
  for (size_t e = 0; e < s.entities.size(); e++) {
    if (!living_monster(s, int(e))) continue;
    const sh::Vec3f c = middle(s.entities[e]);
    const int d = points(sh::length(c - at));
    if (d > 0) monster_damage(s, int(e), m.owner, d, c - at, true);
  }
  m.stage = 5;  // 3 s of quake
  m.stage_at = s.level_time + 3;
  m.velocity = {};
}

void nuke(Q2Server& s, Q2Missile& m, float gravity) {
  const double now = s.level_time;
  const int self = fight_missile_entity(s, m);
  if (m.stage == 5) {  // the quake: everyone on the ground thrown, a rumble every 0.5 s
    if (now + 0.001 >= m.stage_at) {
      m.live = false;
      return;
    }
    ge::core::Rng shake =
        ge::core::make_rng(static_cast<uint32_t>(std::lround(now * 10)) * 7919u + 0x4e4bu);
    for (int i = 0; i < sh::kMaxClients; i++) {
      Q2Player& p = s.players[i];
      if (!living(s, i) || !p.on_ground) continue;
      p.on_ground = false;
      const float x = static_cast<float>(ge::core::range(shake, -150.0, 150.0));
      const float y = static_cast<float>(ge::core::range(shake, -150.0, 150.0));
      sh::Vec3f v = unpack(p.move.velocity);
      v = {v.x + x, v.y + y, 50};
      fight_kick(s, i, v - unpack(p.move.velocity));
    }
    const auto tenth = static_cast<int64_t>(std::floor((m.stage_at - now) * 10 + 0.5));
    if (tenth % 5 == 0) sound_far(s, self, m.origin, "world/rumble.wav", 0.75f, 0);
    return;
  }
  if (!tumble(s, m, gravity, m.owner >= 0 && now < m.stage_at - 6 ? 1u << m.owner : 0u)) {
    m.live = false;
    return;
  }
  const double left = m.stage_at - now;
  const auto tenths = static_cast<int64_t>(std::floor(left * 10 + 0.5));
  const float carry = m.multiplier >= 8   ? 0.36f
                      : m.multiplier >= 4 ? 0.6f
                      : m.multiplier >= 2 ? 0.9f
                                          : 1.8f / 1.4f;
  if (left <= 6) {
    m.frame = m.frame >= 11 || m.frame < 6 ? 6 : m.frame + 1;
    if ((map_contents(s, m.origin) & (kSlime | kLava)) != 0) return nuke_blast(s, m);
    if (tenths % (left > 3 ? 5 : 3) == 0) sound_far(s, self, m.origin, kBeep, 1, carry);
  } else if (tenths % 10 == 0) {
    sound_far(s, self, m.origin, kBeep, 1, carry);
  }
  if (now + 0.001 >= m.stage_at) nuke_blast(s, m);
}

}  // namespace

void gz_fire(Q2Server& s, int slot, int shots) {
  (void)shots;
  Q2Player& p = s.players[slot];
  const int w = p.hand.weapon;
  const SqWeaponRow& r = kSqWeapons[w];
  const sh::ViewAxes ax = sh::angle_vectors(p.view_angles);
  const int multiplier = fight_multiplier(s, p);
  const bool silenced = p.silencer_shots > 0;
  if (!silenced && w != int(SqWeapon::tesla)) monster_noise(s, slot);
  p.hostile_until = s.level_time + 1;
  p.pose.shot = true;
  if (const char* boost = fight_multiplier_sound(multiplier)) fight_item_sound(s, slot, boost);
  const auto flash = [&](int value, bool quiet) {
    fight_flash(s, slot, value | (quiet && silenced ? kQ2FlashSilenced : 0));
    if (silenced) p.silencer_shots--;
  };
  const bool dm = deathmatch(s);
  switch (static_cast<SqWeapon>(w)) {
    case SqWeapon::chainfist:
      chainfist(s, slot, ax, multiplier);
      return;
    case SqWeapon::plasma_beam:
      flash(33, true);
      plasma_beam(s, slot, ax, multiplier);
      return;
    case SqWeapon::etf_rifle: {
      const sh::Vec3f start = start_at(p, ax, 15, p.hand.swing == 0 ? 8.0f : 6.0f, -8);
      const sh::Vec3f aim = fight_aim(s, slot, start, ax.forward);
      if (Q2Missile* m = fly(s, slot, SqShot::flechette, w, start, aim * 750, multiplier)) {
        m->damage = r.damage_dm * multiplier;
        m->burst = r.kick * multiplier;  // its push
        m->dies_at = s.level_time + 8000.0 / 750.0;
      }
      flash(30, false);
      ge::core::Rng shake =
          ge::core::make_rng(static_cast<uint32_t>(s.frame_number) * 64u + uint32_t(slot) + 0xe7fu);
      const auto j = [&shake] { return static_cast<float>(ge::core::range(shake, -0.85, 0.85)); };
      p.view.weapon_angles = {j(), j(), j()};
      p.view.weapon_origin = {j(), j(), j()};
      return;
    }
    case SqWeapon::prox_launcher: {
      const sh::Vec3f start = start_at(p, ax, 8, 8, -8);
      const sh::Vec3f aim = fight_aim(s, slot, start, ax.forward);
      if (Q2Missile* m = fly(s, slot, SqShot::prox, w, start, thrown(s, ax, aim, 600), multiplier))
        m->dies_at = s.level_time + mine_life(multiplier);
      flash(8, true);
      kick(p, ax.forward, 2, 1);
      return;
    }
    case SqWeapon::disruptor: {
      const sh::Vec3f start = start_at(p, ax, 24, 8, -8);
      const sh::Vec3f aim = fight_aim(s, slot, start, ax.forward);
      // What the aim's line meets first, a body, is what the bolt follows.
      const Line l = fight_line(s, start, start + aim * kBeamRange, 1u << slot);
      const int prey = l.player >= 0 ? l.player : living_monster(s, l.mover) ? -2 - l.mover : -1;
      if (Q2Missile* m = fly(s, slot, SqShot::tracker, w, start, aim * 1000, multiplier)) {
        m->damage = (dm ? r.damage_dm : r.damage_sp) * multiplier;
        m->prey = prey;
        m->dies_at = prey == -1 ? s.level_time + 10 : 1e18;
      }
      flash(35, false);
      kick(p, ax.forward, 2, 1);
      return;
    }
    case SqWeapon::tesla: {
      const sh::Vec3f start = start_at(p, ax, 0, -4, -22);
      const sh::Vec3f aim = fight_aim(s, slot, start, ax.forward);
      if (Q2Missile* m = fly(s, slot, SqShot::tesla, w, start,
                             thrown(s, ax, aim, p.hand.throw_speed), multiplier)) {
        m->stage_at = s.level_time + 3;
        m->dies_at = 1e18;
        m->health = deathmatch(s) ? 20 : 30;  // it can be shot from the throw
      }
      return;
    }
    default:
      return;
  }
}

void gz_throw_nuke(Q2Server& s, int slot) {
  Q2Player& p = s.players[slot];
  const sh::ViewAxes ax = sh::angle_vectors(p.view_angles);
  const sh::Vec3f origin = unpack(p.move.origin);
  if (Q2Missile* m = fly(s, slot, SqShot::nuke, kSqByAmBomb, origin, thrown(s, ax, ax.forward, 100),
                         fight_multiplier(s, p))) {
    m->stage_at = s.level_time + 10;
    m->dies_at = 1e18;
  }
}

bool gz_missile(Q2Server& s, Q2Missile& m) {
  const float gravity = static_cast<float>(s.cvars.find("sv_gravity")->number);
  switch (m.kind) {
    case SqShot::flechette:
      flechette(s, m);
      return true;
    case SqShot::tracker:
      tracker(s, m);
      return true;
    case SqShot::prox:
      prox(s, m, gravity);
      return true;
    case SqShot::tesla:
      tesla(s, m, gravity);
      return true;
    case SqShot::nuke:
      nuke(s, m, gravity);
      return true;
    default:
      return gz_sphere_missile(s, m);
  }
}

bool gz_drawn(const Q2Missile& m) {
  return !((m.kind == SqShot::tracker && m.stage == 4) || (m.kind == SqShot::nuke && m.stage == 5));
}

void gz_tesla_damage(Q2Server& s, int i, int damage) {
  if (i < 0 || size_t(i) >= std::size(s.missiles)) return;
  Q2Missile& m = s.missiles[size_t(i)];
  if (!m.live || m.kind != SqShot::tesla || damage <= 0) return;
  m.health -= damage;
  if (m.health <= 0) burst(s, m, m.origin, 0, 0, int(SqWeapon::tesla));  // the small burst
}

}  // namespace shooters_edge::server
