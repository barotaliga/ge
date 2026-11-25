// Quake II's medic at work: the corpse it sees with the most health, the cable, the dead raised
// (q2_monster_ai.hpp; q2-monsters-full.md § The medic).
//
// ge:summary Quake II's medic raising the dead: the patient, the cable, the revive
// ge:keywords q2 monsters medic revive raise-dead patient medic-cable
// ge:skill shooter
// ge:test tests/shooters_edge_q2_monsters_test.cpp
#include "shooters_edge/server/q2_monster_ai.hpp"

#include <algorithm>
#include <cstdlib>
#include <string_view>

#include "core/prng.hpp"
#include "shooters_edge/server/q2_fight.hpp"
#include "shooters_edge/server/q2_flash.hpp"
#include "shooters_edge/server/q2_map.hpp"
#include "shooters_edge/server/q2_monsters.hpp"
#include "shooters_edge/server/q2_server.hpp"

namespace shooters_edge::server::monster_ai {

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
    if (!gz_patient_ok(s, medic, c)) continue;  // Ground Zero's: not one it gave up on
    if (sh::length(b.origin - e.origin) > gz_patient_reach(s, medic) || !sees_body(s, medic, c))
      continue;
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
  fresh.shield = k.shield_flag == 0 ? k.shield : 0;
  fresh.beam = c.beam;
  c = fresh;
  b.health = b.max_health = k.health;
  b.mins = k.mins;
  b.maxs = k.maxs;
  b.box = true;
  b.corpse = false;
  b.target.clear();
  b.targetname.clear();
  b.spawnflags = 0;
  if (gz_ai(s))  // Ground Zero's: out of the level's count, out of infighting
    c.revived = true;
  else
    s.monsters_total++;
  if (targetable(s, enemy)) found(s, c, enemy, false);
}

// A cable's sound, the launch (0) or the heal (1): the medic's, or Ground Zero's commander's own.
static const char* cable_sound(const Q2Monster& m, int which) {
  const bool commander = kind(m).classname == std::string_view("monster_medic_commander");
  if (commander)
    return which == 0 ? "medic_commander/medatck2c.wav" : "medic_commander/medatck4a.wav";
  return which == 0 ? "medic/medatck2.wav" : "medic/medatck4.wav";
}

// A medic's frame with a corpse to raise: the cable within 256 (410 in Ground Zero), else toward
// it. True when it was busy with one.
bool tend(Q2Server& s, Q2Monster& m) {
  if (m.patient < 0 && (s.frame_number % 10 == 0 || m.enemy == -1)) {
    m.patient = find_patient(s, m);
    if (m.patient >= 0) {
      s.monsters[size_t(m.patient)].claimed = true;
      m.patient_since = s.level_time;
      m.old_enemy = m.enemy;
      m.revive_at = 0;
    }
  }
  if (m.patient < 0) return false;
  Q2Monster& c = s.monsters[size_t(m.patient)];
  Q2Entity& e = body(s, m);
  Q2Entity& b = body(s, c);
  const auto release = [&] {
    c.claimed = false;
    m.patient = -1;
    m.revive_at = 0;
    m.enemy = foe_alive(s, m.old_enemy) ? m.old_enemy : -1;
  };
  if (!c.dead || b.gone || !gz_patient_keep(s, m, c)) {
    release();
    return false;
  }
  if (kind(m).repairs) {  // the repair bot's healing beam: a point a frame within 128
    if (s.level_time + 0.001 < m.busy_until) return true;
    const sh::Vec3f mid = b.origin + (b.mins + b.maxs) * 0.5f;
    turn(e, kind(m), yaw_to(b.origin - e.origin));
    if (sh::length(b.origin - e.origin) > 128 || !sees_body(s, m, c)) {
      go_to(s, m, b.origin, kind(m).run);
      return true;
    }
    sh::Vec3f d = mid - e.origin;
    sh::normalize(d);
    sound(s, m.entity, "misc/lasfly.wav", 3);
    beam_show(s, m, 0, e.origin + d * 16.0f, mid, true);
    b.health += 1;
    if (b.health > kind(c).mass / 10) {  // enough to stand again: up it comes, with no enemy
      release();
      revive(s, c, -1);
      e.origin.z += 1;
    }
    return true;
  }
  const float reach = s.opt.pack == Q2Pack::ground_zero ? 410.0f : 256.0f;  // the pack's farther
  const bool near = sh::length(b.origin - e.origin) <= reach && sees_body(s, m, c);
  if (m.revive_at > 0) {  // the cable is out
    if (near) fight_monster_line(s, "te_medic_cable_attack", m.entity, eye(e, kind(m)), b.origin);
    if (s.level_time + 0.001 < m.revive_at) return true;
    if (near) {
      fight_effect(s, "te_medic_cable", b.origin);
      sound(s, m.entity, cable_sound(m, 1));
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
  if (near && gz_cable_instead(s, m)) return true;  // a commander calls its guards instead
  if (near) {  // the cable: launched, hooked, and 1.7 s on the corpse rises
    sound(s, m.entity, cable_sound(m, 0));
    m.revive_at = s.level_time + 1.7;
    m.busy_until = s.level_time + 2.8;
    return true;
  }
  go_to(s, m, b.origin, kind(m).run);
  return true;
}

}  // namespace shooters_edge::server::monster_ai
