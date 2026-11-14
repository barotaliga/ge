// The arena's damage and death: what a hit does (armour first, then health, a push away from the
// blow), a blast to everything near it, a telefrag, and how a death is scored and told. A thing
// with its own `th_hurt` and `th_die` (a door, a button) answers the hit itself; a player's are
// arena_player's.
//
// ge:summary The arena's damage, blasts, telefrags and the scoring of a death in C++
// ge:keywords arena native-arena damage radius_damage telefrag score_death blow_push armour
// ge:skill progs shooter
// ge:test tests/shooters_edge_arena_rules_test.cpp
#include <cstdint>

#include "shooters_edge/rules/variant.hpp"

namespace shooters_edge::server::rules::SE_RULES_NS {

// score_death(targ, attacker): the frags a death is worth (+1 a kill, -1 to the killer for a
// teammate, -1 for one's own hand or the world's), told in the loaded game's words when it has
// them, else ours; nothing before the match when the game asks for quiet.
void score_death(A& q, int32_t targ, int32_t attacker) {
  if (!q.is(q.i(targ, q.L.f.classname), "player")) return;
  if (game_quiet(q)) return;
  int32_t how = q.i(targ, f_deathtype);
  const int32_t told = death_words(q, targ, attacker);
  if (attacker == targ) {
    q.set_f(targ, q.L.f.frags, q.f(targ, q.L.f.frags) - 1);
    if (!q.is(told, "")) {
      say_all(q, kPrintMedium, {told});
    } else {
      say_all(q, kPrintMedium, {q.i(targ, q.L.f.netname), q.s(s_fragged_themselves)});
    }
    return;
  }
  if (q.is(q.i(attacker, q.L.f.classname), "player")) {
    if (same_team(q, targ, attacker)) {
      q.set_f(attacker, q.L.f.frags, q.f(attacker, q.L.f.frags) - 1);
    } else {
      q.set_f(attacker, q.L.f.frags, q.f(attacker, q.L.f.frags) + 1);
    }
    note_frag(q, attacker, targ);
    if (!q.is(told, "")) {
      say_all(q, kPrintMedium, {told});
      return;
    }
    if (q.is(how, "")) how = q.s(s_fragged);
    say_all(q, kPrintMedium,
            {q.i(targ, q.L.f.netname), q.s(s_was), how, q.s(s_by), q.i(attacker, q.L.f.netname),
             q.s(s_nl)});
    return;
  }
  q.set_f(targ, q.L.f.frags, q.f(targ, q.L.f.frags) - 1);
  if (!q.is(told, "")) {
    say_all(q, kPrintMedium, {told});
    return;
  }
  if (q.is(how, "")) how = q.s(s_died);
  say_all(q, kPrintMedium, {q.i(targ, q.L.f.netname), q.s(s_space), how, q.s(s_nl)});
}

// blow_push(targ, inflictor, damage): away from the blow's centre, `damage` x 8.
void blow_push(A& q, int32_t targ, int32_t inflictor, float damage) {
  if ((inflictor == q.world()) | (q.f(targ, q.L.f.movetype) != kMovetypeWalk)) return;
  Vec3 dir = q.v(targ, q.L.f.origin) -
             (q.v(inflictor, q.L.f.absmin) + q.v(inflictor, q.L.f.absmax)) * 0.5f;
  dir = q.normalize(dir);
  q.set_v(targ, q.L.f.velocity, q.v(targ, q.L.f.velocity) + dir * damage * 8);
}

// damage(targ, inflictor, attacker, damage): `damage` points from `attacker` through `inflictor`.
void damage(A& q, int32_t targ, int32_t inflictor, int32_t attacker, float points) {
  float amount = points;
  if ((q.f(targ, q.L.f.takedamage) == 0) | (q.f(targ, q.L.f.health) <= 0)) return;
  if (on(q.g(g_match_over))) return;
  if (q.f(targ, f_invincible_finished) > q.time()) return;  // the shield
  {
    const bool not_tele = q.g(g_telefragging) == 0;
    const bool mates = same_team(q, targ, attacker);  // asked either way: no short circuit
    if (not_tele & mates) {
      if (team_shoves(q, targ, attacker)) blow_push(q, targ, inflictor, amount);
      return;
    }
  }
  if (q.f(attacker, f_super_damage_finished) > q.time()) {  // the quad (deathmatch 4: eight)
    if (q.deathmatch() == 4) {
      amount = amount * 8;
    } else {
      amount = amount * 4;
    }
  }
  // Armour takes its share, as far as it lasts.
  float saved = q.ceil(q.f(targ, q.L.f.armortype) * amount);
  if (saved >= q.f(targ, q.L.f.armorvalue)) {
    saved = q.f(targ, q.L.f.armorvalue);
    q.set_f(targ, q.L.f.armortype, 0);
    const float items = q.f(targ, q.L.f.items);
    q.set_f(targ, q.L.f.items, items - band(items, kItArmorAll));
  }
  q.set_f(targ, q.L.f.armorvalue, q.f(targ, q.L.f.armorvalue) - saved);
  const float taken = q.ceil(amount - saved);
  q.set_f(targ, q.L.f.dmg_take, q.f(targ, q.L.f.dmg_take) + taken);
  q.set_f(targ, q.L.f.dmg_save, q.f(targ, q.L.f.dmg_save) + saved);
  q.set_i(targ, q.L.f.dmg_inflictor, inflictor);
  blow_push(q, targ, inflictor, amount);
  // Deathmatch 4's spawn protection: the armour wore and the blow pushed, the health is spared.
  if ((q.f(targ, f_shield_finished) > q.time()) & (q.g(g_telefragging) == 0)) {
    dmm4_soaked(q, targ);
    return;
  }
  q.set_f(targ, q.L.f.health, q.f(targ, q.L.f.health) - taken);
  q.set_ge(g_damage_attacker, attacker);
  const int32_t was_self = q.self();
  q.set_self(targ);
  if (q.f(targ, q.L.f.health) <= 0) {
    score_death(q, targ, attacker);
    const int32_t die = q.i(q.self(), f_th_die);
    if (die != 0) {
      q.call(die);
    } else {
      player_die(q);
    }
  } else {
    const int32_t hurt = q.i(q.self(), f_th_hurt);
    if (hurt != 0) {
      q.call(hurt);
    } else {
      player_pain(q);
    }
  }
  q.set_self(was_self);
}

// telefrag(who): `who` takes the spot: a living player there dies, teammate or not, unless a
// shield holds him, when `who` dies instead.
void telefrag(A& q, int32_t who) {
  int32_t p = q.findradius(q.v(who, q.L.f.origin), 48);
  while (p != 0) {
    const int32_t next = q.i(p, q.L.f.chain);  // a death may search the world again
    if ((p != who) & q.is(q.i(p, q.L.f.classname), "player") & (q.f(p, q.L.f.health) > 0)) {
      q.set_g(g_telefragging, 1);
      if ((q.deathmatch() == 4) & (q.f(p, f_shield_finished) > q.time())) {
        // Deathmatch 4 onto a protected player: both protected, both die; else `who` does.
        if (q.f(who, f_shield_finished) > q.time()) {
          q.set_f(p, f_shield_finished, 0);
          q.set_i(p, f_deathtype, q.s(s_telefragged));
          damage(q, p, q.world(), q.world(), 50000);
        }
        q.set_f(who, f_shield_finished, 0);
        q.set_i(who, f_deathtype, q.s(s_telefragged));
        damage(q, who, q.world(), q.world(), 50000);
        q.set_g(g_telefragging, 0);
        return;
      }
      if (q.f(p, f_invincible_finished) > q.time()) {
        q.set_i(who, f_deathtype, q.s(s_telefragged));
        damage(q, who, p, p, 50000);
        q.set_g(g_telefragging, 0);
        return;
      }
      q.set_i(p, f_deathtype, q.s(s_telefragged));
      damage(q, p, who, who, 50000);
      q.set_g(g_telefragging, 0);
    }
    p = next;
  }
}

// radius_damage(inflictor, attacker, points, ignore, how): a blast at `inflictor`: `points` less
// half the distance to each thing's centre (the attacker half that, a thing with a blast_share
// that share).
void radius_damage(A& q, int32_t inflictor, int32_t attacker, float points, int32_t ignore,
                   int32_t how) {
  int32_t e = q.findradius(q.v(inflictor, q.L.f.origin), points + 40);
  while (e != 0) {
    if ((e != ignore) & (q.f(e, q.L.f.takedamage) != 0)) {
      const Vec3 centre = q.v(e, q.L.f.origin) + (q.v(e, q.L.f.mins) + q.v(e, q.L.f.maxs)) * 0.5f;
      float d = points - 0.5f * q.vlen(q.v(inflictor, q.L.f.origin) - centre);
      if (e == attacker) d = d * 0.5f;
      if (on(q.f(e, f_blast_share))) d = d * q.f(e, f_blast_share);
      if (d > 0) {
        q.set_i(e, f_deathtype, how);
        damage(q, e, inflictor, attacker, d);
      }
    }
    e = q.i(e, q.L.f.chain);
  }
}

}  // namespace shooters_edge::server::rules::SE_RULES_NS
