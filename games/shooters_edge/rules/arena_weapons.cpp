// The arena's weapons: eight slots, each a bit in .items when carried and in .weapon when held,
// filled by the ARSENAL, one row of words a slot (games/shooters_edge/profiles/arsenal.hpp) that
// says how the slot fires and with what numbers, models and sounds. The game puts its rows in the
// server's settings (`arsenal_weapon1` .. `arsenal_weapon8`); a slot with none keeps our own row.
// A row's words: 0 fire, 1 ammo, 2 cost, 3 refire, 4 count, 5 damage, 6 spread, 7 reach or speed,
// 8 life, 9 splash, 10 view model, 11 fire sound, 12 missile model, 13 land sound, 14 blast
// sound, 15 pickup model, 16 pickup ammunition ("-" is none).
//
// ge:summary The arena's arsenal, weapon choice and firing (pellets, blade, spikes, grenades,
// ge:summary rockets, the arc) in C++
// ge:keywords arena native-arena arsenal weapon_frame fire_pellets fire_rocket fire_arc
// ge:keywords weapon_select_best weapon_impulse weapon_precache
// ge:skill progs shooter
// ge:test tests/shooters_edge_arena_rules_test.cpp
#include <cstdint>

#include "shooters_edge/rules/arena_decl.hpp"

