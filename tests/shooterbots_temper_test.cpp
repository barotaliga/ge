// A PLAYER'S TEMPERAMENT: the aggressive player takes every fight, the coward only one he wins
// many times over, and the balanced one a fight his skill wins: an expert's with a bare scattergun
// against a fresh enemy, a novice's only once he is far better armed.
//
// ge:summary Tests: temperaments take or turn from fights by might and skill
// ge:keywords test temper temperament aggressive balanced coward takes_fight chases_sound
// ge:skill shooter testing
#include "check.hpp"
#include "shooterbots/temper.hpp"

namespace sh = ge::shooter;

namespace sb = ge::shooterbots;

namespace {

constexpr float kFresh = 100;  // a fresh spawn: 100 health, the scattergun

}  // namespace

TEST(the_aggressive_take_every_fight_and_the_coward_only_a_lopsided_one) {
  for (sb::AimSkill k : {sb::AimSkill::novice, sb::AimSkill::average, sb::AimSkill::expert}) {
    CHECK(sb::takes_fight(sb::Temper::aggressive, k, 10, 1000));
    CHECK(sb::chases_sound(sb::Temper::aggressive, k, 200, 100));
    CHECK(!sb::takes_fight(sb::Temper::coward, k, kFresh, kFresh));
    CHECK(!sb::chases_sound(sb::Temper::coward, k, 5000, kFresh));
  }
  // An aggressive novice runs at any sound; a better one arms himself first.
  CHECK(sb::chases_sound(sb::Temper::aggressive, sb::AimSkill::novice, 10, 1000));
  CHECK(!sb::chases_sound(sb::Temper::aggressive, sb::AimSkill::expert, kFresh, 120));
  // The big armour, the mega and the launcher: (200 * 0.8 + 250) * 2 against a fresh enemy.
  const float stacked = (200 * 0.8f + 250) * 2;
  CHECK(sb::takes_fight(sb::Temper::coward, sb::AimSkill::novice, stacked, kFresh));
}

TEST(a_balanced_expert_fights_with_the_scattergun_and_a_balanced_novice_does_not) {
  CHECK(sb::takes_fight(sb::Temper::balanced, sb::AimSkill::expert, kFresh, kFresh));
  CHECK(sb::takes_fight(sb::Temper::balanced, sb::AimSkill::average, kFresh, kFresh));
  CHECK(!sb::takes_fight(sb::Temper::balanced, sb::AimSkill::novice, kFresh, kFresh));
  // The novice, with the yellow armour and the launcher, against the same enemy.
  CHECK(sb::takes_fight(sb::Temper::balanced, sb::AimSkill::novice, (100 + 90) * 2, kFresh));
  // An enemy with the launcher and the big armour turns the expert with a bare scattergun.
  CHECK(!sb::takes_fight(sb::Temper::balanced, sb::AimSkill::expert, kFresh, (100 + 160) * 2));
}

TEST(the_coward_retreats_first_and_the_aggressive_last) {
  CHECK(sb::retreat_below(sb::Temper::coward) > sb::retreat_below(sb::Temper::balanced));
  CHECK(sb::retreat_below(sb::Temper::balanced) > sb::retreat_below(sb::Temper::aggressive));
}

TEST(temperaments_are_named_and_read_back) {
  for (sb::Temper t : {sb::Temper::aggressive, sb::Temper::balanced, sb::Temper::coward}) {
    sb::Temper back = sb::Temper::balanced;
    CHECK(sb::temper_of(sb::temper_name(t), back) && back == t);
    CHECK(sb::temper_of(std::string_view(sb::temper_name(t), 1), back) && back == t);
  }
  sb::Temper x;
  CHECK(!sb::temper_of("brave", x));
}
