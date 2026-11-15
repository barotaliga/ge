// The sequel's monsters in the classic campaign (classic remix's `cr_monsters 2`): each kind of
// campaign_sequel.hpp spawned with the roster's health, box, speeds and attacks, the sequel's
// model, frames and sounds, over the campaign's minds (campaign_ai.cpp: sight, the chase, pain
// and death). An attack is the roster's: a wind-up, shots on a loop `every` s apart, another loop
// at its odds while the enemy is in sight, then the recovery; the shots are the campaign's own
// kinds of harm (bolts, pellets, bullets, blows, grenades, rockets, a rail, a drain, a zap, a
// leap). A medic raises the dead it finds while it hunts; a power screen is armour that takes a
// third of each blow until it has taken 100 (from any side: the campaign's harm has no front).
//
// ge:summary The sequel's monsters run by the classic campaign: roster numbers, frames, attacks
// ge:keywords campaign cr_monsters remix sequel-monsters monster-swap attack-loop roster
// ge:keywords monster_soldier monster_gunner monster_tank monster_gladiator
// ge:skill progs shooter
// ge:test tests/shooters_edge_campaign_rules_test.cpp
#include <cstdint>
#include <string_view>

#include "shooters_edge/rules/campaign_decl.hpp"
#include "shooters_edge/rules/campaign_sequel.hpp"
#include "shooters_edge/server/sequel_monster_poses.hpp"

