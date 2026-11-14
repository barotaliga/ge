// The set pieces: trains on their paths, secret doors, exploding boxes, fireball spouts, spike
// and laser traps, bubbles and the ambient sounds.
//
// ge:summary The campaign's set pieces in C++: trains, secret doors, boxes, traps, bubbles, sounds
// ge:keywords campaign native-campaign func_train func_door_secret misc_explobox misc_fireball
// ge:keywords trap_spikeshooter trap_shooter air_bubbles ambient-sounds
// ge:skill progs shooter
// ge:test tests/shooters_edge_campaign_rules_test.cpp
#include <cstdint>

#include "shooters_edge/rules/campaign_decl.hpp"

namespace shooters_edge::server::rules::campaign {

namespace {

float ltime(A& q) { return q.f(q.self(), q.L.f.ltime); }

void then_at(A& q, Fn fn, float at) {
  q.set_i(q.self(), q.L.f.think, q.fn(fn));
  q.set_f(q.self(), q.L.f.nextthink, at);
}

// What blocks a mover takes its .dmg every half second, told by `how`.
void crush(A& q, St how) {
  const int32_t me = q.self();
  if (q.time() < q.f(me, f_attack_finished)) return;
  q.set_f(me, f_attack_finished, q.time() + 0.5f);
  q.set_i(q.other(), f_deathtype, q.s(how));
  damage(q, q.other(), me, me, q.f(me, f_dmg));
}

}  // namespace

// THE TRAIN (func_train): a brush that rides its path of corners. It waits at each corner for that
// corner's .wait (none when unset; -1 stops it until used again), crushes what blocks it for .dmg
// (2) and rides at .speed (100). Its lowest corner is the one that rides the path. Sounds 1 gives
// it a voice.
void train_blocked(A& q) { crush(q, s_crushed); }

void train_arrived(A& q) {
  const int32_t me = q.self();
  if (!q.is(q.i(me, q.L.f.noise1), "")) q.sound(me, kChanVoice, q.i(me, q.L.f.noise1), 1, kAttnNorm);
  if (q.f(me, f_wait) < 0) {  // stops here until used
    q.set_i(me, q.L.f.use, q.fn(fn_train_next));
    return;
  }
  if (q.f(me, f_wait) > 0) {
    then_at(q, fn_train_next, ltime(q) + q.f(me, f_wait));
    return;
  }
  train_next(q);
}

void train_next(A& q) {
  const int32_t me = q.self();
  const int32_t corner = q.find(q.world(), q.L.f.targetname, q.i(me, q.L.f.target));
  if (corner == 0) {
    q.objerror({q.s(s_a_train_s_path), q.i(me, q.L.f.target)});
    return;
  }
  q.set_i(me, q.L.f.use, q.fn(fn_noop));
  q.set_i(me, q.L.f.target, q.i(corner, q.L.f.target));
  q.set_f(me, f_wait, q.f(corner, f_wait));
  if (!q.is(q.i(me, q.L.f.noise), "")) q.sound(me, kChanVoice, q.i(me, q.L.f.noise), 1, kAttnNorm);
  calc_move(q, q.v(corner, q.L.f.origin) - q.v(me, q.L.f.mins), q.f(me, f_speed),
            q.fn(fn_train_arrived));
}

// Once every corner has spawned: stood on its first, and away unless something names it.
void train_place(A& q) {
  const int32_t me = q.self();
  const int32_t corner = q.find(q.world(), q.L.f.targetname, q.i(me, q.L.f.target));
  if (corner == 0) {
    q.objerror({q.s(s_a_train_cannot_find), q.i(me, q.L.f.target)});
    return;
  }
  q.set_i(me, q.L.f.target, q.i(corner, q.L.f.target));
  q.setorigin(me, q.v(corner, q.L.f.origin) - q.v(me, q.L.f.mins));
  if (q.is(q.i(me, q.L.f.targetname), "")) then_at(q, fn_train_next, ltime(q) + 0.1f);
  else q.set_i(me, q.L.f.use, q.fn(fn_train_next));
}

void func_train(A& q) {
  const int32_t me = q.self();
  if (q.f(me, f_speed) == 0) q.set_f(me, f_speed, 100);
  if (q.f(me, f_dmg) == 0) q.set_f(me, f_dmg, 2);
  if (q.is(q.i(me, q.L.f.target), "")) q.objerror({q.s(s_a_train_needs_a_path)});
  if (q.f(me, q.L.f.sounds) == 1) {
    q.set_i(me, q.L.f.noise, q.s(s_plats_train2_wav));
    q.set_i(me, q.L.f.noise1, q.s(s_plats_train1_wav));
    q.precache_sound(q.i(me, q.L.f.noise));
    q.precache_sound(q.i(me, q.L.f.noise1));
  }
  q.set_v(me, q.L.f.angles, {0, 0, 0});
  brush_start(q);
  q.set_i(q.self(), q.L.f.blocked, q.fn(fn_train_blocked));
  q.set_i(q.self(), q.L.f.use, q.fn(fn_noop));
  then_at(q, fn_train_place, ltime(q) + 0.1f);
}

// THE SECRET DOOR (func_door_secret): back into the wall, then aside. It first moves back into the
// wall, across its angle to its right (to its left with spawn flag 2) by its depth that way
// (.t_length when set), rests a second, then slides along its angle by its size that way
// (.t_width when set), or down by its height with flag 4. It stands open .wait seconds (5) and
// closes the way it came, or stays open with flag 1 or a wait of -1. Something names it: used;
// else shot (and flag 16 makes it shot as well; 8 never).
namespace {

// Each leg starts with its moving sound (on the body channel, beside the start's own sound on the
// voice channel) and ends with the stop sound; at the first stop the door rests a second.
void secret_leg(A& q, Vec3 to, Fn then) {
  const int32_t me = q.self();
  if (!q.is(q.i(me, q.L.f.noise2), "")) q.sound(me, kChanBody, q.i(me, q.L.f.noise2), 1, kAttnNorm);
  calc_move(q, to, q.f(me, f_speed), q.fn(then));
}

void secret_stop(A& q) {
  const int32_t me = q.self();
  if (!q.is(q.i(me, q.L.f.noise3), "")) q.sound(me, kChanVoice, q.i(me, q.L.f.noise3), 1, kAttnNorm);
}

}  // namespace

void secret_closed(A& q) {
  secret_stop(q);
  const int32_t me = q.self();
  q.set_f(me, f_state, kStateBottom);
  if (on(q.f(me, q.L.f.max_health))) {
    q.set_f(me, q.L.f.health, q.f(me, q.L.f.max_health));
    q.set_f(me, q.L.f.takedamage, kDamageYes);
  }
}

void secret_back(A& q) { secret_leg(q, q.v(q.self(), f_pos1), fn_secret_closed); }

// Back at the first stop on the way in: a second's rest, then home.
void secret_shut_rest(A& q) {
  secret_stop(q);
  then_at(q, fn_secret_back, ltime(q) + 1);
}

void secret_shut(A& q) {
  q.set_f(q.self(), f_state, kStateDown);
  secret_leg(q, q.v(q.self(), f_dest1), fn_secret_shut_rest);
}

void secret_open_full(A& q) {
  secret_stop(q);
  const int32_t me = q.self();
  q.set_f(me, f_state, kStateTop);
  if ((q.f(me, f_wait) < 0) | (band(q.f(me, q.L.f.spawnflags), 1) != 0)) return;  // for good
  then_at(q, fn_secret_shut, ltime(q) + q.f(me, f_wait));
}

void secret_aside(A& q) { secret_leg(q, q.v(q.self(), f_dest2), fn_secret_open_full); }

// Back in the wall: a second's rest, then aside.
void secret_open_rest(A& q) {
  secret_stop(q);
  then_at(q, fn_secret_aside, ltime(q) + 1);
}

void secret_use(A& q) {
  if (q.f(q.self(), f_state) != kStateBottom) return;
  q.set_f(q.self(), f_state, kStateUp);
  q.set_f(q.self(), q.L.f.takedamage, kDamageNo);
  use_targets(q);
  const int32_t me = q.self();
  if (!q.is(q.i(me, q.L.f.noise1), "")) q.sound(me, kChanVoice, q.i(me, q.L.f.noise1), 1, kAttnNorm);
  secret_leg(q, q.v(me, f_dest1), fn_secret_open_rest);
}

void secret_shot(A& q) {
  q.set_f(q.self(), q.L.f.health, q.f(q.self(), q.L.f.max_health));
  q.set_ge(g_activator, q.ge(g_damage_attacker));
  secret_use(q);
}

void secret_blocked(A& q) { crush(q, s_squished); }

// A message on it is told to whoever walks into it, at most every 2 s.
void secret_door_touch(A& q) {
  const int32_t me = q.self();
  const int32_t other = q.other();
  if (!q.is(q.i(other, q.L.f.classname), "player") | q.is(q.i(me, q.L.f.message), "")) return;
  if (q.time() < q.f(me, f_attack_finished)) return;
  q.set_f(me, f_attack_finished, q.time() + 2);
  q.centerprint(other, {q.i(me, q.L.f.message)});
  q.sound(other, kChanBody, q.s(s_misc_talk_wav), 1, kAttnNorm);
}

void func_door_secret(A& q) {
  const int32_t me = q.self();
  if (q.f(me, q.L.f.sounds) == 0) q.set_f(me, q.L.f.sounds, 3);
  if (q.f(me, q.L.f.sounds) == 1) {
    q.set_i(me, q.L.f.noise1, q.s(s_doors_latch2_wav));
    q.set_i(me, q.L.f.noise2, q.s(s_doors_winch2_wav));
    q.set_i(me, q.L.f.noise3, q.s(s_drclos4_wav));
  } else if (q.f(me, q.L.f.sounds) == 2) {
    q.set_i(me, q.L.f.noise1, q.s(s_doors_airdoor2_wav));
    q.set_i(me, q.L.f.noise2, q.s(s_doors_airdoor1_wav));
    q.set_i(me, q.L.f.noise3, q.s(s_doors_airdoor2_wav));
  } else {
    q.set_i(me, q.L.f.noise1, q.s(s_doors_basesec2_wav));
    q.set_i(me, q.L.f.noise2, q.s(s_doors_basesec1_wav));
    q.set_i(me, q.L.f.noise3, q.s(s_doors_basesec2_wav));
  }
  q.precache_sound(q.i(me, q.L.f.noise1));
  q.precache_sound(q.i(me, q.L.f.noise2));
  q.precache_sound(q.i(me, q.L.f.noise3));
  q.makevectors(q.v(me, q.L.f.angles));
  Vec3 side = q.v_right();
  set_movedir(q);
  const Vec3 fwd = q.v(q.self(), q.L.f.movedir);
  brush_start(q);
  if (q.f(me, f_speed) == 0) q.set_f(me, f_speed, 50);
  if (q.f(me, f_wait) == 0) q.set_f(me, f_wait, 5);
  if (q.f(me, f_dmg) == 0) q.set_f(me, f_dmg, 2);
  // First back into the wall: across its angle, to its right (its left with flag 2), by its
  // depth there. Then along its angle by its size that way, or down by its height with flag 4.
  const float flags = q.f(me, q.L.f.spawnflags);
  if (on(band(flags, 2))) side = Vec3{0, 0, 0} - side;
  float depth = q.f(me, f_t_length);
  if (depth == 0) depth = q.fabs(dot(side, q.v(me, q.L.f.size)));
  q.set_v(me, f_pos1, q.v(me, q.L.f.origin));
  q.set_v(me, f_dest1, q.v(me, q.L.f.origin) + side * depth);
  float width = q.f(me, f_t_width);
  if (on(band(flags, 4))) {
    if (width == 0) width = q.v(me, q.L.f.size).z;
    q.set_v(me, f_dest2, q.v(me, f_dest1) - Vec3{0, 0, 1} * width);
  } else {
    if (width == 0) width = q.fabs(dot(fwd, q.v(me, q.L.f.size)));
    q.set_v(me, f_dest2, q.v(me, f_dest1) + fwd * width);
  }
  q.set_f(me, f_state, kStateBottom);
  q.set_i(me, q.L.f.use, q.fn(fn_secret_use));
  q.set_i(me, q.L.f.blocked, q.fn(fn_secret_blocked));
  q.set_i(me, q.L.f.touch, q.fn(fn_secret_door_touch));
  const bool never = band(flags, 8) != 0;
  const bool shot = q.is(q.i(me, q.L.f.targetname), "") | (band(flags, 16) != 0);
  if (!never & shot) {
    q.set_f(me, q.L.f.max_health, 10000);
    q.set_f(me, q.L.f.health, 10000);
    q.set_f(me, q.L.f.takedamage, kDamageYes);
    q.set_i(me, f_th_hurt, q.fn(fn_secret_shot));
    q.set_i(me, f_th_die, q.fn(fn_secret_shot));
  }
}

// THE EXPLODING BOXES (misc_explobox, misc_explobox2): 20 health, then a blast of 160. The box
// stands on its origin, a bottom corner: 32 by 32, 64 high (the small one 32 high). Dying: it
// blows at once, in the damage that killed it, so a box beside it in the blast goes up in the same
// frame. The blast starts from the origin; the flash and the sound 32 higher.
void explobox_die(A& q) {
  const int32_t me = q.self();
  q.set_f(me, q.L.f.takedamage, kDamageNo);
  q.set_i(me, q.L.f.classname, q.s(s_explo_box));
  radius_damage(q, me, me, 160, q.world(), q.s(s_blew_up));
  q.set_v(q.self(), q.L.f.origin, q.v(q.self(), q.L.f.origin) + Vec3{0, 0, 32});
  q.sound(q.self(), kChanVoice, q.s(s_weapons_r_exp3_wav), 1, kAttnNorm);
  effect_explosion(q, q.v(q.self(), q.L.f.origin));
  q.remove(q.self());
}

namespace {

void explobox_start(A& q, St mdl, Vec3 size) {
  const int32_t me = q.self();
  if (on(q.deathmatch())) {
    q.remove(me);
    return;
  }
  q.precache_model(q.s(mdl));
  q.precache_sound(q.s(s_weapons_r_exp3_wav));
  q.set_f(me, q.L.f.solid, kSolidBbox);
  q.set_f(me, q.L.f.movetype, kMovetypeNone);
  q.setmodel(me, q.s(mdl));
  q.setsize(me, {0, 0, 0}, size);
  Vec3 o = q.v(me, q.L.f.origin);
  o.z = o.z + 2;
  q.set_v(me, q.L.f.origin, o);
  q.setorigin(me, q.v(me, q.L.f.origin));
  if (q.droptofloor() == 0) q.setorigin(me, q.v(me, q.L.f.origin));  // left where the map put it
  q.set_f(me, q.L.f.health, 20);
  q.set_f(me, q.L.f.takedamage, kDamageAim);
  q.set_i(me, f_th_hurt, q.fn(fn_noop));
  q.set_i(me, f_th_die, q.fn(fn_explobox_die));
}

}  // namespace

void misc_explobox(A& q) { explobox_start(q, s_b_explob_bsp, {32, 32, 64}); }
void misc_explobox2(A& q) { explobox_start(q, s_b_exbox2_bsp, {32, 32, 32}); }

// THE FIREBALL SPOUT (misc_fireball): a ball of lava up at .speed (1000) every 0-5 s.
void fireball_touch(A& q) {
  const int32_t other = q.other();
  if (on(q.f(other, q.L.f.takedamage))) {
    q.set_i(other, f_deathtype, q.s(s_burned));
    damage(q, other, q.self(), q.self(), 20);
  }
  q.remove(q.self());
}

void fireball_fly(A& q) {
  const int32_t me = q.self();
  const int32_t b = q.spawn();
  q.set_i(b, q.L.f.classname, q.s(s_fireball));
  q.set_f(b, q.L.f.solid, kSolidTrigger);
  q.set_f(b, q.L.f.movetype, kMovetypeToss);
  Vec3 v = Vec3{0, 0, 1} * q.f(me, f_speed);
  const float c1 = crandom(q);
  v = v + fv(c1, {50, 0, 0});
  const float c2 = crandom(q);
  q.set_v(b, q.L.f.velocity, v + fv(c2, {0, 50, 0}));
  q.set_v(b, q.L.f.avelocity, {200, 100, 300});
  q.set_i(b, q.L.f.touch, q.fn(fn_fireball_touch));
  q.set_i(b, q.L.f.think, q.fn(fn_remove_self));
  q.set_f(b, q.L.f.nextthink, q.time() + 5);
  q.setmodel(b, q.s(s_lavaball_mdl));
  q.setsize(b, {0, 0, 0}, {0, 0, 0});
  q.setorigin(b, q.v(me, q.L.f.origin));
  q.set_f(me, q.L.f.nextthink, q.time() + q.random() * 5);
}

void misc_fireball(A& q) {
  q.precache_model(q.s(s_lavaball_mdl));
  const int32_t me = q.self();
  if (q.f(me, f_speed) == 0) q.set_f(me, f_speed, 1000);
  q.set_i(me, q.L.f.think, q.fn(fn_fireball_fly));
  q.set_f(me, q.L.f.nextthink, q.time() + q.random() * 5);
}

// THE TRAPS (trap_spikeshooter, trap_shooter): a spike along their angle when used. A nail at 500
// for 9; spawn flag 1 a heavy one for 15, flag 2 a laser at 600 for 15. The shooter also fires on
// its own every .wait seconds (1).
void trap_fire(A& q) {
  const int32_t me = q.self();
  q.set_gv(q.L.g.v_forward, q.v(me, q.L.f.movedir));  // what launch aims along
  const float flags = q.f(me, q.L.f.spawnflags);
  int32_t m = 0;
  if (on(band(flags, 2))) {
    q.sound(me, kChanVoice, q.s(s_enforcer_enfire_wav), 1, kAttnNorm);
    m = launch(q, q.s(s_laser_mdl), kMovetypeFlymissile, 600, 5);
    q.set_f(m, f_dmg, 15);
    q.set_i(m, q.L.f.touch, q.fn(fn_monster_missile_touch));
    q.set_i(m, f_deathtype, q.s(s_lasered));
  } else {
    q.sound(me, kChanVoice, q.s(s_weapons_spike2_wav), 1, kAttnNorm);
    if (on(band(flags, 1))) m = launch(q, q.s(s_s_spike_mdl), kMovetypeFlymissile, 500, 6);
    else m = launch(q, q.s(s_spike_mdl), kMovetypeFlymissile, 500, 6);
    q.set_i(m, q.L.f.classname, q.s(s_spike));
    q.set_f(m, f_dmg, 9);
    if (on(band(q.f(q.self(), q.L.f.spawnflags), 1))) q.set_f(m, f_dmg, 15);
    q.set_i(m, q.L.f.touch, q.fn(fn_spike_touch));
  }
  q.setsize(m, {0, 0, 0}, {0, 0, 0});
  q.setorigin(m, q.v(q.self(), q.L.f.origin));
}

namespace {

void trap_start(A& q) {
  set_movedir(q);
  q.precache_model(q.s(s_spike_mdl));
  q.precache_model(q.s(s_s_spike_mdl));
  q.precache_model(q.s(s_laser_mdl));
  q.precache_sound(q.s(s_weapons_spike2_wav));
  q.precache_sound(q.s(s_enforcer_enfire_wav));
  q.set_i(q.self(), q.L.f.use, q.fn(fn_trap_fire));
}

}  // namespace

void trap_spikeshooter(A& q) { trap_start(q); }

void trap_shooter_think(A& q) {
  trap_fire(q);
  q.set_f(q.self(), q.L.f.nextthink, q.time() + q.f(q.self(), f_wait));
}

void trap_shooter(A& q) {
  trap_start(q);
  const int32_t me = q.self();
  if (q.f(me, f_wait) == 0) q.set_f(me, f_wait, 1);
  q.set_i(me, q.L.f.think, q.fn(fn_trap_shooter_think));
  q.set_f(me, q.L.f.nextthink, q.time() + q.f(me, f_wait));
}

// BUBBLES (air_bubbles): one a second rising out of the water's floor.
void bubble_rise(A& q) {
  const int32_t me = q.self();
  const bool out = q.pointcontents(q.v(me, q.L.f.origin)) != kContentWater;
  if (out | (q.time() > q.f(me, f_attack_finished))) {
    q.remove(me);
    return;
  }
  Vec3 v = {0, 0, 15};
  const float c1 = crandom(q);
  v = v + fv(c1, {4, 0, 0});
  const float c2 = crandom(q);
  q.set_v(me, q.L.f.velocity, v + fv(c2, {0, 4, 0}));
  q.set_f(me, q.L.f.nextthink, q.time() + 0.5f);
}

void bubbles_make(A& q) {
  const int32_t me = q.self();
  const int32_t b = q.spawn();
  q.set_i(b, q.L.f.classname, q.s(s_bubble));
  q.set_f(b, q.L.f.solid, kSolidNot);
  q.set_f(b, q.L.f.movetype, kMovetypeNoclip);
  q.set_v(b, q.L.f.velocity, {0, 0, 15});
  q.set_f(b, f_attack_finished, q.time() + 10);
  q.set_i(b, q.L.f.think, q.fn(fn_bubble_rise));
  q.set_f(b, q.L.f.nextthink, q.time() + 0.5f);
  q.setmodel(b, q.s(s_s_bubble_spr));
  q.setsize(b, {0, 0, 0}, {0, 0, 0});
  q.setorigin(b, q.v(me, q.L.f.origin));
  const float t = q.time() + 0.5f;
  q.set_f(me, q.L.f.nextthink, t + q.random());
}

void air_bubbles(A& q) {
  if (on(q.deathmatch())) {
    q.remove(q.self());
    return;
  }
  q.precache_model(q.s(s_s_bubble_spr));
  q.set_i(q.self(), q.L.f.think, q.fn(fn_bubbles_make));
  q.set_f(q.self(), q.L.f.nextthink, q.time() + 1);
}

// THE AMBIENT SOUNDS: each a loop the clients play where it stands.
namespace {

void ambient_start(A& q, St snd, float vol) {
  q.precache_sound(q.s(snd));
  q.ambientsound(q.v(q.self(), q.L.f.origin), q.s(snd), vol, kAttnStatic);
  q.remove(q.self());
}

}  // namespace

void ambient_comp_hum(A& q) { ambient_start(q, s_ambience_comp1_wav, 1); }
void ambient_drip(A& q) { ambient_start(q, s_ambience_drip1_wav, 0.5f); }
void ambient_drone(A& q) { ambient_start(q, s_ambience_drone6_wav, 0.5f); }
void ambient_flouro_buzz(A& q) { ambient_start(q, s_ambience_buzz1_wav, 1); }
void ambient_light_buzz(A& q) { ambient_start(q, s_ambience_fl_hum1_wav, 0.5f); }
void ambient_suck_wind(A& q) { ambient_start(q, s_ambience_suck1_wav, 1); }
void ambient_swamp1(A& q) { ambient_start(q, s_ambience_swamp1_wav, 0.5f); }
void ambient_swamp2(A& q) { ambient_start(q, s_ambience_swamp2_wav, 0.5f); }
void ambient_thunder(A& q) { ambient_start(q, s_ambience_thunder1_wav, 0.5f); }

}  // namespace shooters_edge::server::rules::campaign
