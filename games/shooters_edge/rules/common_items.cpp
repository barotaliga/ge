// The arena's items: health, armour, ammunition, weapons, backpacks and the powers lying on the
// map. Each is a trigger a living player picks up by touching; it is gone for its respawn time,
// then comes back with a sound. What a pickup prints names the item in the loaded game's words
// when its profile gives them (`name_of`), in ours when it gives none.
//
//   item_health  25 (spawnflags 1: 15; 2: 100 that may go to 250 and decays a point a second) 20 s
//   item_armor1  100 at 30 %   item_armor2  150 at 60 %   item_armorInv  200 at 80 %            20
//   s item_shells 20  item_spikes 25  item_rockets 5  item_cells 6 (spawnflags 1: twice) 20 s
//   weapon_*     the weapon and some of its ammunition                                         30 s
//   the quad (four times the damage) and the shield (no damage taken), 30 s each                60
//   s
//
// ge:summary The arena's pickups (health, armour, ammunition, weapons, packs, powers) in C++
// ge:keywords arena native-arena items pickup item_health item_armor1 backpack power_frame
// ge:keywords ammo_give weapon_touch quad shield
// ge:skill progs shooter
// ge:test tests/shooters_edge_arena_rules_test.cpp
#include <cstdint>

#include "shooters_edge/rules/variant.hpp"

