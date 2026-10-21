// WHAT A SOUND IS: the one-shot voices fired together, as plain data, and how one PLAY of it
// differs from the authored one. A game's bank is an array of `SoundDef`s; `play_def` hands a
// def's voices to the synth verbatim, scaled by the play's `PlayShape`. Nothing here interprets:
// a voice's fields are the synth's (`voice.hpp`), so a bank can never ask for what the
// instrument cannot do, and a sound is read by reading its numbers.
//
// THE SHAPE. A bank is static and almost nothing in a race is: a landing at 4 m/s of descent and
// one at 11 are the same event and nothing like the same noise. A play scales what the author
// wrote on the three axes the ear reads as "bigger": louder (`gain`), lower and darker (`pitch`
// multiplies every frequency and every cutoff together, or a big thing keeps a small thing's
// brightness) and longer (`stretch`, durations and delays), plus where it is (`pan`).
//
// ge:summary What a sound is: SoundVoice, SoundDef, PlayShape, play_def, find_sound
// ge:keywords sound sound-def bank one-shot play play_def play-shape shape gain pitch stretch
// ge:keywords pan find_sound shaped description voices
// ge:skill audio
// ge:test tests/audio_sound_test.cpp
#pragma once

#include <span>
#include <string_view>

#include "audio/synth.hpp"
#include "audio/voice.hpp"

namespace ge::audio {

struct SoundVoice {
  enum class Kind : uint8_t { tone, noise };
  Kind kind = Kind::tone;
  Tone tone;
  Noise noise;
};

constexpr SoundVoice tone_voice(const Tone& t) { return {SoundVoice::Kind::tone, t, {}}; }
constexpr SoundVoice noise_voice(const Noise& n) { return {SoundVoice::Kind::noise, {}, n}; }

// One sound: its id, a sentence saying what it is meant to FEEL like (what the next author
// retuning the numbers checks their work against), and its voices.
struct SoundDef {
  std::string_view id;
  std::string_view description;
  std::span<const SoundVoice> voices;
};

struct PlayShape {
  float gain = 1;
  float pitch = 1;
  float stretch = 1;
  bool place = false;  // replace every voice's pan with `pan`
  float pan = 0;
};

// One authored voice under a play's shape. A tone with no glide keeps none.
SoundVoice shaped(const SoundVoice& voice, const PlayShape& shape);

// Fire a def's voices on `bus`.
void play_def(Synth& synth, const SoundDef& def, const PlayShape& shape = {}, int bus = 0);

// The def named `id` in `bank`, or nullptr: an unknown id is a quiet moment, never a throw.
const SoundDef* find_sound(std::span<const SoundDef> bank, std::string_view id);

}  // namespace ge::audio
