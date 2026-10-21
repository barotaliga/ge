// THE FADERS: what the player sets (a master, the wind, the effects and the sound switch) folded
// into the synth's buses. The wind is its own fader because it is the loudest thing in the game
// and the one a player most often wants down; every other sound is the effects. `mix_of` is the
// one place the master and the switch are folded in; nothing else changes the volume.
//
// ge:summary The faders: master, wind, effects and the switch folded into the synth's buses
// ge:keywords faders mix mix_of bus master wind effects volume switch
// ge:skill audio sim
// ge:test tests/fall_line_sound_test.cpp
#pragma once

#include "audio/synth.hpp"

namespace fall_line::sound {

inline constexpr int kBusWind = 0;     // the skier's own wind
inline constexpr int kBusEffects = 1;  // the snow under him and every one-shot

struct Faders {
  float master = 0.8f;
  float wind = 1;
  float effects = 1;
  bool on = true;
};

struct BusGains {
  float wind;
  float effects;
};

inline BusGains mix_of(const Faders& f) {
  const auto unit = [](float v) { return v < 0 ? 0.0f : v > 1 ? 1.0f : v; };
  const float master = f.on ? unit(f.master) : 0;
  return {master * unit(f.wind), master * unit(f.effects)};
}

// Hand the faders to the synth; a fader moved is heard over `glide_s`.
inline void apply_mix(ge::audio::Synth& synth, const Faders& f, float glide_s = 0.05f) {
  const BusGains g = mix_of(f);
  synth.bus_gain(kBusWind, g.wind, glide_s);
  synth.bus_gain(kBusEffects, g.effects, glide_s);
}

}  // namespace fall_line::sound
