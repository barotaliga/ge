// A PLAYER'S PERSONALITY (plugins/shooter/traits.hpp): the presets put every trait at the skill's
// value and the temperament's aggression; the steps read a preset back as its skill; the smooth
// reading hits the presets' rows exactly, so a preset's aim is the skill's to the bit; and the
// text form reads back.
//
// ge:summary Tests: the traits' presets, steps, smooth reading, aim hands and text form
// ge:keywords test traits preset tier_of scale aim_profile apply_traits traits_text
// ge:skill shooter testing
#include <cstring>

#include "check.hpp"
#include "shooter/aim.hpp"
#include "shooter/traits.hpp"

namespace sh = ge::shooter;

TEST(a_preset_is_its_skill_trait_by_trait_and_its_temperament_in_aggression) {
  for (sh::AimSkill k : {sh::AimSkill::novice, sh::AimSkill::average, sh::AimSkill::expert}) {
    const sh::Traits t = sh::traits_of(k, sh::Temper::balanced);
    for (size_t i = 0; i < sh::kTraits; i++)
      if (i != static_cast<size_t>(sh::Trait::aggression)) CHECK(sh::tier_of(t.v[i]) == k);
    CHECK(sh::temper_of(t) == sh::Temper::balanced);
    CHECK(sh::selfish(t) == (k == sh::AimSkill::novice));  // a novice is a poor teammate
    CHECK(sh::temper_of(sh::traits_of(k, sh::Temper::aggressive)) == sh::Temper::aggressive);
    CHECK(sh::temper_of(sh::traits_of(k, sh::Temper::coward)) == sh::Temper::coward);
    CHECK(sh::selfish(sh::traits_of(k, sh::Temper::aggressive)));
  }
}

TEST(a_presets_aim_is_its_skills_to_the_bit_and_between_lies_between) {
  for (sh::AimSkill k : {sh::AimSkill::novice, sh::AimSkill::average, sh::AimSkill::expert}) {
    const uint8_t v = sh::traits_of(k, sh::Temper::balanced)[sh::Trait::aim_rl];
    const sh::AimProfile a = sh::aim_profile(k), b = sh::aim_profile(v, v);
    CHECK_BITS(a.lead_s, b.lead_s);
    for (size_t h = 0; h < 2; h++) {
      CHECK_BITS(a.hands[h].delay_s, b.hands[h].delay_s);
      CHECK_BITS(a.hands[h].wander, b.hands[h].wander);
      CHECK_BITS(a.hands[h].settle_s, b.hands[h].settle_s);
      CHECK_BITS(a.hands[h].damping, b.hands[h].damping);
      CHECK_BITS(a.hands[h].slew, b.hands[h].slew);
    }
  }
  // A 6 aims between the average and the expert; a 10 reacts faster than the expert.
  const sh::AimProfile six = sh::aim_profile(6, 6);
  CHECK(six.hands[0].wander < 1.5 && six.hands[0].wander > 0.6);
  CHECK(sh::aim_profile(8, 10).hands[0].delay_s < sh::aim_profile(8, 8).hands[0].delay_s);
  CHECK(sh::edge_of(8) == sh::self_edge(sh::AimSkill::expert));
  CHECK(sh::edge_of(10) == sh::self_edge(sh::AimSkill::perfect));
}

TEST(traits_are_written_and_read_back_and_a_bad_one_is_named) {
  sh::Traits t = sh::traits_of(sh::AimSkill::average, sh::Temper::balanced);
  std::string error;
  CHECK(sh::apply_traits("aim_rl=9/timing=3", t, error));
  CHECK(t[sh::Trait::aim_rl] == 9 && t[sh::Trait::timing] == 3 && t[sh::Trait::aim_lg] == 5);
  sh::Traits back = sh::traits_of(sh::AimSkill::novice, sh::Temper::coward);
  CHECK(sh::apply_traits(sh::traits_text(t), back, error));
  CHECK(back.v == t.v);
  CHECK(!sh::apply_traits("aim_bfg=9", t, error));
  CHECK(error.find("aim_bfg") != std::string::npos);
  CHECK(!sh::apply_traits("timing=11", t, error));
  CHECK(!sh::apply_traits("timing", t, error));
}
