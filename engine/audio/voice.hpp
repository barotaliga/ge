// WHAT A VOICE IS: the parameters every sound is written in, and the few pure rules about them
// that a test can read without rendering anything. The instrument that plays them is
// `synth.hpp`; a bank, a bed and a lab describe sounds with this file alone.
//
// TWO KINDS OF VOICE, and the split is the whole design of the audio:
//
//   * A ONE-SHOT (`Tone`, `Noise`) starts, runs its envelope and stops: a thump, a crack, a
//     chime. Every sound in a bank is a list of them (`sound.hpp`).
//   * A LAYER (`LayerSpec`) never stops. It is a source, a filter, a saturation stage and a gain,
//     built once and STEERED: its level, pitch, cutoff, grit and pan are moved toward a
//     `LayerTarget` on a glide, sample by sample on the render thread. A bed (the wind, the skis
//     on the snow) is a rack of layers (`rack.hpp`). A late frame leaves a layer holding its last
//     target; it never leaves a hole.
//
// THE REGISTER is an arcade racer's, not a chip's, and four things here exist for it: noise has a
// COLOUR (white hisses, pink is spray and wind, brown rumbles), noise has an ENVELOPE (it swells,
// holds and thins), filters SWEEP (a moving cutoff is a gesture), and oscillators SATURATE (a soft
// tanh curve gives a triangle combustion behind it).
//
// Units: Hz, ms for a one-shot's times, s for a layer's glide, linear gain for every level
// (0.02 to 0.1 is the range a bank lives in, about -34 to -20 dBFS).
//
// ge:summary The synth's vocabulary: Tone, Noise, LayerSpec, LayerTarget, the envelope's rules
// ge:keywords audio sound voice tone noise layer layer-spec layer-target wave sine square saw
// ge:keywords triangle colour color white pink brown filter lowpass highpass bandpass sweep
// ge:keywords vibrato detune drive grit pan echo bus envelope attack hold decay safe_cutoff
// ge:keywords shaper_steepness shaper_push min-attack
// ge:skill audio
// ge:test tests/audio_synth_test.cpp
#pragma once

#include <cstdint>

