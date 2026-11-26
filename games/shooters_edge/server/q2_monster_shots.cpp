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

int cause_of(const Q2Monster& m) {
  switch (attack_of(m).words) {
    case roster::Words::light:
      return kSqByLaser;  // "saw the light."
    case roster::Words::hurt:
      return kSqByGotta;  // "that's gotta hurt."
    case roster::Words::usual:
      break;
  }
  return kSqByMonster + m.kind;
}

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
  else  // a thing an anger set it on (a wall, a point); none for no foe
    map_damage(s, foe_entity(m.enemy), foe_of_entity(m.entity), dmg, dir);
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

// A beam of a laser from `m`'s eye `i` (one of two side by side when it has two) at its enemy's
// middle: through every body on it to the first wall, `a.damage` to each, sparks where it stops,
// drawn as the monster's beam for this frame and the next.
void laser(Q2Server& s, Q2Monster& m, const Attack& a, int i) {
  const Q2Entity& e = body(s, m);
  sh::Vec3f from = eye(e, kind(m));
  if (a.count > 1) {
    const sh::ViewAxes ax = sh::angle_vectors({0, e.angles.y, 0});
    from = e.origin + ax.forward * 8.0f + ax.right * (i == 0 ? -4.0f : 4.0f) + sh::Vec3f{0, 0, 32};
  }
  sh::Vec3f lo, hi;
  foe_box(s, m.enemy, lo, hi);
  sh::Vec3f d = foe_at(s, m.enemy) + (lo + hi) * 0.5f - from;
  sh::normalize(d);
  const LaserEnd end = fight_laser(s, from, from + d * 2048.0f, a.damage, -1, m.entity);
  beam_show(s, m, i, from, end.at, false);
  if (i == 0 && random01(s) < 0.2) sound(s, m.entity, "misc/lasfly.wav", 3);
  if (end.normal.x == 0 && end.normal.y == 0 && end.normal.z == 0) return;
  NetEvent ev;
  ev.kind = EventKind::point_effect;
  ev.to = kEveryone;
  ev.origin = end.at;
  ev.values[0] = end.normal.x;
  ev.values[1] = end.normal.y;
  ev.values[2] = end.normal.z;
  ev.values[3] = 10;
  ev.level = 240;
  ev.text = "te_laser_sparks";
  event(s, std::move(ev));
}

// The brain's tongue: from 24 ahead and 16 up at its enemy's origin (else the top of his box less
// 8, else its bottom plus 8), within 512 and 30 degrees of level, by a clear line that meets him:
// the drain's line drawn, `a.damage` and his speed made a pull of `-a.kick` toward the brain.
void tongue(Q2Server& s, Q2Monster& m, const Attack& a) {
  Q2Entity& e = body(s, m);
  const sh::Vec3f fwd = forward_of(e.angles.y);
  const sh::Vec3f from = e.origin + fwd * 24.0f + sh::Vec3f{0, 0, 16};
  sh::Vec3f lo, hi;
  foe_box(s, m.enemy, lo, hi);
  const sh::Vec3f at = foe_at(s, m.enemy);
  const sh::Vec3f tries[3] = {at, at + sh::Vec3f{0, 0, hi.z - 8}, at + sh::Vec3f{0, 0, lo.z + 8}};
  bool fits = false;
  for (const sh::Vec3f& t : tries) {
    const sh::Vec3f d = t - from;
    const float flat = sh::length(sh::Vec3f{d.x, d.y, 0});
    if (sh::length(d) <= 512 && std::abs(d.z) <= 0.57735f * flat) {  // tan 30°
      fits = true;
      break;
    }
  }
  if (!fits || !reaches(s, m, m.enemy, kShotMask & ~(sh::kSqMonster | sh::kSqDeadMonster))) return;
  sound(s, m.entity, "brain/brnatck3.wav");
  fight_monster_line(s, "te_parasite_attack", m.entity, from, at);
  hurt_foe(s, m, a.damage, {});
  const sh::Vec3f pull = fwd * float(a.kick);  // `kick` negative: toward the brain
  if (hunted(s, m.enemy)) fight_kick(s, m.enemy, pull - foe_velocity(s, m.enemy));
  e.origin.z += 1;
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
  if (a.shot == roster::Shot::plasma || a.shot == roster::Shot::spit) flash = nullptr;  // unlit
  if (flash != nullptr) {
    const sh::Vec3f to = foe_at(s, m.enemy);
    sh::Vec3f d = to - from;
    sh::normalize(d);
    fight_monster_line(
        s, flash, m.entity, from, from + d * 16.0f,
        kind(m).flash != 0
            ? kind(m).flash
            : q2_monster_flash_value(looks_like(kind(m)), std::string_view(flash).substr(17)));
  }
  if (gz_base_shoot(s, m, a, from)) return;  // Ground Zero's own aim (q2_gz_base_monsters.cpp)
  switch (a.fire) {
    case Fire::bolt:
      for (int i = 0; i < a.count; i++)
        fight_monster_launch(s, m.entity, shot_of(a, SqShot::bolt), from, aim(s, m, a, from),
                             a.damage, a.speed, cause_of(m));
      return;
    case Fire::grenade:
      fight_monster_launch(s, m.entity, SqShot::grenade, from, aim(s, m, a, from), a.damage,
                           a.speed, cause_of(m));
      return;
    case Fire::rockets: {
      const int volley = a.every > 0 ? a.count : 1;  // on a loop: all at once
      for (int i = 0; i < volley; i++)
        fight_monster_launch(s, m.entity, shot_of(a, SqShot::rocket), from, aim(s, m, a, from),
                             a.damage, a.speed, cause_of(m));
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
      // Ground Zero's: never from a muzzle inside a wall (no shot through it).
      if (s.opt.pack == Q2Pack::ground_zero &&
          (map_contents(s, from) & (sh::kSqSolid | sh::kSqWindow)) != 0)
        return;
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
      fight_monster_launch(s, m.entity, shot_of(a, SqShot::bolt), from, d, a.damage, a.speed,
                           cause_of(m));
      return;
    }
    case Fire::leap: {  // off the ground at the enemy
      const sh::Vec3f fwd = forward_of(e.angles.y);
      e.origin.z += 1;
      m.velocity = fwd * a.speed + sh::Vec3f{0, 0, a.rise};
      m.on_ground = false;
      m.leaping = true;
      m.struck = false;
      sound(s, m.entity, kind(m).sight);
      return;
    }
    case Fire::laser:
      for (int i = 0; i < a.count; i++) laser(s, m, a, i);
      return;
    case Fire::tongue:
      tongue(s, m, a);
      return;
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
