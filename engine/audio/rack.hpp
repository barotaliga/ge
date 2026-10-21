// A RACK OF LAYERS: the one piece of plumbing every bed shares. A bed is a table of layer specs,
// a table of glides and a function from the frame's state to a table of targets; the rack stands
// between that function and the synth. It builds every layer the first time it is applied and
// steers each toward its target on its own glide, once a frame. The synth skips a target that has
// not moved, so a bed holding still costs the ring nothing.
//
// ge:summary A rack of layers: a bed's specs and glides, built once and steered every frame
// ge:keywords rack bed layers steer apply rack_apply rack_stop specs glides targets
// ge:skill audio
// ge:test tests/audio_synth_test.cpp
#pragma once

#include <array>
#include <span>

#include "audio/synth.hpp"

namespace ge::audio {

inline constexpr int kRackMax = 16;  // layers in one rack

struct Rack {
  std::span<const LayerSpec> specs;
  std::span<const float> glides_s;  // one a layer
  int bus = 0;
  std::array<LayerId, kRackMax> ids{};
  bool built = false;
};

// Steer every layer of `rack` toward `targets` (one a layer, in the specs' order), building them
// first if this is the first frame.
void rack_apply(Rack& rack, Synth& synth, std::span<const LayerTarget> targets);

// Tear the layers down; the next apply builds them again.
void rack_stop(Rack& rack, Synth& synth);

}  // namespace ge::audio
