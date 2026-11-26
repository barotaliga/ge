// QUAKE'S ARMOUR AND HEALTH, AS ANY RULES TAKE THEM: the three armours' shares and values, when a
// picked-up armour replaces the one worn, and what each health box gives, in one place, so Quake's
// own rules and another game's rules wearing Quake's things (classic remix's `cr_items q1` on
// Quake II's rules) take them by the same numbers. Header-only and plain.
//
// - **Armour** (`kQ1Armours`, `q1_takes_armour`): the green 100 taking 30% of a blow, the yellow
//   150 taking 60%, the red 200 taking 80%. A picked-up armour replaces the worn one outright
//   when its share times its value is more than the worn one's, and is left lying otherwise.
//   Quake's armour knows no energy blows: every blow is shared alike.
// - **Health** (`kQ1Health*`): the small box 15 and the plain one 25 up to the limit, the mega
//   health 100 up to 250, its surplus draining from 5 s after it is taken.
//
// ge:summary Quake's armour and health as any rules take them: shares, values, the replace rule
// ge:keywords q1 armour armor health green-armour yellow-armour red-armour mega-health q1-body
// ge:keywords remix-items cr_items real-items
// ge:skill classic shooter
// ge:test tests/shooters_edge_remix_bodies_test.cpp
#pragma once

namespace ge::classic {

// An armour's share of a blow and its value when picked up; indexed by tier: 0 none, 1 green,
// 2 yellow, 3 red.
struct Q1ArmourRow {
  float share;
  int value;
};
inline constexpr Q1ArmourRow kQ1Armours[] = {{0, 0}, {0.3f, 100}, {0.6f, 150}, {0.8f, 200}};

// Whether armour `tier` picked up replaces `worn` (a tier) holding `value`.
constexpr bool q1_takes_armour(int worn, int value, int tier) {
  return kQ1Armours[worn].share * float(value) <
         kQ1Armours[tier].share * float(kQ1Armours[tier].value);
}

// Health: the boxes' points, the mega health's and its limit.
inline constexpr int kQ1HealthSmall = 15, kQ1HealthPlain = 25, kQ1HealthMega = 100;
inline constexpr int kQ1MegaLimit = 250;
inline constexpr float kQ1MegaDrainAfter = 5;  // s, then a point a second down to the limit

}  // namespace ge::classic
