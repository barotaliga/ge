// ONE SAMPLED VOICE, from `Synth::sample` to its recording's end or the end of its fade: the
// slot the synth's pool holds, how a play of a recording starts in it, and how a block of it is
// rendered. Its place in the recording is fixed point, 32.32 frames, so the step a 22 050 Hz file
// takes at 48 kHz is exact to 2^-32 and the voice lands on the same sample on every platform.
// The synth owns the pool, the ids and the stealing (`synth.cpp`); this is the per-sample half.
//
// ge:summary A sampled voice's slot, its start and its render: 32.32 position, Catmull-Rom, loops
// ge:keywords sampled-voice resample catmull-rom cubic-interpolation fixed-point loop pitch pan
// ge:keywords binaural hrtf two-ears interaural-delay head-shadow far_ear
// ge:skill audio
// ge:test tests/audio_synth_test.cpp
#pragma once

#include <cstdint>

#include "audio/pcm.hpp"
#include "audio/synth.hpp"

namespace ge::audio {

// THE EARS' GAP: a sound square to one side reaches the far ear 0.66 ms after the near one
// (kItdFrames at 48 kHz), through the head's shadow: at most kFarShade of its level lost and its
// treble cut by a one-pole down to kFarCoeff (about 1.5 kHz). Centred, both ears hear it alike.
inline constexpr int kEarFrames = 64;  // a power of two over the largest delay
inline constexpr float kItdFrames = 31.7f, kFarShade = 0.3f, kFarCoeff = 0.18f;

struct SampleVoice {
  bool live = false;
  bool stopping = false;
  uint32_t id = 0;
  uint64_t order = 0;
  const Pcm* pcm = nullptr;
  int64_t start = 0;  // the absolute frame its first sample plays on
  uint64_t pos = 0, step = 0;
  int64_t loop_start = -1, loop_end = 0;
  int64_t free_at = 0;
  int bus = 0;
  int ctl = 0;
  float gain = 0, t_gain = 0, c_gain = 1;
  float pan = 0, t_pan = 0, c_pan = 1;
  float gl = 1, gr = 1, echo = 0;
  // THE TWO EARS (`Synth::binaural`): the far ear hears the voice later, quieter and duller. The
  // last kEarFrames frames as heard, where the next is written, the far ear's delay (frames), its
  // gain and its one-pole's coefficient and state, set from the pan every kControl frames.
  float ear[kEarFrames] = {};
  uint32_t ear_at = 0;
  float far_delay = 0, far_gain = 1, far_coeff = 1, far_low = 0;
};

// Start a play of `pcm` in `s`, its first sample on the absolute frame `start`.
void begin_sample(SampleVoice& s, const Pcm& pcm, const SamplePlay& play, uint32_t id,
                  uint64_t order, int bus, int64_t start);

// Mix `count` frames of `s` from the absolute frame `b0` into `l`, `r` and the echo send `e`,
// through its bus's per-frame gains `bus`. Ends the voice at its recording's end or its fade's.
// `binaural`: its pan heard by two ears (the far one later, quieter, duller) rather than by level.
void render_sample(SampleVoice& s, int64_t b0, int count, const float* bus, float* l, float* r,
                   float* e, bool binaural = false);
// The far ear's delay, gain and one-pole coefficient for a pan (-1..1).
void far_ear(float pan, float& delay, float& gain, float& coeff);

}  // namespace ge::audio
