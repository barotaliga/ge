// Quake II's monsters' mind (server/q2_monster_mind.hpp): the decisions Quake II's rules and the
// classic campaign wearing Quake II's monsters both take. Each rule is held on its own here; the
// monsters at work are held by shooters_edge_q2_monsters_test.cpp and the campaign's by
// shooters_edge_campaign_rules_test.cpp.
//
// ge:summary Tests Quake II's monster mind: ranges, sight, attack chance, picks, loops, fights
// ge:keywords q2 monster-mind test attack-chance pick-ranged sight infighting leap-test
// ge:skill testing shooter
#include <string_view>
#include <vector>

#include "check.hpp"
#include "shooters_edge/server/q2_monster_kinds.hpp"
#include "shooters_edge/server/q2_monster_mind.hpp"

namespace {

using namespace shooters_edge::server;
using q2_mind::Range;

// A draw that hands out the numbers given, in turn, and counts them.
struct Draws {
  std::vector<double> next;
  size_t used = 0;
  double operator()() { return next[used++]; }
};

const roster::Kind& kind(std::string_view classname) {
  for (const roster::Kind& k : roster::kKinds)
    if (classname == k.classname) return k;
  CHECK(false);
  return roster::kKinds[0];
}

TEST(the_ranges_are_80_500_and_1000) {
  CHECK(q2_mind::range_of(79.9f) == Range::melee);
  CHECK(q2_mind::range_of(80) == Range::near);
  CHECK(q2_mind::range_of(499) == Range::near);
  CHECK(q2_mind::range_of(500) == Range::mid);
  CHECK(q2_mind::range_of(1000) == Range::far);
}

TEST(sight_needs_a_clear_line_and_in_front_but_close_by_or_hunted) {
  q2_mind::Sighting s{Range::mid, true, false, false, false};
  CHECK(!q2_mind::sees(s));  // behind it, mid range
  s.ahead = true;
  CHECK(q2_mind::sees(s));
  s.clear = false;
  CHECK(!q2_mind::sees(s));  // never through a wall
  s = {Range::melee, true, false, false, false};
  CHECK(q2_mind::sees(s));  // close by: whichever way it faces
  s = {Range::near, true, false, true, false};
  CHECK(q2_mind::sees(s));  // near and he just fired
  s = {Range::near, true, true, false, false};
  CHECK(q2_mind::sees(s));  // near and already its foe
  s = {Range::far, true, true, true, true};
  CHECK(!q2_mind::sees(s));
}

TEST(facing_is_within_45_degrees_round_the_turn) {
  CHECK(q2_mind::faces(350, 30));
  CHECK(!q2_mind::faces(350, 40));
  CHECK(q2_mind::faces(90, 135));
}

TEST(the_attack_chance_by_range_and_skill) {
  CHECK(q2_mind::attack_chance(roster::Test::usual, false, Range::near, 1) == 0.1);
  CHECK(q2_mind::attack_chance(roster::Test::usual, false, Range::near, 0) == 0.05);
  CHECK(q2_mind::attack_chance(roster::Test::usual, false, Range::mid, 3) == 0.04);
  CHECK(q2_mind::attack_chance(roster::Test::usual, true, Range::mid, 1) == 0.4);
  CHECK(q2_mind::attack_chance(roster::Test::hover, false, Range::mid, 0) == 0.8);
  CHECK(q2_mind::attack_chance(roster::Test::rider, false, Range::near, 3) == 0.4);
}

TEST(a_blow_is_let_go_three_times_in_four_only_at_the_easiest_skill) {
  Draws d{{0.5}};
  CHECK(q2_mind::blow_skipped(roster::Test::usual, 0, d));
  CHECK(d.used == 1);
  CHECK(!q2_mind::blow_skipped(roster::Test::usual, 1, d));
  CHECK(d.used == 1);  // no draw above it
  CHECK(!q2_mind::blow_skipped(roster::Test::hover, 0, d));
}

TEST(the_tank_picks_by_range_and_the_rest_by_weight) {
  const roster::Kind& tank = kind("monster_tank");
  Draws d{{0.3, 0.3, 0.9, 0.5}};
  CHECK(q2_mind::pick_ranged(tank, 100, d) == 4);  // within 125: the gun 60% of the time
  CHECK(q2_mind::pick_ranged(tank, 200, d) == 4);  // within 250: even
  CHECK(q2_mind::pick_ranged(tank, 200, d) == 2);
  CHECK(q2_mind::pick_ranged(tank, 600, d) == 3);  // a third each farther
  const roster::Kind& soldier = kind("monster_soldier");
  Draws one{{0.99}};
  CHECK(q2_mind::pick_ranged(soldier, 300, one) == 2);  // its one shot
}

TEST(a_loop_goes_on_by_its_chance_only_while_its_foe_is_in_reach) {
  roster::Attack a;
  a.again = 0.5f;
  q2_mind::Loop l;
  l.foe_alive = true;
  l.clear = true;
  Draws d{{0.4, 0.6}};
  CHECK(q2_mind::goes_on(a, l, d));
  CHECK(!q2_mind::goes_on(a, l, d));
  l.clear = false;
  CHECK(!q2_mind::goes_on(a, l, d));  // no line: no draw, no loop
  l.blow = true;
  l.range = Range::near;
  CHECK(!q2_mind::goes_on(a, l, d));  // a blow out of reach
  l.mutant = true;
  l.skill = 3;
  Draws m{{0.1}};
  CHECK(q2_mind::goes_on(a, l, m));  // but the mutant's at skill 3
  a.again_skill = 2;
  l.skill = 1;
  CHECK(!q2_mind::goes_on(a, l, m));
}

TEST(the_mutant_leaps_at_a_foe_level_with_it_and_100_away) {
  Draws d{{0.05, 0.05, 0.05}};
  CHECK(q2_mind::leap_test(-24, 32, -24, 56, 150, d));
  CHECK(!q2_mind::leap_test(-24, 32, -24, 56, 90, d));    // too near
  CHECK(!q2_mind::leap_test(100, 156, -24, 56, 150, d));  // far above him
  CHECK(d.used == 1);
}

TEST(hurt_by_another_monster_it_turns_on_one_unlike_it_or_joins_it) {
  q2_mind::Attacker a;
  a.alike = true;
  CHECK(q2_mind::infight(a) == q2_mind::Turn::attacker);  // another kind, walking as it does
  a.same_kind = true;
  CHECK(q2_mind::infight(a) == q2_mind::Turn::none);
  a.has_live_foe = true;
  CHECK(q2_mind::infight(a) == q2_mind::Turn::attackers_foe);  // its own kind: joins it
  a.same_kind = false;
  a.heavy = true;
  CHECK(q2_mind::infight(a) == q2_mind::Turn::attackers_foe);  // a tank's shot starts no fight
  a.aiming_at_me = true;
  CHECK(q2_mind::infight(a) == q2_mind::Turn::attacker);
  CHECK(q2_mind::heavy("monster_tank_commander"));
  CHECK(!q2_mind::heavy("monster_gunner"));
}

}  // namespace
