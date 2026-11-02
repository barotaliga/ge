// A player's temperament (temper.hpp): the fight taken or not, by temperament, skill and might.
//
// ge:summary A player's temperament: the fight taken or turned from, by the odds he needs
// ge:keywords temper_name temper_of self_edge takes_fight chases_sound retreat_below
// ge:skill shooter bots
// ge:test tests/shooter_temper_test.cpp
#include "shooter/temper.hpp"

namespace ge::shooter {

const char* temper_name(Temper t) {
  switch (t) {
    case Temper::aggressive:
      return "aggressive";
    case Temper::balanced:
      return "balanced";
    case Temper::coward:
      return "coward";
  }
  return "?";
}

bool temper_of(std::string_view name, Temper& out) {
  for (Temper t : {Temper::aggressive, Temper::balanced, Temper::coward}) {
    const std::string_view full = temper_name(t);
    if (name == full || (name.size() == 1 && name[0] == full[0])) {
      out = t;
      return true;
    }
  }
  return false;
}

float self_edge(AimSkill skill) {
  switch (skill) {
    case AimSkill::perfect:
      return 2.5f;
    case AimSkill::expert:
      return 2.f;  // a fresh enemy falls to his scattergun
    case AimSkill::average:
      return 1.f;
    case AimSkill::novice:
      return 0.7f;  // he needs more than the enemy has
  }
  return 1.f;
}

bool takes_fight(Temper t, AimSkill skill, float mine, float theirs) {
  switch (t) {
    case Temper::aggressive:
      return true;
    case Temper::balanced:
      return mine * self_edge(skill) >= theirs;
    case Temper::coward:
      return mine * self_edge(skill) >= theirs * kCowardRatio;
  }
  return true;
}

bool chases_sound(Temper t, AimSkill skill, float mine, float theirs) {
  switch (t) {
    // A novice runs at every sound; a better player arms himself first (a weapon, armour), then
    // goes and gibs: he gets there with something to win with.
    case Temper::aggressive:
      return skill == AimSkill::novice || mine >= theirs;
    case Temper::balanced:
      return takes_fight(t, skill, mine, theirs);
    case Temper::coward:
      return false;
  }
  return false;
}

float retreat_below(Temper t) {
  switch (t) {
    case Temper::aggressive:
      return 25;
    case Temper::balanced:
      return 50;
    case Temper::coward:
      return 70;
  }
  return 50;
}

}  // namespace ge::shooter
