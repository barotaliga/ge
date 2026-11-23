// Quake II's monsters' shots: the aim and its spread, blows within reach, and every kind of shot an
// attack fires (q2_monster_ai.hpp; the numbers are the roster's, q2_monster_kinds.hpp).
//
// ge:summary Quake II's monsters' aim and shots: spread, blows, bolts, rockets, rails, beams
// ge:keywords q2 monsters monster-shots aim spread blow bolt rocket rail bfg drain sweep leap
// ge:skill shooter
// ge:test tests/shooters_edge_q2_monsters_test.cpp
#include "shooters_edge/server/q2_monster_ai.hpp"

#include <algorithm>
#include <cstdlib>
#include <string_view>

#include "core/prng.hpp"
#include "shooters_edge/server/q2_fight.hpp"
#include "shooters_edge/server/q2_fight_kit.hpp"
#include "shooters_edge/server/q2_flash.hpp"
#include "shooters_edge/server/q2_map.hpp"
#include "shooters_edge/server/q2_monsters.hpp"
#include "shooters_edge/server/q2_server.hpp"

namespace shooters_edge::server::monster_ai {

// The enemy's eye, `lead` s along his motion.
sh::Vec3f mark(const Q2Server& s, const Q2Monster& m, float lead) {
  if (m.blind) return m.blind_at;  // Ground Zero's blind fire: where he was last seen
  return foe_eye(s, m.enemy) + foe_velocity(s, m.enemy) * lead;
}

// A shot from `from` at `target`, with the attack's spread.
sh::Vec3f aim_at(Q2Server& s, const Attack& a, sh::Vec3f from, sh::Vec3f target) {
  sh::Vec3f dir = target - from;
  sh::normalize(dir);
  if (a.h == 0 && a.v == 0) return dir;
  const sh::ViewAxes ax = sh::angle_vectors({0, yaw_to(dir), 0});
  const float across = crandom(s) * a.h;  // the draws in a fixed order: one draw a statement
  const float up = crandom(s) * a.v;
  const sh::Vec3f end = from + dir * 8192 + ax.right * across + ax.up * up;
  sh::Vec3f out = end - from;
  sh::normalize(out);
  return out;
}
// The point a shot is aimed at: the mark, spread first by the soldiers' own aim.
sh::Vec3f aim_point(Q2Server& s, const Q2Monster& m, const Attack& a, sh::Vec3f from) {
  const sh::Vec3f t = mark(s, m, a.lead);
  if (a.aim_h == 0 && a.aim_v == 0) return t;
  Attack wide;
  const float keep = gz_base_spread(s, m);  // Ground Zero's soldiers' half on hard
  wide.h = a.aim_h * keep;
  wide.v = a.aim_v * keep;
  return from + aim_at(s, wide, from, t) * 8192;
}
sh::Vec3f aim(Q2Server& s, Q2Monster& m, const Attack& a, sh::Vec3f from) {
  return aim_at(s, a, from, aim_point(s, m, a, from));
}

int cause_of(const Q2Monster& m) { return kSqByMonster + m.kind; }

// A blow of `dmg` from `m` on its foe: a player hurt as by a monster, a monster as by this one.
void hurt_foe(Q2Server& s, const Q2Monster& m, int dmg, sh::Vec3f dir) {
  if (m.enemy >= 0) {
    const FightBlowFrom by(s, m.entity);
    fight_hurt(s, m.enemy, -1, {}, dmg, 0, cause_of(m));
    return;
  }
  if (const Q2Monster* o = foe_monster(s, m.enemy))
    monster_damage(s, o->entity, foe_of_entity(m.entity), dmg, dir);
  else if (foe_missile(m.enemy) >= 0)  // Ground Zero's tesla
    gz_tesla_damage(s, foe_missile(m.enemy), dmg);
}

const Attack& attack_at(const Q2Monster& m, int i) {
  return i < 2 ? kind(m).melee[i] : kind(m).missile[i - 2];
}
const Attack& attack_of(const Q2Monster& m) { return attack_at(m, m.attack); }

// A blow within 80 of the monster's origin, by a clear line: the damage and a kick added straight
// to the enemy's velocity (negative pulls). True when it landed.
bool blow(Q2Server& s, Q2Monster& m, const Attack& a) {
  const Q2Entity& e = body(s, m);
  const sh::Vec3f at = foe_at(s, m.enemy);
  if (sh::length(at - e.origin) > kMelee) return false;  // swung at the air
  if (!reaches(s, m, m.enemy, kOpaque)) return false;
  const int dmg = a.damage + static_cast<int>(random01(s) * (a.damage_max - a.damage + 1));
  sh::Vec3f dir = at - e.origin;
  sh::normalize(dir);
  hurt_foe(s, m, dmg, dir);
  if (a.kick != 0 && hunted(s, m.enemy)) fight_kick(s, m.enemy, dir * float(a.kick));
  return true;
}

// One frame's shots of attack `a` (`n` the shot's number in its loop).
void shoot(Q2Server& s, Q2Monster& m, const Attack& a, int n) {
  if (!foe_alive(s, m.enemy)) return;
  Q2Entity& e = body(s, m);
  const sh::Vec3f from = eye(e, kind(m));
  m.ideal_yaw = yaw_to(foe_at(s, m.enemy) - e.origin);
  if (a.fire != Fire::sweep) e.angles.y = m.ideal_yaw;
  // The shot leaving: its flash at the gun along the aim (a burst's every frame).
  const char* flash = nullptr;
  switch (a.fire) {
    case Fire::bolt:
    case Fire::sweep:
      flash = "te_monster_flash.blaster";
      break;
    case Fire::pellets:
    case Fire::burst:
    case Fire::guns:
      flash = "te_monster_flash.gun";
      break;
    case Fire::grenade:
    case Fire::rockets:
      flash = "te_monster_flash.rocket";
      break;
    case Fire::rail:
      flash = "te_monster_flash.rail";
      break;
    case Fire::bfg:
      flash = "te_monster_flash.bfg";
      break;
    default:
      break;
  }
  if (flash != nullptr) {
    const sh::Vec3f to = foe_at(s, m.enemy);
    sh::Vec3f d = to - from;
    sh::normalize(d);
    fight_monster_line(
        s, flash, m.entity, from, from + d * 16.0f,
        q2_monster_flash_value(looks_like(kind(m)), std::string_view(flash).substr(17)));
  }
  if (gz_base_shoot(s, m, a, from)) return;  // Ground Zero's own aim (q2_gz_base_monsters.cpp)
  switch (a.fire) {
    case Fire::bolt:
      for (int i = 0; i < a.count; i++)
        fight_monster_launch(s, m.entity, shot_of(a, SqShot::bolt), from, aim(s, m, a, from), a.damage, a.speed,
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
    case Fire::pellets: {
      const sh::Vec3f to = aim_point(s, m, a, from);  // one aim, each pellet spread about it
      for (int i = 0; i < a.count; i++) {
        const sh::Vec3f d = aim_at(s, a, from, to);
        fight_monster_bullet(s, m.entity, from, from + d * 8192, a.damage, a.kick, cause_of(m));
      }
      return;
    }
    case Fire::burst:
    case Fire::guns:
      for (int i = 0; i < (a.fire == Fire::guns ? a.count : 1); i++) {
        const sh::Vec3f d = aim(s, m, a, from);
        fight_monster_bullet(s, m.entity, from, from + d * 8192, a.damage, a.kick, cause_of(m));
      }
      return;
    case Fire::drain: {  // a tongue that holds while the line is clear, within 256, ±30° of pitch
      if (!reaches(s, m, m.enemy, kShotMask & ~(sh::kSqMonster | sh::kSqDeadMonster))) return;
      const sh::Vec3f d = foe_at(s, m.enemy) - from;
      const float flat = sh::length(sh::Vec3f{d.x, d.y, 0});
      if (sh::length(d) > 256 || std::abs(d.z) > 0.57735f * flat) return;  // tan 30°
      fight_monster_line(s, "te_parasite_attack", m.entity, from, foe_at(s, m.enemy));
      const int dmg = m.burst_frame == 0 ? a.damage : a.damage_max;
      hurt_foe(s, m, dmg, {});
      return;
    }
    case Fire::club:
      if (blow(s, m, a)) m.struck = true;
      return;
    case Fire::zap: {  // wherever he is
      const int dmg = a.damage + static_cast<int>(random01(s) * (a.damage_max - a.damage + 1));
      fight_effect(s, "te_sparks", e.origin + forward_of(e.angles.y) * 18.5f);
      hurt_foe(s, m, dmg, {});
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
      fight_monster_launch(s, m.entity, shot_of(a, SqShot::bolt), from, d, a.damage, a.speed, cause_of(m));
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

SqShot shot_of(const Attack& a, SqShot usual) {
  switch (a.shot) {
    case roster::Shot::ripper:
      return SqShot::ripper;
    case roster::Shot::blue_bolt:
      return SqShot::blue_bolt;
    case roster::Shot::plasma:
      return SqShot::plasma;
    case roster::Shot::heat_rocket:
      return SqShot::heat_rocket;
    case roster::Shot::spit:
      return SqShot::spit;
    case roster::Shot::green_bolt:
      return SqShot::green_bolt;
    case roster::Shot::usual:
      break;
  }
  return usual;
}

int shield_take(Q2Server& s, Q2Monster& m, int damage, sh::Vec3f dir) {
  const int covered = std::min(damage * 2 / 3, m.shield * 2);
  if (covered <= 0) return damage;
  m.shield -= (covered + 1) / 2;
  const Q2Entity& e = body(s, m);
  sh::Vec3f way = dir;
  if (sh::normalize(way) == 0) way = forward_of(e.angles.y) * -1.0f;
  m.shield_flash = fight_power_hit(s, e.origin + way * -16.0f, way * -1.0f, true);
  return damage - covered;
}

}  // namespace shooters_edge::server::monster_ai
