// QUAKE'S MONSTERS' MIND, AS ANY RULES RUN IT: the decisions a Quake monster takes, in one place,
// so the classic campaign's own monsters (rules/campaign_ai.cpp, campaign_monsters*.cpp) and
// Quake's monsters under Quake II's rules (classic remix's `cr_monsters q1`, server/
// q2_monsters.cpp) decide alike: when it sees, when it attacks and how, whom it turns on when hurt.
// Header-only and plain (numbers in, a decision out, random numbers from the caller's draw in the
// order the caller's world draws them), as `q2_monster_mind.hpp` is for Quake II's.
//
// - **Ranges** (eye to eye): melee under 120, near under 500, mid under 1000, else far.
// - **Sight**: never far, never through anything solid; at melee range whichever way it faces;
//   near, in front or a player who fired in the last second; mid, only in front (the cosine above
//   0.3). It hears nothing and follows no trail: once it has an enemy it makes for him.
// - **Attacking**: a clear shot at him first. At melee range its blow when it has one; else, its
//   wait spent and he is not far, a chance a frame: 0.9 at melee range, near 0.2 (0.4 for a kind
//   with no blow), mid 0.05 (0.1), by no skill; after a shot it waits 2 × random s.
// - **Its own tests**: the hound leaps at a foe 80 to 150 away within 32 of its height (1 s
//   between), the fiend 100 to 350 within 64 (2 s); the knight only strikes, within 80; the death
//   knight strikes within 80 and charges within 250 on his level (within 20, 2 s between), else
//   the usual test; the spawn jumps at a foe it sees near.
// - **Hurt by another monster**: it turns on one of another kind (a rifleman on its own too).
// - **Pain**: each kind its wait (most 1 s, the brute 1.5, the shambler 2, the vore 3) and its
//   flinch; a shambler above 400 health shrugs off half its blows; at the hardest skill every
//   pain puts the next 5 s off.
//
// ge:summary Quake's monsters' decisions as any rules run them: ranges, sight, attacks, leaps
// ge:keywords q1 monster-mind monster-ai cr_monsters remix real-ai check-attack attack-chance
// ge:keywords sight in-front infighting dog-leap fiend-leap knight-charge q1_check_attack pain
// ge:keywords pain-wait flinch shambler-shrug
// ge:skill shooter progs
// ge:test tests/shooters_edge_q1_monster_mind_test.cpp
#pragma once

#include <cstdint>
#include <string_view>

#include "shooters_edge/server/q2_monster_kinds.hpp"

namespace shooters_edge::server::q1_mind {

enum class Range : uint8_t { melee, near, mid, far };

inline constexpr float kMelee = 120, kNear = 500, kMid = 1000;
inline constexpr float kInFront = 0.3f;  // the cosine of in front
inline constexpr float kRefire = 2;      // × random s after a shot
inline constexpr float kPainEveryHard = 5;

constexpr Range range_of(float eye_to_eye) {
  if (eye_to_eye < kMelee) return Range::melee;
  if (eye_to_eye < kNear) return Range::near;
  if (eye_to_eye < kMid) return Range::mid;
  return Range::far;
}

// What it knows of a candidate: its range, a clear line to him, whether he fired in the last
// second, whether he is in front of it.
struct Sighting {
  Range range = Range::far;
  bool clear = false, fired = false, ahead = false;
};
constexpr bool sees(const Sighting& s) {
  if (s.range == Range::far || !s.clear) return false;
  if (s.range == Range::near) return s.fired || s.ahead;
  if (s.range == Range::mid) return s.ahead;
  return true;
}

enum class Act : uint8_t { none, melee, missile };

// The usual test with a clear shot at him: `ready` its wait spent.
struct Shot {
  Range range = Range::far;
  bool has_melee = false, has_missile = false, ready = false;
};
constexpr float attack_chance(Range r, bool has_melee) {
  if (r == Range::melee) return 0.9f;
  if (r == Range::near) return has_melee ? 0.2f : 0.4f;
  return has_melee ? 0.05f : 0.1f;
}
// One draw, only when it may fire.
template <class Roll>
Act check_attack(const Shot& s, Roll&& roll) {
  if (s.range == Range::melee && s.has_melee) return Act::melee;
  if (!s.has_missile || !s.ready || s.range == Range::far) return Act::none;
  return roll() < attack_chance(s.range, s.has_melee) ? Act::missile : Act::none;
}

// A leap's reach: the least and most distance (origin to origin), the most height between them,
// and the wait after it.
struct Leap {
  float least, most, rise, refire;
};
inline constexpr Leap kHoundLeap{80, 150, 32, 1};
inline constexpr Leap kFiendLeap{100, 350, 64, 2};
// `rise` the height between them (either way), `dist` the distance.
constexpr bool leaps(const Leap& l, float rise, float dist) {
  return !(rise > l.rise) && !(dist < l.least) && !(dist > l.most);
}

inline constexpr float kKnightReach = 80;   // the knights' blow, origin to origin
inline constexpr float kChargeReach = 250;  // the death knight's charge
inline constexpr float kChargeRise = 20;    // on his level
inline constexpr float kChargeRefire = 2;

// The kinds with a test of their own.
enum class Test : uint8_t { usual, hound, fiend, knight, death_knight, spawn };
constexpr Test test_of(std::string_view classname) {
  if (classname == "monster_dog") return Test::hound;
  if (classname == "monster_demon1") return Test::fiend;
  if (classname == "monster_knight") return Test::knight;
  if (classname == "monster_hell_knight") return Test::death_knight;
  if (classname == "monster_tarbaby") return Test::spawn;
  return Test::usual;
}

// Hurt by another monster: whether it turns on it.
constexpr bool turns_on(bool other_kind, bool rifleman) { return other_kind || rifleman; }

// Its pain: how long before the next (`wait`, s) and how many frames its flinch holds it (0:
// none, 0.1 s each). A kind with neither feels none.
struct Pain {
  float wait = 0, frames = 0;
};
inline constexpr float kShamblerStrong = 400;  // above this health a shambler may shrug a blow
inline constexpr float kShamblerShrug = 0.5f;  // off, at this chance
constexpr Pain pain_of(std::string_view classname) {
  if (classname == "monster_ogre" || classname == "monster_ogre_marksman") return {1.5f, 4};
  if (classname == "monster_wizard" || classname == "monster_fish") return {1, 2};
  if (classname == "monster_shambler") return {2, 4};
  if (classname == "monster_shalrath") return {3, 4};
  if (classname == "monster_zombie") return {1, 0};
  if (classname == "monster_army" || classname == "monster_dog" || classname == "monster_knight" ||
      classname == "monster_demon1" || classname == "monster_enforcer" ||
      classname == "monster_hell_knight")
    return {1, 3};
  return {};
}
// Whether a blow is shrugged off: a strong shambler's, by one draw (taken for a shambler only).
template <class Roll>
bool shrugs(std::string_view classname, float health, Roll&& roll) {
  if (classname != "monster_shambler") return false;
  const bool strong = health > kShamblerStrong;
  const bool shrug = roll() < kShamblerShrug;  // drawn either way
  return strong && shrug;
}

// Whether a kind of the roster is one of Quake's (its model one of Quake's).
constexpr bool runs(const roster::Kind& k) {
  return std::string_view(k.model).starts_with("progs/");
}

}  // namespace shooters_edge::server::q1_mind
