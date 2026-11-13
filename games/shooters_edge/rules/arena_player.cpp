// The arena's players: coming in, spawning, the frame around each command (dying, drowning,
// burning, respawning, firing), pain and death, the body's cycles, the jump and the swim heard,
// the kill command and leaving.
//
// ge:summary The arena's player rules in C++: spawning, the frame round a command, pain, death
// ge:keywords arena native-arena putclientinserver playerprethink playerpostthink player_die
// ge:keywords player_pain clientkill clientdisconnect player_body drowning
// ge:skill progs shooter
// ge:test tests/shooters_edge_arena_rules_test.cpp
#include <cstdint>

#include "shooters_edge/rules/arena_decl.hpp"

namespace shooters_edge::server::arena {

namespace {

// A dead player comes back when he presses attack, jump or use after a second, or after five.
void player_dead_frame(A& q) {
  const int32_t me = q.self();
  const float ready = q.f(me, f_respawn_ready);
  if (q.time() < ready) return;
  if ((q.f(me, q.L.f.button0) != 0) | (q.f(me, q.L.f.button1) != 0) |
      (q.f(me, q.L.f.button2) != 0) | (q.time() > ready + 4)) {
    player_respawn(q);
  }
}

// Drowning, and lava and slime.
void player_water(A& q) {
  const int32_t me = q.self();
  if (q.f(me, q.L.f.waterlevel) == 3) {
    if ((q.f(me, f_air_finished) < q.time()) & (q.f(me, f_pain_finished) < q.time())) {
      q.set_i(me, f_deathtype, q.s(s_drowned));
      damage(q, me, q.world(), q.world(), 4);
      q.set_f(q.self(), f_pain_finished, q.time() + 1);
    }
  } else {
    q.set_f(me, f_air_finished, q.time() + 12);
  }
  const int32_t s = q.self();
  const float level = q.f(s, q.L.f.waterlevel);
  const float type = q.f(s, q.L.f.watertype);
  const bool due = q.f(s, f_hazard_finished) < q.time();
  if ((level != 0) & (type == kContentLava) & due) {
    q.set_f(s, f_hazard_finished, q.time() + 0.2f);
    q.set_i(s, f_deathtype, q.s(s_burned));
    damage(q, s, q.world(), q.world(), 10 * q.f(s, q.L.f.waterlevel));
  } else if ((level != 0) & (type == kContentSlime) & due) {
    q.set_f(s, f_hazard_finished, q.time() + 1);
    q.set_i(s, f_deathtype, q.s(s_dissolved));
    damage(q, s, q.world(), q.world(), 4 * q.f(s, q.L.f.waterlevel));
  }
}

// The player model's cycles, a frame every 0.1 s: running (0-5 with the blade, 6-11 with any
// other weapon) while he moves across the plan, standing (17-28 with the blade, 12-16 otherwise)
// when he does not; a change of cycle starts the new one at its first frame.
void player_body(A& q) {
  const int32_t me = q.self();
  if (q.time() < q.f(me, f_body_next)) return;
  q.set_f(me, f_body_next, q.time() + 0.1f);
  const Vec3 vel = q.v(me, q.L.f.velocity);
  float first;
  float count;
  if ((vel.x != 0) | (vel.y != 0)) {
    count = 6;
    first = q.f(me, q.L.f.weapon) == kWBlade ? 0 : 6;
  } else if (q.f(me, q.L.f.weapon) == kWBlade) {
    first = 17;
    count = 12;
  } else {
    first = 12;
    count = 5;
  }
  if ((first != q.f(me, f_body_first)) | (q.f(me, f_body_step) >= count)) {
    q.set_f(me, f_body_first, first);
    q.set_f(me, f_body_step, 0);
  }
  q.set_f(me, q.L.f.frame, first + q.f(me, f_body_step));
  q.set_f(me, f_body_step, q.f(me, f_body_step) + 1);
}

}  // namespace

// void() ClientConnect.
void ClientConnect(A& q) {
  say_all(q, kPrintHigh, {q.i(q.self(), q.L.f.netname), q.s(s_entered_the_game)});
}

// void() PutClientInServer: a fresh player on the next clear spawn point.
void PutClientInServer(A& q) {
  const int32_t me = q.self();
  q.set_i(me, q.L.f.classname, q.s(s_player));
  q.set_f(me, q.L.f.health, 100);
  q.set_f(me, q.L.f.max_health, 100);
  q.set_f(me, q.L.f.takedamage, kDamageAim);
  q.set_f(me, q.L.f.solid, kSolidSlidebox);
  q.set_f(me, q.L.f.movetype, kMovetypeWalk);
  q.set_f(me, q.L.f.flags, kFlClient);
  q.set_f(me, q.L.f.deadflag, kDeadNo);
  q.set_f(me, q.L.f.effects, 0);
  q.set_f(me, f_super_damage_finished, 0);
  q.set_f(me, f_invincible_finished, 0);
  q.set_f(me, f_power_said, 0);
  q.set_f(me, f_attack_finished, q.time());
  q.set_f(me, f_pain_finished, 0);
  q.set_f(me, f_air_finished, q.time() + 12);
  q.set_f(me, q.L.f.dmg_take, 0);
  q.set_f(me, q.L.f.dmg_save, 0);
  q.set_i(me, f_deathtype, 0);
  q.set_f(me, f_maxspeed, 320);
  q.set_f(me, f_gravity, 1);
  q.set_f(me, f_spawn_time, q.time());
  give_inventory(q);
  const int32_t spot = spawn_point(q);
  const int32_t s = q.self();
  q.set_v(s, q.L.f.origin, q.v(spot, q.L.f.origin) + Vec3{0, 0, 1});
  q.set_v(s, q.L.f.angles, q.v(spot, q.L.f.angles));
  q.set_f(s, q.L.f.fixangle, 1);
  q.set_v(s, q.L.f.velocity, {0, 0, 0});
  q.set_v(s, q.L.f.view_ofs, {0, 0, 22});
  q.setmodel(s, q.s(s_player_mdl));
  q.setsize(q.self(), {-16, -16, -24}, {16, 16, 32});
  q.setorigin(q.self(), q.v(q.self(), q.L.f.origin));
  if (on(q.g(g_deathmatch))) {
    // A deathmatch spawn is heard, and it takes the spot, for when every spawn point is taken.
    q.sound(q.self(), kChanVoice, q.s(s_r_tele1_wav), 1, kAttnNorm);
    telefrag(q, q.self());
  }
  if (on(q.g(g_match_over))) {
    q.set_f(q.self(), q.L.f.takedamage, kDamageNo);
    q.set_f(q.self(), q.L.f.movetype, kMovetypeNone);
  }
}

void player_pain(A& q) {
  const int32_t me = q.self();
  if (q.time() < q.f(me, f_pain_finished)) return;
  q.set_f(me, f_pain_finished, q.time() + 0.5f);
  if (q.is(q.i(me, f_deathtype), "drowned")) {
    q.sound(me, kChanVoice, q.s(s_drown1_wav), 1, kAttnNorm);
  } else {
    q.sound(me, kChanVoice, q.s(s_pain1_wav), 1, kAttnNorm);
  }
}

void player_die(A& q) {
  const int32_t me = q.self();
  q.set_f(me, q.L.f.deadflag, kDeadDead);
  q.set_f(me, q.L.f.takedamage, kDamageNo);
  q.set_f(me, q.L.f.solid, kSolidNot);
  q.set_f(me, q.L.f.movetype, kMovetypeToss);
  const float flags = q.f(me, q.L.f.flags);
  q.set_f(me, q.L.f.flags, flags - band(flags, kFlOnground));
  q.set_v(me, q.L.f.view_ofs, {0, 0, -8});
  Vec3 vel = q.v(me, q.L.f.velocity);
  vel.z = vel.z + 160;
  q.set_v(me, q.L.f.velocity, vel);
  q.set_i(me, q.L.f.weaponmodel, 0);
  q.set_f(me, q.L.f.effects, 0);
  q.set_f(me, f_super_damage_finished, 0);
  q.set_f(me, f_invincible_finished, 0);
  const float items = q.f(me, q.L.f.items);
  q.set_f(me, q.L.f.items, items - band(items, kItQuad | kItInvulnerability));
  drop_on_death(q);
  const int32_t s = q.self();
  q.set_f(s, f_respawn_ready, q.time() + 1);
  if (q.f(s, q.L.f.health) < -40) {
    q.sound(s, kChanVoice, q.s(s_udeath_wav), 1, kAttnNone);
  } else {
    q.sound(s, kChanVoice, q.s(s_death1_wav), 1, kAttnNorm);
  }
}

// A jump leaving the ground: its sound on the body.
void jump_sound(A& q) { q.sound(q.self(), kChanBody, q.s(s_plyrjmp8_wav), 1, kAttnNorm); }

// Swimming up: a splash, one of two, at most once a second.
void swim_sound(A& q) {
  const int32_t me = q.self();
  if (q.time() < q.f(me, f_splash_next)) return;
  q.set_f(me, f_splash_next, q.time() + 1);
  if (q.random() < 0.5f) {
    q.sound(me, kChanBody, q.s(s_water1_wav), 1, kAttnNorm);
  } else {
    q.sound(me, kChanBody, q.s(s_water2_wav), 1, kAttnNorm);
  }
}

// void() PlayerPreThink.
void PlayerPreThink(A& q) {
  const int32_t me = q.self();
  const float effects = q.f(me, q.L.f.effects);
  q.set_f(me, q.L.f.effects, effects - band(effects, kEfMuzzleflash));
  if (on(q.g(g_match_over))) return;
  if (q.f(me, q.L.f.deadflag) != kDeadNo) {
    player_dead_frame(q);
    return;
  }
  player_water(q);
  player_body(q);
  player_jump(q);
}

// void() PlayerPostThink: a weapon select waits out the refire; the weapon fired; the powers.
void PlayerPostThink(A& q) {
  const int32_t me = q.self();
  if ((q.f(me, q.L.f.deadflag) != kDeadNo) | (q.g(g_match_over) != 0)) return;
  if ((q.f(me, q.L.f.impulse) != 0) & (q.time() >= q.f(me, f_attack_finished))) weapon_impulse(q);
  weapon_frame(q);
  power_frame(q);
  q.set_f(q.self(), q.L.f.dmg_take, 0);
}

// void() ClientKill: the kill command, a second after a spawn at the earliest.
void ClientKill(A& q) {
  const int32_t me = q.self();
  if ((q.f(me, q.L.f.deadflag) != kDeadNo) | (q.g(g_match_over) != 0)) return;
  if (q.time() < q.f(me, f_spawn_time) + 1) return;
  q.set_i(me, f_deathtype, q.s(s_suicides));
  q.set_f(me, q.L.f.armorvalue, 0);
  damage(q, me, me, me, 1000);
}

// void() ClientDisconnect: told in the loaded game's words (`left`) when it has them, and then
// only in the match.
void ClientDisconnect(A& q) {
  const int32_t me = q.self();
  const int32_t frags = q.ftos(q.f(me, q.L.f.frags));
  const int32_t told = game_words(q, q.s(s_left), q.i(me, q.L.f.netname), frags, 0, 0);
  if (q.is(told, "")) {
    say_all(q, kPrintHigh,
            {q.i(me, q.L.f.netname), q.s(s_left_the_game_with), q.ftos(q.f(me, q.L.f.frags)),
             q.s(s_frags_nl)});
    q.sound(me, kChanBody, q.s(s_tornoff2_wav), 1, kAttnNone);
  } else if (!game_quiet(q)) {
    say_all(q, kPrintHigh, {told});
    q.sound(me, kChanBody, q.s(s_tornoff2_wav), 1, kAttnNone);
  }
  q.set_i(me, q.L.f.classname, q.s(s_disconnected));
  q.set_f(me, q.L.f.takedamage, kDamageNo);
  q.set_f(me, q.L.f.solid, kSolidNot);
}

void player_precache(A& q) {
  q.precache_model(q.s(s_player_mdl));
  for (St s : {s_pain1_wav, s_drown1_wav, s_death1_wav, s_udeath_wav, s_tornoff2_wav,
               s_plyrjmp8_wav, s_water1_wav, s_water2_wav, s_r_item1_wav})
    q.precache_sound(q.s(s));
}

}  // namespace shooters_edge::server::arena