namespace shooters_edge::server::rules::SE_RULES_NS {

namespace {

constexpr float kMaxShells = 100;
constexpr float kMaxNails = 200;
constexpr float kMaxRockets = 100;
constexpr float kMaxCells = 100;
constexpr float kPowerTime = 30;

// Takes the item away for its respawn time, its targets fired.
void item_taken(A& q) {
  const int32_t me = q.self();
  q.sound(q.other(), kChanItem, q.i(me, q.L.f.noise), 1, kAttnNorm);
  q.stuffcmd(q.other(), q.s(s_bf));
  q.set_f(me, q.L.f.solid, kSolidNot);
  q.set_f(me, q.L.f.modelindex, 0);
  q.set_f(me, q.L.f.nextthink, q.time() + q.f(me, f_respawn_time));
  q.set_i(me, q.L.f.think, q.fn(fn_item_return));
  if (!items_come_back(q)) {
    q.set_f(me, q.L.f.nextthink, q.time());
    q.set_i(me, q.L.f.think, q.fn(fn_remove_self));
  }
  q.set_ge(g_activator, q.other());
  use_targets(q);
}

// Common setup: a trigger box on the floor, its model and pick-up sound.
void item_start(A& q, int32_t mdl, int32_t snd, Vec3 lo, Vec3 hi, float respawn) {
  q.precache_model(mdl);
  q.precache_sound(snd);
  q.setmodel(q.self(), mdl);
  const int32_t me = q.self();
  q.set_i(me, q.L.f.noise, snd);
  q.set_f(me, q.L.f.flags, kFlItem);
  q.set_f(me, q.L.f.solid, kSolidTrigger);
  q.set_f(me, q.L.f.movetype, kMovetypeToss);
  q.set_f(me, f_respawn_time, respawn);
  q.setsize(me, lo, hi);
  q.set_v(me, q.L.f.origin, q.v(me, q.L.f.origin) + Vec3{0, 0, 6});
  if (q.droptofloor() == 0) {
    q.dprint({q.s(s_fell_out), q.vtos(q.v(q.self(), q.L.f.origin)), q.s(s_nl)});
    q.remove(q.self());
  }
}

// A living player who may pick things up.
bool can_pick_up(A& q) {
  const int32_t o = q.other();
  return q.is(q.i(o, q.L.f.classname), "player") & (q.f(o, q.L.f.health) > 0) &
         (q.g(g_match_over) == 0);
}

// And may take health, armour or a pack: in deathmatch 4 not while a shield holds him.
bool can_heal(A& q) {
  if (!can_pick_up(q)) return false;
  const bool not_dm4 = q.deathmatch() != 4;
  const bool open = !dmm4_shielded(q, q.other());
  return not_dm4 | open;
}

// Adds to `other`'s ammunition up to the maxima; true when anything was taken.
bool ammo_give(A& q, float shells, float nails, float rockets, float cells) {
  const int32_t o = q.other();
  auto sum = [&] {
    return q.f(o, q.L.f.ammo_shells) + q.f(o, q.L.f.ammo_nails) + q.f(o, q.L.f.ammo_rockets) +
           q.f(o, q.L.f.ammo_cells);
  };
  const float before = sum();
  q.set_f(o, q.L.f.ammo_shells, min_of(q.f(o, q.L.f.ammo_shells) + shells, kMaxShells));
  q.set_f(o, q.L.f.ammo_nails, min_of(q.f(o, q.L.f.ammo_nails) + nails, kMaxNails));
  q.set_f(o, q.L.f.ammo_rockets, min_of(q.f(o, q.L.f.ammo_rockets) + rockets, kMaxRockets));
  q.set_f(o, q.L.f.ammo_cells, min_of(q.f(o, q.L.f.ammo_cells) + cells, kMaxCells));
  return sum() > before;
}

// `self`'s own ammunition given to `other`.
bool ammo_give_own(A& q) {
  const int32_t me = q.self();
  return ammo_give(q, q.f(me, q.L.f.ammo_shells), q.f(me, q.L.f.ammo_nails),
                   q.f(me, q.L.f.ammo_rockets), q.f(me, q.L.f.ammo_cells));
}

// `other` takes `self`'s weapon in hand when `better` says so (a weapon on the map and a pack ask
// it differently), else the status bar is shown afresh.
void take_in_hand(A& q, bool (*better)(A&, int32_t pickup, float had), float had) {
  const int32_t was_self = q.self();
  q.set_self(q.other());
  if (better(q, was_self, had)) {
    weapon_set(q, q.f(was_self, q.L.f.weapon));
  } else {
    weapon_show(q);
  }
  q.set_self(was_self);
}

// A weapon on the map: new to him and ranked above the one he holds.
bool weapon_better(A& q, int32_t pickup, float had) {
  const bool fresh = had == 0;
  const bool ranked =
      weapon_rank(q.f(q.self(), q.L.f.weapon)) < weapon_rank(q.f(pickup, q.L.f.weapon));
  return fresh & ranked;
}

// A pack: it holds a weapon new to him, ranked below the one he holds (as the rule reads).
bool pack_better(A& q, int32_t pickup, float had) {
  const bool fresh = had == 0;
  const bool has = q.f(pickup, q.L.f.weapon) != 0;
  const bool ranked =
      weapon_rank(q.f(pickup, q.L.f.weapon)) < weapon_rank(q.f(q.self(), q.L.f.weapon));
  return fresh & has & ranked;
}

// An item in another game's look (classic remix, `cr_items`): the setting `setting`'s row,
// "<model> <sound> <lift>" ("-" keeps the item's own sound), else its own `mdl` and `snd`. Another
// game's model is drawn about its origin, where a brush model of ours starts at it: an item whose
// box starts at its origin takes the box's middle as its origin and a box about it, so the trigger
// stays where it was and the model stands in its middle; its origin is raised by the lift (that
// game's models stand that far above the floor) and its box lowered as far.
void dressed_start(A& q, int32_t setting, int32_t mdl, int32_t snd, Vec3 lo, Vec3 hi,
                   float respawn) {
  const int32_t row = q.cvar_string(setting);
  if (!q.is(row, "")) {
    const float words = q.tokenize(row);
    mdl = q.strzone(q.argv(0));  // held by the item for as long as the map runs
    if (words > 1 && !q.is(q.argv(1), "-")) snd = q.strzone(q.argv(1));
    const float lift = words > 2 ? number_of(q, q.argv(2)) : 0;
    const int32_t me = q.self();
    const Vec3 mid{(lo.x + hi.x) * 0.5f, (lo.y + hi.y) * 0.5f, -lift};
    const Vec3 at = q.v(me, q.L.f.origin);
    q.set_v(me, q.L.f.origin, {at.x + mid.x, at.y + mid.y, at.z - mid.z});
    lo = {lo.x - mid.x, lo.y - mid.y, lo.z - lift};
    hi = {hi.x - mid.x, hi.y - mid.y, hi.z - lift};
  }
  item_start(q, mdl, snd, lo, hi, respawn);
}

void armour_start(A& q, float slot, float bit) {
  int32_t row;
  if (slot == 1) {
    row = q.cvar_string(q.s(s_arsenal_armour1));
  } else if (slot == 2) {
    row = q.cvar_string(q.s(s_arsenal_armour2));
  } else {
    row = q.cvar_string(q.s(s_arsenal_armour3));
  }
  if (q.is(row, "")) {
    if (slot == 1) {
      row = q.s(s_armour_row1);
    } else if (slot == 2) {
      row = q.s(s_armour_row2);
    } else {
      row = q.s(s_armour_row3);
    }
  }
  q.tokenize(row);
  const int32_t mdl = q.strzone(q.argv(3));  // held by the item for as long as the map runs
  const int32_t snd = q.strzone(q.argv(4));
  const int32_t me = q.self();
  q.set_f(me, q.L.f.armortype, number_of(q, q.argv(0)));
  q.set_f(me, q.L.f.armorvalue, number_of(q, q.argv(1)));
  q.set_f(me, q.L.f.skin, number_of(q, q.argv(2)));
  q.set_f(me, q.L.f.items, bit);
  q.set_i(me, q.L.f.touch, q.fn(fn_armour_touch));
  item_start(q, mdl, snd, {-16, -16, 0}, {16, 16, 56}, 20);
}

void ammo_start(A& q, int32_t mdl, int32_t setting) {
  const int32_t me = q.self();
  if (q.deathmatch() == 4) {  // none lies on the map in deathmatch 4
    q.remove(me);
    return;
  }
  float n = 1;
  if (on(band(q.f(me, q.L.f.spawnflags), 1))) n = 2;
  q.set_f(me, q.L.f.ammo_shells, q.f(me, q.L.f.ammo_shells) * n);
  q.set_f(me, q.L.f.ammo_nails, q.f(me, q.L.f.ammo_nails) * n);
  q.set_f(me, q.L.f.ammo_rockets, q.f(me, q.L.f.ammo_rockets) * n);
  q.set_f(me, q.L.f.ammo_cells, q.f(me, q.L.f.ammo_cells) * n);
  q.set_i(me, q.L.f.touch, q.fn(fn_ammo_touch));
  dressed_start(q, setting, mdl, q.s(s_lock4_wav), {0, 0, 0}, {32, 32, 56}, 20);
}

// A weapon lying in slot `w`'s place: the arsenal's row gives its pickup model and ammunition;
// a row with none leaves the map's own.
void weapon_start(A& q, float w, int32_t name, int32_t mdl) {
  const int32_t me = q.self();
  if (q.deathmatch() == 4) {  // every player holds them all in deathmatch 4
    q.remove(me);
    return;
  }
  q.set_f(me, q.L.f.weapon, w);
  q.set_i(me, q.L.f.netname, name);
  q.set_i(me, q.L.f.touch, q.fn(fn_weapon_touch));
  const int32_t pickup = weapon_word(q, w, 15);
  if (!q.is(pickup, "") & !q.is(pickup, "-")) {
    mdl = q.strzone(pickup);  // held by the item for as long as the map runs
    const float n = weapon_number(q, w, 16);
    const int32_t kind = weapon_word(q, w, 1);
    q.set_f(me, q.L.f.ammo_shells, 0);
    q.set_f(me, q.L.f.ammo_nails, 0);
    q.set_f(me, q.L.f.ammo_rockets, 0);
    q.set_f(me, q.L.f.ammo_cells, 0);
    if (q.is(kind, "shells")) {
      q.set_f(me, q.L.f.ammo_shells, n);
    } else if (q.is(kind, "nails")) {
      q.set_f(me, q.L.f.ammo_nails, n);
    } else if (q.is(kind, "rockets")) {
      q.set_f(me, q.L.f.ammo_rockets, n);
    } else if (q.is(kind, "cells")) {
      q.set_f(me, q.L.f.ammo_cells, n);
    }
  }
  // Another game's weapons (classic remix) bring their own pickup sound.
  int32_t snd = q.s(s_pkup_wav);
  if (const int32_t own = q.cvar_string(q.s(s_arsenal_pickup)); !q.is(own, ""))
    snd = q.strzone(own);
  item_start(q, mdl, snd, {-16, -16, 0}, {16, 16, 56}, 30);
}

int32_t pack_weapon_name(A& q, float w) {
  if (w == kWDouble) return q.s(s_double_scattergun);
  if (w == kWSpiker) return q.s(s_spiker);
  if (w == kWHeavySpiker) return q.s(s_heavy_spiker);
  if (w == kWGrenades) return q.s(s_grenade_launcher);
  if (w == kWRockets) return q.s(s_rocket_launcher);
  return q.s(s_arc_gun);
}

// A weapon's class name: the key of its name in the loaded game's words.
int32_t pack_weapon_class(A& q, float w) {
  if (w == kWDouble) return q.s(s_weapon_supershotgun);
  if (w == kWSpiker) return q.s(s_weapon_nailgun);
  if (w == kWHeavySpiker) return q.s(s_weapon_supernailgun);
  if (w == kWGrenades) return q.s(s_weapon_grenadelauncher);
  if (w == kWRockets) return q.s(s_weapon_rocketlauncher);
  return q.s(s_weapon_lightning);
}

// One amount of a backpack's list: ", " before all but the first.
float pack_say_amount(A& q, float listed, float amount, int32_t unit) {
  if (amount == 0) return listed;
  if (on(listed)) say_to(q, q.other(), kPrintLow, {q.s(s_comma)});
  say_to(q, q.other(), kPrintLow, {q.ftos(amount), q.s(s_space), unit});
  return 1;
}

// What `other` is told of the backpack `self` (`had`: he had its weapon).
void backpack_say(A& q, float had) {
  const int32_t me = q.self();
  const int32_t o = q.other();
  if (q.is(name_of(q, q.s(s_item_shells), 0), "")) {
    if (on(q.f(me, q.L.f.weapon))) {
      say_to(q, o, kPrintLow, {q.s(s_got_backpack_with), q.i(me, q.L.f.netname), q.s(s_nl)});
    } else {
      say_to(q, o, kPrintLow, {q.s(s_got_backpack)});
    }
    return;
  }
  say_to(q, o, kPrintLow, {q.s(s_you_get)});
  float listed = 0;
  if ((q.f(me, q.L.f.weapon) != 0) & (had == 0)) {
    say_to(q, o, kPrintLow,
           {q.s(s_the),
            name_of(q, pack_weapon_class(q, q.f(me, q.L.f.weapon)), q.i(me, q.L.f.netname))});
    listed = 1;
  }
  listed = pack_say_amount(q, listed, q.f(me, q.L.f.ammo_shells),
                           name_of(q, q.s(s_item_shells), q.s(s_shells)));
  listed = pack_say_amount(q, listed, q.f(me, q.L.f.ammo_nails),
                           name_of(q, q.s(s_item_spikes), q.s(s_nails)));
  listed = pack_say_amount(q, listed, q.f(me, q.L.f.ammo_rockets),
                           name_of(q, q.s(s_item_rockets), q.s(s_rockets)));
  pack_say_amount(q, listed, q.f(me, q.L.f.ammo_cells),
                  name_of(q, q.s(s_item_cells), q.s(s_cells)));
  say_to(q, o, kPrintLow, {q.s(s_nl)});
}

void power_start(A& q, int32_t mdl, int32_t snd, float bit) {
  const int32_t me = q.self();
  q.set_f(me, q.L.f.items, bit);
  q.set_i(me, q.L.f.touch, q.fn(fn_power_touch));
  if (bit == kItQuad) {
    q.set_f(me, q.L.f.effects, kEfDimlight | kEfBlue);
  } else {
    q.set_f(me, q.L.f.effects, kEfDimlight | kEfRed);
  }
  item_start(q, mdl, snd, {-16, -16, -24}, {16, 16, 32}, 60);
}

// A power's end told 3 s before it comes, once, in the loaded game's words (none in ours).
void power_warn(A& q, float bit, float finished, int32_t key, int32_t sample) {
  const int32_t me = q.self();
  if ((finished == 0) | (finished - 3 > q.time()) | (band(q.f(me, f_power_said), bit) != 0)) return;
  q.set_f(me, f_power_said, bor(q.f(me, f_power_said), bit));
  const int32_t told = game_words(q, key, 0, 0, 0, 0);
  if (q.is(told, "")) return;
  say_to(q, q.self(), kPrintHigh, {told});
  q.sound(q.self(), kChanItem, sample, 1, kAttnNorm);
}

// `field` less its `bit`.
void clear_bit(A& q, int32_t e, int field, float bit) {
  const float v = q.f(e, field);
  q.set_f(e, field, v - band(v, bit));
}

}  // namespace

// void() item_return: shown again after its respawn time.
void item_return(A& q) {
  const int32_t me = q.self();
  q.set_f(me, q.L.f.solid, kSolidTrigger);
  q.setmodel(me, q.i(me, q.L.f.model));
  q.setorigin(q.self(), q.v(q.self(), q.L.f.origin));
  q.sound(q.self(), kChanVoice, q.s(s_itembk2_wav), 1, kAttnNorm);
}

// void() health_decay: the big box's extra a point a second, then it comes back after its wait.
void health_decay(A& q) {
  const int32_t me = q.self();
  const int32_t p = q.i(me, q.L.f.owner);
  if (q.f(p, q.L.f.health) > q.f(p, q.L.f.max_health)) {
    q.set_f(p, q.L.f.health, q.f(p, q.L.f.health) - 1);
    q.set_f(me, q.L.f.nextthink, q.time() + 1);
    return;
  }
  q.set_i(me, q.L.f.owner, q.world());
  if (!items_come_back(q)) {
    q.remove(me);
    return;
  }
  q.set_f(me, q.L.f.nextthink, q.time() + q.f(me, f_respawn_time));
  q.set_i(me, q.L.f.think, q.fn(fn_item_return));
}

void health_touch(A& q) {
  if (!can_heal(q)) return;
  const int32_t me = q.self();
  const int32_t o = q.other();
  float limit = q.f(o, q.L.f.max_health);
  if (on(band(q.f(me, q.L.f.spawnflags), 2))) limit = 250;
  if (q.f(o, q.L.f.health) >= limit) return;
  q.set_f(o, q.L.f.health, q.f(o, q.L.f.health) + q.f(me, q.L.f.health));
  if (q.f(o, q.L.f.health) > limit) q.set_f(o, q.L.f.health, limit);
  say_to(q, o, kPrintLow, {q.s(s_you_receive), q.ftos(q.f(me, q.L.f.health)), q.s(s_health_nl)});
  item_taken(q);
  if (on(band(q.f(q.self(), q.L.f.spawnflags), 2))) {
    q.set_i(q.self(), q.L.f.owner, q.other());
    q.set_f(q.self(), q.L.f.nextthink, q.time() + 5);
    q.set_i(q.self(), q.L.f.think, q.fn(fn_health_decay));
  }
}

void item_health(A& q) {
  const int32_t me = q.self();
  const float flags = q.f(me, q.L.f.spawnflags);
  if ((q.deathmatch() == 4) & (band(flags, 2) != 0)) {  // no mega health in deathmatch 4
    q.remove(me);
    return;
  }
  q.set_i(me, q.L.f.touch, q.fn(fn_health_touch));
  if (on(band(flags, 1))) {
    q.set_f(me, q.L.f.health, 15);
    dressed_start(q, q.s(s_arsenal_health1), q.s(s_b_bh10), q.s(s_r_item1_wav), {0, 0, 0},
                  {32, 32, 56}, 20);
  } else if (on(band(flags, 2))) {
    q.set_f(me, q.L.f.health, 100);
    dressed_start(q, q.s(s_arsenal_health3), q.s(s_b_bh100), q.s(s_r_item2_wav), {0, 0, 0},
                  {32, 32, 56}, 20);
  } else {
    q.set_f(me, q.L.f.health, 25);
    dressed_start(q, q.s(s_arsenal_health2), q.s(s_b_bh25), q.s(s_health1_wav), {0, 0, 0},
                  {32, 32, 56}, 20);
  }
}

void armour_touch(A& q) {
  if (!can_heal(q)) return;
  const int32_t me = q.self();
  const int32_t o = q.other();
  if (q.f(o, q.L.f.armortype) * q.f(o, q.L.f.armorvalue) >=
      q.f(me, q.L.f.armortype) * q.f(me, q.L.f.armorvalue))
    return;
  q.set_f(o, q.L.f.armortype, q.f(me, q.L.f.armortype));
  q.set_f(o, q.L.f.armorvalue, q.f(me, q.L.f.armorvalue));
  const float items = q.f(o, q.L.f.items);
  q.set_f(o, q.L.f.items, items - band(items, kItArmorAll) + q.f(me, q.L.f.items));
  if (q.is(name_of(q, q.i(me, q.L.f.classname), 0), "")) {
    say_to(q, o, kPrintLow, {q.s(s_you_got_armor)});
  } else {
    say_to(q, o, kPrintLow,
           {q.s(s_you_got_the), name_of(q, q.i(me, q.L.f.classname), 0), q.s(s_nl)});
  }
  item_taken(q);
}

void item_armor1(A& q) { armour_start(q, 1, kItArmor1); }
void item_armor2(A& q) { armour_start(q, 2, kItArmor2); }
void item_armorInv(A& q) { armour_start(q, 3, kItArmor3); }

void ammo_touch(A& q) {
  if (!can_pick_up(q)) return;
  if (!ammo_give_own(q)) return;
  const int32_t n = name_of(q, q.i(q.self(), q.L.f.classname), 0);
  if (q.is(n, "")) {
    say_to(q, q.other(), kPrintLow, {q.s(s_you_got_ammunition)});
  } else {
    say_to(q, q.other(), kPrintLow, {q.s(s_you_got_the), n, q.s(s_nl)});
  }
  item_taken(q);
  const int32_t was_self = q.self();
  q.set_self(q.other());
  weapon_show(q);
  q.set_self(was_self);
}

void item_shells(A& q) {
  q.set_f(q.self(), q.L.f.ammo_shells, 20);
  ammo_start(q, q.s(s_b_shell0), q.s(s_arsenal_ammo1));
}
void item_spikes(A& q) {
  q.set_f(q.self(), q.L.f.ammo_nails, 25);
  ammo_start(q, q.s(s_b_nail0), q.s(s_arsenal_ammo2));
}
void item_rockets(A& q) {
  q.set_f(q.self(), q.L.f.ammo_rockets, 5);
  ammo_start(q, q.s(s_b_rock0), q.s(s_arsenal_ammo3));
}
void item_cells(A& q) {
  q.set_f(q.self(), q.L.f.ammo_cells, 6);
  ammo_start(q, q.s(s_b_batt0), q.s(s_arsenal_ammo4));
}

void weapon_touch(A& q) {
  if (!can_pick_up(q)) return;
  const int32_t me = q.self();
  const int32_t o = q.other();
  const float had = band(q.f(o, q.L.f.items), q.f(me, q.L.f.weapon));
  ammo_give_own(q);
  q.set_f(o, q.L.f.items, bor(q.f(o, q.L.f.items), q.f(me, q.L.f.weapon)));
  say_to(q, o, kPrintLow,
         {q.s(s_you_got_the), name_of(q, q.i(me, q.L.f.classname), q.i(me, q.L.f.netname)),
          q.s(s_nl)});
  item_taken(q);
  take_in_hand(q, &weapon_better, had);
}

void weapon_supershotgun(A& q) {
  q.set_f(q.self(), q.L.f.ammo_shells, 5);
  weapon_start(q, kWDouble, q.s(s_double_scattergun), q.s(s_g_shot));
}
void weapon_nailgun(A& q) {
  q.set_f(q.self(), q.L.f.ammo_nails, 30);
  weapon_start(q, kWSpiker, q.s(s_spiker), q.s(s_g_nail));
}
void weapon_supernailgun(A& q) {
  q.set_f(q.self(), q.L.f.ammo_nails, 30);
  weapon_start(q, kWHeavySpiker, q.s(s_heavy_spiker), q.s(s_g_nail2));
}
void weapon_grenadelauncher(A& q) {
  q.set_f(q.self(), q.L.f.ammo_rockets, 5);
  weapon_start(q, kWGrenades, q.s(s_grenade_launcher), q.s(s_g_rock));
}
void weapon_rocketlauncher(A& q) {
  q.set_f(q.self(), q.L.f.ammo_rockets, 5);
  weapon_start(q, kWRockets, q.s(s_rocket_launcher), q.s(s_g_rock2));
}
void weapon_lightning(A& q) {
  q.set_f(q.self(), q.L.f.ammo_cells, 15);
  weapon_start(q, kWArc, q.s(s_arc_gun), q.s(s_g_light));
}

// A backpack: what a dead player carried, for a living player who touches it.
void backpack_touch(A& q) {
  if (!can_heal(q)) return;
  if (q.deathmatch() == 4) {  // health, and the bonus powers
    dmm4_pack_take(q);
    q.remove(q.self());
    return;
  }
  const int32_t me = q.self();
  const int32_t o = q.other();
  const float had = band(q.f(o, q.L.f.items), q.f(me, q.L.f.weapon));
  ammo_give_own(q);
  q.set_f(o, q.L.f.items, bor(q.f(o, q.L.f.items), q.f(me, q.L.f.weapon)));
  backpack_say(q, had);
  q.sound(q.other(), kChanItem, q.s(s_lock4_wav), 1, kAttnNorm);
  q.stuffcmd(q.other(), q.s(s_bf));
  take_in_hand(q, &pack_better, had);
  q.remove(q.self());
}

void backpack_gone(A& q) { q.remove(q.self()); }

// The dead player `self` drops his pack where he fell: nothing when he carried no ammunition.
void backpack_drop(A& q) {
  const int32_t me = q.self();
  if (q.f(me, q.L.f.ammo_shells) + q.f(me, q.L.f.ammo_nails) + q.f(me, q.L.f.ammo_rockets) +
          q.f(me, q.L.f.ammo_cells) <=
      0)
    return;
  const int32_t pack = q.spawn();
  q.set_i(pack, q.L.f.classname, q.s(s_backpack));
  q.set_v(pack, q.L.f.origin, q.v(me, q.L.f.origin) - Vec3{0, 0, 24});
  q.set_f(pack, q.L.f.ammo_shells, q.f(me, q.L.f.ammo_shells));
  q.set_f(pack, q.L.f.ammo_nails, q.f(me, q.L.f.ammo_nails));
  q.set_f(pack, q.L.f.ammo_rockets, q.f(me, q.L.f.ammo_rockets));
  q.set_f(pack, q.L.f.ammo_cells, q.f(me, q.L.f.ammo_cells));
  // The weapon in hand, unless it is one every player starts with.
  const float w = q.f(me, q.L.f.weapon);
  if ((w != kWBlade) & (w != kWScatter)) {
    q.set_f(pack, q.L.f.weapon, w);
    q.set_i(pack, q.L.f.netname, pack_weapon_name(q, w));
  }
  Vec3 vel = q.v(pack, q.L.f.velocity);
  vel.z = 300;  // tossed straight up (no draw on the server's generator)
  q.set_v(pack, q.L.f.velocity, vel);
  q.set_f(pack, q.L.f.flags, kFlItem);
  q.set_f(pack, q.L.f.solid, kSolidTrigger);
  q.set_f(pack, q.L.f.movetype, kMovetypeToss);
  q.setmodel(pack, q.s(s_backpack_mdl));
  q.setsize(pack, {-16, -16, 0}, {16, 16, 56});
  q.setorigin(pack, q.v(pack, q.L.f.origin));
  q.set_i(pack, q.L.f.touch, q.fn(fn_backpack_touch));
  q.set_f(pack, q.L.f.nextthink, q.time() + 120);
  q.set_i(pack, q.L.f.think, q.fn(fn_backpack_gone));
}

void power_touch(A& q) {
  if (!can_pick_up(q)) return;
  const int32_t me = q.self();
  const int32_t o = q.other();
  const float bit = q.f(me, q.L.f.items);
  if ((bit == kItQuad) & (q.deathmatch() == 4)) {  // the OctaPower
    q.set_f(o, f_super_damage_finished, q.time() + kPowerTime);
    clear_bit(q, o, q.own_field(f_power_said), kItQuad);
    say_to(q, o, kPrintLow, {q.s(s_got_octapower)});
    dmm4_octa_taken(q);
  } else if (bit == kItQuad) {
    q.set_f(o, f_super_damage_finished, q.time() + kPowerTime);
    clear_bit(q, o, q.own_field(f_power_said), kItQuad);
    say_to(
        q, o, kPrintLow,
        {q.s(s_you_got_the), name_of(q, q.i(me, q.L.f.classname), q.s(s_quad_damage)), q.s(s_nl)});
  } else {
    q.set_f(o, f_invincible_finished, q.time() + kPowerTime);
    clear_bit(q, o, q.own_field(f_power_said), kItInvulnerability);
    say_to(q, o, kPrintLow,
           {q.s(s_you_got_the), name_of(q, q.i(me, q.L.f.classname), q.s(s_shield)), q.s(s_nl)});
  }
  q.set_f(q.other(), q.L.f.items, bor(q.f(q.other(), q.L.f.items), q.f(q.self(), q.L.f.items)));
  item_taken(q);
}

void item_artifact_super_damage(A& q) {
  power_start(q, q.s(s_quaddama_mdl), q.s(s_damage_wav), kItQuad);
}
void item_artifact_invulnerability(A& q) {
  power_start(q, q.s(s_invulner_mdl), q.s(s_protect_wav), kItInvulnerability);
}

// Each frame for a living player: the powers that ran out taken away, the glow of those held.
void power_frame(A& q) {
  if (q.deathmatch() == 4) dmm4_frame(q);
  power_warn(q, kItQuad, q.f(q.self(), f_super_damage_finished), q.s(s_power_quad_off),
             q.s(s_damage2_wav));
  power_warn(q, kItInvulnerability, q.f(q.self(), f_invincible_finished), q.s(s_power_pent_off),
             q.s(s_protect2_wav));
  const int32_t me = q.self();
  const float quad = q.f(me, f_super_damage_finished);
  if ((quad != 0) & (quad < q.time())) {
    q.set_f(me, f_super_damage_finished, 0);
    clear_bit(q, me, q.own_field(f_power_said), kItQuad);
    clear_bit(q, me, q.L.f.items, kItQuad);
    if (q.deathmatch() == 4) dmm4_octa_off(q);
  }
  const float shield = q.f(me, f_invincible_finished);
  if ((shield != 0) & (shield < q.time())) {
    q.set_f(me, f_invincible_finished, 0);
    clear_bit(q, me, q.own_field(f_power_said), kItInvulnerability);
    if (q.f(me, f_shield_finished) <= q.time()) clear_bit(q, me, q.L.f.items, kItInvulnerability);
  }
  clear_bit(q, me, q.L.f.effects, kEfDimlight | kEfBlue | kEfRed);
  if (on(q.f(me, f_super_damage_finished)))
    q.set_f(me, q.L.f.effects, bor(bor(q.f(me, q.L.f.effects), kEfDimlight), kEfBlue));
  if (on(q.f(me, f_invincible_finished)))
    q.set_f(me, q.L.f.effects, bor(bor(q.f(me, q.L.f.effects), kEfDimlight), kEfRed));
}

void item_precache(A& q) { q.precache_sound(q.s(s_itembk2_wav)); }

}  // namespace shooters_edge::server::rules::SE_RULES_NS
