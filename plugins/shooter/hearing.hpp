// THE BOT'S EARS: what a player makes of the sounds his client is sent, by how good a player he is.
//
// A sound tells a listener three things: that something happened (a shot, a pickup, a spawn), WHO
// made it (the entity it came from), and roughly WHERE (the direction exactly, the distance off the
// loudness). How much of it a player takes in depends on him:
// - an EXPERT hears everything that reaches him, knows every sample (the mega's pickup, the
// armour's,
//   a spawn), and judges the distance to within `range_error` of it;
// - an AVERAGE player takes in what is CLOSE (within `near`) and what is UNIQUE (a pickup, a power,
//   a spawn: one sound, one meaning) most of the time, and a common sound far off (a shot, a jump,
//   pain) seldom; less of either in a fight;
// - a NOVICE takes in only what is close, and never the meaning of a sample beyond its being a shot
//   or a cry: he hears "someone", not "someone took the mega".
//
// The MEANINGS are a game's data (`SoundMeaning`: a sample's path, its class, whether it is unique,
// and a word the game reads it by, such as the item it is the pickup of); the mechanism here knows
// none of them. A sound whose sample is not in the table is heard as `SoundClass::other` (a common
// sound with no meaning beyond where it came from).
//
// A heard sound's place (`HeardSound::guess`) is the true direction at a distance off by up to
// `range_error` of it either way, drawn off the caller's own stream: what the listener believes,
// not where the source is. A game that knows the map snaps a guess to the spot it must be (the item
// whose pickup it is, the spawn point nearest).
//
// Deterministic: a pure function of the view, the skill and the stream, which it advances once a
// sound considered.
//
// ge:summary The bot's ears: sounds taken in by skill, their meaning, the distance judged
// ge:keywords hearing sound listen sample meaning pickup-sound spawn-sound unique-sound
// ge:keywords hear_skill_of sound_meaning hear distance-guess range-error
// ge:skill shooter bots
// ge:test tests/shooter_hearing_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

#include "shooter/aim.hpp"
#include "shooter/senses.hpp"

namespace ge::shooter {

enum class SoundClass : uint8_t { other, move, fire, pain, death, pickup, power, spawn };
const char* sound_class_name(SoundClass c);

// What one sample means, as a game's table states it.
struct SoundMeaning {
  std::string_view sample;  // its path, as the server names it
  SoundClass cls = SoundClass::other;
  bool unique = false;    // one sound, one meaning: a listener who knows it reads it at once
  std::string_view word;  // what the game reads it as (an item's call word); may be empty
};

struct HearSkill {
  float near = 600;           // a sound this close is "close", units
  float unique_near = 1;      // the chance a close unique sound is taken in
  float unique_far = 1;       // and one further off
  float common_near = 1;      // a close common sound
  float common_far = 1;       // a far common sound
  float fight_scale = 1;      // the chances above in a fight are this many times as big
  float range_error = 0.1f;   // the distance is judged to within this share of it, either way
  bool knows_samples = true;  // reads a sample's meaning (a novice hears a sound, not what it is)
};

// The ears of a player of aim `skill` (the perfect aim's ears are the expert's).
HearSkill hear_skill_of(AimSkill skill);

// The meaning of `sample` in `table`, or null.
const SoundMeaning* sound_meaning(std::span<const SoundMeaning> table, std::string_view sample);

struct HeardSound {
  int ent = 0;                            // the entity the sound came from
  Vec3f at;                               // where it came from (the truth: for the caller's checks)
  Vec3f guess;                            // where the listener believes it came from
  float dist = 0;                         // how far it was from the ear, units
  const SoundMeaning* meaning = nullptr;  // null: not read (unknown, or the listener cannot)
  SoundClass cls = SoundClass::other;     // as the listener takes it (`other` when not read)
};

struct HearStats {
  int64_t sounds = 0, taken = 0, missed = 0, read = 0;
};

// The sounds of `view` that the listener at `ear` takes in this frame, in `out` (cleared first);
// `viewer` is his own entity (his own sounds are not heard), `fighting` whether he is in a fight.
// `rng` is the caller's stream (a 32-bit LCG state), advanced once a sound considered.
void hear(const ClientView& view, int viewer, Vec3f ear, const HearSkill& skill,
          std::span<const SoundMeaning> table, bool fighting, uint32_t& rng,
          std::vector<HeardSound>& out, HearStats* stats = nullptr);

}  // namespace ge::shooter
