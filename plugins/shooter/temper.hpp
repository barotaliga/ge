// A PLAYER'S TEMPERAMENT: when he takes a fight, beside how well he plays (his skill).
//
// Three temperaments, each sizing a fight up as MIGHT against MIGHT (a body's worth of health and
// armour times what it carries; a game states both, `Might`):
// - AGGRESSIVE goes at every enemy he sees, whatever the odds, and stays in a fight long past the
//   point the others turn. A novice like that runs at every sound too and dies a lot; a better
//   player arms himself before he goes after a sound (a weapon, armour), then gibs, and frags a
//   lot. He plays for frags, not for his team.
// - BALANCED takes a fight he believes he wins: his might, times the edge his skill gives him,
//   against the enemy's as far as he can tell it. Outmatched, he turns for armour, a weapon or his
//   team. An expert's edge makes him nearly as forward as the aggressive (a fresh enemy falls to
//   his scattergun); a novice's makes him nearly as careful as the coward.
// - COWARD takes a fight only when he is far stronger (`kCowardRatio` times, his edge counted): the
//   big armour, the mega health and the launcher first. He never goes after a sound.
//
// What the enemy's might is, the caller believes (what he saw and heard him take and fire); the
// decision is here. Pure, no state: the same numbers give the same answer.
//
// ge:summary A player's temperament: aggressive, balanced or coward; whether he takes a fight
// ge:keywords temper temperament aggressive balanced coward engage takes_fight chases_sound
// ge:keywords might self_edge retreat-below fight-odds
// ge:skill shooter bots
// ge:test tests/shooter_temper_test.cpp
#pragma once

#include <cstdint>
#include <string_view>

#include "shooter/aim.hpp"

namespace ge::shooter {

enum class Temper : uint8_t { aggressive, balanced, coward };
const char* temper_name(Temper t);
// `name` as `temper_name` writes it, or its first letter; false when it is neither.
bool temper_of(std::string_view name, Temper& out);

// What a player brings to a fight: `body` the damage he takes before he dies (health and what his
// armour saves), `arms` what his weapon and powers multiply his damage by (1: the starting gun).
struct Might {
  float body = 100;
  float arms = 1;
};
inline float might(const Might& m) { return m.body * m.arms; }

constexpr float kCowardRatio = 2.5f;  // a coward's might over the enemy's before he fights

// How much better than the average player a player of `skill` is in a fight, and believes he is.
float self_edge(AimSkill skill);

// Takes the fight with an enemy of might `theirs`, being of might `mine`.
bool takes_fight(Temper t, AimSkill skill, float mine, float theirs);
// Goes toward an enemy he only heard.
bool chases_sound(Temper t, AimSkill skill, float mine, float theirs);
// Turns from a fight for health or armour at this body (health and armour's saving) and below.
float retreat_below(Temper t);

}  // namespace ge::shooter
