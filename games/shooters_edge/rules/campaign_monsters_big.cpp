// The roster, the bigger kinds: the leaper, the giant and the caster in the large hull, then the
// trooper, the spawn and the heavy swordsman, each numbers and attacks over the minds in
// campaign_ai.cpp.
//
// ge:summary The campaign's bigger monsters in C++: leaper, giant, caster, trooper, spawn, knight
// ge:keywords campaign native-campaign monster_demon1 monster_shambler monster_shalrath
// ge:keywords monster_enforcer monster_tarbaby monster_hell_knight roster
// ge:skill progs shooter
// ge:test tests/shooters_edge_campaign_rules_test.cpp
#include <cstdint>

#include "shooters_edge/rules/campaign_decl.hpp"

namespace shooters_edge::server::rules::campaign {

namespace {

void precache_sounds(A& q, std::initializer_list<St> list) {
  for (St s : list) q.precache_sound(q.s(s));
}

bool on_ground(A& q) { return band(q.f(q.self(), q.L.f.flags), kFlOnground) != 0; }

int32_t enemy(A& q) { return q.i(q.self(), q.L.f.enemy); }

}  // namespace

// THE LEAPER (monster_demon1): 300 health, claws, and a leap that strikes on contact. Two claws as
// it closes in, 10-15 each.
void demon_claw(A& q) {
  think_again(q, fn_demon_claw);
  anim_tick(q);
  ai_charge(q, 8);
  if ((anim_step(q) == 3) | (anim_step(q) == 6)) {
    const bool near = enemy_distance(q) <= 80;
    const bool seen = can_see(q, enemy(q));  // asked either way
    if (near & seen) {
      q.sound(q.self(), kChanWeapon, q.s(s_demon_dhit2_wav), 1, kAttnNorm);
      q.set_i(enemy(q), f_deathtype, q.s(s_eviscerated));
      damage(q, enemy(q), q.self(), q.self(), 10 + 5 * q.random());
    }
  }
  if (anim_step(q) >= 8) monster_run(q);
}

void demon_melee(A& q) {
  anim_begin(q, q.v(q.self(), f_fr_melee));
  demon_claw(q);
}

// In the air: the first thing that takes damage it meets at speed takes 40-50.
void demon_leap_touch(A& q) {
  const int32_t me = q.self();
  const int32_t other = q.other();
  const bool hurts = q.f(other, q.L.f.takedamage) != 0;
  const bool fast = q.vlen(q.v(me, q.L.f.velocity)) > 400;
  if (hurts & fast) {
    q.set_i(other, f_deathtype, q.s(s_eviscerated));
    damage(q, other, me, me, 40 + 10 * q.random());
    q.set_i(q.self(), q.L.f.touch, q.fn(fn_noop));
  }
}

void demon_leap_step(A& q) {
  think_again(q, fn_demon_leap_step);
  anim_tick(q);
  if (((anim_step(q) > 2) & on_ground(q)) | (anim_step(q) > 30)) {
    if (on_ground(q)) q.sound(q.self(), kChanBody, q.s(s_demon_dland2_wav), 1, kAttnNorm);
    q.set_i(q.self(), q.L.f.touch, q.fn(fn_noop));
    monster_run(q);
  }
}

namespace {

// 600 forward, 250 up.
void demon_leap(A& q) {
  leap_off(q, 600, 250, fn_demon_leap_touch);
  q.sound(q.self(), kChanVoice, q.s(s_demon_djump_wav), 1, kAttnNorm);
  anim_begin(q, q.v(q.self(), f_fr_leap));
  demon_leap_step(q);
}

// The leaper's own decision: the claws in melee range; the leap from 100 to 350 units off, the
// enemy not far above or below, at most every 2 s.
bool demon_decides(A& q) {
  const int32_t me = q.self();
  const bool melee = range_to(q, enemy(q)) == kRangeMelee;
  const bool seen = can_see(q, enemy(q));  // asked either way
  if (melee & seen) {
    q.set_f(me, f_attack_state, kAsMelee);
    return true;
  }
  const Vec3 delta = enemy_origin(q) - q.v(me, q.L.f.origin);
  if (q.fabs(delta.z) > 64) return false;
  const float d = q.vlen(delta);
  if ((d < 100) | (d > 350)) return false;
  if (q.time() < q.f(me, f_attack_finished)) return false;
  if (!can_see(q, enemy(q))) return false;
  q.set_f(me, f_attack_finished, q.time() + 2);
  demon_leap(q);
  return true;
}

}  // namespace

void demon_check(A& q) { pg::set_return_float(q.vm, demon_decides(q) ? 1 : 0); }

void demon_flinch(A& q) { flinch(q, fn_demon_flinch, 3); }

void demon_pain(A& q) {
  if (q.i(q.self(), q.L.f.touch) == q.fn(fn_demon_leap_touch)) return;  // no flinch in the air
  hold_pain(q, 1);
  q.sound(q.self(), kChanVoice, q.s(s_demon_dpain1_wav), 1, kAttnNorm);
  anim_begin(q, q.v(q.self(), f_fr_pain));
  demon_flinch(q);
}

void monster_demon1(A& q) {
  if (no_monsters_here(q)) return;
  q.precache_model(q.s(s_demon_mdl));
  q.precache_model(q.s(s_h_demon_mdl));
  precache_sounds(q, {s_demon_ddeath_wav, s_demon_dhit2_wav, s_demon_djump_wav,
                      s_demon_dpain1_wav, s_demon_idle1_wav, s_demon_sight2_wav,
                      s_demon_dland2_wav});
  monster_kind(q, s_demon_sight2_wav, s_demon_ddeath_wav, s_h_demon_mdl, -80, 8, 20);
  const int32_t me = q.self();
  q.set_i(me, f_th_melee, q.fn(fn_demon_melee));
  q.set_i(me, f_th_check, q.fn(fn_demon_check));
  q.set_i(me, f_th_pain, q.fn(fn_demon_pain));
  q.set_i(me, f_th_die, q.fn(fn_monster_die));
  set_frames(q, f_fr_stand, 0, 12);
  set_frames(q, f_fr_walk, 13, 20);
  set_frames(q, f_fr_run, 21, 26);
  set_frames(q, f_fr_melee, 54, 68);
  set_frames(q, f_fr_leap, 27, 38);
  set_frames(q, f_fr_pain, 39, 44);
  set_frames(q, f_fr_death, 45, 53);
  q.set_f(me, f_death_solid, 5);
  q.set_i(me, f_gib_model, q.s(s_gib1_mdl));
  walkmonster_start(q, q.s(s_demon_mdl), 300, {-32, -32, -24}, {32, 32, 64});
}

// THE GIANT (monster_shambler): 600 health, claws, lightning; blasts do it half. A swipe (3
// randoms x 20) or, one time in three, the two-handed smash (x 40), closing in.
void shambler_claw(A& q) {
  think_again(q, fn_shambler_claw);
  anim_tick(q);
  ai_charge(q, 6);
  if (anim_step(q) == 1) {
    if (q.random() < 0.33f) {
      q.set_f(q.self(), f_claw_factor, 40);  // the smash
      q.sound(q.self(), kChanWeapon, q.s(s_shambler_melee2_wav), 1, kAttnNorm);
    } else {
      q.set_f(q.self(), f_claw_factor, 20);
      q.sound(q.self(), kChanWeapon, q.s(s_shambler_melee1_wav), 1, kAttnNorm);
    }
  }
  if (anim_step(q) == 5) {
    const bool near = enemy_distance(q) <= 100;
    const bool seen = can_see(q, enemy(q));  // asked either way
    if (near & seen) q.sound(q.self(), kChanVoice, q.s(s_shambler_smack_wav), 1, kAttnNorm);
    melee_blow_n(q, 3, q.f(q.self(), f_claw_factor));
  }
  if (anim_step(q) >= 9) monster_run(q);
}

void shambler_melee(A& q) {
  anim_begin(q, q.v(q.self(), f_fr_melee));
  shambler_claw(q);
}

// The bolt: three strokes along the line to the enemy's eye, 10 each on what they reach.
void shambler_bolt(A& q) {
  think_again(q, fn_shambler_bolt);
  anim_tick(q);
  face_enemy(q);
  const int32_t me = q.self();
  if (anim_step(q) == 1) q.sound(me, kChanWeapon, q.s(s_shambler_sattck1_wav), 1, kAttnNorm);
  if ((anim_step(q) >= 4) & (anim_step(q) <= 6)) {
    const Vec3 from = q.v(me, q.L.f.origin) + Vec3{0, 0, 40};
    const Vec3 dir =
        q.normalize(enemy_origin(q) + q.v(enemy(q), q.L.f.view_ofs) - from);
    q.traceline(from, from + dir * 600, 1, me);
    effect_beam(q, from, q.gv(q.L.g.trace_endpos));
    q.traceline(from, from + dir * 600, 0, me);
    if (on(q.f(q.gi(q.L.g.trace_ent), q.L.f.takedamage))) {
      q.set_i(q.gi(q.L.g.trace_ent), f_deathtype, q.s(s_struck_by_lightning));
      damage(q, q.gi(q.L.g.trace_ent), me, me, 10);
    }
  }
  if (anim_step(q) >= 9) monster_run(q);
}

void shambler_missile(A& q) {
  anim_begin(q, q.v(q.self(), f_fr_missile));
  shambler_bolt(q);
}

void shambler_flinch(A& q) { flinch(q, fn_shambler_flinch, 4); }

// Only a hit that takes a good share of it makes it flinch, and at most every 2 s.
void shambler_pain(A& q) {
  const bool strong = q.f(q.self(), q.L.f.health) > 400;
  const bool shrug = q.random() < 0.5f;  // drawn either way
  if (strong & shrug) return;
  hold_pain(q, 2);
  q.sound(q.self(), kChanVoice, q.s(s_shambler_shurt2_wav), 1, kAttnNorm);
  anim_begin(q, q.v(q.self(), f_fr_pain));
  shambler_flinch(q);
}

void monster_shambler(A& q) {
  if (no_monsters_here(q)) return;
  q.precache_model(q.s(s_shambler_mdl));
  q.precache_model(q.s(s_h_shams_mdl));
  precache_sounds(q, {s_shambler_sattck1_wav, s_shambler_sboom_wav, s_shambler_sdeath_wav,
                      s_shambler_shurt2_wav, s_shambler_sidle_wav, s_shambler_ssight_wav,
                      s_shambler_melee1_wav, s_shambler_melee2_wav, s_shambler_smack_wav});
  monster_kind(q, s_shambler_ssight_wav, s_shambler_sdeath_wav, s_h_shams_mdl, -60, 8, 20);
  const int32_t me = q.self();
  q.set_f(me, f_blast_share, 0.5f);
  q.set_i(me, f_th_melee, q.fn(fn_shambler_melee));
  q.set_i(me, f_th_missile, q.fn(fn_shambler_missile));
  q.set_i(me, f_th_pain, q.fn(fn_shambler_pain));
  q.set_i(me, f_th_die, q.fn(fn_monster_die));
  set_frames(q, f_fr_stand, 0, 16);
  set_frames(q, f_fr_walk, 17, 28);
  set_frames(q, f_fr_run, 29, 34);
  set_frames(q, f_fr_melee, 35, 46);
  set_frames(q, f_fr_missile, 65, 76);
  set_frames(q, f_fr_pain, 77, 82);
  set_frames(q, f_fr_death, 83, 93);
  q.set_f(me, f_death_solid, 2);
  walkmonster_start(q, q.s(s_shambler_mdl), 600, {-32, -32, -24}, {32, 32, 64});
}

// THE CASTER (monster_shalrath): 400 health, a homing ball. The ball turns toward its target
// every 0.2 s; it hits for 110, with a blast of 40 round it.
void caster_ball_touch(A& q) {
  const int32_t me = q.self();
  const int32_t other = q.other();
  if (other == q.i(me, q.L.f.owner)) return;
  if (on(q.f(other, q.L.f.takedamage))) {
    q.set_i(other, f_deathtype, q.s(s_exploded_by_a_homing_ball));
    damage(q, other, me, q.i(me, q.L.f.owner), 110);
  }
  radius_damage(q, q.self(), q.i(q.self(), q.L.f.owner), 40, q.other(),
                q.s(s_caught_by_a_homing));
  q.sound(q.self(), kChanVoice, q.s(s_weapons_r_exp3_wav), 1, kAttnNorm);
  effect_explosion(q, q.v(q.self(), q.L.f.origin));
  q.remove(q.self());
}

void caster_ball_home(A& q) {
  const int32_t me = q.self();
  if ((q.time() > q.f(me, f_attack_finished)) | (q.f(enemy(q), q.L.f.health) <= 0)) {
    q.remove(me);
    return;
  }
  q.set_v(me, q.L.f.velocity,
          q.normalize(enemy_origin(q) + Vec3{0, 0, 10} - q.v(me, q.L.f.origin)) * 400);
  q.set_v(me, q.L.f.angles, q.vectoangles(q.v(me, q.L.f.velocity)));
  q.set_f(me, q.L.f.nextthink, q.time() + 0.2f);
}

void caster_cast(A& q) {
  think_again(q, fn_caster_cast);
  anim_tick(q);
  face_enemy(q);
  if (anim_step(q) == 1) q.sound(q.self(), kChanVoice, q.s(s_shalrath_attack_wav), 1, kAttnNorm);
  if (anim_step(q) == 6) {
    q.sound(q.self(), kChanWeapon, q.s(s_shalrath_attack2_wav), 1, kAttnNorm);
    const int32_t m = monster_missile(q, q.s(s_v_spike_mdl), 400, 110, 0);
    q.set_i(m, q.L.f.enemy, enemy(q));
    q.set_i(m, q.L.f.touch, q.fn(fn_caster_ball_touch));
    q.set_i(m, q.L.f.think, q.fn(fn_caster_ball_home));
    q.set_f(m, q.L.f.nextthink, q.time() + 0.1f);
    q.set_f(m, f_attack_finished, q.time() + 20);
  }
  if (anim_step(q) >= 9) monster_run(q);
}

void caster_missile(A& q) {
  anim_begin(q, q.v(q.self(), f_fr_missile));
  caster_cast(q);
}

void caster_flinch(A& q) { flinch(q, fn_caster_flinch, 4); }

void caster_pain(A& q) {
  hold_pain(q, 3);
  q.sound(q.self(), kChanVoice, q.s(s_shalrath_pain_wav), 1, kAttnNorm);
  anim_begin(q, q.v(q.self(), f_fr_pain));
  caster_flinch(q);
}

void monster_shalrath(A& q) {
  if (no_monsters_here(q)) return;
  q.precache_model(q.s(s_shalrath_mdl));
  q.precache_model(q.s(s_h_shal_mdl));
  q.precache_model(q.s(s_v_spike_mdl));
  precache_sounds(q, {s_shalrath_attack_wav, s_shalrath_attack2_wav, s_shalrath_death_wav,
                      s_shalrath_idle_wav, s_shalrath_pain_wav, s_shalrath_sight_wav,
                      s_weapons_r_exp3_wav});
  monster_kind(q, s_shalrath_sight_wav, s_shalrath_death_wav, s_h_shal_mdl, -90, 6, 6);
  const int32_t me = q.self();
  q.set_i(me, f_th_missile, q.fn(fn_caster_missile));
  q.set_i(me, f_th_pain, q.fn(fn_caster_pain));
  q.set_i(me, f_th_die, q.fn(fn_monster_die));
  set_frames(q, f_fr_stand, 23, 34);
  set_frames(q, f_fr_walk, 23, 34);
  set_frames(q, f_fr_run, 23, 34);
  set_frames(q, f_fr_missile, 0, 10);
  set_frames(q, f_fr_pain, 11, 15);
  set_frames(q, f_fr_death, 16, 22);
  walkmonster_start(q, q.s(s_shalrath_mdl), 400, {-32, -32, -24}, {32, 32, 64});
}

// THE TROOPER (monster_enforcer): 80 health, laser bolts. Aiming, two bolts (600, 15 each) a step
// apart, recovering.
void enforcer_fire(A& q) {
  think_again(q, fn_enforcer_fire);
  const float e = q.f(q.self(), q.L.f.effects);
  q.set_f(q.self(), q.L.f.effects, e - band(e, kEfMuzzleflash));
  anim_tick(q);
  face_enemy(q);
  if ((anim_step(q) == 4) | (anim_step(q) == 6)) {
    q.sound(q.self(), kChanWeapon, q.s(s_enforcer_enfire_wav), 1, kAttnNorm);
    q.set_f(q.self(), q.L.f.effects, bor(q.f(q.self(), q.L.f.effects), kEfMuzzleflash));
    const int32_t m = monster_missile(q, q.s(s_laser_mdl), 600, 15, 0);
    q.set_i(m, f_deathtype, q.s(s_lasered));
  }
  if (anim_step(q) >= 9) monster_run(q);
}

void enforcer_missile(A& q) {
  anim_begin(q, q.v(q.self(), f_fr_missile));
  enforcer_fire(q);
}

void enforcer_flinch(A& q) { flinch(q, fn_enforcer_flinch, 3); }

void enforcer_pain(A& q) {
  hold_pain(q, 1);
  if (q.random() < 0.5f) q.sound(q.self(), kChanVoice, q.s(s_enforcer_pain1_wav), 1, kAttnNorm);
  else q.sound(q.self(), kChanVoice, q.s(s_enforcer_pain2_wav), 1, kAttnNorm);
  anim_begin(q, q.v(q.self(), f_fr_pain));
  enforcer_flinch(q);
}

void monster_enforcer(A& q) {
  if (no_monsters_here(q)) return;
  q.precache_model(q.s(s_enforcer_mdl));
  q.precache_model(q.s(s_h_mega_mdl));
  q.precache_model(q.s(s_laser_mdl));
  precache_sounds(q, {s_enforcer_death1_wav, s_enforcer_enfire_wav, s_enforcer_enfstop_wav,
                      s_enforcer_idle1_wav, s_enforcer_pain1_wav, s_enforcer_pain2_wav,
                      s_enforcer_sight1_wav, s_enforcer_sight2_wav, s_enforcer_sight3_wav,
                      s_enforcer_sight4_wav});
  const float r = q.random();  // one of four sight calls
  St sight = s_enforcer_sight4_wav;
  if (r < 0.25f) sight = s_enforcer_sight1_wav;
  else if (r < 0.5f) sight = s_enforcer_sight2_wav;
  else if (r < 0.75f) sight = s_enforcer_sight3_wav;
  monster_kind(q, sight, s_enforcer_death1_wav, s_h_mega_mdl, -35, 2, 14);
  const int32_t me = q.self();
  q.set_i(me, f_th_missile, q.fn(fn_enforcer_missile));
  q.set_i(me, f_th_pain, q.fn(fn_enforcer_pain));
  q.set_i(me, f_th_die, q.fn(fn_monster_die));
  set_frames(q, f_fr_stand, 0, 6);
  set_frames(q, f_fr_walk, 7, 22);
  set_frames(q, f_fr_run, 23, 30);
  set_frames(q, f_fr_missile, 31, 40);
  set_frames(q, f_fr_pain, 66, 69);
  set_frames(q, f_fr_death, 41, 54);
  set_frames(q, f_fr_death2, 55, 65);
  q.set_f(me, f_death_solid, 2);
  q.set_f(me, f_drop_ammo, kDropCells);
  walkmonster_start(q, q.s(s_enforcer_mdl), 80, {-16, -16, -24}, {16, 16, 40});
}

// THE SPAWN (monster_tarbaby): 80 health, bouncing leaps; it bursts when it dies. Each landing
// near its enemy springs it again: 600 at the enemy, 200 up; what it meets at speed takes 10-20.
void tarbaby_touch(A& q) {
  const int32_t me = q.self();
  const int32_t other = q.other();
  const bool hurts = q.f(other, q.L.f.takedamage) != 0;
  const bool not_owner = other != q.i(me, q.L.f.owner);
  const bool fast = q.vlen(q.v(me, q.L.f.velocity)) > 400;
  if (hurts & not_owner & fast) {
    q.set_i(other, f_deathtype, q.s(s_slimed));
    damage(q, other, me, me, 10 + 10 * q.random());
    q.sound(q.self(), kChanWeapon, q.s(s_blob_hit1_wav), 1, kAttnNorm);
  } else if (q.f(other, q.L.f.solid) == kSolidBsp) {
    q.sound(me, kChanWeapon, q.s(s_blob_land1_wav), 1, kAttnNorm);
  }
}

namespace {

void tarbaby_jump(A& q);

}  // namespace

void tarbaby_bounce(A& q) {
  think_again(q, fn_tarbaby_bounce);
  anim_tick(q);
  if (((anim_step(q) > 2) & on_ground(q)) | (anim_step(q) > 30)) {
    const bool alive = q.f(enemy(q), q.L.f.health) > 0;
    const bool seen = can_see(q, enemy(q));                   // asked either way
    const bool near = range_to(q, enemy(q)) <= kRangeNear;  // and this
    if (alive & seen & near) {
      tarbaby_jump(q);
      return;
    }
    q.set_i(q.self(), q.L.f.touch, q.fn(fn_noop));
    monster_run(q);
  }
}

namespace {

void tarbaby_jump(A& q) {
  leap_off(q, 600, 200, fn_tarbaby_touch);
  anim_begin(q, q.v(q.self(), f_fr_leap));
  tarbaby_bounce(q);
}

}  // namespace

// The spawn's own decision: a jump whenever its enemy is near and seen.
void tarbaby_check(A& q) {
  bool yes = false;
  if (range_to(q, enemy(q)) <= kRangeNear && can_see(q, enemy(q))) {
    tarbaby_jump(q);
    yes = true;
  }
  pg::set_return_float(q.vm, yes ? 1 : 0);
}

// It bursts: 120 less half the distance to everything near.
void tarbaby_die(A& q) {
  q.set_f(q.self(), q.L.f.health, 0);  // never gibbed: it bursts instead
  monster_die(q);
  q.set_i(q.self(), q.L.f.touch, q.fn(fn_noop));
  q.sound(q.self(), kChanVoice, q.s(s_blob_death1_wav), 1, kAttnNorm);
  radius_damage(q, q.self(), q.self(), 120, q.world(), q.s(s_caught_by_a_spawn_s_burst));
  effect_explosion(q, q.v(q.self(), q.L.f.origin));
  q.remove(q.self());
}

void monster_tarbaby(A& q) {
  if (no_monsters_here(q)) return;
  q.precache_model(q.s(s_tarbaby_mdl));
  precache_sounds(q, {s_blob_death1_wav, s_blob_hit1_wav, s_blob_land1_wav, s_blob_sight1_wav});
  monster_kind(q, s_blob_sight1_wav, s_empty, s_gib1_mdl, -1000, 2, 2);
  const int32_t me = q.self();
  q.set_i(me, f_th_check, q.fn(fn_tarbaby_check));
  q.set_i(me, f_th_die, q.fn(fn_tarbaby_die));
  set_frames(q, f_fr_stand, 0, 24);
  set_frames(q, f_fr_walk, 0, 24);
  set_frames(q, f_fr_run, 25, 49);
  set_frames(q, f_fr_leap, 50, 59);
  walkmonster_start(q, q.s(s_tarbaby_mdl), 80, {-16, -16, -24}, {16, 16, 40});
}

// THE HEAVY SWORDSMAN (monster_hell_knight): 250 health, a sword, a charge, magic bolts. Six steps
// of the sword, closing in, landing on the third and the fifth.
void hknight_swing(A& q) {
  think_again(q, fn_hknight_swing);
  anim_tick(q);
  ai_charge(q, 10);
  if (anim_step(q) == 1) q.sound(q.self(), kChanWeapon, q.s(s_hknight_slash1_wav), 1, kAttnNorm);
  if ((anim_step(q) == 3) | (anim_step(q) == 5)) melee_blow(q, 3);
  if (anim_step(q) >= 6) monster_run(q);
}

void hknight_melee(A& q) {
  anim_begin(q, q.v(q.self(), f_fr_melee));
  hknight_swing(q);
}

// The charge: a dozen steps at speed, the blade landing whenever it reaches.
void hknight_charge(A& q) {
  think_again(q, fn_hknight_charge);
  anim_tick(q);
  ai_charge(q, 20);
  if ((anim_step(q) >= 4) & (anim_step(q) <= 10)) melee_blow(q, 2);
  if (anim_step(q) >= 12) monster_run(q);
}

// A fan of six slow bolts (300, 9 each) across its enemy.
void hknight_cast(A& q) {
  think_again(q, fn_hknight_cast);
  anim_tick(q);
  face_enemy(q);
  if (anim_step(q) == 1) q.sound(q.self(), kChanVoice, q.s(s_hknight_attack1_wav), 1, kAttnNorm);
  if (anim_step(q) == 4) {
    for (int k = 0; k < 6; k++) {  // -2.5 to 2.5 a step of 1, as the QC counts it
      const float i = static_cast<float>(k) - 2.5f;
      const int32_t m = monster_missile(q, q.s(s_k_spike_mdl), 300, 9, 0);
      const Vec3 fan = q.v_right() * i * 0.1f * 300;
      q.set_v(m, q.L.f.velocity, q.normalize(q.v(m, q.L.f.velocity) + fan) * 300);
      q.set_v(m, q.L.f.angles, q.vectoangles(q.v(m, q.L.f.velocity)));
      q.set_i(m, f_deathtype, q.s(s_burned_by_magic));
    }
  }
  if (anim_step(q) >= 8) monster_run(q);
}

void hknight_missile(A& q) {
  anim_begin(q, q.v(q.self(), f_fr_missile));
  hknight_cast(q);
}

namespace {

// Its own decision: the sword in reach, the charge from 80 to 250 units, else the bolts.
bool hknight_decides(A& q) {
  const int32_t me = q.self();
  if (!can_see(q, enemy(q))) return false;
  const float d = enemy_distance(q);
  if (d <= 80) {
    q.set_f(me, f_attack_state, kAsMelee);
    return true;
  }
  if (q.time() < q.f(me, f_attack_finished)) return false;
  const bool near = d <= 250;
  const bool level = q.fabs(enemy_origin(q).z - q.v(me, q.L.f.origin).z) <= 20;  // either way
  if (near & level) {
    q.set_f(me, f_attack_finished, q.time() + 2);
    face_enemy(q);
    anim_begin(q, q.v(q.self(), f_fr_leap));
    hknight_charge(q);
    return true;
  }
  return check_attack(q);
}

}  // namespace

void hknight_check(A& q) { pg::set_return_float(q.vm, hknight_decides(q) ? 1 : 0); }

void hknight_flinch(A& q) { flinch(q, fn_hknight_flinch, 3); }

void hknight_pain(A& q) {
  hold_pain(q, 1);
  q.sound(q.self(), kChanVoice, q.s(s_hknight_pain1_wav), 1, kAttnNorm);
  anim_begin(q, q.v(q.self(), f_fr_pain));
  hknight_flinch(q);
}

void monster_hell_knight(A& q) {
  if (no_monsters_here(q)) return;
  q.precache_model(q.s(s_hknight_mdl));
  q.precache_model(q.s(s_h_hellkn_mdl));
  q.precache_model(q.s(s_k_spike_mdl));
  precache_sounds(q, {s_hknight_attack1_wav, s_hknight_death1_wav, s_hknight_pain1_wav,
                      s_hknight_sight1_wav, s_hknight_hit_wav, s_hknight_slash1_wav,
                      s_hknight_idle_wav, s_hknight_grunt_wav});
  monster_kind(q, s_hknight_sight1_wav, s_hknight_death1_wav, s_h_hellkn_mdl, -40, 2, 16);
  const int32_t me = q.self();
  q.set_i(me, f_th_melee, q.fn(fn_hknight_melee));
  q.set_i(me, f_th_missile, q.fn(fn_hknight_missile));
  q.set_i(me, f_th_check, q.fn(fn_hknight_check));
  q.set_i(me, f_th_pain, q.fn(fn_hknight_pain));
  q.set_i(me, f_th_die, q.fn(fn_monster_die));
  set_frames(q, f_fr_stand, 0, 8);
  set_frames(q, f_fr_walk, 9, 28);
  set_frames(q, f_fr_run, 29, 36);
  set_frames(q, f_fr_melee, 112, 121);
  set_frames(q, f_fr_missile, 155, 165);
  set_frames(q, f_fr_leap, 63, 78);
  set_frames(q, f_fr_pain, 37, 41);
  set_frames(q, f_fr_death, 42, 53);
  set_frames(q, f_fr_death2, 54, 62);
  q.set_f(me, f_death_solid, 2);
  walkmonster_start(q, q.s(s_hknight_mdl), 250, {-16, -16, -24}, {16, 16, 40});
}

}  // namespace shooters_edge::server::rules::campaign
