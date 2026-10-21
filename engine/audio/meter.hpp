// THE METER: what a rendered buffer sounds like, as numbers an agent can read without ears. A
// session that cannot listen checks a sound by its SHAPE: how loud it gets (the loudest 10 ms),
// where its energy sits (ten octave bands and their centroid), when it starts and how long it
// rings, and which side it is on. The audition labs print these for every sound and bed, and the
// tests hold a sound to its intent with them ("the gate's slap sits under the landing", "a brown
// rumble's centroid is under 500 Hz").
//
// The bands are measured with the synth's own state-variable band passes (about an octave wide,
// centred on 31.25 Hz x 2^k), so a meter is as deterministic as the buffer it reads.
//
// ge:summary The meter: a buffer's level, peak, octave bands, centroid, onset, length, balance
// ge:keywords meter metering level rms peak dbfs loudness spectrum octave-bands centroid
// ge:keywords brightness envelope onset length decay balance stereo measure listen
// ge:skill audio labs
// ge:test tests/audio_meter_test.cpp
#pragma once

#include <array>
#include <span>
#include <vector>

namespace ge::audio {

inline constexpr int kBands = 10;
inline constexpr std::array<float, kBands> kBandHz = {31.25f, 62.5f, 125,  250,  500,
                                                      1000,   2000,  4000, 8000, 16000};
inline constexpr float kWindowS = 0.01f;   // the envelope's window
inline constexpr float kSilenceDb = -120;  // what silence meters as

struct Meter {
  float peak_db = kSilenceDb;     // the largest sample on either side, dBFS
  float rms_db = kSilenceDb;      // the whole buffer's RMS, both sides, dBFS
  float loudest_db = kSilenceDb;  // the loudest 10 ms window's RMS, dBFS: "how loud it is"
  float onset_s = 0;              // the first window within 40 dB of the loudest
  float length_s = 0;             // from the onset to the last window within 40 dB of the loudest
  float centroid_hz = 0;          // the bands' energy-weighted centre
  float balance = 0;              // -1 all left .. 1 all right, by energy
  std::array<float, kBands> band_db{};  // each band's RMS, dBFS
  std::vector<float> envelope_db;       // the RMS of each 10 ms window, dBFS
};

// Meter `frames` of interleaved stereo at the synth's rate.
Meter measure(std::span<const float> stereo);

// 20 log10 of a level, floored at `kSilenceDb`.
float to_db(float level);

}  // namespace ge::audio
