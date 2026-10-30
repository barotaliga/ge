// THE RAIN'S SOUND: what an ear hears of a fall round it (engine/wind/precip.hpp's `Precip`),
// played on the synth (`audio/`) as the water's own bed is (sound.hpp). Three pieces:
//
//   hear_rain   WHAT IS HEARD: the rain's loudness is the power its drops bring down, their
//               kinetic energy a square metre a second (1/2 rho R v^2, R the rate and v the
//               median drop's terminal speed), and a voice's level is the root of that over a
//               downpour's (50 mm/h): a drizzle is a whisper, a cloudburst past full. Where the
//               ear is splits it: the drops on the ground round the ear (the patter) where the
//               fall reaches, on water (each drop that goes in traps a bubble that rings: a
//               light rain's drops of about a millimetre trap ones that ring near 14 kHz, so a
//               lake in the rain hisses high, and a heavy rain's bigger drops ring lower), on a
//               roof over the ear (the drumming), and the rain over the land round it out of
//               sight (the wash), heard under a roof too. A storm's SNOW is near silent as it
//               falls; driven on a wind it hisses past the ear, by its rate and the wind.
//   the bed     five noise layers steered from that (`rain_targets`, a pure function): the
//               patter, the wash, the drops on water, the drum and the driven snow.
//   the drops   in a light rain the drops near the ear are heard one by one: a tick on the
//               ground, a plink on water (a bubble at its Minnaert pitch, sound.hpp's `bubble`),
//               drawn each frame by the drops landing within a couple of metres (`rain_drops`);
//               a heavier rain's are too many to tell apart and are the layers alone.
//
// ge:summary The rain's sound: what is heard of a fall by its rate, the rain bed, single drops
// ge:keywords rain sound audio hear_rain patter wash drum roof drops-on-water bubble driven-snow
// ge:keywords blizzard hiss rain_targets rain_bed rain_drops kinetic-energy rate
// ge:skill water audio
// ge:test tests/water_rain_test.cpp
#pragma once

#include <array>
#include <cstdint>

#include "audio/rack.hpp"
#include "audio/synth.hpp"
#include "core/prng.hpp"
#include "water/sound.hpp"
#include "wind/precip.hpp"

namespace ge::water {

// Where the ear is, against the fall.
struct RainPlace {
  double open = 1;   // the share of the sky over the ear the fall reaches: 0 under a roof
  double water = 0;  // the share of the ground round the ear that is water
  double roof = 0;   // the share of the sky over the ear a hard roof covers, drummed on
};

// What the ear hears of a fall: each voice's level, the root of its power over a downpour's.
struct RainHearing {
  float patter = 0;  // the drops on the ground round the ear
  float wash = 0;    // the rain over the land round it, out of sight
  float water = 0;   // the drops on water round the ear
  float drum = 0;    // the drops on a roof over the ear
  float driven = 0;  // a storm's snow driven past the ear
  float drop = 0;    // mm, the median drop
  float drops = 0;   // the drops of a millimetre and more landing a m^2 a second, open ground
  float lake = 0;    // the share of those that land on water
  float wind = 0;    // m/s
};

// The rain's power each voice is fully up at: a downpour's, 50 mm/h.
inline constexpr double kFullRain = 50;
// Snow driven past the ear is fully up at 5 mm/h of water (half a metre of new snow a day) in a
// 20 m/s wind; under 5 m/s it falls silently.
inline constexpr double kFullDriven = 5;
inline constexpr double kDrivenFrom = 5, kDrivenFull = 20;  // m/s

// The kinetic energy the rain at `rate` mm/h brings down, J / m^2 / s.
double rain_power(double rate);

// What the ear at `place` hears of the fall `p` in a wind of `wind` m/s.
RainHearing hear_rain(const wind::Precip& p, double wind, const RainPlace& place);

// THE BED.
enum RainLayer : uint8_t { kPatter, kWash, kOnWater, kDrum, kDriven, kRainLayers };

extern const std::array<audio::LayerSpec, kRainLayers> kRainSpecs;
extern const std::array<float, kRainLayers> kRainGlides;

std::array<audio::LayerTarget, kRainLayers> rain_targets(const RainHearing& heard,
                                                         const WaterMix& mix);

struct RainBed {
  audio::Rack rack;
  core::Rng rng{1};
};

RainBed make_rain_bed(int bus, uint32_t seed = 1);

// Steer the layers to what is heard and loose this frame's single drops; `dt` is the frame's
// own elapsed time, s.
void rain_bed_update(RainBed& bed, audio::Synth& synth, const RainHearing& heard, float dt,
                     const WaterMix& mix = {});
void rain_bed_silence(RainBed& bed, audio::Synth& synth);

// The drops landing within this many m^2 of the ear (a hood, a puddle at the feet) are heard one
// by one, up to this many a frame; past it the layers carry them.
inline constexpr double kNearDrops = 0.1;
inline constexpr int kMostDrops = 3;
// The single drops of `dt` s off what is heard, drawn from `rng`. Returns how many were played.
int rain_drops(audio::Synth& synth, core::Rng& rng, const RainHearing& heard, float dt,
               int bus = 0);

}  // namespace ge::water
