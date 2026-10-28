// A SOUND AT ANOTHER RATE: band-limited resampling by a windowed sinc, for offline work (bringing a
// file recorded at 11 025 Hz or 22 050 Hz to the synth's 48 kHz). The kernel is a Kaiser-windowed
// sinc tabulated at `phases` positions between input samples and read with linear interpolation
// between neighbouring phases; its cutoff is `cutoff` times the lower of the two Nyquist rates, so
// going up adds nothing above the source's band and going down leaves no alias. Deterministic:
// the table comes from `core::libm`. Not for the audio thread: it allocates.
//
// ge:summary Offline windowed-sinc resampling between any two rates (Kaiser window)
// ge:keywords resample sample-rate upsample downsample sinc kaiser polyphase band-limited
// ge:skill audio
// ge:test tests/audio_resample_test.cpp
#pragma once

#include <span>
#include <vector>

namespace ge::audio {

struct ResampleOptions {
  int taps = 32;          // kernel half-width, in samples of the lower rate
  double cutoff = 0.95;   // of the lower Nyquist rate
  double beta = 9.0;      // the Kaiser window's shape (about 90 dB stopband)
  int phases = 512;       // kernel positions tabulated between two input samples
};

// `in` at `from` Hz, returned at `to` Hz; the result is round(in.size() * to / from) long.
std::vector<float> resample(std::span<const float> in, double from, double to,
                            const ResampleOptions& opts = {});

}  // namespace ge::audio
