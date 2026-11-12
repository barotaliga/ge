// WEAPON GROUPS: a group key asks for the best ready weapon of its group, again the next one round
// to the first; a group with nothing ready asks for nothing; a key that is no group is not taken;
// the attack folds the open group, and so does kOpenS without a press; the look shows the group's
// carried weapons best first with the one asked for lit, folds to the weapon in hand, and marks a
// weapon whose group holds another weapon carried.
//
// ge:summary Tests: weapon groups' press, cycle, fire, fold and the look's second-weapon mark
// ge:keywords test weapon-groups group_press group_fire group_look group_of is_group
// ge:skill shooter testing
#include <initializer_list>

#include "check.hpp"
#include "shooter/quick_weapon.hpp"
#include "shooter/weapon_groups.hpp"

namespace sh = ge::shooter;

namespace {

// The classic arena's groups: 1 explosives (7 6), 2 the beam gun (8), 3 the shotguns (3 2),
// 4 the nailguns (5 4), 5 the melee weapon (1).
sh::WeaponGroups classic() {
  sh::WeaponGroups g;
  const char* text[] = {"7 6", "8", "3 2", "5 4", "1"};
  for (size_t k = 0; k < 5; ++k) g[k] = sh::weapon_list(text[k]);
  return g;
}

bool near(double a, double b, double e) { return a > b - e && a < b + e; }

uint32_t bits(std::initializer_list<int> ws) {
  uint32_t b = 0;
  for (const int w : ws) b |= 1u << w;
  return b;
}

}  // namespace

TEST(weapon_groups) {
  const sh::WeaponGroups g = classic();

  // Which keys are groups, and which group holds a weapon.
  CHECK(sh::is_group(g, 1) && sh::is_group(g, 5));
  CHECK(!sh::is_group(g, 6) && !sh::is_group(g, 0) && !sh::is_group(g, 10));
  CHECK(sh::group_of(g, 6) == 0);
  CHECK(sh::group_of(g, 2) == 2);
  CHECK(sh::group_of(g, 0) == -1);

  // The best ready first, then the next, then round to the first.
  {
    sh::GroupState s;
    const sh::GroupArmed a{
        .carried = bits({1, 2, 6, 7}), .ready = bits({1, 2, 6, 7}), .in_hand = 2};
    CHECK(sh::group_press(s, g, a, 1, 0.0) == 7);
    CHECK(sh::group_press(s, g, a, 1, 0.3) == 6);
    CHECK(sh::group_press(s, g, a, 1, 0.6) == 7);
    // Another group's key starts at its own best.
    CHECK(sh::group_press(s, g, a, 3, 0.7) == 2);
    // A group with nothing ready asks for nothing.
    CHECK(sh::group_press(s, g, a, 2, 0.8) == 0);
    // A key that is no group is not taken.
    CHECK(sh::group_press(s, g, a, 7, 0.9) == 0);
  }

  // A weapon carried without ammunition is passed over, but still shown, unlit as not ready.
  {
    sh::GroupState s;
    const sh::GroupArmed a{.carried = bits({1, 2, 6, 7}), .ready = bits({1, 2, 6}), .in_hand = 2};
    CHECK(sh::group_press(s, g, a, 1, 0.0) == 6);
    CHECK(sh::group_press(s, g, a, 1, 0.2) == 6);  // the only one ready, again
    const sh::GroupLook l = sh::group_look(s, g, a, 0.3);
    CHECK(l.count == 2);
    CHECK(l.weapons[0] == 7);
    CHECK(!l.ready[0] && l.ready[1]);
    CHECK(l.lit == 1);
  }

  // Open after a press; the attack folds it over kCloseS into the weapon in hand; a press after
  // that starts at the best again.
  {
    sh::GroupState s;
    sh::GroupArmed a{.carried = bits({1, 2, 6, 7}), .ready = bits({1, 2, 6, 7}), .in_hand = 2};
    CHECK(sh::group_press(s, g, a, 1, 1.0) == 7);
    CHECK(sh::group_press(s, g, a, 1, 1.2) == 6);
    a.in_hand = 6;
    sh::GroupLook l = sh::group_look(s, g, a, 1.5);
    CHECK(l.group == 0);
    CHECK(l.count == 2);
    CHECK(l.weapons[0] == 7);
    CHECK(l.weapons[1] == 6);
    CHECK(l.lit == 1);
    CHECK(near(l.open, 1.0, 1e-6));
    sh::group_fire(s, 1.6);
    l = sh::group_look(s, g, a, 1.6 + sh::kCloseS / 2);
    CHECK(near(l.open, 0.5, 1e-3));
    CHECK(l.count == 2);
    l = sh::group_look(s, g, a, 1.6 + sh::kCloseS + 0.01);
    CHECK(l.count == 1);
    CHECK(l.weapons[0] == 6);
    CHECK(near(l.open, 0.0, 1e-6));
    CHECK(l.more);  // the rocket launcher is the group's other weapon
    CHECK(sh::group_press(s, g, a, 1, 2.0) == 7);
  }

  // Without the attack the group folds kOpenS after its last press; a press then starts afresh.
  {
    sh::GroupState s;
    const sh::GroupArmed a{.carried = bits({1, 2, 3}), .ready = bits({1, 2, 3}), .in_hand = 3};
    CHECK(sh::group_press(s, g, a, 3, 0.0) == 3);
    CHECK(near(sh::group_look(s, g, a, sh::kOpenS - 0.01).open, 1.0, 1e-6));
    CHECK(sh::group_look(s, g, a, sh::kOpenS + sh::kCloseS + 0.01).count == 1);
    CHECK(sh::group_press(s, g, a, 3, sh::kOpenS + 1) == 3);  // the best, not the next
  }

  // Closed: the weapon in hand alone, marked only while its group holds another carried.
  {
    const sh::GroupState s;
    sh::GroupArmed a{.carried = bits({1, 2, 8}), .ready = bits({1, 2, 8}), .in_hand = 8};
    sh::GroupLook l = sh::group_look(s, g, a, 5.0);
    CHECK(l.group == 1);
    CHECK(l.count == 1);
    CHECK(!l.more);  // the beam gun is alone in its group
    a.in_hand = 2;
    a.carried = bits({1, 2, 3});
    l = sh::group_look(s, g, a, 5.0);
    CHECK(l.more);  // the shotgun's group holds the double shotgun
    a.carried = bits({1, 2});
    CHECK(!sh::group_look(s, g, a, 5.0).more);
  }
}