namespace shooters_edge::server::arena {

namespace {

// Our own rows: what a slot holds when the game gives none.
int32_t weapon_own_row(A& q, float slot) {
  if (slot == 1) return q.s(s_row1);
  if (slot == 2) return q.s(s_row2);
  if (slot == 3) return q.s(s_row3);
  if (slot == 4) return q.s(s_row4);
  if (slot == 5) return q.s(s_row5);
  if (slot == 6) return q.s(s_row6);
  if (slot == 7) return q.s(s_row7);
  return q.s(s_row8);
}

// The setting that holds slot `slot`'s row.
int32_t arsenal_key(A& q, float slot) {
  if (slot == 1) return q.s(s_arsenal_weapon1);
  if (slot == 2) return q.s(s_arsenal_weapon2);
  if (slot == 3) return q.s(s_arsenal_weapon3);
  if (slot == 4) return q.s(s_arsenal_weapon4);
  if (slot == 5) return q.s(s_arsenal_weapon5);
  if (slot == 6) return q.s(s_arsenal_weapon6);
  if (slot == 7) return q.s(s_arsenal_weapon7);
  return q.s(s_arsenal_weapon8);
}

// The slot's three kept strings (row, view model, missile model) are globals in slot order.
Gl row_global(int slot) { return static_cast<Gl>(g_arsenal_row1 + slot - 1); }
Gl view_global(int slot) { return static_cast<Gl>(g_arsenal_view1 + slot - 1); }
Gl missile_global(int slot) { return static_cast<Gl>(g_arsenal_missile1 + slot - 1); }

// Slot `slot`'s row from the game's setting, else ours, kept (with its two models).
void arsenal_keep(A& q, float slot) {
  int32_t row = q.cvar_string(arsenal_key(q, slot));
  if (q.is(row, "")) row = weapon_own_row(q, slot);
  row = q.strzone(row);
  q.tokenize(row);
  const int32_t view = q.strzone(q.argv(10));
  const int32_t missile = q.strzone(q.argv(12));
  for (int k = 1; k <= 8; k++) {
    if (slot != static_cast<float>(k)) continue;
    q.set_ge(row_global(k), row);
    q.set_ge(view_global(k), view);
    q.set_ge(missile_global(k), missile);
  }
}

float weapon_of_rank(float rank) {
  if (rank == 1) return kWBlade;
  if (rank == 2) return kWScatter;
  if (rank == 3) return kWDouble;
  if (rank == 4) return kWSpiker;
  if (rank == 5) return kWHeavySpiker;
  if (rank == 6) return kWGrenades;
  if (rank == 7) return kWRockets;
  if (rank == 8) return kWArc;
  return 0;
}

// The slot (1..8) a weapon bit's strings are kept in; 8 for any other value, as the rows read.
int kept_slot(float w) {
  const float r = weapon_rank(w);
  for (int k = 1; k <= 7; k++)
    if (r == static_cast<float>(k)) return k;
  return 8;
}

int32_t weapon_row(A& q, float w) { return q.ge(row_global(kept_slot(w))); }
int32_t weapon_model(A& q, float w) { return q.ge(view_global(kept_slot(w))); }
int32_t weapon_missile(A& q, float w) { return q.ge(missile_global(kept_slot(w))); }

float weapon_cost(A& q, float w) { return weapon_number(q, w, 2); }
float weapon_delay(A& q, float w) { return weapon_number(q, w, 3); }

// A sound of weapon `w` (word `i`) played on `e`, when its row has one.
void weapon_sound(A& q, int32_t e, float w, float i) {
  const int32_t snd = weapon_word(q, w, i);
  if (!q.is(snd, "-")) q.sound(e, kChanWeapon, snd, 1, kAttnNorm);
}

float ammo_bit(A& q, float w) {
  const int32_t kind = weapon_word(q, w, 1);
  if (q.is(kind, "shells")) return kItShells;
  if (q.is(kind, "nails")) return kItNails;
  if (q.is(kind, "rockets")) return kItRockets;
  if (q.is(kind, "cells")) return kItCells;
  return 0;
}

void ammo_spend(A& q, float w, float n) {
  if (q.g(g_deathmatch) == 4) return;  // never used up in deathmatch 4
  const int32_t kind = weapon_word(q, w, 1);
  const int32_t me = q.self();
  if (q.is(kind, "shells")) {
    q.set_f(me, q.L.f.ammo_shells, q.f(me, q.L.f.ammo_shells) - n);
  } else if (q.is(kind, "nails")) {
    q.set_f(me, q.L.f.ammo_nails, q.f(me, q.L.f.ammo_nails) - n);
  } else if (q.is(kind, "rockets")) {
    q.set_f(me, q.L.f.ammo_rockets, q.f(me, q.L.f.ammo_rockets) - n);
  } else if (q.is(kind, "cells")) {
    q.set_f(me, q.L.f.ammo_cells, q.f(me, q.L.f.ammo_cells) - n);
  }
}

// Carried and loaded: both halves asked, as the bytecode's && asks them.
bool carried_and_loaded(A& q, float w) {
  const bool carried = band(q.f(q.self(), q.L.f.items), w) != 0;
  const bool loaded = ammo_for(q, w) >= weapon_cost(q, w);
  return carried & loaded;
}

// A hitscan line from the eye along v_forward with a spread, `reach` long, per pellet.
void fire_pellets_far(A& q, float pellets, float points, float spread, float reach) {
  const int32_t me = q.self();
  const Vec3 from = q.v(me, q.L.f.origin) + q.v(me, q.L.f.view_ofs);
  int32_t struck = q.world();
  float summed = 0;
  for (int i = 0; static_cast<float>(i) < pellets; i++) {  // the QC counts in floats
    const float r1 = crandom(q);
    const Vec3 a = q.v_forward() + q.v_right() * (r1 * spread);
    const float r2 = crandom(q);
    const Vec3 dir = a + q.v_up() * (r2 * spread);
    const Vec3 to = from + dir * reach;
    q.traceline(from, to, 0, me);
    if (q.gf(q.L.g.trace_fraction) < 1) {
      const int32_t hit = q.gi(q.L.g.trace_ent);
      if (on(q.f(hit, q.L.f.takedamage))) {
        effect_impact(q, q.gv(q.L.g.trace_endpos), 1, 1);
        if (!pellets_as_one_blow()) {
          q.set_i(q.gi(q.L.g.trace_ent), f_deathtype, q.s(s_shot));
          damage(q, q.gi(q.L.g.trace_ent), me, me, points);
        } else {
          if (q.gi(q.L.g.trace_ent) != struck) {
            if (summed > 0) damage(q, struck, me, me, summed);
            struck = q.gi(q.L.g.trace_ent);
            q.set_i(struck, f_deathtype, q.s(s_shot));
            summed = 0;
          }
          summed = summed + points;
        }
      } else {
        effect_impact(q, q.gv(q.L.g.trace_endpos), 1, 0);
      }
    }
  }
  if (summed > 0) damage(q, struck, me, me, summed);
}

// A cut `reach` in front: `points` to what bleeds, the weapon's land sound on a wall.
void fire_blade(A& q, float w) {
  const int32_t me = q.self();
  const Vec3 from = q.v(me, q.L.f.origin) + q.v(me, q.L.f.view_ofs);
  q.traceline(from, from + q.v_forward() * weapon_number(q, w, 7), 0, me);
  if (q.gf(q.L.g.trace_fraction) == 1) return;
  if (on(q.f(q.gi(q.L.g.trace_ent), q.L.f.takedamage))) {
    q.set_i(q.gi(q.L.g.trace_ent), f_deathtype, q.s(s_cut_down));
    if (q.g(g_deathmatch) == 4) {
      damage(q, q.gi(q.L.g.trace_ent), me, me, 75);  // deathmatch 4's blade
    } else {
      damage(q, q.gi(q.L.g.trace_ent), me, me, weapon_number(q, w, 5));
    }
    effect_impact(q, q.gv(q.L.g.trace_endpos), 3, 1);
  } else {
    weapon_sound(q, me, w, 13);
    effect_impact(q, q.gv(q.L.g.trace_endpos), 3, 0);
  }
}

// A missile spawned at the eye, flying along v_forward at `spd`, removed after `life` seconds.
int32_t launch(A& q, int32_t mdl, float mtype, float spd, float life) {
  const int32_t me = q.self();
  const int32_t m = q.spawn();
  q.set_i(m, q.L.f.owner, me);
  q.set_f(m, q.L.f.movetype, mtype);
  q.set_f(m, q.L.f.solid, kSolidBbox);
  q.set_v(m, q.L.f.velocity, q.v_forward() * spd);
  q.set_v(m, q.L.f.angles, q.vectoangles(q.v(m, q.L.f.velocity)));
  q.set_f(m, q.L.f.nextthink, q.time() + life);
  q.set_i(m, q.L.f.think, q.fn(fn_remove_self));
  q.setmodel(m, mdl);
  q.setsize(m, {0, 0, 0}, {0, 0, 0});
  q.setorigin(m,
              q.v(me, q.L.f.origin) + q.v(me, q.L.f.view_ofs) + q.v_forward() * 8 - Vec3{0, 0, 8});
  q.set_newmis(m);  // the multiplayer form: a missile made this frame runs once at once
  return m;
}

// Weapon `w`'s missile at its row's speed and life, carrying `w` for its touch and blast.
int32_t launch_weapon(A& q, float w, float mtype) {
  const int32_t mdl = weapon_missile(q, w);
  const float spd = weapon_number(q, w, 7);
  const float life = weapon_number(q, w, 8);
  const int32_t m = launch(q, mdl, mtype, spd, life);
  q.set_f(m, q.L.f.weapon, w);
  return m;
}

void fire_spike(A& q, float w) {
  const int32_t m = launch_weapon(q, w, kMovetypeFlymissile);
  q.set_i(m, q.L.f.classname, q.s(s_spike));
  q.set_f(m, f_dmg, weapon_number(q, w, 5));
  q.set_i(m, q.L.f.touch, q.fn(fn_spike_touch));
}

// A missile's blast where it is: its row's splash to everything near but `spare`, its sound.
void blast_at(A& q, int32_t spare) {
  const int32_t me = q.self();
  radius_damage(q, me, q.i(me, q.L.f.owner), weapon_number(q, q.f(me, q.L.f.weapon), 9), spare,
                q.i(me, f_deathtype));
  const int32_t snd = weapon_word(q, q.f(q.self(), q.L.f.weapon), 14);
  if (!q.is(snd, "-")) q.sound(q.self(), kChanVoice, snd, 1, kAttnNorm);
  effect_explosion(q, q.v(q.self(), q.L.f.origin));
  q.remove(q.self());
}

void fire_grenade(A& q, float w) {
  const int32_t m = launch_weapon(q, w, kMovetypeBounce);
  q.set_i(m, q.L.f.classname, q.s(s_grenade));
  q.set_v(m, q.L.f.velocity, q.v(m, q.L.f.velocity) + Vec3{0, 0, 200});
  q.set_v(m, q.L.f.avelocity, {300, 300, 300});
  q.set_v(m, q.L.f.angles, q.vectoangles(q.v(m, q.L.f.velocity)));
  q.set_i(m, q.L.f.touch, q.fn(fn_grenade_touch));
  q.set_i(m, q.L.f.think, q.fn(fn_blast));
  q.set_i(m, f_deathtype, q.s(s_gibbed_by_a_grenade));
}

void fire_rocket(A& q, float w) {
  const int32_t m = launch_weapon(q, w, kMovetypeFlymissile);
  q.set_i(m, q.L.f.classname, q.s(s_rocket));
  q.set_i(m, q.L.f.touch, q.fn(fn_rocket_touch));
}

// A beam its reach long; in water it discharges its splash a cell to everything near.
void fire_arc(A& q, float w) {
  const int32_t me = q.self();
  const float reach = weapon_number(q, w, 7);
  const Vec3 from = q.v(me, q.L.f.origin) + q.v(me, q.L.f.view_ofs);
  q.traceline(from, from + q.v_forward() * reach, 1, me);
  effect_beam(q, from, q.gv(q.L.g.trace_endpos));
  if (q.f(me, q.L.f.waterlevel) > 1) {
    // Deathmatch 4: one time in two it kills the firer outright (the draw made either way).
    const bool dm4 = q.g(g_deathmatch) == 4;
    const bool half = q.random() < 0.5f;
    if (dm4 & half) {
      q.set_i(me, f_deathtype, q.s(s_discharged));
      damage(q, me, me, me, 4000);
      return;
    }
    q.set_i(me, f_deathtype, q.s(s_discharged));
    const float splash = weapon_number(q, w, 9);
    radius_damage(q, me, me, splash * ammo_for(q, w), q.world(), q.s(s_electrocuted));
    ammo_spend(q, w, ammo_for(q, w));
    weapon_select_best(q);
    return;
  }
  q.traceline(from, from + q.v_forward() * reach, 0, me);
  if (on(q.f(q.gi(q.L.g.trace_ent), q.L.f.takedamage))) {
    effect_impact(q, q.gv(q.L.g.trace_endpos), 3, 1);
    q.set_i(q.gi(q.L.g.trace_ent), f_deathtype, q.s(s_shafted));
    damage(q, q.gi(q.L.g.trace_ent), me, me, weapon_number(q, w, 5));
  }
}

}  // namespace

// Word `i` of weapon `w`'s row (good until the next row is read), and as a number.
int32_t weapon_word(A& q, float w, float i) {
  q.tokenize(weapon_row(q, w));
  return q.argv(i);
}
float weapon_number(A& q, float w, float i) { return number_of(q, weapon_word(q, w, i)); }

// The number key of weapon bit `w` (1..8), the order the best-weapon choice prefers.
float weapon_rank(float w) {
  if (w == kWBlade) return 1;
  if (w == kWScatter) return 2;
  if (w == kWDouble) return 3;
  if (w == kWSpiker) return 4;
  if (w == kWHeavySpiker) return 5;
  if (w == kWGrenades) return 6;
  if (w == kWRockets) return 7;
  if (w == kWArc) return 8;
  return 0;
}

// The ammunition `self` has for weapon `w`.
float ammo_for(A& q, float w) {
  const int32_t kind = weapon_word(q, w, 1);
  const int32_t me = q.self();
  if (q.is(kind, "shells")) return q.f(me, q.L.f.ammo_shells);
  if (q.is(kind, "nails")) return q.f(me, q.L.f.ammo_nails);
  if (q.is(kind, "rockets")) return q.f(me, q.L.f.ammo_rockets);
  if (q.is(kind, "cells")) return q.f(me, q.L.f.ammo_cells);
  return 1;
}

// The status bar's ammunition and the view model for the held weapon.
void weapon_show(A& q) {
  const int32_t me = q.self();
  q.set_f(me, q.L.f.currentammo, ammo_for(q, q.f(me, q.L.f.weapon)));
  const float items = q.f(me, q.L.f.items);
  q.set_f(me, q.L.f.items, items - band(items, kItShells | kItNails | kItRockets | kItCells));
  q.set_f(me, q.L.f.items, bor(q.f(me, q.L.f.items), ammo_bit(q, q.f(me, q.L.f.weapon))));
  q.set_i(me, q.L.f.weaponmodel, weapon_model(q, q.f(me, q.L.f.weapon)));
  q.set_f(me, q.L.f.weaponframe, 0);
}

void weapon_set(A& q, float w) {
  q.set_f(q.self(), q.L.f.weapon, w);
  weapon_show(q);
}

// The best carried weapon with ammunition: the highest rank, the arc only out of water.
void weapon_select_best(A& q) {
  for (int rank = 8; rank > 1; rank--) {
    const float w = weapon_of_rank(static_cast<float>(rank));
    if (carried_and_loaded(q, w)) {
      const bool not_beam = !q.is(weapon_word(q, w, 0), "beam");
      const bool dry = q.f(q.self(), q.L.f.waterlevel) <= 1;
      if (not_beam | dry) {
        weapon_set(q, w);
        return;
      }
    }
  }
  weapon_set(q, kWBlade);
}

// Impulses 1..8 pick that weapon when carried and loaded; 10 the next carried one.
void weapon_impulse(A& q) {
  const int32_t me = q.self();
  const float impulse = q.f(me, q.L.f.impulse);
  if ((impulse >= 1) & (impulse <= 8)) {
    const float w = weapon_of_rank(impulse);
    if (band(q.f(me, q.L.f.items), w) == 0) {
      say_to(q, me, kPrintHigh, {q.s(s_no_weapon)});
    } else if (ammo_for(q, w) < weapon_cost(q, w)) {
      say_to(q, me, kPrintHigh, {q.s(s_not_enough_ammo)});
    } else {
      weapon_set(q, w);
    }
  } else if (impulse == 10) {
    float rank = weapon_rank(q.f(me, q.L.f.weapon));
    for (int tries = 0; tries < 8; tries++) {
      rank = rank + 1;
      if (rank > 8) rank = 1;
      const float w = weapon_of_rank(rank);
      if (carried_and_loaded(q, w)) {
        weapon_set(q, w);
        break;
      }
    }
  }
  q.set_f(q.self(), q.L.f.impulse, 0);
}

// void() spike_touch.
void spike_touch(A& q) {
  const int32_t me = q.self();
  if (q.other() == q.i(me, q.L.f.owner)) return;
  if (q.pointcontents(q.v(me, q.L.f.origin)) == kContentSky) {
    q.remove(me);
    return;
  }
  if (on(q.f(q.other(), q.L.f.takedamage))) {
    effect_impact(q, q.v(me, q.L.f.origin), 1, 1);
    q.set_i(q.other(), f_deathtype, q.s(s_nailed));
    damage(q, q.other(), me, q.i(me, q.L.f.owner), q.f(me, f_dmg));
  } else {
    effect_head(q, kTeSpike);
    write_vec(q, q.v(q.self(), q.L.f.origin));
    effect_send(q, q.v(q.self(), q.L.f.origin), kMulticastPhs);
  }
  q.remove(q.self());
}

// void() blast: blast_at(world).
void blast(A& q) { blast_at(q, q.world()); }

// void() rocket_touch.
void rocket_touch(A& q) {
  const int32_t me = q.self();
  if (q.other() == q.i(me, q.L.f.owner)) return;
  if (q.pointcontents(q.v(me, q.L.f.origin)) == kContentSky) {
    q.remove(me);
    return;
  }
  if (on(q.f(q.other(), q.L.f.takedamage))) {
    q.set_i(q.other(), f_deathtype, q.s(s_blown_apart));
    damage(q, q.other(), me, q.i(me, q.L.f.owner), weapon_number(q, q.f(me, q.L.f.weapon), 5));
  }
  // Back the blast off the wall so its centre is in the open.
  q.set_v(me, q.L.f.origin, q.v(me, q.L.f.origin) - q.normalize(q.v(me, q.L.f.velocity)) * 8);
  q.set_i(me, f_deathtype, q.s(s_blown_apart));
  blast_at(q, q.other());
}

// void() grenade_touch.
void grenade_touch(A& q) {
  const int32_t me = q.self();
  if (q.other() == q.i(me, q.L.f.owner)) return;
  if (q.f(q.other(), q.L.f.takedamage) == kDamageAim) {
    blast(q);
    return;
  }
  const int32_t snd = weapon_word(q, q.f(me, q.L.f.weapon), 13);
  if (!q.is(snd, "-")) q.sound(me, kChanVoice, snd, 1, kAttnNorm);
  if (same_vec(q.v(me, q.L.f.velocity), {0, 0, 0})) q.set_v(me, q.L.f.avelocity, {0, 0, 0});
}

// Fires the held weapon when attack is down and it is ready: its row says how.
void weapon_frame(A& q) {
  const int32_t me = q.self();
  if (q.time() < q.f(me, f_attack_finished)) return;
  if (q.f(me, q.L.f.deadflag) != kDeadNo) return;
  if (q.f(me, q.L.f.button0) == 0) return;
  const float w = q.f(me, q.L.f.weapon);
  if (ammo_for(q, w) < weapon_cost(q, w)) {
    weapon_select_best(q);
    return;
  }
  q.makevectors(q.v(me, q.L.f.v_angle));
  q.set_f(me, f_attack_finished, q.time() + weapon_delay(q, w));
  q.set_f(me, f_show_hostile, q.time() + 1);  // the noise wakes monsters that hear it
  ammo_spend(q, w, weapon_cost(q, w));
  q.set_f(me, q.L.f.effects, bor(q.f(me, q.L.f.effects), kEfMuzzleflash));
  weapon_sound(q, me, w, 11);
  const int32_t fire = weapon_word(q, w, 0);
  if (q.is(fire, "melee")) {
    fire_blade(q, w);
  } else if (q.is(fire, "pellets")) {
    const float n = weapon_number(q, w, 4);
    const float points = weapon_number(q, w, 5);
    const float spread = weapon_number(q, w, 6);
    const float reach = weapon_number(q, w, 7);
    fire_pellets_far(q, n, points, spread, reach);
  } else if (q.is(fire, "spike")) {
    fire_spike(q, w);
  } else if (q.is(fire, "grenade")) {
    fire_grenade(q, w);
  } else if (q.is(fire, "rocket")) {
    fire_rocket(q, w);
  } else if (q.is(fire, "beam")) {
    fire_arc(q, w);
  }
  weapon_show(q);
  if (ammo_for(q, w) < weapon_cost(q, w)) weapon_select_best(q);
}

// Reads the arsenal and loads what its rows name: every view model, every missile, every sound.
void weapon_precache(A& q) {
  // The QC counts its slots and words in floats; small whole numbers, the same in ints.
  for (int slot = 1; slot <= 8; slot++) arsenal_keep(q, static_cast<float>(slot));
  for (int slot = 1; slot <= 8; slot++)
    q.precache_model(weapon_model(q, weapon_of_rank(static_cast<float>(slot))));
  for (int slot = 1; slot <= 8; slot++) {
    const int32_t name = weapon_missile(q, weapon_of_rank(static_cast<float>(slot)));
    if (!q.is(name, "-")) q.precache_model(name);
  }
  for (int slot = 1; slot <= 8; slot++) {
    for (int i = 11; i <= 14; i++) {
      if (i != 12) {
        const int32_t name =
            weapon_word(q, weapon_of_rank(static_cast<float>(slot)), static_cast<float>(i));
        if (!q.is(name, "-")) q.precache_sound(name);
      }
    }
  }
}

}  // namespace shooters_edge::server::arena
