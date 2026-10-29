// THUNDER: what a flash's channel sounds like at a listener (docs/storms.md § Thunder). Each
// stroke heats its channel at once, and every stretch of it is heard when its sound has come the
// distance: the nearest part first, the farthest last, so a channel's spread in distance is the
// thunder's length, and its bends and branches are the rumble's lumps.
//
// - THE ARRIVAL (`hear_thunder`): every segment's sound at the stroke's moment plus its distance
//   over the speed of sound (`sound_speed`: 331.3 + 0.606 m/s per degree Celsius, 343 at 20), its
//   intensity its length and glow over its distance squared, binned in 10 ms. The first stroke is
//   heard off the branches too, the later ones off the main channel they follow.
// - WHAT THE AIR TAKES: the high frequencies die with distance (the cut-off falls from a crack's
//   several kilohertz near to under 100 Hz at 15 km), and past 12 km the sound is bent up over a
//   listener by the air cooling with height, gone by about 25 km (beyond that a flash is heat
//   lightning: seen, never heard).
// - THE SOUND (`render_thunder`): hashed noise under the arrival's envelope, low-passed bin by bin
//   at its cut-off, over a low rumble that lingers, soft-clipped, mono at any rate.
//
// ge:summary Thunder: a channel's sound by distance, its envelope and cut-off, rendered as PCM
// ge:keywords thunder rumble clap crack sound-speed arrival delay absorption refraction
// ge:keywords hear_thunder render_thunder sound_speed heat-lightning envelope
// ge:skill water audio
// ge:test tests/wind_lightning_test.cpp
#pragma once

#include <cstdint>
#include <span>

#include "wind/lightning.hpp"

namespace ge::wind {

inline constexpr double kThunderBin = 0.01;  // s
inline constexpr int kThunderBins = 6000;    // 60 s of thunder
inline constexpr double kSilent = 25000;     // m, past which no thunder is heard

// The speed of sound in air at `celsius`, m/s.
double sound_speed(double celsius);

struct Thunder {
  double first = 0;    // s after the flash's start that the first sound arrives
  double length = 0;   // s from the first sound to the last
  double nearest = 0;  // m, the channel's nearest point
  double peak = 0;     // the loudest bin's intensity
  int bins = 0;
  float loud[kThunderBins];    // intensity: 1 km of channel heard from 1 km, energy 1
  float cutoff[kThunderBins];  // Hz, the bin's high-frequency cut-off
};

// The thunder of `flash` along `channel` at a listener at (x, y, z).
void hear_thunder(const Flash& flash, const Channel& channel, double x, double y, double z,
                  double speed, Thunder& out);

// `thunder` as mono samples at `rate` Hz into `out` (from its first sound; the rest is silence),
// louder by `gain` (the pressure is the square root of the intensity times it).
void render_thunder(const Thunder& thunder, double rate, uint32_t seed, double gain,
                    std::span<float> out);

}  // namespace ge::wind
