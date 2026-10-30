// THUNDER HEARD: a storm's thunder played on the synth's sampled voices, each clap at the moment
// its first sound reaches the ear (docs/storms.md § Thunder heard). The storm's flashes are a pure
// function of its clock (`flashes_between`), so the player only remembers which it has scheduled.
//
// - `hear_storm(player, storm, t, ear, ...)` looks at every flash that started since the last
//   call (the first call looks back as far as thunder is heard, 25 km at the speed of sound, so a
//   listener arriving under a storm hears the thunder already on its way), works out its thunder
//   at the ear (`hear_thunder`), renders it (`render_thunder`) and queues it at its first sound,
//   panned by the bearing to the channel's nearest point.
// - `play_thunder(player, synth, t, block_s, bus)` starts every queued clap due before
//   `t + block_s`, its first sample placed inside the block by the play's delay, and lets a clap's
//   recording go once its voice has ended. A clap due in the past is started at once if it is less
//   than `kThunderLate` late, and dropped otherwise (a run paused through it).
//
// The rendering is the cost: a near strike's thunder is up to 60 s of samples, done once a flash.
// No more than `kThunderClaps` wait or sound at once; past that the quietest is let go.
//
// ge:summary Thunder heard: a storm's flashes scheduled at their arrival and played as samples
// ge:keywords thunder-play thunder-heard hear_storm play_thunder clap arrival pan sample-voice
// ge:skill water audio
// ge:test tests/wind_thunder_play_test.cpp
#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "audio/pcm.hpp"
#include "audio/synth.hpp"
#include "wind/storm.hpp"

namespace ge::wind {

inline constexpr int kThunderClaps = 12;
inline constexpr double kThunderLate = 0.25;  // s

// Where the thunder is heard: the ear's place (world m) and the way it faces (heading, radians,
// 0 = +z, clockwise from above), and the air's temperature for the speed of sound.
struct Ear {
  double x = 0, y = 0, z = 0;
  double heading = 0;
  double celsius = 10;
};

struct ThunderClap {
  double at = 0;  // absolute s: its first sound at the ear
  uint64_t key = 0;
  double distance = 0;  // m, the channel's nearest point
  float gain = 1, pan = 0;
  std::unique_ptr<audio::Pcm> pcm;
  audio::SampleId id{};
};

struct ThunderPlayer {
  std::vector<ThunderClap> waiting;   // by `at`
  std::vector<ThunderClap> sounding;  // started, the synth reading their recordings
  std::vector<uint64_t> heard;        // every flash queued, so none is queued twice
  double last = 0;
  bool started = false;
  int rate = 48000;
  double gain = 0.8;  // `render_thunder`'s: the pressure of 1 km of channel heard from 1 km
  uint32_t seed = 1;
};

// The flash's key: its cell and its slot.
uint64_t thunder_key(int cell, double t0);

// Queue the thunder of every flash of `storm` started in (the last call, `t`] at `ear`.
void hear_storm(ThunderPlayer& player, const Storm& storm, double t, const Ear& ear);

// Start every clap due before `t + block_s` on the synth's `bus`; free the ones that ended.
void play_thunder(ThunderPlayer& player, audio::Synth& synth, double t, double block_s,
                  int bus = 0);

}  // namespace ge::wind
