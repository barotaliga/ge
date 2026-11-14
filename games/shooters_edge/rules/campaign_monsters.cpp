// The roster, the smaller kinds: each kind's numbers and attacks over the minds in campaign_ai.cpp.
// The rifleman, the hound, the swordsman, the brute (and its marksman, the same), the ghoul, the
// flyer and the eel. The classnames are the maps'; the models and sounds are the pack's names,
// mapped to our own by content/classic.assetmap.
//
// ge:summary The campaign's smaller monsters in C++: rifleman, hound, swordsman, brute, ghoul, flyer
// ge:keywords campaign native-campaign monster_army monster_dog monster_knight monster_ogre
// ge:keywords monster_zombie monster_wizard monster_fish roster
// ge:skill progs shooter
// ge:test tests/shooters_edge_campaign_rules_test.cpp
#include <cstdint>

#include "shooters_edge/rules/campaign_decl.hpp"

namespace shooters_edge::server::rules::campaign {

namespace {

void precache_sounds(A& q, std::initializer_list<St> list) {
  for (St s : list) q.precache_sound(q.s(s));
}

}  // namespace

// Off the ground toward the enemy: `fwd` along its facing and `up`, a unit raised, `touch` its
// contact while in the air.
void leap_off(A& q, float fwd, float up, Fn touch) {
  face_enemy(q);
  const int32_t me = q.self();
  q.makevectors(q.v(me, q.L.f.angles));
  q.set_v(me, q.L.f.velocity, q.v_forward() * fwd + Vec3{0, 0, up});
  Vec3 o = q.v(me, q.L.f.origin);
  o.z = o.z + 1;
  q.set_v(me, q.L.f.origin, o);
  q.setorigin(me, q.v(me, q.L.f.origin));
  lands_no_flag(q, me);
  q.set_i(me, q.L.f.touch, q.fn(touch));
}

// THE RIFLEMAN (monster_army): 30 health, a four-pellet blast after a short wind-up.
namespace {

void army_fire(A& q) {
  const int32_t me = q.self();
  aim_along(q, enemy_origin(q) + q.v(q.i(me, q.L.f.enemy), q.L.f.view_ofs) -
                   (q.v(me, q.L.f.origin) + q.v(me, q.L.f.view_ofs)));
  q.sound(me, kChanWeapon, q.s(s_soldier_sattck1_wav), 1, kAttnNorm);
  q.set_f(me, q.L.f.effects, bor(q.f(me, q.L.f.effects), kEfMuzzleflash));
  fire_pellets(q, 4, 4, 0.1f);
}

void no_flash(A& q) {
  const float e = q.f(q.self(), q.L.f.effects);
  q.set_f(q.self(), q.L.f.effects, e - band(e, kEfMuzzleflash));
}

}  // namespace

// Four steps aiming, the shot, three steps to recover.
void army_attack(A& q) {
  think_again(q, fn_army_attack);
  no_flash(q);
  anim_tick(q);
  if (anim_step(q) <= 4) face_enemy(q);
  if (anim_step(q) == 5) army_fire(q);
  if (anim_step(q) >= 8) monster_run(q);
}

void army_missile(A& q) {
  anim_begin(q, q.v(q.self(), f_fr_missile));
  army_attack(q);
}

// Flinches for 0.3 s.
void army_flinch(A& q) { flinch(q, fn_army_flinch, 3); }

void army_pain(A& q) {
  hold_pain(q, 1);
  if (q.random() < 0.5f) q.sound(q.self(), kChanVoice, q.s(s_soldier_pain1_wav), 1, kAttnNorm);
  else q.sound(q.self(), kChanVoice, q.s(s_soldier_pain2_wav), 1, kAttnNorm);
  anim_begin(q, q.v(q.self(), f_fr_pain));
  army_flinch(q);
}

void monster_army(A& q) {
  if (on(q.deathmatch())) {
    q.remove(q.self());
    return;
  }
  q.precache_model(q.s(s_soldier_mdl));
  q.precache_model(q.s(s_h_guard_mdl));
  precache_sounds(q, {s_soldier_death1_wav, s_soldier_idle_wav, s_soldier_pain1_wav,
                      s_soldier_pain2_wav, s_soldier_sattck1_wav, s_soldier_sight1_wav});
  monster_kind(q, s_soldier_sight1_wav, s_soldier_death1_wav, s_h_guard_mdl, -35, 1, 12);
  const int32_t me = q.self();
  q.set_i(me, f_th_missile, q.fn(fn_army_missile));
  q.set_i(me, f_th_pain, q.fn(fn_army_pain));
  q.set_i(me, f_th_die, q.fn(fn_monster_die));
  set_frames(q, f_fr_stand, 0, 7);
  set_frames(q, f_fr_walk, 90, 113);
  set_frames(q, f_fr_run, 73, 80);
  set_frames(q, f_fr_missile, 81, 89);
  set_frames(q, f_fr_pain, 40, 45);
  set_frames(q, f_fr_death, 8, 17);
  set_frames(q, f_fr_death2, 18, 28);
  q.set_f(me, f_death_solid, 2);
  q.set_f(me, f_drop_ammo, kDropShells);
  walkmonster_start(q, q.s(s_soldier_mdl), 30, {-16, -16, -24}, {16, 16, 40});
}

// THE HOUND (monster_dog): 25 health, a bite, and a leap that bites on contact.
void dog_bite_step(A& q) {
  think_again(q, fn_dog_bite_step);
  anim_tick(q);
  ai_charge(q, 12);
  if (anim_step(q) == 3) {
    q.sound(q.self(), kChanVoice, q.s(s_dog_dattack1_wav), 1, kAttnNorm);
    melee_blow(q, 8);
  }
  if (anim_step(q) >= 5) monster_run(q);
}

void dog_melee(A& q) {
  anim_begin(q, q.v(q.self(), f_fr_melee));
  dog_bite_step(q);
}

// In the air: the first thing that takes damage it meets at speed is bitten.
void dog_leap_touch(A& q) {
  const int32_t me = q.self();
  const int32_t other = q.other();
  const bool hurts = q.f(other, q.L.f.takedamage) != 0;
  const bool fast = q.vlen(q.v(me, q.L.f.velocity)) > 300;
  if (hurts & fast) {
    q.set_i(other, f_deathtype, q.s(s_mauled));
    damage(q, other, me, me, 10 + 10 * q.random());
    q.set_i(q.self(), q.L.f.touch, q.fn(fn_noop));
  }
}

// On the ground again it runs (stuck in the air, on a monster or a ledge's lip: it runs anyway).
void dog_leap_step(A& q) {
  think_again(q, fn_dog_leap_step);
  anim_tick(q);
  if ((anim_step(q) > 2) & (band(q.f(q.self(), q.L.f.flags), kFlOnground) != 0)) {
    q.set_i(q.self(), q.L.f.touch, q.fn(fn_noop));
    monster_run(q);
  }
  if (anim_step(q) > 30) {
    q.set_i(q.self(), q.L.f.touch, q.fn(fn_noop));
    monster_run(q);
  }
}

namespace {

void dog_leap(A& q) {
  leap_off(q, 300, 200, fn_dog_leap_touch);
  q.sound(q.self(), kChanVoice, q.s(s_dog_dattack1_wav), 1, kAttnNorm);
  anim_begin(q, q.v(q.self(), f_fr_leap));
  dog_leap_step(q);
}

// The hound's own decision: the bite in melee range; the leap from 80 to 150 units off, the enemy
// not far above or below.
bool dog_decides(A& q) {
  const int32_t me = q.self();
  if (range_to(q, q.i(me, q.L.f.enemy)) == kRangeMelee) {
    const int32_t enemy = q.i(me, q.L.f.enemy);
    q.traceline(q.v(me, q.L.f.origin) + q.v(me, q.L.f.view_ofs),
                q.v(enemy, q.L.f.origin) + q.v(enemy, q.L.f.view_ofs), 0, me);
    if (q.gi(q.L.g.trace_ent) == q.i(me, q.L.f.enemy)) {
      q.set_f(me, f_attack_state, kAsMelee);
      return true;
    }
  }
  const Vec3 delta = enemy_origin(q) - q.v(me, q.L.f.origin);
  if (q.fabs(delta.z) > 32) return false;
  const float d = q.vlen(delta);
  if ((d < 80) | (d > 150)) return false;
  if (q.time() < q.f(me, f_attack_finished)) return false;
  if (!can_see(q, q.i(me, q.L.f.enemy))) return false;
  q.set_f(me, f_attack_finished, q.time() + 1);
  dog_leap(q);
  return true;
}

}  // namespace

void dog_check(A& q) { pg::set_return_float(q.vm, dog_decides(q) ? 1 : 0); }

void dog_flinch(A& q) { flinch(q, fn_dog_flinch, 3); }

void dog_pain(A& q) {
  hold_pain(q, 1);
  q.sound(q.self(), kChanVoice, q.s(s_dog_dpain1_wav), 1, kAttnNorm);
  q.set_i(q.self(), q.L.f.touch, q.fn(fn_noop));
  anim_begin(q, q.v(q.self(), f_fr_pain));
  dog_flinch(q);
}

void monster_dog(A& q) {
  if (on(q.deathmatch())) {
    q.remove(q.self());
    return;
  }
  q.precache_model(q.s(s_dog_mdl));
  q.precache_model(q.s(s_h_dog_mdl));
  precache_sounds(q, {s_dog_dattack1_wav, s_dog_ddeath_wav, s_dog_dpain1_wav, s_dog_dsight_wav,
                      s_dog_idle_wav});
  monster_kind(q, s_dog_dsight_wav, s_dog_ddeath_wav, s_h_dog_mdl, -35, 8, 18);
  const int32_t me = q.self();
  q.set_i(me, f_th_melee, q.fn(fn_dog_melee));
  q.set_i(me, f_th_check, q.fn(fn_dog_check));
  q.set_i(me, f_th_pain, q.fn(fn_dog_pain));
  q.set_i(me, f_th_die, q.fn(fn_monster_die));
  set_frames(q, f_fr_stand, 69, 77);
  set_frames(q, f_fr_walk, 78, 85);
  set_frames(q, f_fr_run, 48, 59);
  set_frames(q, f_fr_melee, 0, 7);
  set_frames(q, f_fr_leap, 60, 68);
  set_frames(q, f_fr_pain, 26, 31);
  set_frames(q, f_fr_death, 8, 16);
  set_frames(q, f_fr_death2, 17, 25);
  q.set_i(me, f_gib_model, q.s(s_gib3_mdl));
  q.set_f(me, f_gib_first, 1);
  walkmonster_start(q, q.s(s_dog_mdl), 25, {-32, -32, -24}, {32, 32, 40});
}

// THE SWORDSMAN (monster_knight): 75 health, a sword swung while charging. Six steps: closing in,
// the blade landing on the third and the fifth.
void knight_swing(A& q) {
  think_again(q, fn_knight_swing);
  anim_tick(q);
  ai_charge(q, 10);
  if (anim_step(q) == 1) {
    if (q.random() < 0.5f) q.sound(q.self(), kChanWeapon, q.s(s_knight_sword1_wav), 1, kAttnNorm);
    else q.sound(q.self(), kChanWeapon, q.s(s_knight_sword2_wav), 1, kAttnNorm);
  }
  if ((anim_step(q) == 3) | (anim_step(q) == 5)) melee_blow(q, 3);
  if (anim_step(q) >= 6) monster_run(q);
}

void knight_melee(A& q) {
  anim_begin(q, q.v(q.self(), f_fr_melee));
  knight_swing(q);
}

// The swordsman's own decision: the swing once the enemy is within 80 units and seen.
void knight_check(A& q) {
  bool yes = false;
  if (enemy_distance(q) <= 80 && can_see(q, q.i(q.self(), q.L.f.enemy))) {
    q.set_f(q.self(), f_attack_state, kAsMelee);
    yes = true;
  }
  pg::set_return_float(q.vm, yes ? 1 : 0);
}

void knight_flinch(A& q) { flinch(q, fn_knight_flinch, 3); }

void knight_pain(A& q) {
  hold_pain(q, 1);
  q.sound(q.self(), kChanVoice, q.s(s_knight_khurt_wav), 1, kAttnNorm);
  anim_begin(q, q.v(q.self(), f_fr_pain));
  knight_flinch(q);
}

void monster_knight(A& q) {
  if (no_monsters_here(q)) return;
  q.precache_model(q.s(s_knight_mdl));
  q.precache_model(q.s(s_h_knight_mdl));
  precache_sounds(q, {s_knight_kdeath_wav, s_knight_khurt_wav, s_knight_ksight_wav,
                      s_knight_sword1_wav, s_knight_sword2_wav, s_knight_idle_wav});
  monster_kind(q, s_knight_ksight_wav, s_knight_kdeath_wav, s_h_knight_mdl, -40, 3, 14);
  const int32_t me = q.self();
  q.set_i(me, f_th_melee, q.fn(fn_knight_melee));
  q.set_i(me, f_th_check, q.fn(fn_knight_check));
  q.set_i(me, f_th_pain, q.fn(fn_knight_pain));
  q.set_i(me, f_th_die, q.fn(fn_monster_die));
  set_frames(q, f_fr_stand, 0, 8);
  set_frames(q, f_fr_walk, 53, 66);
  set_frames(q, f_fr_run, 9, 16);
  set_frames(q, f_fr_melee, 17, 27);
  set_frames(q, f_fr_pain, 28, 30);
  set_frames(q, f_fr_death, 76, 85);
  set_frames(q, f_fr_death2, 86, 96);
  q.set_f(me, f_death_solid, 2);
  walkmonster_start(q, q.s(s_knight_mdl), 75, {-16, -16, -24}, {16, 16, 40});
}

// THE BRUTE (monster_ogre): 200 health, a chainsaw and a lobbed grenade. The grenade's blast: 40
// less half the distance to everything near, the brute's own kind too.
void ogre_grenade_blast(A& q) {
  const int32_t me = q.self();
  radius_damage(q, me, q.i(me, q.L.f.owner), 40, q.world(), q.s(s_blown_up_by_a_grenade));
  q.sound(q.self(), kChanVoice, q.s(s_weapons_r_exp3_wav), 1, kAttnNorm);
  effect_explosion(q, q.v(q.self(), q.L.f.origin));
  q.remove(q.self());
}

void ogre_grenade_touch(A& q) {
  if (q.other() == q.i(q.self(), q.L.f.owner)) return;
  if (q.f(q.other(), q.L.f.takedamage) == kDamageAim) {
    ogre_grenade_blast(q);
    return;
  }
  q.sound(q.self(), kChanVoice, q.s(s_weapons_bounce_wav), 1, kAttnNorm);
}

// Five steps: aiming, the grenade (600 at the enemy, 200 up, bouncing, a 2.5 s fuse), recovering.
void ogre_throw(A& q) {
  think_again(q, fn_ogre_throw);
  anim_tick(q);
  if (anim_step(q) <= 2) face_enemy(q);
  if (anim_step(q) == 3) {
    q.sound(q.self(), kChanWeapon, q.s(s_weapons_grenade_wav), 1, kAttnNorm);
    const int32_t g = monster_missile(q, q.s(s_grenade_mdl), 600, 0, 200);
    q.set_f(g, q.L.f.movetype, kMovetypeBounce);
    q.set_v(g, q.L.f.avelocity, {300, 300, 300});
    q.set_i(g, q.L.f.touch, q.fn(fn_ogre_grenade_touch));
    q.set_i(g, q.L.f.think, q.fn(fn_ogre_grenade_blast));
    q.set_f(g, q.L.f.nextthink, q.time() + 2.5f);
  }
  if (anim_step(q) >= 5) monster_run(q);
}

void ogre_missile(A& q) {
  anim_begin(q, q.v(q.self(), f_fr_missile));
  ogre_throw(q);
}

// Eight steps of the saw, closing in, biting on four of them.
void ogre_saw(A& q) {
  think_again(q, fn_ogre_saw);
  anim_tick(q);
  ai_charge(q, 6);
  if (anim_step(q) == 1) q.sound(q.self(), kChanWeapon, q.s(s_ogre_ogsawatk_wav), 1, kAttnNorm);
  if ((anim_step(q) >= 3) & (anim_step(q) <= 6)) melee_blow(q, 4);
  if (anim_step(q) >= 8) monster_run(q);
}

void ogre_melee(A& q) {
  anim_begin(q, q.v(q.self(), f_fr_melee));
  ogre_saw(q);
}

void ogre_flinch(A& q) { flinch(q, fn_ogre_flinch, 4); }

void ogre_pain(A& q) {
  hold_pain(q, 1.5f);
  q.sound(q.self(), kChanVoice, q.s(s_ogre_ogpain1_wav), 1, kAttnNorm);
  anim_begin(q, q.v(q.self(), f_fr_pain));
  ogre_flinch(q);
}

void monster_ogre(A& q) {
  if (no_monsters_here(q)) return;
  q.precache_model(q.s(s_ogre_mdl));
  q.precache_model(q.s(s_h_ogre_mdl));
  q.precache_model(q.s(s_grenade_mdl));
  precache_sounds(q, {s_ogre_ogdrag_wav, s_ogre_ogdth_wav, s_ogre_ogidle_wav, s_ogre_ogpain1_wav,
                      s_ogre_ogsawatk_wav, s_ogre_ogwake_wav, s_weapons_grenade_wav,
                      s_weapons_bounce_wav});
  monster_kind(q, s_ogre_ogwake_wav, s_ogre_ogdth_wav, s_h_ogre_mdl, -80, 2, 14);
  const int32_t me = q.self();
  q.set_i(me, f_th_melee, q.fn(fn_ogre_melee));
  q.set_i(me, f_th_missile, q.fn(fn_ogre_missile));
  q.set_i(me, f_th_pain, q.fn(fn_ogre_pain));
  q.set_i(me, f_th_die, q.fn(fn_monster_die));
  set_frames(q, f_fr_stand, 0, 8);
  set_frames(q, f_fr_walk, 9, 24);
  set_frames(q, f_fr_run, 25, 32);
  set_frames(q, f_fr_melee, 33, 46);
  set_frames(q, f_fr_missile, 61, 66);
  set_frames(q, f_fr_pain, 67, 71);
  set_frames(q, f_fr_death, 112, 125);
  set_frames(q, f_fr_death2, 126, 135);
  q.set_f(me, f_death_solid, 2);
  q.set_f(me, f_drop_ammo, kDropRockets);
  q.set_i(me, f_gib_model, q.s(s_gib3_mdl));
  walkmonster_start(q, q.s(s_ogre_mdl), 200, {-32, -32, -24}, {32, 32, 64});
}

void monster_ogre_marksman(A& q) { monster_ogre(q); }

// THE GHOUL (monster_zombie): 60 health that every hit short of a big one gives back.
namespace {
constexpr float kGhoulHealth = 60;
constexpr float kGhoulKnockdown = 25;  // a single hit this big knocks it down
}  // namespace

// Thrown flesh: 600 at the enemy, 200 up, 10 on what it hits.
void ghoul_throw(A& q) {
  think_again(q, fn_ghoul_throw);
  anim_tick(q);
  if (anim_step(q) <= 3) face_enemy(q);
  if (anim_step(q) == 4) {
    q.sound(q.self(), kChanWeapon, q.s(s_zombie_z_shot1_wav), 1, kAttnNorm);
    const int32_t m = monster_missile(q, q.s(s_zom_gib_mdl), 600, 10, 200);
    q.set_i(m, f_deathtype, q.s(s_hit_by_flesh));
  }
  if (anim_step(q) >= 6) monster_run(q);
}

void ghoul_missile(A& q) {
  anim_begin(q, q.v(q.self(), f_fr_missile));
  ghoul_throw(q);
}

// Down: not solid to shots for five seconds, then up again with full health.
void ghoul_down(A& q) {
  think_again(q, fn_ghoul_down);
  anim_tick(q);
  if (anim_step(q) == 50) {
    const int32_t me = q.self();
    q.set_f(me, q.L.f.solid, kSolidSlidebox);
    q.set_f(me, q.L.f.takedamage, kDamageAim);
    Vec3 a = q.v(me, q.L.f.angles);
    a.z = 0;
    q.set_v(me, q.L.f.angles, a);
    q.setorigin(me, q.v(me, q.L.f.origin) + Vec3{0, 0, 16});
    q.set_f(me, q.L.f.health, kGhoulHealth);
    q.sound(me, kChanVoice, q.s(s_zombie_z_idle_wav), 1, kAttnIdle);
  }
  if (anim_step(q) >= 55) monster_run(q);
}

// A hit it lives through is shrugged off (its health back), unless big enough to knock it down;
// it still turns on whoever hit it.
void ghoul_hurt(A& q) {
  const int32_t me = q.self();
  const float taken = kGhoulHealth - q.f(me, q.L.f.health);
  q.set_f(me, q.L.f.health, kGhoulHealth);
  monster_hurt(q);
  if (taken < kGhoulKnockdown) return;
  q.sound(q.self(), kChanVoice, q.s(s_zombie_z_fall_wav), 1, kAttnNorm);
  q.set_f(q.self(), q.L.f.solid, kSolidNot);
  q.set_f(q.self(), q.L.f.takedamage, kDamageNo);
  Vec3 a = q.v(q.self(), q.L.f.angles);
  a.z = 90;
  q.set_v(q.self(), q.L.f.angles, a);
  q.setorigin(q.self(), q.v(q.self(), q.L.f.origin) - Vec3{0, 0, 16});
  anim_begin(q, q.v(q.self(), f_fr_pain));
  ghoul_down(q);
}

void ghoul_pain(A& q) {
  hold_pain(q, 1);
  q.sound(q.self(), kChanVoice, q.s(s_zombie_z_pain_wav), 1, kAttnNorm);
}

// Only a single blow of its whole health (an explosion, say) ends it, in pieces.
void ghoul_die(A& q) {
  q.set_f(q.self(), q.L.f.health, -1000);  // whatever killed it: it always bursts
  monster_die(q);
}

void monster_zombie(A& q) {
  if (no_monsters_here(q)) return;
  q.precache_model(q.s(s_zombie_mdl));
  q.precache_model(q.s(s_h_zombie_mdl));
  q.precache_model(q.s(s_zom_gib_mdl));
  precache_sounds(q, {s_zombie_z_idle_wav, s_zombie_z_idle1_wav, s_zombie_z_shot1_wav,
                      s_zombie_z_gib_wav, s_zombie_z_pain_wav, s_zombie_z_pain1_wav,
                      s_zombie_z_fall_wav, s_zombie_z_miss_wav, s_zombie_z_hit_wav,
                      s_zombie_idle_w2_wav});
  monster_kind(q, s_zombie_z_idle_wav, s_zombie_z_gib_wav, s_h_zombie_mdl, 0, 1, 6);
  const int32_t me = q.self();
  q.set_i(me, f_gib_noise, q.s(s_zombie_z_gib_wav));
  q.set_i(me, f_th_missile, q.fn(fn_ghoul_missile));
  q.set_i(me, f_th_pain, q.fn(fn_ghoul_pain));
  q.set_i(me, f_th_die, q.fn(fn_ghoul_die));
  set_frames(q, f_fr_stand, 0, 14);
  set_frames(q, f_fr_walk, 15, 33);
  set_frames(q, f_fr_run, 34, 51);
  set_frames(q, f_fr_missile, 52, 64);
  set_frames(q, f_fr_pain, 91, 102);
  walkmonster_start(q, q.s(s_zombie_mdl), kGhoulHealth, {-16, -16, -24}, {16, 16, 40});
  q.set_i(q.self(), f_th_hurt, q.fn(fn_ghoul_hurt));
}

// THE FLYER (monster_wizard): 80 health, spit fired two at a time. Two spits at 600, 9 each, a
// step apart.
void wizard_spit(A& q) {
  think_again(q, fn_wizard_spit);
  anim_tick(q);
  face_enemy(q);
  if ((anim_step(q) == 2) | (anim_step(q) == 3)) {
    q.sound(q.self(), kChanWeapon, q.s(s_wizard_wattack_wav), 1, kAttnNorm);
    const int32_t m = monster_missile(q, q.s(s_w_spike_mdl), 600, 9, 0);
    q.set_i(m, f_deathtype, q.s(s_spat_on));
  }
  if (anim_step(q) >= 5) monster_run(q);
}

void wizard_missile(A& q) {
  anim_begin(q, q.v(q.self(), f_fr_missile));
  wizard_spit(q);
}

void wizard_flinch(A& q) { flinch(q, fn_wizard_flinch, 2); }

void wizard_pain(A& q) {
  hold_pain(q, 1);
  q.sound(q.self(), kChanVoice, q.s(s_wizard_wpain_wav), 1, kAttnNorm);
  anim_begin(q, q.v(q.self(), f_fr_pain));
  wizard_flinch(q);
}

// Dead, it falls.
void wizard_die(A& q) {
  monster_die(q);
  const int32_t me = q.self();
  const float flags = q.f(me, q.L.f.flags);
  q.set_f(me, q.L.f.flags, flags - band(flags, kFlFly));
  q.set_f(me, q.L.f.movetype, kMovetypeToss);
}

void monster_wizard(A& q) {
  if (no_monsters_here(q)) return;
  q.precache_model(q.s(s_wizard_mdl));
  q.precache_model(q.s(s_h_wizard_mdl));
  q.precache_model(q.s(s_w_spike_mdl));
  precache_sounds(q, {s_wizard_hit_wav, s_wizard_wattack_wav, s_wizard_wdeath_wav,
                      s_wizard_widle1_wav, s_wizard_widle2_wav, s_wizard_wpain_wav,
                      s_wizard_wsight_wav});
  monster_kind(q, s_wizard_wsight_wav, s_wizard_wdeath_wav, s_h_wizard_mdl, -40, 8, 16);
  const int32_t me = q.self();
  q.set_i(me, f_th_missile, q.fn(fn_wizard_missile));
  q.set_i(me, f_th_pain, q.fn(fn_wizard_pain));
  q.set_i(me, f_th_die, q.fn(fn_wizard_die));
  set_frames(q, f_fr_stand, 0, 14);
  set_frames(q, f_fr_walk, 0, 14);
  set_frames(q, f_fr_run, 15, 28);
  set_frames(q, f_fr_missile, 29, 41);
  set_frames(q, f_fr_pain, 42, 45);
  set_frames(q, f_fr_death, 46, 53);
  q.set_f(me, f_death_solid, 2);
  q.set_i(me, f_gib_model, q.s(s_gib2_mdl));
  flymonster_start(q, q.s(s_wizard_mdl), 80, {-16, -16, -24}, {16, 16, 40});
}

// THE EEL (monster_fish): 25 health, a bite.
void fish_bite_step(A& q) {
  think_again(q, fn_fish_bite_step);
  anim_tick(q);
  ai_charge(q, 10);
  if (anim_step(q) == 3) {
    q.sound(q.self(), kChanVoice, q.s(s_fish_bite_wav), 1, kAttnNorm);
    melee_blow_n(q, 2, 3);
  }
  if (anim_step(q) >= 5) monster_run(q);
}

void fish_melee(A& q) {
  anim_begin(q, q.v(q.self(), f_fr_melee));
  fish_bite_step(q);
}

void fish_pain(A& q) {
  hold_pain(q, 1);
  anim_begin(q, q.v(q.self(), f_fr_pain));
  wizard_flinch(q);
}

void monster_fish(A& q) {
  if (no_monsters_here(q)) return;
  q.precache_model(q.s(s_fish_mdl));
  precache_sounds(q, {s_fish_bite_wav, s_fish_death_wav, s_fish_idle_wav});
  monster_kind(q, s_empty, s_fish_death_wav, s_gib2_mdl, -1000, 8, 12);  // never gibbed
  const int32_t me = q.self();
  q.set_i(me, f_th_melee, q.fn(fn_fish_melee));
  q.set_i(me, f_th_pain, q.fn(fn_fish_pain));
  q.set_i(me, f_th_die, q.fn(fn_monster_die));
  set_frames(q, f_fr_stand, 39, 56);
  set_frames(q, f_fr_walk, 39, 56);
  set_frames(q, f_fr_run, 39, 56);
  set_frames(q, f_fr_melee, 0, 17);
  set_frames(q, f_fr_pain, 57, 65);
  set_frames(q, f_fr_death, 18, 38);
  q.set_f(me, f_death_solid, 20);
  swimmonster_start(q, q.s(s_fish_mdl), 25, {-16, -16, -24}, {16, 16, 24});
}

}  // namespace shooters_edge::server::rules::campaign
