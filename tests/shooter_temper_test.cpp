// A PLAYER'S TEMPERAMENT: the aggressive player takes every fight, the coward only one he wins
// many times over, and the balanced one a fight his skill wins: an expert's with a bare scattergun
// against a fresh enemy, a novice's only once he is far better armed.
//
// ge:summary Tests: temperaments take or turn from fights by might and skill
// ge:keywords test temper temperament aggressive balanced coward takes_fight chases_sound
// ge:skill shooter testing
#include "check.hpp"
#include "shooter/temper.hpp"

namespace sh = ge::shooter;

namespace {

constexpr float kFresh = 100;  // a fresh spawn: 100 health, the scattergun

}  // namespace

TEST(the_aggressive_take_every_fight_and_the_coward_only_a_lopsided_one) {
  for (sh::AimSkill k : {sh::AimSkill::novice, sh::AimSkill::average, sh::AimSkill::expert}) {
    CHECK(sh::takes_fight(sh::Temper::aggressive, k, 10, 1000));
    CHECK(sh::chases_sound(sh::Temper::aggressive, k, 200, 100));
    CHECK(!sh::takes_fight(sh::Temper::coward, k, kFresh, kFresh));
    CHECK(!sh::chases_sound(sh::Temper::coward, k, 5000, kFresh));
  }
  // An aggressive novice runs at any sound; a better one arms himself first.
  CHECK(sh::chases_sound(sh::Temper::aggressive, sh::AimSkill::novice, 10, 1000));
  CHECK(!sh::chases_sound(sh::Temper::aggressive, sh::AimSkill::expert, kFresh, 120));
  // The big armour, the mega and the launcher: (200 * 0.8 + 250) * 2 against a fresh enemy.
  const float stacked = (200 * 0.8f + 250) * 2;
  CHECK(sh::takes_fight(sh::Temper::coward, sh::AimSkill::novice, stacked, kFresh));
}

TEST(a_balanced_expert_fights_with_the_scattergun_and_a_balanced_novice_does_not) {
  CHECK(sh::takes_fight(sh::Temper::balanced, sh::AimSkill::expert, kFresh, kFresh));
  CHECK(sh::takes_fight(sh::Temper::balanced, sh::AimSkill::average, kFresh, kFresh));
  CHECK(!sh::takes_fight(sh::Temper::balanced, sh::AimSkill::novice, kFresh, kFresh));
  // The novice, with the yellow armour and the launcher, against the same enemy.
  CHECK(sh::takes_fight(sh::Temper::balanced, sh::AimSkill::novice, (100 + 90) * 2, kFresh));
  // An enemy with the launcher and the big armour turns the expert with a bare scattergun.
  CHECK(!sh::takes_fight(sh::Temper::balanced, sh::AimSkill::expert, kFresh, (100 + 160) * 2));
}

TEST(the_coward_retreats_first_and_the_aggressive_last) {
  CHECK(sh::retreat_below(sh::Temper::coward) > sh::retreat_below(sh::Temper::balanced));
  CHECK(sh::retreat_below(sh::Temper::balanced) > sh::retreat_below(sh::Temper::aggressive));
}

TEST(temperaments_are_named_and_read_back) {
  for (sh::Temper t : {sh::Temper::aggressive, sh::Temper::balanced, sh::Temper::coward}) {
    sh::Temper back = sh::Temper::balanced;
    CHECK(sh::temper_of(sh::temper_name(t), back) && back == t);
    CHECK(sh::temper_of(std::string_view(sh::temper_name(t), 1), back) && back == t);
  }
  sh::Temper x;
  CHECK(!sh::temper_of("brave", x));
}
