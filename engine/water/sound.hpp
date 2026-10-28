// THE WATER'S SOUND: what an ear hears of the water round it, driven by the water's own numbers
// and played on the synth (`audio/`). Three pieces, in the order a game calls them:
//
//   hear        WHAT IS HEARD at the ear: the water round it asked through `sample` on rings
//               out to 96 m (so the sound, the physics and the picture agree about where the
//               water is), and the flow regions' sheets. Every voice is an INTENSITY at the
//               ear, the power the water dissipates where it is over the square of the distance:
//               the lapping is the waves' energy (rho g eta^2) spent on a shore, a river's rush
//               is its bed's friction (rho c_f |v|^3), a waterfall's roar is its fall
//               (rho g Q h). So a waterfall twice as far is half as loud, a fresh wind on the
//               lake is heard on the shore it blows onto, and a passing wake laps there too.
//   the bed     seven noise layers steered from that (`water_targets`, a pure function): the
//               lapping and the surf's hiss, the river's rush and its babble, the falls' roar
//               and their spray's hiss, and the room under water. Under water every layer is
//               MUFFLED (a low-pass at a few hundred hertz: what crosses the surface and the
//               ear's own air is the low end) and the room comes up; `muffle` does the same to
//               a game's own beds and one-shots, so the whole world goes under together.
//   one-shots   splashes off a `Splash` (float.hpp) by its speed and size, a swimmer's strokes,
//               breaths and gasp off `SwimEvents` (swim.hpp), and BUBBLES: a bubble rings at
//               its Minnaert frequency (f r = 3.26 m/s at the surface's pressure), so a splash's
//               cloud, a river's babble and the churn under a fall are bubbles of their sizes.
//
// The intensities are W/m^2 up to one constant: every voice's `kFull*` names the intensity it
// is fully up at, and the levels follow the root (an amplitude) and saturate at 1.5 of it.
//
// ge:summary The water's sound: what an ear hears (hear), the water bed, splashes, strokes, bubbles
// ge:keywords water sound audio hear hearing bed lapping lap surf rush babble roar waterfall
// ge:keywords splash stroke breath gasp bubble minnaert muffle under-water room intensity
// ge:keywords water_targets play_splash play_swim water_bubbles water_bank
// ge:skill water audio
// ge:test tests/water_sound_test.cpp
#pragma once

#include <array>
#include <cstdint>
#include <span>

#include "audio/rack.hpp"
#include "audio/sound.hpp"
#include "audio/synth.hpp"
#include "core/prng.hpp"
#include "water/float.hpp"
#include "water/swim.hpp"
#include "water/world.hpp"

