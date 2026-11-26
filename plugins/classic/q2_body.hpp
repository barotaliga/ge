// QUAKE II'S ARMOUR AND HEALTH, AS ANY RULES TAKE THEM: the three armours' counts and shares, how
// a picked-up armour joins the one worn, and what each health item gives, in one place, so Quake
// II's own rules and another game's rules wearing Quake II's things (classic remix's `cr_items
// q2` on Quake's rules) take them by the same numbers. Header-only and plain, so a rules package
// that links nothing but the program interface can include it.
//
// - **Armour** (`kQ2Armours`, `q2_take_armour`): the jacket 25 (at most 50) taking 30% of a normal
//   blow, the combat 50 (100) taking 60% (30% of an energy blow), the body 100 (200) taking 80%
//   (60%). A better armour takes the worn one's count over at the worn one's share against its
//   own; a worse or equal one adds its count at its share against the worn one's, up to the worn
//   one's limit, and is left lying when that adds nothing. A shard adds 2, onto a jacket if none
//   is worn.
// - **Health** (`kQ2Health*`): the stimpack 2 and the mega health 100 past the limit, the medium
//   10 and the large 25 up to it; the mega's surplus drains from 5 s after it is taken.
// - **Ammunition** (`kQ2Ammo*`, `kQ2Most*`): what a box gives and how much is carried.
//
// ge:summary Quake II's armour and health as any rules take them: counts, shares, the take rule
// ge:keywords q2 armour armor health jacket combat-armour body-armour shard salvage mega-health
// ge:keywords stimpack q2-body remix-items cr_items real-items
// ge:skill classic shooter
// ge:test tests/shooters_edge_remix_bodies_test.cpp
#pragma once

#include <cstdint>

namespace ge::classic {

// An armour's count when picked up, its most, and the share of a normal and an energy blow it
// takes. Indexed by tier: 0 none, 1 jacket, 2 combat, 3 body.
struct Q2ArmourRow {
  int base, maximum;
  float normal, energy;
};
inline constexpr Q2ArmourRow kQ2Armours[] = {
    {0, 0, 0, 0},              // none
    {25, 50, 0.30f, 0.00f},    // jacket
    {50, 100, 0.60f, 0.30f},   // combat
    {100, 200, 0.80f, 0.60f},  // body
};
inline constexpr int kQ2ArmourShard = 2;

// The armour worn after `tier` (0: a shard) is picked up over `worn` holding `count`; `taken`
// false when it stays lying.
struct Q2Armour {
  int tier = 0;
  int count = 0;
  bool taken = false;
};
constexpr Q2Armour q2_take_armour(int worn, int count, int tier) {
  if (tier == 0) return {worn == 0 ? 1 : worn, count + kQ2ArmourShard, true};
  const Q2ArmourRow& fresh = kQ2Armours[tier];
  if (worn == 0) return {tier, fresh.base, true};
  const Q2ArmourRow& had = kQ2Armours[worn];
  if (fresh.normal > had.normal) {  // the new one is better: the old converted onto it
    const int carried = static_cast<int>(float(count) * (had.normal / fresh.normal));
    const int now = fresh.base + carried;
    return {tier, now < fresh.maximum ? now : fresh.maximum, true};
  }
  // The worn one is as good or better: the new converted onto it.
  const int added = static_cast<int>(float(fresh.base) * (fresh.normal / had.normal));
  int now = count + added;
  if (now > had.maximum) now = had.maximum;
  if (now <= count) return {worn, count, false};
  return {worn, now, true};
}

// The tier whose normal share is `share` (another game's armour row read back), 0 for none.
constexpr int q2_armour_tier(float share) {
  for (int t = 3; t >= 1; t--)
    if (share >= kQ2Armours[t].normal - 0.001f) return t;
  return 0;
}

// Health: what each gives and whether it goes past the limit.
inline constexpr int kQ2HealthStim = 2, kQ2HealthMedium = 10, kQ2HealthLarge = 25,
                     kQ2HealthMega = 100;
// Ammunition: what a box gives (shells, bullets, rockets, slugs; cells 50 and grenades 5 too).
inline constexpr int kQ2AmmoShells = 10, kQ2AmmoBullets = 50, kQ2AmmoRockets = 5, kQ2AmmoSlugs = 10;
inline constexpr int kQ2AmmoCells = 50, kQ2AmmoGrenades = 5;

// The carry limits.
inline constexpr int kQ2MostShells = 100, kQ2MostBullets = 200, kQ2MostRockets = 50,
                     kQ2MostGrenades = 50, kQ2MostCells = 200, kQ2MostSlugs = 50;

inline constexpr float kQ2MegaDrainAfter = 5;  // s, then a point a second down to the limit

}  // namespace ge::classic
