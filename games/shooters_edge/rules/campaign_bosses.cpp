// The bosses and their set pieces: the lava boss and the lightning that alone can hurt it, the
// final boss and the moving teleport destination that alone can kill it, and the campaign's
// teleporting under both (monsters go through too; arriving inside something kills it).
//
// ge:summary The campaign's bosses in C++: the lava boss, the lightning, the final boss, teleports
// ge:keywords campaign native-campaign monster_boss event_lightning monster_oldone
// ge:keywords teleport_takes teleport_arrived boss_struck finale
// ge:skill progs shooter
// ge:test tests/shooters_edge_campaign_rules_test.cpp
#include <cstdint>

#include "shooters_edge/rules/campaign_decl.hpp"

namespace shooters_edge::server::rules::campaign {

namespace {

void precache_sounds(A& q, std::initializer_list<St> list) {
  for (St s : list) q.precache_sound(q.s(s));
}

bool is_player(A& q, int32_t e) { return q.is(q.i(e, q.L.f.classname), "player"); }

}  // namespace

// TELEPORTING, the campaign's way. A teleporter sends a living player or monster; one with
// spawnflags 1 sends players only.
bool teleport_takes(A& q, int32_t who) {
  if (q.f(who, q.L.f.health) <= 0) return false;
  if (is_player(q, who)) return true;
  if (on(band(q.f(q.self(), q.L.f.spawnflags), 1))) return false;
  return band(q.f(who, q.L.f.flags), kFlMonster) != 0;
}

// Arriving inside anything that takes damage kills it, whatever its health.
void teleport_arrived(A& q, int32_t who) {
  int32_t e = q.findradius(q.v(who, q.L.f.origin), 400);
  while (e != 0) {
    const Vec3 emin = q.v(e, q.L.f.absmin), emax = q.v(e, q.L.f.absmax);
    const Vec3 wmin = q.v(who, q.L.f.absmin), wmax = q.v(who, q.L.f.absmax);
    if ((e != who) & (q.f(e, q.L.f.takedamage) != 0) & (q.f(e, q.L.f.health) > 0) &
        (emin.x < wmax.x) & (emax.x > wmin.x) & (emin.y < wmax.y) & (emax.y > wmin.y) &
        (emin.z < wmax.z) & (emax.z > wmin.z)) {
      q.set_i(e, f_deathtype, q.s(s_telefragged));
      damage(q, e, who, who, 50000);
    }
    e = q.i(e, q.L.f.chain);
  }
}

// THE LAVA BOSS (monster_boss): out of the lava when used; three strikes of lightning. A ball of
// fire lobbed at its enemy at 300, high enough to land on it; 20 on a hit, a blast of 40 round it.
void boss_ball_touch(A& q) {
  const int32_t me = q.self();
  const int32_t other = q.other();
  if (other == q.i(me, q.L.f.owner)) return;
  if (on(q.f(other, q.L.f.takedamage))) {
    q.set_i(other, f_deathtype, q.s(s_burned_by_the_lava_boss));
    damage(q, other, me, q.i(me, q.L.f.owner), 20);
  }
  radius_damage(q, q.self(), q.i(q.self(), q.L.f.owner), 40, q.other(), q.s(s_caught_by_the_lava));
  effect_explosion(q, q.v(q.self(), q.L.f.origin));
  q.remove(q.self());
}

namespace {

// The nearest living player it can see, kept while he lives; world when there is none.
bool boss_pick(A& q) {
  const int32_t me = q.self();
  const int32_t was = q.i(me, q.L.f.enemy);
  if ((q.f(was, q.L.f.health) <= 0) | !is_player(q, was)) {
    q.set_i(me, q.L.f.enemy, q.world());
    int32_t p = q.find(q.world(), q.L.f.classname, q.s(s_player));
    while (p != 0) {
      const int32_t e = q.i(me, q.L.f.enemy);
      const bool alive = q.f(p, q.L.f.health) > 0;
      const bool none = e == 0;
      const float dp = q.vlen(q.v(p, q.L.f.origin) - q.v(me, q.L.f.origin));
      const float de = q.vlen(q.v(e, q.L.f.origin) - q.v(me, q.L.f.origin));
      if (alive & (none | (dp < de))) q.set_i(me, q.L.f.enemy, p);
      p = q.find(p, q.L.f.classname, q.s(s_player));
    }
  }
  const bool some = q.i(me, q.L.f.enemy) != 0;
  const bool seen = can_see(q, q.i(me, q.L.f.enemy));  // asked either way
  return some & seen;
}

// The throw itself, lobbed to land on the enemy: as much upward as gravity takes back.
void boss_throw(A& q) {
  if (!boss_pick(q)) return;
  face_enemy(q);
  q.sound(q.self(), kChanWeapon, q.s(s_boss1_throw_wav), 1, kAttnNorm);
  const float lob = 400 * enemy_distance(q) / 300;
  const int32_t m = monster_missile(q, q.s(s_lavaball_mdl), 300, 20, lob);
  q.set_v(m, q.L.f.avelocity, {200, 100, 300});
  q.set_i(m, q.L.f.touch, q.fn(fn_boss_ball_touch));
}

}  // namespace

// Up, a frame every 0.1 s: the rise, a strike's shudder or a throw played through (the ball leaves
// on the throw's twelfth frame), else its idle sway; a throw begins every 2 s while a player is
// in sight.
void boss_think(A& q) {
  think_again(q, fn_boss_think);
  const int32_t me = q.self();
  if (on(q.v(me, f_fr_now).z)) {
    anim_tick(q);
    const bool throwing = same_vec(q.v(me, f_fr_now), q.v(me, f_fr_missile));
    if (throwing & (anim_step(q) == 12)) boss_throw(q);
    if (q.f(q.self(), q.L.f.frame) >= q.v(q.self(), f_fr_now).y)
      q.set_v(q.self(), f_fr_now, {0, 0, 0});
    return;
  }
  frame_loop(q, q.v(me, f_fr_walk));
  if (q.time() < q.f(me, f_attack_finished)) return;
  q.set_f(me, f_attack_finished, q.time() + 2);
  if (boss_pick(q)) anim_begin(q, q.v(q.self(), f_fr_missile));
}

// Used (the rune taken): it rises out of the lava, angry at whoever took it.
void boss_use(A& q) {
  const int32_t me = q.self();
  q.set_i(me, q.L.f.use, q.fn(fn_noop));
  q.set_f(me, q.L.f.solid, kSolidSlidebox);
  q.set_f(me, q.L.f.takedamage, kDamageNo);  // only the lightning hurts it
  q.setmodel(me, q.s(s_boss_mdl));
  q.setsize(me, {-128, -128, -24}, {128, 128, 256});
  q.set_v(me, q.L.f.view_ofs, {0, 0, 96});  // it throws from high on its body
  q.setorigin(me, q.v(me, q.L.f.origin));
  q.set_i(me, q.L.f.enemy, q.ge(g_activator));
  q.sound(me, kChanVoice, q.s(s_boss1_sight1_wav), 1, kAttnNorm);
  effect_explosion(q, q.v(me, q.L.f.origin) + Vec3{0, 0, 64});
  anim_begin(q, q.v(me, f_fr_stand));  // the rise
  q.set_f(me, f_attack_finished, q.time() + 1.5f);
  boss_think(q);
}

// Its death frames played, it is gone.
void boss_sink(A& q) {
  anim_tick(q);
  const int32_t me = q.self();
  if (q.f(me, q.L.f.frame) < q.v(me, f_fr_now).y) {
    q.set_f(me, q.L.f.nextthink, q.time() + 0.1f);
    return;
  }
  q.set_f(me, q.L.f.modelindex, 0);
  q.set_i(me, q.L.f.think, q.fn(fn_noop));
  q.set_f(me, q.L.f.nextthink, -1);
}

// One strike: it shudders; three and it sinks back for good, its targets fired.
void boss_struck(A& q) {
  const int32_t was_self = q.self();
  q.set_self(q.ge(g_lava_boss));
  const int32_t me = q.self();
  q.set_f(me, q.L.f.health, q.f(me, q.L.f.health) - 1);
  if (q.f(me, q.L.f.health) > 0) {
    q.sound(me, kChanVoice, q.s(s_boss1_pain_wav), 1, kAttnNorm);
    anim_begin(q, q.v(me, f_fr_pain));
    q.set_f(me, f_attack_finished, q.time() + 2);  // a strike stops its throwing a while
  } else {
    q.sound(me, kChanVoice, q.s(s_boss1_death_wav), 1, kAttnNorm);
    q.add_to(q.L.g.killed_monsters, 1);
    q.WriteByte(kMsgAll, kSvcKilledmonster);
    q.set_ge(g_activator, q.i(me, q.L.f.enemy));
    use_targets(q);
    effect_explosion(q, q.v(q.self(), q.L.f.origin) + Vec3{0, 0, 64});
    q.set_f(q.self(), q.L.f.solid, kSolidNot);
    anim_begin(q, q.v(q.self(), f_fr_death));
    q.set_i(q.self(), q.L.f.think, q.fn(fn_boss_sink));
    q.set_f(q.self(), q.L.f.nextthink, q.time() + 0.1f);
  }
  q.set_self(was_self);
}

void monster_boss(A& q) {
  if (no_monsters_here(q)) return;
  q.precache_model(q.s(s_boss_mdl));
  q.precache_model(q.s(s_lavaball_mdl));
  precache_sounds(q, {s_boss1_death_wav, s_boss1_out1_wav, s_boss1_pain_wav, s_boss1_sight1_wav,
                      s_boss1_throw_wav, s_weapons_r_exp3_wav});
  q.add_to(q.L.g.total_monsters, 1);
  const int32_t me = q.self();
  q.set_f(me, q.L.f.flags, bor(q.f(me, q.L.f.flags), kFlMonster));
  set_frames(q, f_fr_stand, 0, 16);     // the rise
  set_frames(q, f_fr_walk, 17, 47);     // its sway while up
  set_frames(q, f_fr_missile, 57, 79);  // a throw
  set_frames(q, f_fr_pain, 80, 89);     // a strike's shudder
  set_frames(q, f_fr_death, 48, 56);
  q.set_f(me, q.L.f.health, 3);
  q.set_f(me, q.L.f.solid, kSolidNot);  // under the lava until used
  q.set_f(me, q.L.f.movetype, kMovetypeStep);
  q.set_i(me, q.L.f.use, q.fn(fn_boss_use));
  q.set_ge(g_lava_boss, me);
}

// The lightning: used (its button), it strikes between the two doors that name it as their
// target, only while both stand fully open, and then sends them back down. Each strike that
// finds the boss up is one of its three.
void lightning_use(A& q) {
  if (q.time() < q.f(q.self(), f_attack_finished)) return;
  int32_t a = q.world();
  int32_t b = q.world();
  int32_t e = q.find(q.world(), q.L.f.target, q.s(s_lightning));
  while (e != 0) {
    if (q.is(q.i(e, q.L.f.classname), "func_door")) {
      if (a == 0)
        a = e;
      else if (b == 0)
        b = e;
    }
    e = q.find(e, q.L.f.target, q.s(s_lightning));
  }
  if ((a == 0) | (b == 0)) return;
  if ((q.f(a, f_state) != kStateTop) | (q.f(b, f_state) != kStateTop)) return;
  const int32_t boss = q.ge(g_lava_boss);
  if ((boss == 0) | (q.f(boss, q.L.f.health) <= 0) | (q.f(boss, q.L.f.solid) == kSolidNot)) return;
  q.set_f(q.self(), f_attack_finished, q.time() + 1);
  effect_beam(q, (q.v(a, q.L.f.absmin) + q.v(a, q.L.f.absmax)) * 0.5f,
              (q.v(b, q.L.f.absmin) + q.v(b, q.L.f.absmax)) * 0.5f);
  q.sound(q.self(), kChanVoice, q.s(s_misc_power_wav), 1, kAttnNone);
  boss_struck(q);
  const int32_t was_self = q.self();
  q.set_self(a);
  door_go_down(q);
  q.set_self(b);
  door_go_down(q);
  q.set_self(was_self);
}

void event_lightning(A& q) {
  q.precache_sound(q.s(s_misc_power_wav));
  q.set_i(q.self(), q.L.f.use, q.fn(fn_lightning_use));
}

// THE FINAL BOSS (monster_oldone): nothing hurts it but arriving inside it. The blow that kills it
// ends the game at once: everyone is taken to the first intermission spot to watch, no button
// leaves, and the boss tears itself apart over the next eight seconds.

namespace {

// Fifty pieces from all through its body (a grid 32 apart, five by five across, at two heights),
// each the first piece three times in ten, the second three, the third four, torn away as fast as
// a piece can fly.
void oldone_gibs(A& q) {
  for (int zi = 0; zi < 2; zi++) {
    const float z = 16 + 96 * static_cast<float>(zi);
    for (int xi = 0; xi < 5; xi++) {
      const float x = -64 + 32 * static_cast<float>(xi);
      for (int yi = 0; yi < 5; yi++) {
        const float y = -64 + 32 * static_cast<float>(yi);
        const Vec3 org =
            q.v(q.self(), q.L.f.origin) + fv(x, {1, 0, 0}) + fv(y, {0, 1, 0}) + fv(z, {0, 0, 1});
        const float r = q.random();
        St mdl = s_gib3_mdl;
        if (r < 0.3f)
          mdl = s_gib1_mdl;
        else if (r < 0.6f)
          mdl = s_gib2_mdl;
        throw_gib_at(q, q.s(mdl), org, -999);
      }
    }
  }
}

// The thrash, step by step (0.1 s each): the first fifteen shaking frames three times over, then
// four more, then the burst on the fiftieth. Each step dims or brightens the world's light.
constexpr int kThrashPass = 15, kThrashPasses = 3, kThrashSteps = kThrashPass * kThrashPasses + 5;

// The model's frame for shaking frame n (1..20): its first eleven follow the sway's 46 frames,
// and the frame after the eleventh is never shown.
float thrash_frame(int n) { return static_cast<float>(n <= 11 ? 45 + n : 46 + n); }

// The world's light on shaking frame n (1..19).
St thrash_light(int n) {
  static constexpr St kPass[kThrashPass] = {s_style0,  s_style_k, s_style_k,   s_style_i, s_style_g,
                                            s_style_e, s_style_c, s_style_off, s_style_c, s_style_e,
                                            s_style_g, s_style_i, s_style_k,   s_style0,  s_style0};
  static constexpr St kLast[4] = {s_style_g, s_style_c, s_style_b, s_style_off};
  return n <= kThrashPass ? kPass[n - 1] : kLast[n - kThrashPass - 1];
}

// The burst: the pieces fly, the closing words come, a body is left lying where it stood, and the
// music and the light come back.
void oldone_burst(A& q) {
  const int32_t me = q.self();
  q.sound(me, kChanVoice, q.s(s_boss2_pop2_wav), 1, kAttnNorm);
  oldone_gibs(q);
  q.WriteByte(kMsgAll, kSvcFinale);
  q.WriteString(kMsgAll, q.s(s_the_thing_at_the));
  const int32_t body = q.spawn();
  q.setmodel(body, q.s(s_player_mdl));
  q.setorigin(body, q.v(me, q.L.f.origin) - Vec3{32, 264, 0});
  q.set_v(body, q.L.f.angles, {0, 290, 0});
  q.set_f(body, q.L.f.frame, 1);
  q.set_f(body, q.L.f.solid, kSolidNot);
  q.set_f(body, q.L.f.movetype, kMovetypeNone);
  q.remove(me);
  q.WriteByte(kMsgAll, kSvcCdtrack);
  q.WriteByte(kMsgAll, 3);
  q.WriteByte(kMsgAll, 3);
  q.lightstyle(0, q.s(s_style0));
}

}  // namespace

// One step of the thrash.
void oldone_thrash(A& q) {
  const int32_t me = q.self();
  const int step = static_cast<int>(q.f(me, f_state));
  if (step >= kThrashSteps - 1) {
    oldone_burst(q);
    return;
  }
  const int n = step < kThrashPass * kThrashPasses ? step % kThrashPass + 1 : step - 29;
  q.set_f(me, q.L.f.frame, thrash_frame(n));
  q.lightstyle(0, q.s(thrash_light(n)));
  q.set_f(me, f_state, static_cast<float>(step + 1));
  q.set_f(me, q.L.f.nextthink, q.time() + 0.1f);
}

// A second after the blow, a flash beside it and the sound of something coming through.
void oldone_flash(A& q) {
  const int32_t timer = q.self();
  const int32_t boss = q.i(timer, q.L.f.owner);
  effect_head(q, kTeTeleport);
  write_vec(q, q.v(boss, q.L.f.origin) - Vec3{0, 100, 0});
  q.sound(boss, kChanVoice, q.s(s_r_tele1_wav), 1, kAttnNorm);
  q.set_i(timer, q.L.f.think, q.fn(fn_oldone_wake));
  q.set_f(timer, q.L.f.nextthink, q.time() + 2);
}

// Three seconds after the blow it cries out and starts to thrash on its next frame, the light
// pulsing until the thrash takes it over.
void oldone_wake(A& q) {
  const int32_t timer = q.self();
  const int32_t boss = q.i(timer, q.L.f.owner);
  q.set_f(boss, f_state, 0);
  q.set_i(boss, q.L.f.think, q.fn(fn_oldone_thrash));
  q.sound(boss, kChanVoice, q.s(s_boss2_death_wav), 1, kAttnNorm);
  q.lightstyle(0, q.s(s_style_pulse));
  q.remove(timer);
}

// The blow lands: counted, its targets fired, and the game is over on the spot. The boss stays
// where it is, swaying, solid and beyond harm, until the thrash.
void oldone_die(A& q) {
  const int32_t me = q.self();
  q.set_f(me, q.L.f.takedamage, kDamageNo);
  q.set_ge(g_activator, q.ge(g_damage_attacker));
  use_targets(q);
  q.add_to(q.L.g.killed_monsters, 1);
  q.WriteByte(kMsgAll, kSvcKilledmonster);
  q.set_g(g_intermission_exit, q.time() + 10000000);  // never: the game has ended
  q.set_g(g_intermission_running, 1);
  q.set_g(g_match_over, 1);
  int32_t spot = q.find(q.world(), q.L.f.classname, q.s(s_info_intermission));
  if (spot == 0) spot = q.find(q.world(), q.L.f.classname, q.s(s_info_player_start));
  const int32_t ball = q.find(q.world(), q.L.f.classname, q.s(s_misc_teleporttrain_class));
  if (ball != 0) q.remove(ball);
  q.WriteByte(kMsgAll, kSvcFinale);
  q.WriteString(kMsgAll, q.s(s_empty));
  for (int32_t p = q.find(q.world(), q.L.f.classname, q.s(s_player)); p != 0;
       p = q.find(p, q.L.f.classname, q.s(s_player))) {
    q.set_v(p, q.L.f.view_ofs, {0, 0, 0});
    q.set_v(p, q.L.f.angles, q.v(spot, f_mangle));
    q.set_v(p, q.L.f.v_angle, q.v(spot, f_mangle));
    q.set_f(p, q.L.f.fixangle, 1);
    q.set_f(p, q.L.f.nextthink, q.time() + 0.5f);
    q.set_f(p, q.L.f.takedamage, kDamageNo);
    q.set_f(p, q.L.f.solid, kSolidNot);
    q.set_f(p, q.L.f.movetype, kMovetypeNone);
    q.set_v(p, q.L.f.velocity, {0, 0, 0});
    q.set_f(p, q.L.f.modelindex, 0);
    q.setorigin(p, q.v(spot, q.L.f.origin));
  }
  const int32_t timer = q.spawn();
  q.set_i(timer, q.L.f.owner, me);
  q.set_i(timer, q.L.f.think, q.fn(fn_oldone_flash));
  q.set_f(timer, q.L.f.nextthink, q.time() + 1);
}

// Alive, it sways: a frame every 0.1 s.
void oldone_sway(A& q) {
  frame_loop(q, q.v(q.self(), f_fr_stand));
  q.set_f(q.self(), q.L.f.nextthink, q.time() + 0.1f);
}

void monster_oldone(A& q) {
  if (no_monsters_here(q)) return;
  q.precache_model(q.s(s_oldone_mdl));
  precache_sounds(
      q, {s_boss2_death_wav, s_boss2_idle_wav, s_boss2_sight_wav, s_boss2_pop2_wav, s_r_tele1_wav});
  q.add_to(q.L.g.total_monsters, 1);
  const int32_t me = q.self();
  q.set_f(me, q.L.f.flags, bor(q.f(me, q.L.f.flags), kFlMonster));
  q.set_f(me, q.L.f.solid, kSolidSlidebox);
  q.set_f(me, q.L.f.movetype, kMovetypeStep);
  q.setmodel(me, q.s(s_oldone_mdl));
  q.setsize(me, {-160, -128, -24}, {160, 128, 256});
  q.setorigin(me, q.v(me, q.L.f.origin));
  q.set_f(me, q.L.f.health, 40000);
  q.set_f(me, q.L.f.takedamage, kDamageYes);
  q.set_i(me, f_th_hurt, q.fn(fn_noop));  // it does not flinch
  q.set_i(me, f_th_die, q.fn(fn_oldone_die));
  set_frames(q, f_fr_stand, 0, 45);  // its sway
  q.set_i(me, q.L.f.think, q.fn(fn_oldone_sway));
  const float t = q.time() + 0.1f;
  q.set_f(me, q.L.f.nextthink, t + q.random() * 0.5f);
}

}  // namespace shooters_edge::server::rules::campaign
