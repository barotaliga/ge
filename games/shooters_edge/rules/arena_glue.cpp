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

namespace shooters_edge::server::rules::arena {

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
  // Quake's telefrags against the pentagram come first: the one deflected, and both shielded.
  if (quake_rules(q)) {
    if (q.is(how, "deflected")) return say(s_pent_deflect);
    if (q.is(how, "satans power")) return say(s_pent_both);
  }
  if (attacker == targ) {
    // Quake's deathmatch 4 and 5: the arc gun under water that kills only its firer.
    if (quake_rules(q) & q.is(how, "discharged")) return say(s_self_electrocute);
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
      if (gib & quad & quake_rules(q)) {  // Quake's: 30%, 30% and 40%
        const float r = q.random();
        if (r < 0.3f) return say(s_obit_rl_quad_1);
        if (r < 0.6f) return say(s_obit_rl_quad_2);
        return say(s_obit_rl_quad_3);
      }
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
  if (quake_rules(q)) {
    // Quake's: the liquid the body lies in first, whatever the cause; then the trap or the box.
    const float in = q.f(targ, q.L.f.watertype);
    if (in == kContentWater) return pick(s_world_drown);
    if (in == kContentSlime) return pick(s_world_slime);
    if (in == kContentLava) {
      const int32_t key = pick(s_world_lava);
      if (q.f(targ, q.L.f.health) < -15) return say(s_world_lava_gib);
      return key;
    }
    if (q.is(how, "blew up")) return say(s_world_blew_up);
    if (q.is(how, "nailed")) return say(s_world_spiked);
    if (q.is(how, "lasered")) return say(s_world_zapped);
    if (q.is(how, "lavaball")) return say(s_world_lavaball);
    if (q.is(how, "tried to leave")) return say(s_world_left);
  }
  if (q.is(how, "drowned")) return pick(s_world_drown);
  if (q.is(how, "falling")) return pick(s_world_fall);
  if (q.is(how, "dissolved")) return pick(s_world_slime);
  if (q.is(how, "burned")) {
    const int32_t key = pick(s_world_lava);  // drawn before the flames are looked at
    if (q.f(targ, q.L.f.health) < -15) return say(s_world_lava_gib);
    return key;
  }
  if (q.is(how, "squished")) return say(s_world_squish);
  return say(s_world_died);
}

// A dead player leaves a backpack with their ammunition and the weapon they held (sv_droppack, on
// by default), and the Quad Damage, the ring or the hazard suit with the time it had left
// (sv_dropquad, sv_dropring, sv_dropsuit, off by default).
void drop_on_death(A& q) {
  if (on(q.cvar(q.s(s_sv_droppack)))) backpack_drop(q);
  if (quake_rules(q)) {  // Quake's: the server's (or local) info keys `dq` and `dr`
    if (on(number_of(q, q.infokey(q.world(), q.s(s_dq))))) power_drop(q, kItQuad);
    if (on(number_of(q, q.infokey(q.world(), q.s(s_dr))))) power_drop(q, kItInvisibility);
    if (on(q.cvar(q.s(s_sv_dropsuit)))) power_drop(q, kItSuit);
    return;
  }
  if (on(q.cvar(q.s(s_sv_dropquad)))) power_drop(q, kItQuad);
  if (on(q.cvar(q.s(s_sv_dropring)))) power_drop(q, kItInvisibility);
  if (on(q.cvar(q.s(s_sv_dropsuit)))) power_drop(q, kItSuit);
}

// A taken item comes back after its wait: the arena is always a deathmatch.
// Every item comes back; under Quake's rules, not one whose mode keeps it away (`quake_respawn`).
bool items_come_back(A& q) { return !quake_rules(q) || q.f(q.self(), f_respawn_time) >= 0; }

// A weapon is taken and goes as any item: the arena has no coop. In Quake's modes 2, 3 and 5 it
// stays, taken only by those who have not got one.
bool weapon_may_take(A& q, bool held) { return !(quake_rules(q) && quake_weapons_stay(q) && held); }
bool weapon_stays(A& q) { return quake_rules(q) && quake_weapons_stay(q); }

// A plat rises when it is stepped on.
bool plat_waits(A&) { return false; }

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

// void() SetNewParms, SetChangeParms: deathmatch starts everyone fresh; under Quake's rules a
// player alive at the change carries the kit over (quake_change_parms).
void SetNewParms(A& q) { q.set_gf(q.parm(1), 0); }
void SetChangeParms(A& q) {
  if (quake_rules(q)) {
    quake_change_parms(q);
    return;
  }
  q.set_gf(q.parm(1), 0);
}

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

// Every spawn the same: the blade, the scattergun and 25 shells; in deathmatch 4, everything; in
// midair, the launcher.
void give_inventory(A& q) {
  if (midair_on(q)) {
    midair_give(q);
    return;
  }
  if (q.g(g_deathmatch) == 4) {
    dmm4_give(q);
    return;
  }
  const int32_t me = q.self();
  if (quake_rules(q) && quake_carried_kit(q)) return;
  if (quake_rules(q) & (q.g(g_deathmatch) == 5)) {
    // Quake's mode 5: 200 health, the red armour, every weapon, some of each ammunition, and the
    // shield for 3 s.
    q.set_f(me, q.L.f.items,
            kWBlade | kWScatter | kWDouble | kWSpiker | kWHeavySpiker | kWGrenades | kWRockets |
                kWArc | kItArmor3 | kItInvulnerability);
    q.set_f(me, q.L.f.health, 200);
    q.set_f(me, q.L.f.armortype, 0.8f);
    q.set_f(me, q.L.f.armorvalue, 200);
    q.set_f(me, q.L.f.ammo_shells, 30);
    q.set_f(me, q.L.f.ammo_nails, 80);
    q.set_f(me, q.L.f.ammo_rockets, 10);
    q.set_f(me, q.L.f.ammo_cells, 30);
    q.set_f(me, f_invincible_finished, q.time() + 3);
    weapon_set(q, kWRockets);
    return;
  }
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
  if (quake_rules(q)) {
    quake_corpse(q);         // the body stays where it lies
    q.set_gf(q.parm(1), 0);  // and the kit is the default one
  }
  q.set_f(q.self(), q.L.f.deadflag, kDeadRespawnable);
  PutClientInServer(q);
}

// void(entity killer, entity killed) note_frag: the frag log's line.
void note_frag(A& q, int32_t killer, int32_t killed) { q.logfrag(killer, killed); }

// void(entity m) missile_now: a missile made this frame runs once at once (the form's newmis).
void missile_now(A& q, int32_t m) { q.set_newmis(m); }

}  // namespace shooters_edge::server::rules::arena
