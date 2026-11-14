// Deathmatch 4 ("deathmatch 4"): a duel or a free-for-all where nobody gathers anything. Every
// spawn is armed with every weapon and full of everything, so the match is the aim and the move:
//
//   THE SPAWN       health 250 (the maximum stays 100), 200 red armour at 80 %, 255 of every
//                   ammunition, every weapon but those `k_disallow_weapons` names, the rocket
//                   launcher in hand (else the arc, the heavy spiker, the double, the spiker, the
//                   scattergun, the blade).
//   THE PROTECTION  `dmm4_invinc_time` seconds after a spawn (unset or 0: 2 s; negative: none; at
//                   most 30): no harm to health from anything but the kill command; told almost
//                   burned out 3 s before it ends, a flash a second after; a soaked hit heard
//                   every 2 s at most. Health, armour and packs are not taken while a shield holds.
//   THE MAP         no weapon, ammunition or mega health lies on it; ammunition is never used up.
//   THE BLOWS       the quad is the OctaPower (eight times the damage); taking it strips the
//                   armour and the cells, and as it wears off health is cut to 100, the armour set
//                   to a point of red and the cells to 255. The blade cuts for 75; the arc fired
//                   under water kills its firer one time in two.
//   THE PACKS       a dead player's pack is 10 more health; at 300 or more, 30 s of the shield and
//                   the OctaPower, the cells emptied, told to all.
//
// ge:summary The arena's deathmatch 4 rules in C++: the full spawn, the protection, the packs
// ge:keywords arena native-arena dmm4 deathmatch-4 octapower spawn-protection dmm4_give
// ge:skill progs shooter
// ge:test tests/shooters_edge_arena_rules_test.cpp
#include <cstdint>

#include "shooters_edge/rules/variant.hpp"

