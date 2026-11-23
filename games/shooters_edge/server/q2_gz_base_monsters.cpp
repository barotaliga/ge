// Ground Zero's changes to the base game's monsters (q2_monster_ai.hpp § Ground Zero's base
// monsters), each only under the pack's switch: the iron maiden's and the tanks' faster, led
// rockets aimed at the feet or the eye with a clear line, the hornet's bracketing volley, the
// soldiers' tighter spread on hard and their clear line, the fliers' strafing bursts, the kamikaze
// flyer, the infantry and gunner feeling no pain in the air; and, for the shared monster AI that
// the pack also changes, each kind's ledge jump, dodge and blind fire as data.
//
// ge:summary Ground Zero's changes to the base monsters: rockets, volleys, strafing, the kamikaze
// ge:keywords q2 ground-zero rogue monsters iron-maiden tank hornet boss2 soldier spread
// ge:keywords clear-shot strafing-burst flyer hover floater daedalus kamikaze ledge-jump dodge
// ge:keywords blind-fire
// ge:skill shooter
// ge:test tests/shooters_edge_q2_gz_base_monsters_test.cpp
#include <string_view>

#include "shooters_edge/server/q2_fight.hpp"
#include "shooters_edge/server/q2_fight_kit.hpp"
#include "shooters_edge/server/q2_monster_ai.hpp"
#include "shooters_edge/server/q2_monsters.hpp"

