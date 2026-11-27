// QUAKE II'S MONSTERS IN THE CLASSIC CAMPAIGN (classic remix's `cr_monsters q2`): which of the
// Quake II's kinds the campaign runs (profiles/remix_monsters.hpp's `kQ1RunsQ2`, in its
// order), each with the roster's numbers (server/q2_monster_kinds.hpp) and the two sounds its
// shots and blows make, and the strings they add after the rules' own: per kind its model, sight,
// first pain cry, death, shot, blow and up to two more pain cries (its pain table's,
// server/q2_monster_pain_rules.hpp), then the missiles' models. Internal to
// games/shooters_edge/rules.
//
// ge:summary Quake II's monsters the classic campaign runs: their roster rows and their strings
// ge:keywords campaign cr_monsters remix q2-monsters monster-swap roster strings pain-cries
// ge:skill progs shooter
// ge:test tests/shooters_edge_campaign_rules_test.cpp
#pragma once

#include <cstddef>
#include <cstdint>
#include <iterator>
#include <string_view>

#include "shooters_edge/profiles/remix_monsters.hpp"
#include "shooters_edge/server/q2_monster_kinds.hpp"
#include "shooters_edge/server/q2_monster_pain_rules.hpp"

namespace shooters_edge::server::rules::campaign {

struct SqRun {
  const char* classname;
  const char* shot;  // a ranged attack's sound
  const char* blow;  // a melee attack's sound
};

inline constexpr SqRun kSqRuns[] = {
    {"monster_soldier_light", "soldier/solatck2.wav", "soldier/solatck2.wav"},
    {"monster_soldier", "soldier/solatck1.wav", "soldier/solatck1.wav"},
    {"monster_soldier_ss", "soldier/solatck3.wav", "soldier/solatck3.wav"},
    {"monster_infantry", "infantry/infatck1.wav", "infantry/melee2.wav"},
    {"monster_gunner", "gunner/gunatck2.wav", "gunner/gunatck3.wav"},
    {"monster_berserk", "berserk/attack.wav", "berserk/attack.wav"},
    {"monster_gladiator", "gladiator/railgun.wav", "gladiator/melee1.wav"},
    {"monster_flyer", "flyer/flyatck3.wav", "flyer/flyatck1.wav"},
    {"monster_parasite", "parasite/paratck3.wav", "parasite/paratck1.wav"},
    {"monster_mutant", "mutant/mutatck1.wav", "mutant/mutatck2.wav"},
    {"monster_chick", "chick/chkatck2.wav", "chick/chkatck3.wav"},
    {"monster_tank", "tank/tnkatck3.wav", "tank/tnkatck1.wav"},
    {"monster_flipper", "flipper/flpatck1.wav", "flipper/flpatck1.wav"},
    {"monster_floater", "floater/fltatck1.wav", "floater/fltatck2.wav"},
    {"monster_medic", "medic/medatck1.wav", "medic/medatck1.wav"},
    {"monster_brain", "brain/brnatck1.wav", "brain/melee1.wav"},
    {"monster_hover", "hover/hovatck1.wav", "hover/hovatck1.wav"},
    {"monster_tank_commander", "tank/tnkatck3.wav", "tank/tnkatck1.wav"},
};
inline constexpr int kSqRunCount = static_cast<int>(std::size(kSqRuns));

// The words each kind adds, in order, then the shared ones once.
enum SqWord : uint8_t {
  kSqModel,
  kSqSight,
  kSqPain,  // its first pain cry; kSqCry1 and kSqCry2 the others (the first again when none)
  kSqDeath,
  kSqShot,
  kSqBlow,
  kSqCry1,
  kSqCry2,
  kSqWords
};
inline constexpr const char* kSqShared[] = {
    "models/objects/laser/tris.md2", "models/objects/grenade/tris.md2",
    "models/objects/rocket/tris.md2", "models/objects/gibs/head2/tris.md2"};
enum SqSharedWord : uint8_t { kSqLaser, kSqGrenade, kSqRocket, kSqHead };
inline constexpr int kSqStringCount =
    kSqRunCount * kSqWords + static_cast<int>(std::size(kSqShared));

// The roster's row of a class name; -1 none.
constexpr int sq_roster_of(std::string_view classname) {
  for (size_t i = 0; i < std::size(roster::kKinds); i++)
    if (classname == roster::kKinds[i].classname) return static_cast<int>(i);
  return -1;
}

// Kind `run`'s word `w`.
constexpr const char* sq_word(int run, int w) {
  const SqRun& r = kSqRuns[run];
  const roster::Kind& k = roster::kKinds[sq_roster_of(r.classname)];
  switch (w) {
    case kSqModel:
      return k.model;
    case kSqSight:
      return k.sight;
    case kSqPain:
    case kSqCry1:
    case kSqCry2: {
      const int i = w == kSqPain ? 0 : w == kSqCry1 ? 1 : 2;
      const char* c = q2_pain::cry(r.classname, i);
      if (c == nullptr) c = q2_pain::cry(r.classname, 0);
      return c != nullptr ? c : k.pain;
    }
    case kSqDeath:
      return k.death;
    case kSqShot:
      return r.shot;
    default:
      return r.blow;
  }
}

constexpr bool sq_runs_agree() {
  if (std::size(profiles::kQ1RunsQ2) != std::size(kSqRuns)) return false;
  for (int i = 0; i < kSqRunCount; i++) {
    if (profiles::kQ1RunsQ2[i] != kSqRuns[i].classname) return false;
    if (sq_roster_of(kSqRuns[i].classname) < 0) return false;
    if (q2_pain::rule_of(kSqRuns[i].classname) == nullptr) return false;  // its pains as data
    if (q2_pain::cry(kSqRuns[i].classname, 3) != nullptr) return false;   // at most 3 cries
    for (int w = 0; w < kSqWords; w++)
      if (sq_word(i, w) == nullptr) return false;
  }
  return true;
}
static_assert(sq_runs_agree(),
              "each kind the campaign runs is the remix table's, in the roster, with its pains");

}  // namespace shooters_edge::server::rules::campaign