namespace shooters_edge::server::rules::SE_RULES_NS {

namespace {

constexpr float kDmm4Health = 250;
constexpr float kDmm4Armour = 200;
constexpr float kDmm4Ammo = 255;
constexpr float kDmm4Pack = 10;    // a pack's health
constexpr float kDmm4Bonus = 300;  // health that wins the bonus powers
constexpr float kDmm4Powers = 30;  // their seconds

// The protection's seconds after a spawn (0: none).
float dmm4_shield_time(A& q) {
  const float t = q.cvar(q.s(s_dmm4_invinc_time));
  if (t < 0) return 0;
  if (t == 0) return 2;
  if (t > 30) return 30;
  return t;
}

// The weapons a spawn holds.
float dmm4_weapons(A& q) {
  const float all =
      kWBlade | kWScatter | kWDouble | kWSpiker | kWHeavySpiker | kWGrenades | kWRockets | kWArc;
  return all - band(all, q.cvar(q.s(s_k_disallow_weapons)));
}

// The weapon in hand at a spawn: the rocket launcher, else the best of the rest but a launcher.
void dmm4_hand(A& q) {
  const int32_t me = q.self();
  const float items = q.f(me, q.L.f.items);
  if (on(band(items, kWRockets))) {
    weapon_set(q, kWRockets);
  } else if ((band(items, kWArc) != 0) & (q.f(me, q.L.f.waterlevel) <= 1)) {
    weapon_set(q, kWArc);
  } else if (on(band(items, kWHeavySpiker))) {
    weapon_set(q, kWHeavySpiker);
  } else if (on(band(items, kWDouble))) {
    weapon_set(q, kWDouble);
  } else if (on(band(items, kWSpiker))) {
    weapon_set(q, kWSpiker);
  } else if (on(band(items, kWScatter))) {
    weapon_set(q, kWScatter);
  } else {
    weapon_set(q, kWBlade);
  }
}

}  // namespace

// A spawn's inventory and protection.
void dmm4_give(A& q) {
  const int32_t me = q.self();
  q.set_f(me, q.L.f.items, bor(dmm4_weapons(q), kItArmor3));
  q.set_f(me, q.L.f.health, kDmm4Health);
  q.set_f(me, q.L.f.armortype, 0.8f);
  q.set_f(me, q.L.f.armorvalue, kDmm4Armour);
  q.set_f(me, q.L.f.ammo_shells, kDmm4Ammo);
  q.set_f(me, q.L.f.ammo_nails, kDmm4Ammo);
  q.set_f(me, q.L.f.ammo_rockets, kDmm4Ammo);
  q.set_f(me, q.L.f.ammo_cells, kDmm4Ammo);
  q.set_f(me, f_shield_finished, 0);
  q.set_f(me, f_shield_said, 0);
  const float t = dmm4_shield_time(q);
  if (t > 0) {
    q.set_f(me, f_shield_finished, q.time() + t);
    q.set_f(me, q.L.f.items, bor(q.f(me, q.L.f.items), kItInvulnerability));
  }
  dmm4_hand(q);
}

// True while any shield holds `p` (the spawn's or the power's).
bool dmm4_shielded(A& q, int32_t p) {
  const bool spawn = q.f(p, f_shield_finished) > q.time();
  const bool power = q.f(p, f_invincible_finished) > q.time();
  return spawn | power;
}

// Each frame for a living player: the protection told running out, a flash a second, and gone.
void dmm4_frame(A& q) {
  const int32_t me = q.self();
  if (q.f(me, f_shield_finished) == 0) return;
  if (q.f(me, f_shield_finished) <= q.time()) {
    q.set_f(me, f_shield_finished, 0);
    if (q.f(me, f_invincible_finished) <= q.time()) {
      const float items = q.f(me, q.L.f.items);
      q.set_f(me, q.L.f.items, items - band(items, kItInvulnerability));
    }
    return;
  }
  if ((q.f(me, f_shield_said) == 0) & (q.f(me, f_shield_finished) - 3 <= q.time())) {
    q.set_f(me, f_shield_said, 1);
    q.set_f(me, f_shield_flash, q.time() + 1);
    say_to(q, me, kPrintHigh, {q.s(s_protection_burned_out)});
    q.sound(me, kChanItem, q.s(s_protect2_wav), 1, kAttnNorm);
    q.stuffcmd(me, q.s(s_bf));
  } else if ((q.f(me, f_shield_said) != 0) & (q.time() >= q.f(me, f_shield_flash))) {
    q.set_f(me, f_shield_flash, q.f(me, f_shield_flash) + 1);
    q.stuffcmd(me, q.s(s_bf));
  }
}

// A hit the protection soaked: heard, every 2 s at most.
void dmm4_soaked(A& q, int32_t targ) {
  if (q.time() < q.f(targ, f_shield_heard)) return;
  q.set_f(targ, f_shield_heard, q.time() + 2);
  q.sound(targ, kChanItem, q.s(s_protect3_wav), 1, kAttnNorm);
}

// The OctaPower taken by `other`: the armour and the cells go.
void dmm4_octa_taken(A& q) {
  const int32_t o = q.other();
  q.set_f(o, q.L.f.armortype, 0);
  q.set_f(o, q.L.f.armorvalue, 0);
  const float items = q.f(o, q.L.f.items);
  q.set_f(o, q.L.f.items, items - band(items, kItArmorAll));
  q.set_f(o, q.L.f.ammo_cells, 0);
}

// The OctaPower worn off `self`: health 100 at most, a point of red armour, the cells back.
void dmm4_octa_off(A& q) {
  const int32_t me = q.self();
  if (q.f(me, q.L.f.health) > 100) q.set_f(me, q.L.f.health, 100);
  q.set_f(me, q.L.f.armortype, 0.8f);
  q.set_f(me, q.L.f.armorvalue, 1);
  const float items = q.f(me, q.L.f.items);
  q.set_f(me, q.L.f.items, items - band(items, kItArmorAll) + kItArmor3);
  q.set_f(me, q.L.f.ammo_cells, kDmm4Ammo);
  weapon_show(q);
}

// A pack taken by `other` (the pack is `self`): 10 health, the bonus powers at 300.
void dmm4_pack_take(A& q) {
  const int32_t o = q.other();
  q.set_f(o, q.L.f.health, q.f(o, q.L.f.health) + kDmm4Pack);
  say_to(q, o, kPrintLow, {q.s(s_additional_health)});
  q.stuffcmd(o, q.s(s_bf));
  const float health = q.f(o, q.L.f.health);
  if ((health > kDmm4Health) & (health < kDmm4Bonus)) {
    q.sound(o, kChanItem, q.s(s_protect3_wav), 1, kAttnNorm);
  } else {
    q.sound(o, kChanItem, q.s(s_pkup_wav), 1, kAttnNorm);
  }
  if (q.f(o, q.L.f.health) >= kDmm4Bonus) {
    q.set_f(o, f_invincible_finished, q.time() + kDmm4Powers);
    q.set_f(o, f_super_damage_finished, q.time() + kDmm4Powers);
    const float said = q.f(o, f_power_said);
    q.set_f(o, f_power_said, said - band(said, kItQuad | kItInvulnerability));
    q.set_f(o, q.L.f.items, bor(bor(q.f(o, q.L.f.items), kItQuad), kItInvulnerability));
    q.set_f(o, q.L.f.ammo_cells, 0);
    q.sound(o, kChanVoice, q.s(s_protect_wav), 1, kAttnNone);
    say_all(q, kPrintHigh, {q.i(o, q.L.f.netname), q.s(s_gained_bonus_powers)});
  }
}

void dmm4_precache(A& q) {
  q.precache_sound(q.s(s_protect_wav));
  q.precache_sound(q.s(s_protect2_wav));
  q.precache_sound(q.s(s_protect3_wav));
  q.precache_sound(q.s(s_pkup_wav));
}

}  // namespace shooters_edge::server::rules::SE_RULES_NS