namespace ge::water {

// One voice of the water as the ear hears it.
struct Heard {
  float level = 0;     // the root of the intensity over its voice's full one, 0..1.5
  float pan = 0;       // -1 left .. 1 right, where the intensity comes from
  float distance = 0;  // m, the intensity-weighted distance (the air takes the highs)
};

struct Hearing {
  Heard lap;         // waves spent on a shore (and slapping on open water)
  Heard surf;        // the breaking share of it: whitecaps, a crest spilling, the bed tripping it
  Heard rush;        // channels and flow regions
  Heard roar;        // the flow regions' sheets landing
  float period = 2;  // s, the lapping waves' peak period
  float speed = 0;   // m/s, the rush's current
  float drop = 0;    // m, the loudest sheet's fall
  float depth = 0;   // m the ear is under the surface (0 above it)
  float under = 0;   // 0..1, how far the ear is under: 1 past 0.4 m
};

// The intensity each voice is fully up at (W/m^2 at the ear, up to the one constant): a
// shore's lapping under a fresh breeze (and its breaking share), a 6 m^3/s river from its bank,
// a 10 m fall of a few m^3/s from 15 m. The `water-audition` lab's places are tuned to them.
inline constexpr double kFullLap = 5;
inline constexpr double kFullSurf = 1;
inline constexpr double kFullRush = 15;
inline constexpr double kFullRoar = 600;
// The rings `hear` asks: radii, m, each twice the last (so each carries the same share of an
// even sheet of water: the area grows as r^2 and the intensity falls as 1 / r^2).
inline constexpr std::array<double, 7> kHearRings = {1.5, 3, 6, 12, 24, 48, 96};
inline constexpr int kHearDirs = 8;
// The bed's friction factor, the share of a current's energy its bed takes.
inline constexpr double kBedFriction = 0.004;

// What the ear at (x, y, z), facing `heading`, hears of `world` at `t`.
Hearing hear(const World& world, double x, double y, double z, double t, double heading = 0);

// The level a voice of intensity `intensity` plays at against its `full` one.
float level_of(double intensity, double full);

// THE BED.
enum WaterLayer : uint8_t { kLap, kSurf, kRush, kBabble, kRoar, kSpray, kRoom, kWaterLayers };

// What the listener's seat does to the water (the game's listener row), like the wind's.
struct WaterMix {
  float level = 1;
  float tone = 1;  // 0..1, how bright
};

extern const std::array<audio::LayerSpec, kWaterLayers> kWaterSpecs;
extern const std::array<float, kWaterLayers> kWaterGlides;

std::array<audio::LayerTarget, kWaterLayers> water_targets(const Hearing& heard,
                                                           const WaterMix& mix);

// UNDER WATER: a layer's cutoff pulled down toward `kMuffleHz` and its level toward
// `kMuffleGain` by `under` (0..1); a one-shot's shape pitched down and quietened the same way.
inline constexpr float kMuffleHz = 420;
inline constexpr float kMuffleGain = 0.55f;
void muffle(audio::LayerTarget& target, float under);
void muffle(audio::PlayShape& shape, float under);

// The water bed's state: its rack and the bubbles' draws.
struct WaterBed {
  audio::Rack rack;
  core::Rng rng{1};
  int strokes = 0;  // a swimmer's, for the breath every other one
};

WaterBed make_water_bed(int bus, uint32_t seed = 1);

// Steer the layers to what is heard and loose this frame's bubbles; `dt` is the frame's own
// elapsed time, s. Returns the hearing, for the game to muffle its own beds by.
Hearing water_bed_update(WaterBed& bed, audio::Synth& synth, const World& world, double x, double y,
                         double z, double t, double heading, float dt, const WaterMix& mix = {});
void water_bed_silence(WaterBed& bed, audio::Synth& synth);

// THE ONE-SHOTS.
extern const std::span<const audio::SoundDef> kWaterBank;

// A bubble of `radius` m rings at f = 3.26 / r Hz (Minnaert, at the surface's pressure).
double minnaert_hz(double radius);
// One bubble: a sine at its Minnaert pitch rising a little as it nears the surface, ringing
// longer the bigger it is.
audio::Tone bubble(double radius, float volume, float delay_ms = 0, float pan = 0);
// `count` bubbles of radii drawn log-even in [lo, hi] m, over `spread_ms`.
void play_bubbles(audio::Synth& synth, core::Rng& rng, int count, double lo, double hi,
                  float spread_ms, float volume, int bus = 0);
// This frame's bubbles off what is heard: a river's babble, the churn under a fall, more of it
// all under water. `dt` s of them, drawn from `rng`.
void water_bubbles(audio::Synth& synth, core::Rng& rng, const Hearing& heard, float dt,
                   int bus = 0);

// A splash's shape off its speed and size against the bank's reference (5 m/s, 0.15 m).
audio::PlayShape splash_shape(const Splash& splash);
// A splash, with the bubbles a body going in takes down with it; `under` muffles it.
void play_splash(audio::Synth& synth, core::Rng& rng, const Splash& splash, float under = 0,
                 int bus = 0, float pan = 0);
// A swimmer's step: the splash in or out, the stroke, a breath every other stroke with his eyes
// out, bubbles from a stroke under, the gasp.
void play_swim(audio::Synth& synth, WaterBed& bed, const SwimEvents& events, const SwimState& state,
               float under = 0, int bus = 0);

}  // namespace ge::water
