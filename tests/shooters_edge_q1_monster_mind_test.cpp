// Quake's monsters' mind (server/q1_monster_mind.hpp): the decisions the classic campaign's own
// monsters and Quake's monsters under Quake II's rules both take. Each rule is held on its own
// here; the campaign at work by shooters_edge_campaign_rules_test.cpp and the lab digests.
//
// ge:summary Tests Quake's monster mind: ranges, sight, the attack test, leaps, kinds, infighting
// ge:keywords q1 monster-mind test check-attack attack-chance sight dog-leap infighting
// ge:skill testing shooter
#include <string_view>
#include <vector>

#include "check.hpp"
#include "shooters_edge/server/q1_monster_mind.hpp"

namespace {

using namespace shooters_edge::server;
using q1_mind::Range;

struct Draws {
  std::vector<float> next;
  size_t used = 0;
  float operator()() { return next[used++]; }
};

TEST(the_ranges_are_120_500_and_1000_eye_to_eye) {
  CHECK(q1_mind::range_of(119.9f) == Range::melee);
  CHECK(q1_mind::range_of(120) == Range::near);
  CHECK(q1_mind::range_of(500) == Range::mid);
  CHECK(q1_mind::range_of(1000) == Range::far);
}

TEST(sight_near_needs_him_in_front_or_firing_and_mid_in_front) {
  q1_mind::Sighting s{Range::near, true, false, false};
  CHECK(!q1_mind::sees(s));
  s.fired = true;
  CHECK(q1_mind::sees(s));
  s = {Range::mid, true, true, false};
  CHECK(!q1_mind::sees(s));  // a shot is no help at mid range
  s.ahead = true;
  CHECK(q1_mind::sees(s));
  s.clear = false;
  CHECK(!q1_mind::sees(s));
  s = {Range::melee, true, false, false};
  CHECK(q1_mind::sees(s));
  s = {Range::far, true, true, true};
  CHECK(!q1_mind::sees(s));
}

TEST(the_usual_test_strikes_in_reach_else_fires_by_its_chance) {
  Draws d{{0.19f, 0.21f, 0.39f, 0.04f}};
  q1_mind::Shot s{Range::melee, true, true, false};
  CHECK(q1_mind::check_attack(s, d) == q1_mind::Act::melee);
  CHECK(d.used == 0);  // no draw for a blow
  s = {Range::near, true, true, true};
  CHECK(q1_mind::check_attack(s, d) == q1_mind::Act::missile);  // 0.2 with a blow
  CHECK(q1_mind::check_attack(s, d) == q1_mind::Act::none);
  s.has_melee = false;
  CHECK(q1_mind::check_attack(s, d) == q1_mind::Act::missile);  // 0.4 without
  s = {Range::mid, true, true, true};
  CHECK(q1_mind::check_attack(s, d) == q1_mind::Act::missile);  // 0.05
  s.ready = false;
  CHECK(q1_mind::check_attack(s, d) == q1_mind::Act::none);
  CHECK(d.used == 4);  // its wait not spent: no draw
  CHECK(q1_mind::attack_chance(Range::melee, false) == 0.9f);
}

TEST(the_hound_and_the_fiend_leap_within_their_reach) {
  CHECK(q1_mind::leaps(q1_mind::kHoundLeap, 32, 80));
  CHECK(!q1_mind::leaps(q1_mind::kHoundLeap, 33, 100));
  CHECK(!q1_mind::leaps(q1_mind::kHoundLeap, 0, 151));
  CHECK(q1_mind::leaps(q1_mind::kFiendLeap, 64, 350));
  CHECK(!q1_mind::leaps(q1_mind::kFiendLeap, 0, 99));
}

TEST(each_kind_waits_and_flinches_its_own_and_a_strong_shambler_shrugs) {
  CHECK(q1_mind::pain_of("monster_army").wait == 1 && q1_mind::pain_of("monster_army").frames == 3);
  CHECK(q1_mind::pain_of("monster_ogre").wait == 1.5f);
  CHECK(q1_mind::pain_of("monster_shambler").wait == 2);
  CHECK(q1_mind::pain_of("monster_shalrath").wait == 3);
  CHECK(q1_mind::pain_of("monster_wizard").frames == 2);
  CHECK(q1_mind::pain_of("monster_zombie").frames == 0);  // it only cries
  CHECK(q1_mind::pain_of("monster_tarbaby").wait == 0);   // none
  Draws d{{0.4f, 0.4f, 0.6f}};
  CHECK(!q1_mind::shrugs("monster_ogre", 900, d));
  CHECK(d.used == 0);  // no draw but a shambler's
  CHECK(q1_mind::shrugs("monster_shambler", 600, d));
  CHECK(!q1_mind::shrugs("monster_shambler", 400, d));  // drawn, but not strong
  CHECK(!q1_mind::shrugs("monster_shambler", 600, d));
  CHECK(d.used == 3);
}

TEST(the_scrag_slides_near_him_goes_straight_far_and_fires_by_its_own_chances) {
  Draws d{{0.59f, 0.61f, 0.19f, 0.21f, 0.5f, 0.0f, 0.2f}};
  auto t = q1_mind::scrag_check(Range::near, true, d);
  CHECK(t.attack && t.mode == q1_mind::Mode::keep);  // 0.6 near
  t = q1_mind::scrag_check(Range::near, true, d);
  CHECK(!t.attack && t.mode == q1_mind::Mode::slide);
  t = q1_mind::scrag_check(Range::mid, true, d);
  CHECK(t.attack);  // 0.2 mid
  t = q1_mind::scrag_check(Range::mid, true, d);
  CHECK(!t.attack && t.mode == q1_mind::Mode::straight);
  CHECK(d.used == 4);
  t = q1_mind::scrag_check(Range::far, true, d);
  CHECK(!t.attack && t.mode == q1_mind::Mode::straight);
  t = q1_mind::scrag_check(Range::melee, false, d);
  CHECK(!t.attack && t.mode == q1_mind::Mode::straight);
  CHECK(d.used == 4);  // no draw without a clear shot in range
  CHECK(q1_mind::scrag_after_spit(Range::melee, true) == q1_mind::Mode::slide);
  CHECK(q1_mind::scrag_after_spit(Range::near, false) == q1_mind::Mode::straight);
  CHECK(q1_mind::scrag_after_spit(Range::mid, true) == q1_mind::Mode::straight);
  CHECK(q1_mind::test_of("monster_wizard") == q1_mind::Test::scrag);
  CHECK(!q1_mind::flinches("monster_wizard", 30, d));  // 0.5 × 70 = 35 > 30
  CHECK(q1_mind::flinches("monster_wizard", 1, d));    // 0
  CHECK(q1_mind::flinches("monster_ogre", 1, d));      // no draw
  CHECK(d.used == 6);
}

TEST(the_kinds_with_tests_of_their_own_and_who_turns_on_whom) {
  CHECK(q1_mind::test_of("monster_dog") == q1_mind::Test::hound);
  CHECK(q1_mind::test_of("monster_hell_knight") == q1_mind::Test::death_knight);
  CHECK(q1_mind::test_of("monster_ogre") == q1_mind::Test::usual);
  CHECK(q1_mind::turns_on(true, false));
  CHECK(!q1_mind::turns_on(false, false));
  CHECK(q1_mind::turns_on(false, true));  // the rifleman turns on its own kind
  bool quake = false, q2 = false;
  for (const roster::Kind& k : roster::kKinds) {
    if (std::string_view(k.classname) == "monster_army") quake = q1_mind::runs(k);
    if (std::string_view(k.classname) == "monster_soldier") q2 = q1_mind::runs(k);
  }
  CHECK(quake && !q2);
}

}  // namespace
