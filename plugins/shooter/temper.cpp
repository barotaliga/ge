// A player's temperament (temper.hpp): the fight taken or not, by temperament, skill and might.
//
// ge:summary A player's temperament: the fight taken or turned from, by the odds he needs
// ge:keywords temper_name temper_of self_edge takes_fight chases_sound retreat_below
// ge:skill shooter bots
// ge:test tests/shooter_temper_test.cpp
#include "shooter/temper.hpp"

#include "shooter/traits.hpp"

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

float edge_of(double aim) { return static_cast<float>(scale(aim, {0.7, 1.0, 2.0, 2.5})); }

bool takes_fight(Temper t, float edge, float mine, float theirs) {
  switch (t) {
    case Temper::aggressive:
      return true;
    case Temper::balanced:
      return mine * edge >= theirs;
    case Temper::coward:
      return mine * edge >= theirs * kCowardRatio;
  }
  return true;
}

bool takes_fight(Temper t, AimSkill skill, float mine, float theirs) {
  return takes_fight(t, self_edge(skill), mine, theirs);
}

bool chases_sound(Temper t, float edge, bool rash, float mine, float theirs) {
  switch (t) {
    // A rash player runs at every sound; a better one arms himself first (a weapon, armour), then
    // goes and gibs: he gets there with something to win with.
    case Temper::aggressive:
      return rash || mine >= theirs;
    case Temper::balanced:
      return takes_fight(t, edge, mine, theirs);
    case Temper::coward:
      return false;
  }
  return false;
}

bool chases_sound(Temper t, AimSkill skill, float mine, float theirs) {
  return chases_sound(t, self_edge(skill), skill == AimSkill::novice, mine, theirs);
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
