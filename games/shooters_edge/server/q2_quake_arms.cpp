// Quake's weapons in Quake II's rules (q2_quake_arms.hpp).
//
// ge:summary Quake's eight weapons fired whole by Quake II's rules: kit, items, shots, missiles
// ge:keywords q1-arms quake-weapons remix-weapons quake_fire quake_missile sq_quake_take
// ge:skill shooter
// ge:test tests/shooters_edge_remix_q1_arms_test.cpp
#include "shooters_edge/server/q2_quake_arms.hpp"

#include <string_view>

#include "classic/q1_arms.hpp"
#include "core/prng.hpp"
#include "shooters_edge/server/q2_fight.hpp"
#include "shooters_edge/server/q2_fight_kit.hpp"
#include "shooters_edge/server/q2_map.hpp"
#include "shooters_edge/server/q2_monsters.hpp"
#include "shooters_edge/server/q2_server.hpp"
#include "shooters_edge/server/q2_stats.hpp"

namespace shooters_edge::server {

namespace gc = ge::classic;

namespace {

int at(SqAmmo a) { return static_cast<int>(a); }

sh::Vec3f origin_of(const Q2Player& p) {
  return {float(p.move.origin[0]) * 0.125f, float(p.move.origin[1]) * 0.125f,
          float(p.move.origin[2]) * 0.125f};
}

gc::Q1ArmId arm_of(int w) { return static_cast<gc::Q1ArmId>(w - kSqQuakeFirst); }

sh::Vec3f unit(sh::Vec3f v) {
  const float n = sh::length(v);
  return n > 0 ? v * (1 / n) : v;
}

bool infinite(const Q2Server& s) { return fight_anyflag(s, 8192) || fight_midair(s); }

// A blow from `slot`'s weapon `w` on what line `l` met: a player, a monster or a shootable map
// entity; `blood` drawn where it bleeds, `wall` where it does not. True when it met a body.
bool strike(Q2Server& s, int slot, const Line& l, sh::Vec3f dir, int damage, int w,
            const char* wall) {
  if (l.player >= 0) {
    fight_hurt(s, l.player, slot, dir, damage, damage, w);
    fight_effect(s, gc::kQ1HitBlood, l.end, l.normal);
    return true;
  }
  if (l.fraction >= 1 || l.sky) return false;
  const bool body = l.mover >= 0 && monster_index(s, l.mover) >= 0;
  if (l.mover >= 0) map_damage(s, l.mover, slot, damage, dir, false, &l.end);
  if (body) {
    fight_effect(s, gc::kQ1HitBlood, l.end, l.normal);
    return true;
  }
  if (wall != nullptr) fight_effect(s, wall, l.end, l.normal);
  monster_noise(s, slot, l.end, true);
  return false;
}

// The shotguns' pellets from the eye, each turned by crandom × `across` and × `up`; a player's
// pellets summed into one blow, as Quake deals them.
void pellets(Q2Server& s, int slot, int count, float across, float up, int quad, int w) {
  const Q2Player& p = s.players[slot];
  const sh::ViewAxes ax = sh::angle_vectors(p.view_angles);
  const sh::Vec3f from = origin_of(p) + sh::Vec3f{0, 0, p.view_height};
  int summed[sh::kMaxClients] = {};
  for (int i = 0; i < count; i++) {
    const float r1 = fight_crandom(s);  // the draws in a fixed order
    const float r2 = fight_crandom(s);
    const sh::Vec3f dir = ax.forward + ax.right * (r1 * across) + ax.up * (r2 * up);
    const Line l = fight_line(s, from, from + dir * gc::kQ1PelletReach, 1u << slot);
    if (l.player >= 0) {
      summed[l.player] += static_cast<int>(gc::kQ1PelletDamage) * quad;
      fight_effect(s, gc::kQ1HitBlood, l.end, l.normal);
      continue;
    }
    strike(s, slot, l, dir, static_cast<int>(gc::kQ1PelletDamage) * quad, w, gc::kQ1HitPellet);
  }
  for (int i = 0; i < sh::kMaxClients; i++)
    if (summed[i] > 0) fight_hurt(s, i, slot, ax.forward, summed[i], summed[i], w);
}

Q2Missile* missile(Q2Server& s, int slot, SqShot kind, sh::Vec3f start, sh::Vec3f velocity,
                   float life, int w) {
  Q2Missile* m = fight_new_missile(s);
  if (m == nullptr) return nullptr;
  *m = Q2Missile{};
  m->live = true;
  m->kind = kind;
  m->owner = slot;
  m->weapon = w;
  m->origin = start;
  m->velocity = velocity;
  m->dies_at = s.level_time + life;
  m->fired_z = origin_of(s.players[slot]).z;
  return m;
}

// The lightning gun's beam from 16 over the origin to the first wall 600 away; 30 to each thing on
// a centre line and two side lines, once each.
void beam(Q2Server& s, int slot, int quad, int w) {
  const Q2Player& p = s.players[slot];
  const sh::ViewAxes ax = sh::angle_vectors(p.view_angles);
  const sh::Vec3f from = origin_of(p) + sh::Vec3f{0, 0, gc::kQ1EyeLift};
  const uint32_t me = 1u << slot;
  const Line wall = fight_line(s, from, from + ax.forward * gc::kQ1BeamReach, me);
  fight_line_effect(s, gc::kQ1Beam, from, wall.end, slot + 1);
  const sh::Vec3f to = wall.end + ax.forward * 4;
  const sh::Vec3f d = to - from;
  const sh::Vec3f side{-d.y * 16, d.x * 16, 0};
  const sh::Vec3f offsets[3] = {{0, 0, 0}, side, sh::Vec3f{0, 0, 0} - side};
  int met_player[3] = {-1, -1, -1}, met_mover[3] = {-1, -1, -1};
  for (int line = 0; line < 3; line++) {
    const Line l = fight_line(s, from + offsets[line], to + offsets[line], me);
    if (l.player < 0 && (l.mover < 0 || l.fraction >= 1)) continue;
    bool seen = false;
    for (int i = 0; i < line; i++)
      seen = seen || (l.player >= 0 ? met_player[i] == l.player : met_mover[i] == l.mover);
    if (seen) continue;
    met_player[line] = l.player;
    met_mover[line] = l.player >= 0 ? -1 : l.mover;
    strike(s, slot, l, ax.forward, static_cast<int>(gc::kQ1BeamDamage) * quad, w, nullptr);
  }
}

// The lightning gun under water: every cell let go at once round the firer.
void discharge(Q2Server& s, int slot, int w) {
  Q2Player& p = s.players[slot];
  int& cells = p.body.ammo[at(SqAmmo::cells)];
  const int points = static_cast<int>(gc::kQ1DischargePerCell) * (cells + 1);  // this shot's too
  cells = 0;
  fight_blast(s, origin_of(p), slot, points, float(points + 40), w);
  p.hand.pending = sq_best_weapon(p.body);
}

// A blast where missile `m` is: its radius damage to all near (`ignore` and `ignore_entity` left
// out), Quake's explosion and its sound, the missile gone.
void burst(Q2Server& s, Q2Missile& m, sh::Vec3f where, int ignore = -1, int ignore_entity = -1) {
  fight_blast(s, where, m.owner, m.radius_damage, m.radius, m.weapon, ignore, false, ignore_entity);
  fight_effect(s, gc::kQ1Blast, where);
  fight_sound(s, fight_missile_entity(s, m), where, gc::kQ1Blasted);
  m.live = false;
}

}  // namespace

SqQuakeTake sq_quake_take(int item) {
  if (item < 0 || item >= kSqItemCount) return {};
  const std::string_view name = kSqItems[item].classname;
  struct Row {
    std::string_view from, keep;
    SqAmmo ammo;
    int amount;
  };
  static constexpr Row kRows[] = {
      {"weapon_shotgun", "weapon_supershotgun", SqAmmo::shells, 5},
      {"weapon_supershotgun", "weapon_supershotgun", SqAmmo::shells, 5},
      {"weapon_machinegun", "weapon_machinegun", SqAmmo::bullets, 30},
      {"weapon_chaingun", "weapon_chaingun", SqAmmo::bullets, 30},
      {"weapon_grenadelauncher", "weapon_grenadelauncher", SqAmmo::rockets, 5},
      {"weapon_rocketlauncher", "weapon_rocketlauncher", SqAmmo::rockets, 5},
      {"weapon_hyperblaster", "weapon_railgun", SqAmmo::cells, 15},
      {"weapon_railgun", "weapon_railgun", SqAmmo::cells, 15},
      {"weapon_bfg", "weapon_railgun", SqAmmo::cells, 15},
      {"ammo_shells", "", SqAmmo::shells, 20},
      {"ammo_bullets", "", SqAmmo::bullets, 25},
      {"ammo_grenades", "", SqAmmo::rockets, 5},
      {"ammo_rockets", "", SqAmmo::rockets, 5},
      {"ammo_cells", "", SqAmmo::cells, 6},
      {"ammo_slugs", "", SqAmmo::cells, 6},
  };
  for (const Row& r : kRows)
    if (r.from == name) return {r.keep.empty() ? -1 : sq_item(r.keep), r.ammo, r.amount};
  return {};
}

void sq_quake_body(SqBody& b) {
  b.quake = true;
  b.blaster = false;
  b.max_ammo[at(SqAmmo::shells)] = 100;
  b.max_ammo[at(SqAmmo::bullets)] = 200;
  b.max_ammo[at(SqAmmo::rockets)] = 100;
  b.max_ammo[at(SqAmmo::cells)] = 100;
  b.max_ammo[at(SqAmmo::grenades)] = 0;
  b.max_ammo[at(SqAmmo::slugs)] = 0;
  b.ammo[at(SqAmmo::shells)] = 25;
}

bool quake_arms_worn(const Q2Server& s) { return s.cvars.find("sv_wear")->value == "q1"; }

void quake_arms_spawn(Q2Server& s, int slot) {
  Q2Player& p = s.players[slot];
  p.q1_cut_at = 0;
  p.q1_nail_left = false;
  p.q1_beam_next = 0;
  p.q1_beam_held = false;
  p.q1_run_from = p.q1_last_shot = -100;
  if (!p.body.quake) return;
  p.hand.weapon = int(SqWeapon::q1_shotgun);
}

void quake_fire(Q2Server& s, int slot, int shots) {
  if (shots <= 0) return;
  Q2Player& p = s.players[slot];
  const int w = p.hand.weapon;
  const gc::Q1ArmId id = arm_of(w);
  const SqWeaponRow& r = kSqWeapons[w];
  const sh::Vec3f origin = origin_of(p);
  monster_noise(s, slot);
  p.hostile_until = s.level_time + 1;
  p.pose.shot = true;
  // The view model's fire run: from this shot, or on round while a held weapon keeps firing.
  if (!gc::kQ1ArmFrames[int(id)].loops || s.frame_number - p.q1_last_shot > 1)
    p.q1_run_from = s.frame_number;
  p.q1_last_shot = s.frame_number;
  const int quad = fight_multiplier(s, p);
  if (const char* boost = fight_multiplier_sound(quad)) fight_item_sound(s, slot, boost);
  stats_shot(s, slot, w, 1);
  const sh::ViewAxes ax = sh::angle_vectors(p.view_angles);
  // The second shell or nail the super weapons' shot takes beside the one the hand took.
  const auto second = [&](SqAmmo a) {
    if (infinite(s)) return true;
    if (p.body.ammo[at(a)] < 1) return false;
    p.body.ammo[at(a)]--;
    return true;
  };
  const auto heard = [&](const char* sample) { fight_sound(s, slot + 1, origin, sample); };
  switch (id) {
    case gc::Q1ArmId::axe:
      heard(r.sound);
      p.q1_cut_at = s.level_time + gc::kQ1AxeDelay;
      return;
    case gc::Q1ArmId::shotgun:
      heard(r.sound);
      pellets(s, slot, gc::kQ1ShotgunPellets, gc::kQ1ShotgunAcross, gc::kQ1ShotgunUp, quad, w);
      return;
    case gc::Q1ArmId::super_shotgun:
      if (!second(SqAmmo::shells)) {  // a single shell: the shotgun's blast
        heard(kSqWeapons[int(SqWeapon::q1_shotgun)].sound);
        pellets(s, slot, gc::kQ1ShotgunPellets, gc::kQ1ShotgunAcross, gc::kQ1ShotgunUp, quad, w);
        return;
      }
      heard(r.sound);
      pellets(s, slot, gc::kQ1SuperPellets, gc::kQ1SuperAcross, gc::kQ1SuperUp, quad, w);
      return;
    case gc::Q1ArmId::nailgun:
    case gc::Q1ArmId::super_nailgun: {
      const bool heavy = id == gc::Q1ArmId::super_nailgun && second(SqAmmo::bullets);
      heard(heavy ? r.sound : kSqWeapons[int(SqWeapon::q1_nailgun)].sound);
      sh::Vec3f start = origin + sh::Vec3f{0, 0, gc::kQ1EyeLift};
      if (!heavy) {  // the nailgun's barrels, right then left
        start = start + ax.right * (p.q1_nail_left ? -gc::kQ1NailSide : gc::kQ1NailSide);
        p.q1_nail_left = !p.q1_nail_left;
      }
      Q2Missile* m = missile(s, slot, heavy ? SqShot::q1_super_spike : SqShot::q1_spike, start,
                             ax.forward * gc::kQ1SpikeSpeed, gc::kQ1SpikeLife, w);
      if (m != nullptr)
        m->damage = static_cast<int>(heavy ? gc::kQ1SuperSpikeDamage : gc::kQ1SpikeDamage) * quad;
      return;
    }
    case gc::Q1ArmId::grenade_launcher: {
      heard(r.sound);
      sh::Vec3f v = ax.forward * gc::kQ1GrenadeSpeed;
      if (p.view_angles.x != 0) {
        const float r1 = fight_crandom(s);  // the draws in a fixed order
        const float r2 = fight_crandom(s);
        v = v + ax.up * gc::kQ1GrenadeLift + ax.right * (r1 * gc::kQ1GrenadeScatter) +
            ax.up * (r2 * gc::kQ1GrenadeScatter);
      } else {
        v.z = gc::kQ1GrenadeLift;
      }
      Q2Missile* m = missile(s, slot, SqShot::q1_grenade, origin, v, gc::kQ1GrenadeFuse, w);
      if (m != nullptr) {
        m->radius_damage = static_cast<int>(gc::kQ1GrenadeSplash) * quad;
        m->radius = float(m->radius_damage + 40);
      }
      return;
    }
    case gc::Q1ArmId::rocket_launcher: {
      heard(r.sound);
      const sh::Vec3f start =
          origin + ax.forward * gc::kQ1RocketForward + sh::Vec3f{0, 0, gc::kQ1EyeLift};
      Q2Missile* m = missile(s, slot, SqShot::q1_rocket, start, ax.forward * gc::kQ1RocketSpeed,
                             gc::kQ1RocketLife, w);
      if (m != nullptr) {
        const double extra = ge::core::next(s.rng) * double(gc::kQ1RocketHitExtra);
        m->damage = static_cast<int>(double(gc::kQ1RocketHit) + extra) * quad;
        m->radius_damage = static_cast<int>(gc::kQ1RocketSplash) * quad;
        m->radius = float(m->radius_damage + 40);
      }
      return;
    }
    case gc::Q1ArmId::lightning:
      if (!p.q1_beam_held) {
        fight_sound(s, slot + 1, origin, gc::kQ1BeamStart);
        p.q1_beam_held = true;
      }
      if (p.water_level > 1) {
        discharge(s, slot, w);
        return;
      }
      if (s.level_time >= p.q1_beam_next - 0.001) {
        heard(gc::kQ1BeamHit);
        p.q1_beam_next = s.level_time + gc::kQ1BeamSoundEvery;
      }
      beam(s, slot, quad, w);
      return;
  }
}

void quake_arms_frame(Q2Server& s, int slot, bool attack) {
  Q2Player& p = s.players[slot];
  if (!attack) p.q1_beam_held = false;
  if (p.q1_cut_at <= 0 || s.level_time + 0.001 < p.q1_cut_at) return;
  p.q1_cut_at = 0;
  if (!p.alive) return;
  // The axe's cut lands: 64 units along the view as it is now, from 16 over the origin.
  const sh::ViewAxes ax = sh::angle_vectors(p.view_angles);
  const sh::Vec3f from = origin_of(p) + sh::Vec3f{0, 0, gc::kQ1EyeLift};
  const Line l = fight_line(s, from, from + ax.forward * gc::kQ1AxeReach, 1u << slot);
  const int w = int(SqWeapon::q1_axe);
  const int damage = static_cast<int>(gc::kQ1AxeDamage) * fight_multiplier(s, p);
  if (strike(s, slot, l, ax.forward, damage, w, gc::kQ1HitPellet) || l.fraction >= 1 || l.sky)
    return;
  const profiles::WeaponRow& row = profiles::kQ1Weapons[0];
  fight_sound(s, slot + 1, l.end, row.land_sound.data());  // the axe on a wall
}

bool quake_missile(Q2Server& s, Q2Missile& m) {
  if (!sq_quake_shot(m.kind)) return false;
  const bool grenade = m.kind == SqShot::q1_grenade;
  if (s.level_time + 0.001 >= m.dies_at) {
    if (grenade)
      burst(s, m, m.origin);
    else
      m.live = false;
    return true;
  }
  if (m.resting) return true;
  if (grenade) m.velocity.z -= static_cast<float>(s.cvars.find("sv_gravity")->number) * s.step;
  const sh::Vec3f to = m.origin + m.velocity * s.step;
  const uint32_t owner = m.owner >= 0 ? 1u << m.owner : 0u;
  const Line l = fight_line(s, m.origin, to, owner);
  const bool met = l.player >= 0 || l.fraction < 1;
  if (!met) {
    m.origin = to;
    return true;
  }
  if (l.player < 0 && l.sky) {
    m.live = false;
    return true;
  }
  const sh::Vec3f dir = unit(m.velocity);
  const bool body = l.player >= 0 || (l.mover >= 0 && monster_index(s, l.mover) >= 0);
  switch (m.kind) {
    case SqShot::q1_spike:
    case SqShot::q1_super_spike:
      strike(s, m.owner, l, dir, m.damage, m.weapon,
             m.kind == SqShot::q1_spike ? gc::kQ1HitSpike : gc::kQ1HitSuperSpike);
      m.live = false;
      return true;
    case SqShot::q1_rocket: {
      if (l.player >= 0)
        fight_hurt(s, l.player, m.owner, dir, m.damage, m.damage, m.weapon);
      else if (l.mover >= 0)
        map_damage(s, l.mover, m.owner, m.damage, dir, false, &l.end);
      burst(s, m, l.end - dir * 8, l.player, l.player >= 0 ? -1 : l.mover);
      return true;
    }
    default:  // the grenade: it bursts on a body it touches, and bounces off anything else
      if (body) {
        burst(s, m, l.end);
        return true;
      }
      m.origin = l.end + l.normal * 0.1f;
      m.velocity = m.velocity - l.normal * (1.5f * sh::dot(m.velocity, l.normal));
      if (l.normal.z > 0.7f && sh::length(m.velocity) < 60) {
        m.velocity = {};
        m.resting = true;
      } else {
        fight_sound(s, fight_missile_entity(s, m), m.origin, gc::kQ1Bounce);
      }
      return true;
  }
}

int quake_view_frame(const Q2Server& s, const Q2Player& p) {
  const int w = p.hand.weapon;
  if (!sq_quake_weapon(w)) return p.hand.frame;
  const gc::Q1ArmId id = arm_of(w);
  if (gc::kQ1ArmFrames[int(id)].loops && s.frame_number - p.q1_last_shot > 1) return 0;
  return gc::q1_view_frame(id, static_cast<int>(s.frame_number - p.q1_run_from));
}

SqQuakeWords sq_quake_words(int w) {
  switch (static_cast<SqWeapon>(w)) {
    case SqWeapon::q1_axe:
      return {"was ax-murdered by"};
    case SqWeapon::q1_shotgun:
      return {"chewed on", "'s boomstick"};
    case SqWeapon::q1_super_shotgun:
      return {"ate 2 loads of", "'s buckshot"};
    case SqWeapon::q1_nailgun:
      return {"was nailed by"};
    case SqWeapon::q1_super_nailgun:
      return {"was punctured by"};
    case SqWeapon::q1_grenade_launcher:
      return {"eats", "'s pineapple"};
    case SqWeapon::q1_rocket_launcher:
      return {"rides", "'s rocket"};
    case SqWeapon::q1_lightning:
      return {"accepts", "'s shaft"};
    default:
      return {};
  }
}

}  // namespace shooters_edge::server
