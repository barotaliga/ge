// A PLAYER'S PERSONALITY: what he is good at, trait by trait, each from 1 (hopeless) to 10 (the
// best there is). A bot is these numbers; the skills (novice, average, expert) and the
// temperaments (temper.hpp) are presets over them, so a bot can be an expert with the rocket
// launcher who cannot time an item, or a novice's aim on a teamplayer.
//
// THE TRAITS (`Trait`):
// - AIM with each kind of weapon: the scattergun (and its double), the spikers, the grenades, the
//   rockets, the arc. How steady the hand is, how fast it turns and how far ahead it leads, per
//   weapon (aim.hpp's `aim_profile`).
// - REACTION: how long before the hand starts on a contact.
// - TIMING: which items a player times, and how well.
// - KNOWLEDGE: what a player knows of the map: where its items and buttons are, and what they are
//   worth on it.
// - CAMPING: working the spots an enemy comes in weak (spawns, teleporter exits, a power's spawn).
// - DISCIPLINE: staying where the team needs him rather than wandering after a frag.
// - MOVES: the bunny hop and the hands that fly it.
// - HEARING: what a player takes in of the game's sounds and what they tell him.
// - COMMUNICATION: saying what he sees and does, and reading what his team says.
// - TEAMPLAY: the team's score over his own: holding an area, leaving an item to a mate.
// - AGGRESSION: which fights he takes (`temper_of`).
//
// THE PRESETS (`traits_of`): every trait 2 for a novice, 5 for an average player, 8 for an expert,
// 10 for the perfect measuring stick; the temperament sets aggression (9, 5 or 2) and an aggressive
// player's teamplay at most 3. A trait read in steps reads it by `tier_of`: 1 to 3 as a novice, 4
// to 6 as an average player, 7 and up as an expert; a trait read smoothly goes through `scale`,
// which hits the presets' values exactly at 2, 5, 8 and 10.
//
// ge:summary A player's personality: traits 1 to 10 (aim per weapon, reaction, timing, teamplay)
// ge:keywords traits personality preset tier_of aim-per-weapon reaction timing knowledge
// ge:keywords camping discipline moves hearing communication teamplay aggression
// ge:skill shooter bots
// ge:test tests/shooter_traits_test.cpp
#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <string_view>

#include "shooter/aim.hpp"
#include "shooter/temper.hpp"

namespace ge::shooter {

enum class Trait : uint8_t {
  aim_sg,
  aim_ng,
  aim_gl,
  aim_rl,
  aim_lg,
  reaction,
  timing,
  knowledge,
  camping,
  discipline,
  moves,
  hearing,
  comms,
  teamplay,
  aggression,
};
constexpr size_t kTraits = 15;
// The trait's name as a lab or a config writes it ("aim_rl", "reaction").
const char* trait_name(Trait t);

struct Traits {
  std::array<uint8_t, kTraits> v{};
  uint8_t& operator[](Trait t) { return v[static_cast<size_t>(t)]; }
  uint8_t operator[](Trait t) const { return v[static_cast<size_t>(t)]; }
};

// The preset: every trait at the skill's value, aggression and teamplay by temperament.
Traits traits_of(AimSkill skill, Temper temper);
// A trait read in steps: novice (1 to 3), average (4 to 6) or expert (7 and up).
AimSkill tier_of(uint8_t v);
// The temperament the aggression trait gives: 7 and up aggressive, 3 and below a coward.
Temper temper_of(const Traits& t);
// A player who plays for himself: no area held, no item left to a mate.
bool selfish(const Traits& t);
// A smooth reading of `v` through the values at 2, 5, 8 and 10 (`at`), straight lines between
// and beyond; exact at each of the four.
double scale(double v, const std::array<double, 4>& at);
// The mean of the five aims, for what aim is worth in a fight (temper.hpp's edge).
double mean_aim(const Traits& t);

// Applies "name=value" changes, separated by '/', to `t` ("aim_rl=9/timing=3"); false with
// `error` set on an unknown name or a value outside 1 to 10.
bool apply_traits(std::string_view text, Traits& t, std::string& error);
// Every trait as "name=value", '/'-separated, in order.
std::string traits_text(const Traits& t);

}  // namespace ge::shooter
