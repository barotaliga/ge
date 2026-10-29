// AN AMBIENT BED: a few looped recordings under everything else, each at a level the game sets
// from where the listener is (a stream within earshot, open sky overhead), each level walking
// toward its target at a fixed rate so moving through a door swells or fades the sound rather than
// cutting it. Not placed and not panned: an ambience is around the listener, not at a point.
//
// - **A layer** (`add`) is a recording and its gain at full level. Its voice is a sampled voice on
//   the synth (`Loop::whole`), started the first step its level is above zero and stopped (a short
//   fade) the step it reaches zero, so a silent layer costs no voice.
// - **The level** (0..1) moves toward the target by at most `rate` a second. A target under
//   `floor` counts as zero, so a faint level a map's compiler left at the edge of a room does not
//   keep a voice sounding.
// - **The voice's gain** is `level * gain`, steered every step on a glide as long as the step.
//
// Deterministic: the same targets and steps give the same synth calls.
//
// ge:summary An ambient bed: looped layers whose levels walk to the game's targets at a fixed rate
// ge:keywords ambient ambience bed loop layer level target rate floor fade swell leaf
// ge:skill audio
// ge:test tests/audio_ambient_test.cpp
#pragma once

#include <vector>

#include "audio/pcm.hpp"
#include "audio/synth.hpp"

namespace ge::audio {

struct AmbientLayer {
  const Pcm* pcm = nullptr;  // read in place; outlives the bed
  float gain = 1;            // the voice's gain at full level
  float level = 0;           // where the level is now, 0..1
  float target = 0;          // where it is going, 0..1 (under the floor: 0)
  SampleId voice;            // invalid while silent
};

class AmbientBed {
 public:
  // `rate`: the most a level moves a second. `floor`: a target under it is silence.
  explicit AmbientBed(float rate = 0.4f, float floor = 0.03f, int bus = 0);

  // A layer playing `pcm` at `gain` when its level is full; its index.
  int add(const Pcm& pcm, float gain = 1);
  // Where layer `i`'s level should go, 0..1.
  void target(int i, float level);
  // Move every level by `dt` seconds toward its target and steer the voices.
  void step(Synth& synth, float dt);
  // Stop every voice at once (a map change).
  void silence(Synth& synth);

  const std::vector<AmbientLayer>& layers() const { return layers_; }

 private:
  std::vector<AmbientLayer> layers_;
  float rate_, floor_;
  int bus_;
};

}  // namespace ge::audio
