// The arena's glue: what the multiplayer form does its own way. Printing takes a level, effects go
// to the multicast buffer, a frag is logged, a missile made this frame runs once at once; teams by
// the user info's `team`; the loaded game's own words (`words`, GE_QC_WORDS) for deaths, items
// and the clock; every spawn fresh, on the next clear deathmatch spawn point; the jump heard.
//
// ge:summary The arena's multiplayer glue in C++: prints, effects, teams, the game's words, spawns
// ge:keywords arena native-arena say_all death_words game_variant same_team spawn_point
// ge:keywords give_inventory player_jump obituary game-words
// ge:skill progs shooter
// ge:test tests/shooters_edge_arena_rules_test.cpp
#include <cstdint>

#include "shooters_edge/rules/arena_decl.hpp"

namespace shooters_edge::server::arena {

namespace {

// A player's pronoun as the game's words say it: "her" for the user info's gender "f".
int32_t pronoun(A& q, int32_t e) {
  if (q.is(q.infokey(e, q.s(s_gender)), "f")) return q.s(s_her);
  return q.s(s_his);
}

}  // namespace

void say_all(A& q, float level, std::initializer_list<int32_t> text) {
  int32_t s[6] = {};
  int k = 0;
  for (int32_t t : text) s[k++] = t;
  q.bprint(level, {s[0], s[1], s[2], s[3], s[4], s[5]});
}

void say_to(A& q, int32_t to, float level, std::initializer_list<int32_t> text) {
  int32_t s[4] = {};
  int k = 0;
  for (int32_t t : text) s[k++] = t;
  q.sprint(to, level, {s[0], s[1], s[2], s[3]});
}

// An item's name in the loaded game's words (the server's local info under its class name);
// `fallback` when the game names none.
int32_t name_of(A& q, int32_t class_name, int32_t fallback) {
  const int32_t s = q.infokey(q.world(), class_name);
  if (q.is(s, "")) return fallback;
  return s;
}

void effect_send(A& q, Vec3 org, float set) { q.multicast(org, set); }

// A puff where a shot hits: blood on something that bleeds, a chip of the wall elsewhere.
void effect_impact(A& q, Vec3 org, float count, float bleeds) {
  effect_head(q, on(bleeds) ? kTeBlood : kTeGunshot);
  q.WriteByte(kMsgEffect, count);
  write_vec(q, org);
  q.multicast(org, kMulticastPvs);
}

// A teammate's blow that does not harm him does not push him either.
bool team_shoves(A&, int32_t, int32_t) { return false; }

// Two players on the same named team while `teamplay` is on: a teammate is not harmed.
bool same_team(A& q, int32_t a, int32_t b) {
  if ((q.g(g_teamplay) == 0) | (a == b)) return false;
  if (!q.is(q.i(a, q.L.f.classname), "player") | !q.is(q.i(b, q.L.f.classname), "player"))
    return false;
  const int32_t t = q.infokey(a, q.s(s_team));
  if (q.is(t, "")) return false;
  return q.same(t, q.infokey(b, q.s(s_team)));
}

int32_t game_words(A& q, int32_t key, int32_t a, int32_t b, int32_t c, int32_t d) {
  return q.words(key, a, b, c, d);
}

// One of the variants `stem`_1 .. `stem`_n drawn at random (one draw, only when there are two or
// more); `stem` itself when it has none.
int32_t game_variant(A& q, int32_t stem, int32_t a, int32_t b, int32_t c, int32_t d) {
  auto numbered = [&](float n) { return q.strcat({stem, q.s(s_underscore), q.ftos(n)}); };
  float n = 0;
  for (;;) {
    const bool below = n < 9;
    const bool named = !q.is(q.words(numbered(n + 1), 0, 0, 0, 0), "");  // asked either way
    if (!(below & named)) break;
    n = n + 1;
  }
  if (n == 0) return q.words(stem, a, b, c, d);
  float k = 0;
  if (n > 1) {
    k = q.floor(q.random() * n);
    if (k >= n) k = n - 1;
  }
  return q.words(numbered(k + 1), a, b, c, d);
}

bool pellets_as_one_blow() { return false; }

// Before the match starts (the lobby's warmup and countdown) a game that asks for it tells no
// death and no leaving, and scores none.
bool game_quiet(A& q) {
  if (q.is(q.words(q.s(s_quiet_warmup), 0, 0, 0, 0), "")) return false;
  const int32_t phase = q.infokey(q.world(), q.s(s_ge_phase));
  return q.is(phase, "warmup") | q.is(phase, "countdown");
}

// A death in the loaded game's words: by whom, with what, how badly ("" when it names none).
int32_t death_words(A& q, int32_t targ, int32_t attacker) {
  if (q.is(q.words(q.s(s_obit_tele), 0, 0, 0, 0), "")) return 0;
  const int32_t how = q.i(targ, f_deathtype);
  const int32_t v = q.i(targ, q.L.f.netname);
  const int32_t k = q.i(attacker, q.L.f.netname);
  const int32_t hv = pronoun(q, targ);
  const int32_t hk = pronoun(q, attacker);
  const bool gib = q.f(targ, q.L.f.health) < -40;
  auto say = [&](St key) { return q.words(q.s(key), v, k, hv, hk); };
  auto pick = [&](St stem) { return game_variant(q, q.s(stem), v, k, hv, hk); };
  if (attacker == targ) {
    if (q.is(how, "gibbed by a grenade")) return say(s_self_gl);
    if (q.is(how, "blown apart")) return pick(s_self_rl);
    if (q.is(how, "electrocuted")) {
      const float c = q.pointcontents(q.v(targ, q.L.f.origin));
      if (c == kContentSlime) return say(s_self_discharge_slime);
      if (c == kContentLava) return say(s_self_discharge_lava);
      return pick(s_self_discharge_water);
    }
    if (q.is(how, "squished")) return say(s_self_squish);
    if (q.is(how, "suicides")) return say(s_self_kill);
    return say(s_self_other);
  }
  if (q.is(q.i(attacker, q.L.f.classname), "player")) {
    if (same_team(q, targ, attacker)) {
      if (q.is(how, "telefragged")) return say(s_tk_tele);
      if (q.is(how, "squished")) return say(s_tk_squish);
      return pick(s_tk);
    }
    const bool quad = q.f(attacker, f_super_damage_finished) > q.time();
    if (q.is(how, "cut down")) return say(s_obit_axe);
    if (q.is(how, "shot")) {
      if (q.f(attacker, q.L.f.weapon) == kWDouble) {
        if (quad) return say(s_obit_ssg_quad);
        return say(s_obit_ssg);
      }
      if (gib) return say(s_obit_sg_gib);
      return say(s_obit_sg);
    }
    if (q.is(how, "nailed")) {
      if (q.f(attacker, q.L.f.weapon) == kWHeavySpiker) {
        const int32_t key = pick(s_obit_sng);  // drawn even when the gib's line wins
        if (gib) return say(s_obit_sng_gib);
        return key;
      }
      return pick(s_obit_ng);
    }
    if (q.is(how, "gibbed by a grenade")) {
      if (gib) return say(s_obit_gl_gib);
      return say(s_obit_gl);
    }
    if (q.is(how, "blown apart")) {
      if (gib & quad) return pick(s_obit_rl_quad);
      if (gib) return say(s_obit_rl_gib);
      return say(s_obit_rl);
    }
    if (q.is(how, "shafted")) {
      if (gib) return say(s_obit_lg_gib);
      return say(s_obit_lg);
    }
    if (q.is(how, "electrocuted")) return pick(s_obit_discharge);
    if (q.is(how, "telefragged")) return say(s_obit_tele);
    if (q.is(how, "squished")) return say(s_obit_squish);
    return say(s_obit_other);
  }
  if (q.is(how, "drowned")) return pick(s_world_drown);
  if (q.is(how, "dissolved")) return pick(s_world_slime);
  if (q.is(how, "burned")) {
    const int32_t key = pick(s_world_lava);  // drawn before the flames are looked at
    if (q.f(targ, q.L.f.health) < -15) return say(s_world_lava_gib);
    return key;
  }
  if (q.is(how, "squished")) return say(s_world_squish);
  return say(s_world_died);
}

// A dead player leaves a backpack with his ammunition and the weapon he held.
void drop_on_death(A& q) { backpack_drop(q); }

// A taken item comes back after its wait: the arena is always a deathmatch.
bool items_come_back() { return true; }

// The multiplayer form's move jumps for the program; the program only hears it.
void player_jump(A& q) {
  const int32_t me = q.self();
  const float flags = q.f(me, q.L.f.flags);
  if (q.f(me, q.L.f.button2) == 0) {
    q.set_f(me, q.L.f.flags, bor(flags, kFlJumpreleased));
    return;
  }
  if (q.f(me, q.L.f.waterlevel) >= 2) {
    swim_sound(q);
    return;
  }
  if ((band(flags, kFlOnground) == 0) | (band(flags, kFlJumpreleased) == 0)) return;
  q.set_f(me, q.L.f.flags, flags - kFlJumpreleased);
  jump_sound(q);
}

// void() SetNewParms, SetChangeParms: deathmatch starts everyone fresh.
void SetNewParms(A& q) { q.set_gf(q.parm(1), 0); }
void SetChangeParms(A& q) { q.set_gf(q.parm(1), 0); }

// The next deathmatch spawn point after the last one used whose spot is clear of players, round
// the map's list; any one when all are taken; the start point on a map without any.
int32_t spawn_point(A& q) {
  int32_t spot = q.ge(g_last_spawn);
  for (int tries = 0; tries < 64; tries++) {  // the QC counts in floats: the same 64
    spot = q.find(spot, q.L.f.classname, q.s(s_info_player_deathmatch));
    if (spot == 0) spot = q.find(q.world(), q.L.f.classname, q.s(s_info_player_deathmatch));
    if (spot == 0) break;
    bool clear = true;
    for (int32_t p = q.findradius(q.v(spot, q.L.f.origin), 84); p != 0; p = q.i(p, q.L.f.chain)) {
      if (q.is(q.i(p, q.L.f.classname), "player") & (q.f(p, q.L.f.health) > 0)) clear = false;
    }
    if (clear) {
      q.set_ge(g_last_spawn, spot);
      return spot;
    }
  }
  if (spot != 0) return spot;
  spot = q.find(q.world(), q.L.f.classname, q.s(s_info_player_start));
  if (spot == 0) q.error({q.s(s_no_spawn_point)});
  return spot;
}

// Every spawn the same: the blade, the scattergun and 25 shells; in deathmatch 4, everything.
void give_inventory(A& q) {
  if (q.g(g_deathmatch) == 4) {
    dmm4_give(q);
    return;
  }
  const int32_t me = q.self();
  q.set_f(me, q.L.f.items, kWBlade | kWScatter);
  q.set_f(me, q.L.f.ammo_shells, 25);
  q.set_f(me, q.L.f.ammo_nails, 0);
  q.set_f(me, q.L.f.ammo_rockets, 0);
  q.set_f(me, q.L.f.ammo_cells, 0);
  q.set_f(me, q.L.f.armortype, 0);
  q.set_f(me, q.L.f.armorvalue, 0);
  weapon_set(q, kWScatter);
}

void player_respawn(A& q) {
  q.set_f(q.self(), q.L.f.deadflag, kDeadRespawnable);
  PutClientInServer(q);
}

}  // namespace shooters_edge::server::arena
