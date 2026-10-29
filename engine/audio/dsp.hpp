// THE SYNTH'S BUILDING BLOCKS, the per-sample arithmetic every voice and layer is made of: the
// tables (a sine, the saturation curves, the noise pools), the band-limited oscillators, the
// state-variable filter and the one-pole glide. Inline because they run once a sample per voice;
// `synth.cpp` composes them.
//
// DETERMINISTIC TO THE BIT on every platform: float arithmetic with no contraction (the engine's
// FP flags), no <cmath> transcendental (every table and coefficient goes through `core/libm`),
// noise drawn from `core::Rng`. So a rendered buffer is the same bytes on every CI row, and a
// sound's digest is a test.
//
// ge:summary The synth's per-sample blocks: tables, polyBLEP oscillators, the SVF, one-poles
// ge:keywords dsp oscillator polyblep sine-table saw square triangle svf state-variable-filter
// ge:keywords biquad resonance cutoff one-pole glide smoothing noise-pool pink brown white
// ge:keywords shaper saturation tanh curve exp2 cents
// ge:skill audio
// ge:test tests/audio_dsp_test.cpp
#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "audio/voice.hpp"

namespace ge::audio {

inline constexpr double kPi = 3.14159265358979323846;

// The sine table: one cycle in `kSineSize` steps (and a guard point), read with linear
// interpolation. Its worst error is under 3e-7, far below a 16-bit sample's step.
inline constexpr int kSineSize = 4096;
// A saturation curve's points over [-1, 1], one curve per drive in steps of 1/20.
inline constexpr int kCurveSize = 1024;
inline constexpr int kCurveCount = 21;
// HOW MUCH NOISE IS KEPT per colour, s. Every noise voice reads a window of its colour's pool at
// a drawn offset, so two voices never audibly share a stretch (identical noise twice over is a
// comb filter, which the ear hears as a pitch).
inline constexpr int kPoolFrames = 4 * kRate;

struct Tables {
  std::vector<float> sine;                  // kSineSize + 1
  std::vector<float> curves;                // kCurveCount * kCurveSize
  std::array<std::vector<float>, 3> noise;  // by Color, kPoolFrames each
};

// Build the tables once (a synth owns its own): the sine and the curves through core/libm, the
// noise off `seed`. White is uniform in [-1, 1); pink and brown are one-pole filters over white
// (poles 0.75 and 0.96: about 3 and 6 dB an octave), each renormalised to white's level so a
// volume reads the same in any colour, and brown highpassed at 8 Hz so its wander under the audio
// band does not pump the limiter.
Tables make_tables(uint32_t seed);

// sin(2 pi phase) for phase in [0, 1).
inline float sine_at(const Tables& t, float phase) {
  const float x = phase * kSineSize;
  const int i = static_cast<int>(x);
  const float f = x - static_cast<float>(i);
  return t.sine[i] + (t.sine[i + 1] - t.sine[i]) * f;
}

// The curve for `drive`, quantised to 1/20, applied to x (clamped to [-1, 1] first, as a
// shaper's input is).
inline const float* curve_for(const Tables& t, float drive) {
  // drive is >= 0, so floor(x + 0.5) is the nearest step.
  const size_t k =
      static_cast<size_t>(clamp01(drive) * 20 + 0.5f);  // NOLINT(bugprone-incorrect-roundings)
  return t.curves.data() + k * static_cast<size_t>(kCurveSize);
}
inline float shape(const float* curve, float x) {
  x = x < -1 ? -1 : x > 1 ? 1 : x;
  const float p = (x + 1) * 0.5f * (kCurveSize - 1);
  int i = static_cast<int>(p);
  if (i >= kCurveSize - 1) i = kCurveSize - 2;
  const float f = p - static_cast<float>(i);
  return curve[i] + (curve[i + 1] - curve[i]) * f;
}

// THE POLYBLEP residual: the band-limited step's correction around a waveform's jump, so a saw or
// a square does not fold its harmonics back down as aliasing. `t` is the phase, `dt` the phase
// step a sample.
inline float blep(float t, float dt) {
  if (t < dt) {
    t /= dt;
    return t + t - t * t - 1;
  }
  if (t > 1 - dt) {
    t = (t - 1) / dt;
    return t * t + t + t + 1;
  }
  return 0;
}

// One sample of `wave` at `phase` in [0, 1), stepping `dt` a sample.
inline float oscillate(const Tables& t, Wave wave, float phase, float dt) {
  switch (wave) {
    case Wave::sine:
      return sine_at(t, phase);
    case Wave::saw:
      return 2 * phase - 1 - blep(phase, dt);
    case Wave::square: {
      float half = phase + 0.5f;
      if (half >= 1) half -= 1;
      return (phase < 0.5f ? 1.0f : -1.0f) + blep(phase, dt) - blep(half, dt);
    }
    case Wave::triangle:
      // Its harmonics fall 12 dB an octave, so the naive shape aliases inaudibly.
      return phase < 0.5f ? 4 * phase - 1 : 3 - 4 * phase;
  }
  return 0;
}

inline float advance(float phase, float dt) {
  phase += dt;
  return phase >= 1 ? phase - static_cast<float>(static_cast<int>(phase)) : phase;
}

// 2^(cents / 1200) for the few tens of cents a vibrato swings, as a cubic in x = cents ln2 / 1200
// (error under 1e-9 at 100 cents): a per-sample ratio with no transcendental.
inline float cents_ratio(float cents) {
  const float x = cents * 0.000577622650f;
  return 1 + x * (1 + x * (0.5f + x * (1.0f / 6)));
}

// A balance law for a pan in -1..1: the centre is full on both sides, and a hard pan silences
// the far side.
inline void pan_gains(float pan, float& gl, float& gr) {
  pan = pan < -1 ? -1 : pan > 1 ? 1 : pan;
  gl = pan > 0 ? 1 - pan : 1;
  gr = pan < 0 ? 1 + pan : 1;
}

// THE STATE-VARIABLE FILTER (the trapezoidal-integrated form): stable under a cutoff that moves
// every few samples, which a direct-form biquad is not. Tuned every `kControl` samples; the band
// pass is scaled to unity gain at its centre.
inline constexpr int kControl = 16;

struct Svf {
  FilterType type = FilterType::none;
  float k = 1.41421356f;  // 1 / q
  float a1 = 0, a2 = 0, a3 = 0;
  float ic1 = 0, ic2 = 0;
};

inline float default_q(FilterType type) {
  return type == FilterType::bandpass ? 1.0f : 0.70710678f;
}

// Set the filter's type and resonance (q 0: the type's default); `tune` sets the cutoff.
void svf_init(Svf& f, FilterType type, float q);
void svf_tune(Svf& f, float hz);

inline float svf_tick(Svf& f, float v0) {
  const float v3 = v0 - f.ic2;
  const float v1 = f.a1 * f.ic1 + f.a2 * v3;
  const float v2 = f.ic2 + f.a2 * f.ic1 + f.a3 * v3;
  f.ic1 = 2 * v1 - f.ic1;
  f.ic2 = 2 * v2 - f.ic2;
  switch (f.type) {
    case FilterType::lowpass:
      return v2;
    case FilterType::highpass:
      return v0 - f.k * v1 - v2;
    case FilterType::bandpass:
      return f.k * v1;
    case FilterType::none:
      break;
  }
  return v0;
}

// THE ONE-POLE GLIDE: a value moves toward its target by this share a sample, reaching 63 % of
// the way in `tau_s` (an RC time constant). Never shorter than 5 ms: a jump is a step.
float glide_coeff(float tau_s);

// The per-sample multiplier that takes `from` to `to` in `frames` samples (an exponential ramp).
float ramp_ratio(float from, float to, int64_t frames);

}  // namespace ge::audio