namespace shooters_edge::server::monster_ai {

namespace {

bool gz(const Q2Server& s) { return s.opt.pack == Q2Pack::ground_zero; }

bool soldier(const Q2Monster& m) {
  return std::string_view(looks_like(kind(m))).starts_with("monster_soldier");
}
bool tank(const Q2Monster& m) {
  return named(m, "monster_tank") || named(m, "monster_tank_commander");
}
bool maiden(const Q2Monster& m) { return named(m, "monster_chick"); }
bool flier_that_strafes(const Q2Monster& m) {
  const std::string_view l = looks_like(kind(m));
  return l == "monster_flyer" || l == "monster_hover" || l == "monster_floater";
}

// Whether the line from `from` to `to` first meets something other than `m`'s enemy or the world
// (another monster, another player): the shot is held.
bool crossed(Q2Server& s, const Q2Monster& m, sh::Vec3f from, sh::Vec3f to) {
  const Line l = fight_line(s, from, to, 0, m.entity);
  if (l.fraction >= 1) return false;
  if (l.player >= 0) return l.player != m.enemy;
  if (l.mover >= 0 && monster_index(s, l.mover) >= 0) return l.mover != foe_entity(m.enemy);
  return false;  // the world, or a mover: the shot goes
}

// The maiden's and the tanks' rocket: 500 + 100 a skill; at the eye `eye_odds` of the time (and
// whenever his feet are above the gun), else at his feet; led by its flight time 0.2 + 0.15 a
// skill below 3 of the time; held when another stands in the line.
void rocket(Q2Server& s, Q2Monster& m, const Attack& a, sh::Vec3f from, float eye_odds) {
  const float speed = 500.0f + 100.0f * float(skill(s));
  sh::Vec3f lo, hi;
  foe_box(s, m.enemy, lo, hi);
  const sh::Vec3f at = foe_at(s, m.enemy);
  const sh::Vec3f feet = at + sh::Vec3f{0, 0, lo.z};
  sh::Vec3f to = feet.z > from.z || random01(s) < eye_odds ? foe_eye(s, m.enemy) : feet;
  if (random01(s) < 0.2 + 0.15 * double(3 - skill(s))) {
    const float flight = sh::length(to - from) / speed;
    to = to + foe_velocity(s, m.enemy) * flight;
  }
  if (crossed(s, m, from, to)) return;
  sh::Vec3f dir = to - from;
  if (sh::normalize(dir) <= 0) return;
  const int volley = a.every > 0 ? a.count : 1;
  for (int i = 0; i < volley; i++)
    fight_monster_launch(s, m.entity, SqShot::rocket, from, dir, a.damage, speed, cause_of(m));
}

// The hornet's four rockets. At a player, nine volleys in ten bracket his path: 750 units/s, each
// aimed where he will be after its flight and −0.3, −0.15, 0 or +0.15 s. Otherwise 500 at his
// origin, the outer two 15 lower and fanned 0.4 to each side, the inner two 0.025.
void hornet(Q2Server& s, Q2Monster& m, const Attack& a, sh::Vec3f from) {
  const sh::Vec3f at = foe_at(s, m.enemy);
  const Q2Entity& e = body(s, m);
  if (m.enemy >= 0 && random01(s) < 0.9) {
    constexpr float kOffsets[4] = {-0.3f, -0.15f, 0, 0.15f};
    for (int i = 0; i < 4; i++) {
      const float flight = sh::length(at - from) / 750.0f + kOffsets[i];
      sh::Vec3f dir = at + foe_velocity(s, m.enemy) * flight - from;
      if (sh::normalize(dir) <= 0) continue;
      fight_monster_launch(s, m.entity, SqShot::rocket, from, dir, a.damage, 750, cause_of(m));
    }
    return;
  }
  const sh::Vec3f right = forward_of(e.angles.y - 90);
  constexpr float kFan[4] = {0.4f, 0.025f, -0.025f, -0.4f};
  for (int i = 0; i < 4; i++) {
    const sh::Vec3f to = at - sh::Vec3f{0, 0, i == 0 || i == 3 ? 15.0f : 0.0f};
    sh::Vec3f dir = to - from;
    if (sh::normalize(dir) <= 0) continue;
    dir = dir + right * kFan[i];
    sh::normalize(dir);
    fight_monster_launch(s, m.entity, SqShot::rocket, from, dir, a.damage, 500, cause_of(m));
  }
}

}  // namespace

bool gz_base_shoot(Q2Server& s, Q2Monster& m, const Attack& a, sh::Vec3f from) {
  if (!gz(s)) return false;
  if (soldier(m) && m.enemy >= 0 && crossed(s, m, from, foe_eye(s, m.enemy)))
    return true;  // a soldier holds its shot
  if (a.fire != Fire::rockets) return false;
  if (maiden(m)) {
    rocket(s, m, a, from, 0.33f);
    return true;
  }
  if (tank(m)) {
    rocket(s, m, a, from, 0.66f);
    return true;
  }
  if (named(m, "monster_boss2")) {
    hornet(s, m, a, from);
    return true;
  }
  return false;
}

float gz_base_spread(const Q2Server& s, const Q2Monster& m) {
  return gz(s) && soldier(m) && skill(s) >= 2 ? 0.5f : 1.0f;
}

double gz_base_again(const Q2Server& s, const Q2Monster& m) {
  return gz(s) && maiden(m) ? 0.05 * double(skill(s)) : 0.0;
}

void gz_base_begin(Q2Server& s, Q2Monster& m, const Attack& a) {
  if (!gz(s) || !flier_that_strafes(m) || a.fire != Fire::bolt) {
    m.strafe = 0;
    return;
  }
  const int k = skill(s);
  double odds = k == 0 ? 0 : 1.0 - 0.5 / double(k);
  if (k > 0 && named(m, "monster_daedalus")) odds += 0.1;
  if (random01(s) >= odds) {
    m.strafe = 0;
    return;
  }
  const int side = m.strafe != 0 ? m.strafe : 1;  // the side it last took, or right
  m.strafe = random01(s) < 0.5 ? -side : side;
}

bool gz_base_think(Q2Server& s, Q2Monster& m) {
  if (!gz(s)) return false;
  if (named(m, "monster_kamikaze")) {
    Q2Entity& e = body(s, m);
    if (m.enemy == -1) notice(s, m);
    if (m.enemy != -1 && !foe_alive(s, m.enemy)) m.enemy = -1;
    if (m.enemy == -1) return true;  // it waits where it is
    const sh::Vec3f at = foe_at(s, m.enemy);
    if (sh::length(at - e.origin) < 90) {  // on him: 50 straight to him, then it bursts
      sh::Vec3f dir = at - e.origin;
      sh::normalize(dir);
      hurt_foe(s, m, 50, dir);
      monster_damage(s, m.entity, -1, 100000, {});
      return true;
    }
    turn(e, kind(m), yaw_to(at - e.origin));
    sh::Vec3f dir = at - e.origin;
    sh::normalize(dir);
    if (!step(s, m, dir * (kind(m).run * kStep))) {  // blocked: a harmless burst
      fight_effect(s, "te_rocket_explosion", e.origin - dir * 2.0f);
      monster_damage(s, m.entity, -1, 100000, {});
    }
    return true;
  }
  // A strafing burst slides 10 a frame to its side while it fires.
  if (m.strafe != 0 && m.busy_until > s.level_time && m.attack >= 2 &&
      s.level_time + 0.001 >= m.fire_at && s.level_time <= m.burst_until + 0.001) {
    const Q2Entity& e = body(s, m);
    step(s, m, forward_of(e.angles.y - 90) * (10.0f * float(m.strafe)));
  }
  return false;
}

bool gz_base_pain(Q2Server& s, Q2Monster& m) {
  if (!gz(s)) return false;
  if (named(m, "monster_kamikaze")) return true;  // it feels none
  // The infantry and the gunner feel none in the air.
  return (named(m, "monster_infantry") || named(m, "monster_gunner")) && !m.on_ground;
}

const GzLedge* gz_ledge(std::string_view classname) {
  // Drop and climb limits, then the jump up's and the jump down's speeds (forward, up).
  static constexpr GzLedge kLedges[] = {
      {"monster_infantry", 192, 40, 150, 400, 100, 300},
      {"monster_gunner", 192, 40, 150, 400, 100, 300},
      {"monster_berserk", 256, 40, 150, 400, 100, 300},
      {"monster_parasite", 256, 68, 200, 450, 100, 300},
      {"monster_mutant", 256, 68, 200, 450, 100, 300},
  };
  for (const GzLedge& l : kLedges)
    if (classname == l.classname) return &l;
  return nullptr;
}

const GzDodge* gz_dodge_of(std::string_view classname) {
  // Whether it ducks and side-steps, and whether above easy it holds still for its attacks.
  static constexpr GzDodge kDodges[] = {
      {"monster_soldier_light", true, true, false}, {"monster_soldier", true, true, false},
      {"monster_soldier_ss", true, true, false},    {"monster_infantry", true, true, true},
      {"monster_gunner", true, true, true},         {"monster_berserk", false, true, false},
      {"monster_brain", true, false, false},        {"monster_chick", true, true, true},
      {"monster_medic", true, true, true},
  };
  for (const GzDodge& d : kDodges)
    if (classname == d.classname) return &d;
  return nullptr;
}

const GzBlind* gz_blind_of(std::string_view classname) {
  // The delay that grows each try (s, lowest and highest), and the time it waits after (s).
  static constexpr GzBlind kBlind[] = {
      {"monster_soldier_light", 4.1f, 7.1f, 1.5f, 2.5f},
      {"monster_gunner", 4.1f, 7.1f, 0, 2},
      {"monster_chick", 5.5f, 6.5f, 0, 2},
      {"monster_tank", 5.2f, 8.2f, 3, 5},
      {"monster_tank_commander", 5.2f, 8.2f, 3, 5},
  };
  for (const GzBlind& b : kBlind)
    if (classname == b.classname) return &b;
  return nullptr;
}

}  // namespace shooters_edge::server::monster_ai
