// THE SOUND ENHANCER: a classic pack's sounds (8-bit or 16-bit PCM, mono, 11 025 Hz, some looping)
// brought to the engine's 48 kHz and cleaned up, for the cache's `sounds` stage.
//
// Each sound goes through four steps, all offline and deterministic:
//
// 1. **Noise reduction** at the source rate: a short-time spectrum (256 samples, hop 64), the
//    noise measured in the sound's own quiet gaps (frames within 6 dB of the quietest that is not
//    digital silence, when it lies 30 dB or more under the loudest; a sound with no gap is held
//    to a quarter of the quantisation floor, so a held voice or a hum is never taken for noise),
//    and a decision-directed Wiener gain with a floor, so the hiss drops by up to `floor_db`
//    without the "musical noise" of plain subtraction.
// 2. **Upsampling** to 48 kHz by `audio::resample` (a Kaiser-windowed sinc), adding nothing above
//    the source's band.
// 3. **Bandwidth extension**: the empty band from the source's edge (about 5.2 kHz) up to
//    `extend_to_hz` is filled by copying the top half-octave of the real spectrum upwards by a
//    shift that is a multiple of the frame rate (so the copies' phases stay coherent and
//    transients stay sharp), each copy attenuated by the frame's own spectral slope over the
//    octaves it moved, plus `rolloff_db` an octave. The sound keeps its timing and its timbre's
//    tilt and stops sounding muffled.
// 4. **Peaks**: a look-ahead limiter holds them under `knee` with a gain that moves smoothly
//    (no clipped harmonics). Levels are not normalised: the set's balance is the original
//    designers'.
//
// A loop start (the WAV's `cue ` chunk) is carried through at the new rate.
//
// ge:summary The classic pack's sounds: WAV read, noise reduced, resampled to 48 kHz, band extended
// ge:keywords sound wav denoise noise-reduction wiener bandwidth-extension upsample loop cue
// ge:keywords hiss quantisation enhance audio
// ge:skill classicenhance
// ge:test tests/classicenhance_sound_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace ge::classicenhance {

// A mono sound as read: samples in [-1, 1], its rate, its bit depth and its loop start (-1: none).
struct Pcm {
  int rate = 0;
  int bits = 16;
  std::vector<float> samples;
  long loop_start = -1;
};

// A RIFF WAV of 8-bit (unsigned) or 16-bit PCM, mono or stereo (folded to mono), with an
// optional `cue ` chunk. Refuses anything else, saying what it found.
bool read_wav(std::span<const uint8_t> bytes, Pcm* out, std::string* error);
// 16-bit mono WAV bytes at the sound's rate, with a `cue ` chunk when it loops.
std::vector<uint8_t> write_wav(const Pcm& pcm);

struct SoundOptions {
  int rate = 48000;
  double denoise = 1.0;         // 0: off; 1: the default strength
  double floor_db = -15.0;      // the deepest the noise reduction cuts a bin
  double extend = 1.0;          // 0: no bandwidth extension; 1: the default level
  double extend_to_hz = 16000;  // the top of the rebuilt band
  double rolloff_db = 3.0;      // extra fall an octave in the rebuilt band
  double knee = 0.9;            // the limiter's ceiling
};

struct SoundReport {
  double noise_db = 0;     // the noise floor found, dBFS per bin (relative)
  double removed_db = 0;   // energy taken out by the noise reduction, dB
  double added_db = 0;     // energy of the rebuilt band against the whole, dB
  double source_hz = 0;    // where the source's band ends
};

Pcm enhance_sound(const Pcm& in, const SoundOptions& opts, SoundReport* report = nullptr);

// The steps on their own, for the lab and the tests.
std::vector<float> reduce_noise(std::span<const float> x, int bits, const SoundOptions& opts,
                                SoundReport* report);
std::vector<float> extend_band(std::span<const float> x, int rate, double source_hz,
                               const SoundOptions& opts, SoundReport* report);

}  // namespace ge::classicenhance
