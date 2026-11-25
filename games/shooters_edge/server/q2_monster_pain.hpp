// Quake II's monsters' pains and deaths as data (q2_monster_pain.cpp): which run a blow plays
// and which cry it gives, by kind, by the blow's size, by the skill and by how the body moves;
// which death run a kind falls by and its cry.
//
// ge:summary Quake II's monsters' pain and death runs and cries by kind, blow, skill and lift
// ge:keywords q2 monsters pain death pain-animation pain-sound death-sound nightmare knocked-up
// ge:skill shooter
// ge:test tests/shooters_edge_q2_monsters_test.cpp
#pragma once

#include <string_view>

#include "core/prng.hpp"
#include "shooters_edge/server/q2_monster_kinds.hpp"
#include "shooters_edge/server/q2_poses.hpp"

namespace shooters_edge::server {

// What a blow does to a kind with its pains as data.
struct PainChoice {
  bool spent = false;           // the pain's wait starts now (a blow ignored by chance spends it)
  const char* sound = nullptr;  // the cry, or none
  bool everywhere = false;      // heard across the whole map
  SqSpan run;                   // the flinch, or none
};

// Whether kind `classname` has its pains as data.
bool monster_pain_known(std::string_view classname);

// The class whose pain and death rows kind `k` reads: its own when it has any (a pack's kind
// with its own cries), else the kind it looks like.
const char* pain_class(const roster::Kind& k);

// The pain of kind `classname` hurt by `damage` at `skill`, rising at `up` units/s, showing
// `frame` of a run that starts at `first` (`act_pain` when that run is a pain). `waiting` when the
// last pain's wait has not run out: only a pain run swapped for the knocked-up one then, with no
// cry. Draws from `rng` as the game does.
PainChoice monster_pain(std::string_view classname, int damage, int skill, float up, int frame,
                        int first, bool act_pain, bool waiting, ge::core::Rng& rng);

// A death as data: the run (its index into the kind's death runs) and the cry.
struct DeathChoice {
  int run = -1;  // -1: the kind's poses pick
  const char* sound = nullptr;
  bool everywhere = false;
};

// The death of kind `classname` by a blow of `damage`, or {-1, null} for a kind not listed.
DeathChoice monster_death(std::string_view classname, int damage, ge::core::Rng& rng);

// The prisoner's (misc_insane's) cries: his four deaths, and his pains two a band of health
// (under 25, 50, 75, else 100).
inline constexpr const char* kInsaneDeaths[] = {"player/male/death1.wav", "player/male/death2.wav",
                                                "player/male/death3.wav", "player/male/death4.wav"};
inline constexpr const char* kInsanePains[] = {
    "player/male/pain25_1.wav",  "player/male/pain25_2.wav", "player/male/pain50_1.wav",
    "player/male/pain50_2.wav",  "player/male/pain75_1.wav", "player/male/pain75_2.wav",
    "player/male/pain100_1.wav", "player/male/pain100_2.wav"};

// Calls `add` with each cry kind `classname`'s pains and deaths may give.
void monster_pain_sounds(std::string_view classname, void (*add)(void*, const char*), void* to);

}  // namespace shooters_edge::server