namespace ge::audio {

// THE RATE every buffer is rendered at, Hz, offline and on the device alike: the platform
// resamples to whatever the output runs at, so a sound's bits never depend on the hardware.
inline constexpr int kRate = 48000;

enum class Wave : uint8_t { sine, square, saw, triangle };

// How a noise source is coloured: white is flat (hiss, grit), pink falls about 3 dB an octave
// (spray, snow, wind), brown about 6 (powder ploughed, a slide's rumble, an impact's body).
enum class Color : uint8_t { white, pink, brown };

enum class FilterType : uint8_t { none, lowpass, highpass, bandpass };

// A one-shot's filter: a state-variable filter whose cutoff starts at `hz` and, when `to_hz` is
// set, sweeps there exponentially across the sound (cutoff is heard in octaves). `q` is the
// resonance (0: the type's default, 0.707 for low and high pass, 1 for band pass); past about 6
// it sings at its own cutoff. A band pass peaks at unity gain.
struct Filter {
  FilterType type = FilterType::none;
  float hz = 1000;
  float to_hz = 0;  // 0: no sweep
  float q = 0;
};

struct Vibrato {
  float rate_hz = 0;      // 5-7 reads as a singer, 2-3 as a wobble; 0: none
  float depth_cents = 0;  // peak deviation
  float delay_ms = 0;     // fades in over 80 ms after this
};

// The default level of a voice that names none.
inline constexpr float kToneVolume = 0.06f;
inline constexpr float kNoiseVolume = 0.05f;

// A PITCHED ONE-SHOT. Its gain rises over `attack_ms` (never faster than `kMinAttackMs`), holds
// for `hold_ms`, then falls EXPONENTIALLY to silence at `duration_ms`: a note with a tail.
struct Tone {
  Wave wave = Wave::square;
  float from_hz = 440;
  float to_hz = 0;  // an exponential glide to here across the sound; 0: no glide
  float duration_ms = 100;
  float volume = kToneVolume;
  float delay_ms = 0;  // start this long after the synth drains the call
  float attack_ms = 0;
  float hold_ms = 0;
  float detune_cents = 0;  // a second oscillator this far each side: the cheap chorus
  Vibrato vibrato{};
  float pan = 0;   // -1 left .. 1 right
  float echo = 0;  // 0..1 send into the shared echo
  Filter filter{};
  float drive = 0;  // 0 clean .. 1 the hardest soft saturation (never a clip)
};

// A NOISE ONE-SHOT: a window onto the synth's pool of its colour. A bare burst (no attack, no
// hold) falls LINEARLY to nothing across its length, which is a hit; one that asks for an attack
// or a hold is a swell and gets the exponential tail a note has.
struct Noise {
  float duration_ms = 100;
  float volume = kNoiseVolume;
  float delay_ms = 0;
  Color color = Color::white;
  Filter filter{};
  float attack_ms = 0;
  float hold_ms = 0;
  float pan = 0;
  float echo = 0;
};

// WHAT A LAYER IS MADE OF, decided once when it is built: everything that cannot move smoothly.
// What moves is its `LayerTarget`.
struct LayerSpec {
  enum class Kind : uint8_t { tone, noise };
  Kind kind = Kind::noise;
  Wave wave = Wave::triangle;  // a tone layer's oscillator
  Color color = Color::white;  // a noise layer's colour
  float detune_cents = 0;      // a tone layer plays a detuned pair this wide
  bool drive = false;          // a tone layer is folded through the saturation curve; how hard
                               // is the target's `grit`
  FilterType filter = FilterType::none;  // its cutoff is the target's
  float q = 0;                           // 0: the type's default
  Vibrato vibrato{};                     // `delay_ms` is ignored: a layer's vibrato is always on
  float echo = 0;
};

// Where a layer is steered to. Every field is reached over the glide the caller names, so a frame
// that arrives late leaves the layer holding rather than stepping.
struct LayerTarget {
  float level = 0;      // linear, on a one-shot's `volume` scale; 0 is silence
  float hz = 100;       // a tone layer's pitch (it glides on half the glide: the ear follows a
                        // pitch more closely than a fade)
  float cutoff = 1000;  // the filter's cutoff, held under `kMaxCutoffRatio` of the rate
  float grit = 0;       // how hard a driven layer is pushed into its curve, 0..1
  float pan = 0;
};

// THE SHORTEST ONSET a voice may have, ms. A gain that jumps from nothing to full between two
// samples is a step, which is a broadband click, and it makes a resonant filter ring at its own
// cutoff. 1.5 ms is far under the ~10 ms the ear hears as an attack, so nothing loses its snap;
// it is a floor, and never more than half the voice's length.
inline constexpr float kMinAttackMs = 1.5f;

// THE HIGHEST CUTOFF a filter is given, as a share of the rate: a filter tuned at or past the
// Nyquist frequency is not a bright filter but undefined coefficients. A tenth of an octave of
// headroom under it.
inline constexpr float kMaxCutoffRatio = 0.45f;

// The cutoff a filter actually gets: what it asked for, held to 20 Hz .. 0.45 of the rate.
inline float safe_cutoff(float hz) {
  const float ceiling = static_cast<float>(kRate) * kMaxCutoffRatio;
  return hz < 20 ? 20 : hz > ceiling ? ceiling : hz;
}

inline float clamp01(float v) { return v < 0 ? 0 : v > 1 ? 1 : v; }

// THE SATURATION CURVE is tanh(k x) / tanh(k) over [-1, 1], k = `shaper_steepness(drive)`: 1
// (nearly linear) to 10 (a fully driven amp), linear in the drive so the top half of the knob
// is not wasted on hash. `shaper_push` is the gain in front of the curve, which is what makes
// a drive audible at all: a curve does nothing to a signal that never reaches its knee.
inline float shaper_steepness(float drive) { return 1 + 9 * clamp01(drive); }
inline float shaper_push(float drive) { return 1 + 3 * clamp01(drive); }

// A driven LAYER folds through one fixed curve at this drive; `grit` moves the gain in front of
// it from `kGritFloor` (under the knee: not shaped) to `kGritPush`, and a trim after it keeps
// grit a change of timbre more than of level.
inline constexpr float kLayerDrive = 0.6f;
inline constexpr float kGritFloor = 0.35f;
inline constexpr float kGritPush = 4.0f;
inline constexpr float kGritTrim = 0.7f;

// The share of a voice's peak each oscillator of a detuned pair plays at.
inline constexpr float kPairShare = 0.6f;

}  // namespace ge::audio
