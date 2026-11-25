// A PLAYER'S PERSONALITY (plugins/shooterbots/traits.hpp): the presets put every trait at the
// skill's value and the temperament's aggression; the steps read a preset back as its skill; the
// smooth reading hits the presets' rows exactly, so a preset's aim is the skill's to the bit; and
// the text form reads back.
//
// ge:summary Tests: the traits' presets, steps, smooth reading, aim hands and text form
// ge:keywords test traits preset tier_of scale aim_profile apply_traits traits_text
// ge:skill shooter testing
#include <cstring>

#include "check.hpp"
#include "shooterbots/aim.hpp"
#include "shooterbots/traits.hpp"

namespace sh = ge::shooter;

namespace sb = ge::shooterbots;

TEST(a_preset_is_its_skill_trait_by_trait_and_its_temperament_in_aggression) {
  for (sb::AimSkill k : {sb::AimSkill::novice, sb::AimSkill::average, sb::AimSkill::expert}) {
    const sb::Traits t = sb::traits_of(k, sb::Temper::balanced);
    for (size_t i = 0; i < sb::kTraits; i++)
      if (i != static_cast<size_t>(sb::Trait::aggression)) CHECK(sb::tier_of(t.v[i]) == k);
    CHECK(sb::temper_of(t) == sb::Temper::balanced);
    CHECK(sb::selfish(t) == (k == sb::AimSkill::novice));  // a novice is a poor teammate
    CHECK(sb::temper_of(sb::traits_of(k, sb::Temper::aggressive)) == sb::Temper::aggressive);
    CHECK(sb::temper_of(sb::traits_of(k, sb::Temper::coward)) == sb::Temper::coward);
    CHECK(sb::selfish(sb::traits_of(k, sb::Temper::aggressive)));
  }
}

TEST(a_presets_aim_is_its_skills_to_the_bit_and_between_lies_between) {
  for (sb::AimSkill k : {sb::AimSkill::novice, sb::AimSkill::average, sb::AimSkill::expert}) {
    const uint8_t v = sb::traits_of(k, sb::Temper::balanced)[sb::Trait::aim_rl];
    const sb::AimProfile a = sb::aim_profile(k), b = sb::aim_profile(v, v);
    CHECK_BITS(a.lead_s, b.lead_s);
    CHECK_BITS(a.react_lo, b.react_lo);
    CHECK_BITS(a.react_med, b.react_med);
    CHECK_BITS(a.react_hi, b.react_hi);
    for (size_t h = 0; h < 2; h++) {
      CHECK_BITS(a.hands[h].delay_s, b.hands[h].delay_s);
      CHECK_BITS(a.hands[h].wander, b.hands[h].wander);
      CHECK_BITS(a.hands[h].settle_s, b.hands[h].settle_s);
      CHECK_BITS(a.hands[h].damping, b.hands[h].damping);
      CHECK_BITS(a.hands[h].slew, b.hands[h].slew);
    }
  }
  // A 6 aims between the average and the expert; a 10 reacts faster than the expert.
  const sb::AimProfile six = sb::aim_profile(6, 6);
  CHECK(six.hands[0].wander < 1.5 && six.hands[0].wander > 0.6);
  CHECK(sb::aim_profile(8, 10).hands[0].delay_s < sb::aim_profile(8, 8).hands[0].delay_s);
  CHECK(sb::edge_of(8) == sb::self_edge(sb::AimSkill::expert));
  // A reaction lies in its spread, half of the draws under the median, the slow side the longer.
  const sb::AimProfile ex = sb::aim_profile(sb::AimSkill::expert);
  CHECK(sb::draw_reaction(ex, 0) == ex.react_lo);
  CHECK(sb::draw_reaction(ex, 0.5) == ex.react_med);
  CHECK(sb::draw_reaction(ex, 0.999) < ex.react_hi && sb::draw_reaction(ex, 0.999) > 0.21);
  CHECK(ex.react_hi - ex.react_med > ex.react_med - ex.react_lo);
  CHECK(sb::aim_profile(sb::AimSkill::perfect).react_med == 0);
  // Every hand's own lag is under the fastest reaction it draws.
  for (uint8_t v = 1; v <= 10; v++)
    CHECK(sb::aim_profile(v, v).hands[0].delay_s < sb::aim_profile(v, v).react_lo);
  CHECK(sb::edge_of(10) == sb::self_edge(sb::AimSkill::perfect));
}

TEST(traits_are_written_and_read_back_and_a_bad_one_is_named) {
  sb::Traits t = sb::traits_of(sb::AimSkill::average, sb::Temper::balanced);
  std::string error;
  CHECK(sb::apply_traits("aim_rl=9/timing=3", t, error));
  CHECK(t[sb::Trait::aim_rl] == 9 && t[sb::Trait::timing] == 3 && t[sb::Trait::aim_lg] == 5);
  sb::Traits back = sb::traits_of(sb::AimSkill::novice, sb::Temper::coward);
  CHECK(sb::apply_traits(sb::traits_text(t), back, error));
  CHECK(back.v == t.v);
  CHECK(!sb::apply_traits("aim_bfg=9", t, error));
  CHECK(error.find("aim_bfg") != std::string::npos);
  CHECK(!sb::apply_traits("timing=11", t, error));
  CHECK(!sb::apply_traits("timing", t, error));
}