namespace shooters_edge::server::rules::campaign {

namespace {

using roster::Fire;

int run_of(A& q, int32_t e) { return static_cast<int>(q.f(e, f_sq_kind)); }
const roster::Kind& kind_of(int run) {
  return roster::kKinds[sq_roster_of(kSqRuns[run].classname)];
}
int32_t word(A& q, int run, int w) { return q.str(kStringCount + run * kSqWords + w); }
int32_t shared(A& q, int w) { return q.str(kStringCount + kSqRunCount * kSqWords + w); }

// Attack `slot` of a kind: 0 and 1 its blows, 2 to 4 its ranged attacks.
const roster::Attack& attack_of(const roster::Kind& k, int slot) {
  return slot < 2 ? k.melee[slot] : k.missile[slot - 2];
}

Vec3 frames_of(SqSpan s) {
  if (s.none()) return {0, 0, 0};
  return {static_cast<float>(s.first), static_cast<float>(s.last), 1};
}

const SqMonsterPoses* poses_of(int run) {
  for (const SqMonsterPoses& p : kSqMonsterPoses)
    if (std::string_view(p.classname) == kSqRuns[run].classname) return &p;
  return nullptr;
}

Vec3 eye_of(A& q, int32_t e) { return q.v(e, q.L.f.origin) + q.v(e, q.L.f.view_ofs); }

float points(A& q, const roster::Attack& a) {
  return static_cast<float>(a.damage) + q.random() * static_cast<float>(a.damage_max - a.damage);
}

// A loop of shots begins: the first after the wind-up, then one every `every` s until its end.
void begin_loop(A& q, const roster::Attack& a, bool first) {
  const int32_t me = q.self();
  float every = a.every, loop = a.loop;
  if (!first) {
    if (a.again_every > 0) every = a.again_every;
    if (a.again_loop > 0) loop = a.again_loop;
  }
  if (a.fire == Fire::burst && loop == 0) {
    loop = a.burst_min + q.random() * (a.burst_max - a.burst_min);
    every = 0.1f;
  } else if (a.fire == Fire::rockets && every == 0 && a.count > 1) {
    every = 0.2f;
    loop = 0.2f * static_cast<float>(a.count);
  } else if (loop == 0) {
    loop = 0.05f;  // one shot
  }
  const float start = first ? q.time() + a.windup : q.time();
  q.set_f(me, f_sq_next, start);
  q.set_f(me, f_sq_every, every > 0 ? every : loop + 1);
  q.set_f(me, f_sq_end, start + loop);
  q.set_f(me, f_sq_rest, 0);
}

// A missile of the kind's at the enemy, `dmg` on what it strikes.
int32_t missile(A& q, int model, float speed, float dmg, float lob) {
  return monster_missile(q, shared(q, model), speed, dmg, lob);
}

void shoot(A& q, int run, const roster::Attack& a, bool blow) {
  const int32_t me = q.self();
  const int32_t enemy = q.i(me, q.L.f.enemy);
  const Vec3 from = eye_of(q, me);
  const Vec3 to = eye_of(q, enemy);
  q.sound(me, kChanWeapon, word(q, run, blow ? kSqBlow : kSqShot), 1, kAttnNorm);
  switch (a.fire) {
    case Fire::bolt:
    case Fire::sweep:
      for (int n = 0; n < a.count; n++) {
        const int32_t m = missile(q, kSqLaser, a.speed, points(q, a), 0);
        q.set_i(m, f_deathtype, q.s(s_lasered));
      }
      break;
    case Fire::pellets:
    case Fire::burst:
    case Fire::guns:
      aim_along(q, to - from);
      fire_pellets(q, a.fire == Fire::pellets ? static_cast<float>(a.count) : 1, points(q, a),
                   a.h / 8192);
      break;
    case Fire::grenade: {
      const int32_t g = missile(q, kSqGrenade, a.speed, 0, 200);
      q.set_f(g, q.L.f.movetype, kMovetypeBounce);
      q.set_v(g, q.L.f.avelocity, {300, 300, 300});
      q.set_f(g, f_dmg, points(q, a));
      q.set_i(g, f_deathtype, q.s(s_blown_up_by_a_grenade));
      q.set_i(g, q.L.f.touch, q.fn(fn_sq_grenade_touch));
      q.set_i(g, q.L.f.think, q.fn(fn_sq_blast));
      q.set_f(g, q.L.f.nextthink, q.time() + 2.5f);
      break;
    }
    case Fire::rockets:
    case Fire::bfg: {
      const int32_t r = missile(q, kSqRocket, a.speed > 0 ? a.speed : 500, 0, 0);
      q.set_f(r, f_dmg, points(q, a));
      q.set_i(r, f_deathtype, q.s(s_blown_apart));
      q.set_i(r, q.L.f.touch, q.fn(fn_sq_rocket_touch));
      break;
    }
    case Fire::rail: {
      aim_along(q, to - from);
      q.traceline(from, from + q.v_forward() * 8192, 0, me);
      const Vec3 end = q.gv(q.L.g.trace_endpos);
      effect_beam(q, from, end);
      const int32_t hit = q.gi(q.L.g.trace_ent);
      if ((hit != 0) & on(q.f(hit, q.L.f.takedamage))) {
        q.set_i(hit, f_deathtype, q.s(s_shot));
        damage(q, hit, me, me, points(q, a));
      }
      break;
    }
    case Fire::drain:
    case Fire::zap:
      if ((a.fire == Fire::drain) & (q.vlen(to - from) > 256)) break;
      if (!can_see(q, enemy)) break;
      if (a.fire == Fire::drain) effect_beam(q, from, to);
      q.set_i(enemy, f_deathtype, q.s(s_mauled));
      damage(q, enemy, me, me, points(q, a));
      break;
    case Fire::club: {
      const Vec3 d = q.v(enemy, q.L.f.origin) - q.v(me, q.L.f.origin);
      if (q.vlen(d) > kind_of(run).maxs.x + 48) break;
      if (!can_see(q, enemy)) break;
      q.set_i(enemy, f_deathtype, q.s(s_mauled));
      damage(q, enemy, me, me, points(q, a));
      if (a.kick != 0)
        q.set_v(enemy, q.L.f.velocity,
                q.v(enemy, q.L.f.velocity) + q.normalize(d) * static_cast<float>(a.kick));
      break;
    }
    case Fire::leap:
      q.set_f(me, f_dmg, points(q, a));
      leap_off(q, a.speed, 250, fn_sq_leap_touch);
      break;
    case Fire::none:
      break;
  }
}

void start_attack(A& q, int slot) {
  const int32_t me = q.self();
  const int run = run_of(q, me);
  q.set_f(me, f_sq_slot, static_cast<float>(slot));
  q.set_f(me, f_sq_loops, 0);
  Vec3 frames = q.v(me, f_fr_run);
  if (const SqMonsterPoses* p = poses_of(run)) {
    const bool two = !p->attack[slot][1].none() && q.random() < 0.5f;
    if (!p->attack[slot][two ? 1 : 0].none()) frames = frames_of(p->attack[slot][two ? 1 : 0]);
  }
  anim_begin(q, frames);
  begin_loop(q, attack_of(kind_of(run), slot), true);
  sq_attack_step(q);
}

// The kind's spawn: its numbers, model, frames and sounds, over the campaign's minds.
void spawn(A& q, int run) {
  if (no_monsters_here(q)) return;
  const roster::Kind& k = kind_of(run);
  for (int w = kSqSight; w < kSqWords; w++) q.precache_sound(word(q, run, w));
  for (int w = kSqLaser; w <= kSqHead; w++) q.precache_model(shared(q, w));
  q.precache_model(word(q, run, kSqModel));
  const int32_t me = q.self();
  q.set_f(me, f_sq_kind, static_cast<float>(run));
  q.set_i(me, q.L.f.noise, word(q, run, kSqSight));
  q.set_i(me, q.L.f.noise1, word(q, run, kSqDeath));
  q.set_i(me, f_head_model, shared(q, kSqHead));
  q.set_f(me, f_gib_health, k.gib == roster::kNoGib ? -100000 : static_cast<float>(k.gib));
  q.set_f(me, f_walk_step, k.walk * 0.1f);
  q.set_f(me, f_run_step, k.run * 0.1f);
  q.set_f(me, q.L.f.skin, static_cast<float>(k.skin));
  if (k.medic) q.set_i(me, f_th_check, q.fn(fn_sq_medic_check));
  if (k.screen) {
    q.set_f(me, q.L.f.armortype, 1.0f / 3.0f);
    q.set_f(me, q.L.f.armorvalue, 100);
  }
  if (k.melee[0].fire != Fire::none) q.set_i(me, f_th_melee, q.fn(fn_sq_melee));
  if (k.missile[0].fire != Fire::none) q.set_i(me, f_th_missile, q.fn(fn_sq_missile));
  q.set_i(me, f_th_pain, q.fn(fn_sq_pain));
  q.set_i(me, f_th_die, q.fn(fn_monster_die));
  if (const SqMonsterPoses* p = poses_of(run)) {
    q.set_v(me, f_fr_stand, frames_of(p->stand));
    q.set_v(me, f_fr_walk, frames_of(p->walk.none() ? p->run : p->walk));
    q.set_v(me, f_fr_run, frames_of(p->run.none() ? p->walk : p->run));
    q.set_v(me, f_fr_pain, frames_of(p->pain[0]));
    q.set_v(me, f_fr_death, frames_of(p->death[0]));
    q.set_v(me, f_fr_death2, frames_of(p->death[1]));
  }
  q.set_f(me, f_death_solid, 2);
  const Vec3 lo{k.mins.x, k.mins.y, k.mins.z}, hi{k.maxs.x, k.maxs.y, k.maxs.z};
  const auto hp = static_cast<float>(k.health);
  if (k.flies)
    flymonster_start(q, word(q, run, kSqModel), hp, lo, hi);
  else if (k.swims)
    swimmonster_start(q, word(q, run, kSqModel), hp, lo, hi);
  else
    walkmonster_start(q, word(q, run, kSqModel), hp, lo, hi);
}

// A body a medic may raise: a monster's corpse lying still (its fall over), not torn apart.
bool raisable(A& q, int32_t e) {
  return (q.f(e, q.L.f.deadflag) == kDeadDead) & (q.f(e, q.L.f.movetype) == kMovetypeStep) &
         (q.i(e, f_th_run) != 0) & (q.f(e, q.L.f.frame) >= q.v(e, f_fr_now).y);
}

// The dead monster `c` up again: its health whole, solid, counted, and after the medic's enemy.
void raise(A& q, int32_t c) {
  const int32_t me = q.self();
  q.set_f(c, q.L.f.health, q.f(c, q.L.f.max_health));
  q.set_f(c, q.L.f.deadflag, 0);
  q.set_f(c, q.L.f.takedamage, kDamageAim);
  q.set_f(c, q.L.f.solid, kSolidSlidebox);
  q.set_f(c, q.L.f.flags, bor(q.f(c, q.L.f.flags), kFlMonster));
  q.set_f(c, q.L.f.frame, q.v(c, f_fr_stand).x);
  q.setorigin(c, q.v(c, q.L.f.origin));
  q.add_to(q.L.g.total_monsters, 1);
  q.set_i(c, q.L.f.enemy, q.i(me, q.L.f.enemy));
  q.set_i(c, q.L.f.think, q.fn(fn_monster_run));
  q.set_f(c, q.L.f.nextthink, q.time() + 0.1f);
}

}  // namespace

// A medic's choice while it hunts: a corpse it can see within 400 units first (raised within 96,
// walked to before that), else the usual attack.
void sq_medic_check(A& q) {
  const int32_t me = q.self();
  const Vec3 at = q.v(me, q.L.f.origin);
  int32_t best = 0;
  float nearest = 400;
  for (int32_t e = q.nextent(q.world()); e != 0; e = q.nextent(e)) {  // corpses are not solid
    if ((e == me) || !raisable(q, e)) continue;
    const float d = q.vlen(q.v(e, q.L.f.origin) - at);
    if (d >= nearest) continue;
    q.traceline(eye_of(q, me), q.v(e, q.L.f.origin), 1, me);
    if (q.gf(q.L.g.trace_fraction) < 1) continue;
    best = e;
    nearest = d;
  }
  if (best == 0) {
    q.ret_f(check_attack(q) ? 1.0f : 0.0f);
    return;
  }
  const float yaw = q.vectoyaw(q.v(best, q.L.f.origin) - at);
  q.set_f(me, q.L.f.ideal_yaw, yaw);
  q.changeyaw();
  if (nearest > 96) {
    q.walkmove(yaw, q.f(me, f_run_step));
  } else {
    q.sound(me, kChanWeapon, word(q, run_of(q, me), kSqBlow), 1, kAttnNorm);
    effect_beam(q, eye_of(q, me), q.v(best, q.L.f.origin));
    raise(q, best);
  }
  q.ret_f(1);
}

// A blow: the second at even odds where the kind has two.
void sq_melee(A& q) {
  const roster::Kind& k = kind_of(run_of(q, q.self()));
  start_attack(q, (k.melee[1].fire != Fire::none) & (q.random() < 0.5f) ? 1 : 0);
}

// A ranged attack: the first always nearer than the kind's `close`, else one by the weights.
void sq_missile(A& q) {
  const int32_t me = q.self();
  const roster::Kind& k = kind_of(run_of(q, me));
  int pick = 0;
  if (!(k.close > 0 && enemy_distance(q) < k.close)) {
    float total = 0;
    for (const roster::Attack& a : k.missile)
      if (a.fire != Fire::none) total += a.weight;
    float r = q.random() * total;
    for (int i = 0; i < 3; i++) {
      if (k.missile[i].fire == Fire::none) break;
      pick = i;
      r -= k.missile[i].weight;
      if (r < 0) break;
    }
  }
  start_attack(q, 2 + pick);
}

// A step of an attack: the shots that fall in it, then another loop or the recovery.
void sq_attack_step(A& q) {
  think_again(q, fn_sq_attack_step);
  const int32_t me = q.self();
  const int run = run_of(q, me);
  const int slot = static_cast<int>(q.f(me, f_sq_slot));
  const roster::Attack& a = attack_of(kind_of(run), slot);
  frame_loop(q, q.v(me, f_fr_now));
  const float rest = q.f(me, f_sq_rest);
  if (rest > 0) {
    if (q.time() >= rest) monster_run(q);
    return;
  }
  if (a.fire != Fire::leap) face_enemy(q);
  if (slot < 2) ai_charge(q, q.f(me, f_run_step) * 0.3f);
  const int32_t enemy = q.i(me, q.L.f.enemy);
  while ((q.time() + 0.001f >= q.f(me, f_sq_next)) & (q.f(me, f_sq_next) < q.f(me, f_sq_end)) &
         (q.f(enemy, q.L.f.health) > 0)) {
    shoot(q, run, a, slot < 2);
    q.set_f(me, f_sq_next, q.f(me, f_sq_next) + q.f(me, f_sq_every));
  }
  if (q.time() + 0.001f < q.f(me, f_sq_end)) return;
  const float loops = q.f(me, f_sq_loops);
  const bool more = (a.again > 0) & (q.f(enemy, q.L.f.health) > 0) && can_see(q, enemy) &&
                    (a.again_max == 0 || loops < static_cast<float>(a.again_max)) &&
                    q.random() < a.again;
  if (more) {
    q.set_f(me, f_sq_loops, loops + 1);
    begin_loop(q, a, false);
    return;
  }
  q.set_f(me, f_sq_rest, q.time() + (a.recover > 0.1f ? a.recover : 0.1f));
}

// Pain: its cry and its flinch, no more than every 3 s.
void sq_pain(A& q) {
  const int32_t me = q.self();
  hold_pain(q, 3);
  q.sound(me, kChanVoice, word(q, run_of(q, me), kSqPain), 1, kAttnNorm);
  anim_begin(q, q.v(me, f_fr_pain));
  sq_flinch(q);
}

void sq_flinch(A& q) {
  const Vec3 f = q.v(q.self(), f_fr_pain);
  const float steps = f.z == 0 ? 3 : f.y - f.x + 1;
  flinch(q, fn_sq_flinch, steps < 8 ? steps : 8);
}

// A grenade's or a rocket's blast: its points less half the distance to everything near.
void sq_blast(A& q) {
  const int32_t me = q.self();
  radius_damage(q, me, q.i(me, q.L.f.owner), q.f(me, f_dmg), q.world(), q.i(me, f_deathtype));
  q.sound(me, kChanVoice, q.s(s_weapons_r_exp3_wav), 1, kAttnNorm);
  effect_explosion(q, q.v(me, q.L.f.origin));
  q.remove(me);
}

void sq_grenade_touch(A& q) {
  if (q.other() == q.i(q.self(), q.L.f.owner)) return;
  if (q.f(q.other(), q.L.f.takedamage) == kDamageAim) {
    sq_blast(q);
    return;
  }
  q.sound(q.self(), kChanVoice, q.s(s_weapons_bounce_wav), 1, kAttnNorm);
}

void sq_rocket_touch(A& q) {
  if (q.other() == q.i(q.self(), q.L.f.owner)) return;
  sq_blast(q);
}

// A leap strikes what it lands on at speed, once.
void sq_leap_touch(A& q) {
  const int32_t me = q.self();
  const int32_t other = q.other();
  if (on(q.f(other, q.L.f.takedamage)) && q.vlen(q.v(me, q.L.f.velocity)) > 400) {
    q.set_i(other, f_deathtype, q.s(s_mauled));
    damage(q, other, me, me, q.f(me, f_dmg));
  }
  q.set_i(me, q.L.f.touch, 0);
}

#define SE_SQ_SPAWN(name, i)                                      \
  static_assert(std::string_view(kSqRuns[i].classname) == #name); \
  void name(A& q) { spawn(q, i); }
SE_SQ_SPAWN(monster_soldier_light, 0)
SE_SQ_SPAWN(monster_soldier, 1)
SE_SQ_SPAWN(monster_soldier_ss, 2)
SE_SQ_SPAWN(monster_infantry, 3)
SE_SQ_SPAWN(monster_gunner, 4)
SE_SQ_SPAWN(monster_berserk, 5)
SE_SQ_SPAWN(monster_gladiator, 6)
SE_SQ_SPAWN(monster_flyer, 7)
SE_SQ_SPAWN(monster_parasite, 8)
SE_SQ_SPAWN(monster_mutant, 9)
SE_SQ_SPAWN(monster_chick, 10)
SE_SQ_SPAWN(monster_tank, 11)
SE_SQ_SPAWN(monster_flipper, 12)
SE_SQ_SPAWN(monster_floater, 13)
SE_SQ_SPAWN(monster_medic, 14)
SE_SQ_SPAWN(monster_brain, 15)
SE_SQ_SPAWN(monster_hover, 16)
SE_SQ_SPAWN(monster_tank_commander, 17)
#undef SE_SQ_SPAWN

}  // namespace shooters_edge::server::rules::campaign
