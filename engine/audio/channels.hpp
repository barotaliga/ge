// SOUNDS IN A WORLD: recordings placed at points, heard by a listener, and how many may sound at
// once. A game says "entity 7 fired, here" (`Channels::play`), moves its sources as they move,
// and once a frame hands the listener over (`update`); everything else (how loud, which side,
// which sound gives way) is decided here and steered on the synth's sampled voices.
//
// HEARING (`hear`) is two numbers, both pure:
//
//   the GAIN  falls linearly with distance, `volume * (1 - distance * attenuation)`, silent past
//             1 / attenuation: an attenuation of 1/1000 a unit is heard a thousand units away,
//             0 is heard everywhere at full level (an announcer, the player's own sounds)
//   the PAN   is the source's direction against the listener's right, `dot(right, to source)`
//             over the distance, times `width`; a source on the listener is centred
//
// THE CHANNELS are a fixed number of sounding slots, each a sampled voice and who made it:
//
//   * a sound on an entity's numbered channel (channel != 0) REPLACES what that entity's channel
//     was playing: a weapon's next shot cuts its last, a voice's next line its first;
//   * channel 0 never replaces, so overlapping footsteps or ricochets stack;
//   * when every slot is taken the new sound STEALS the slot heard most quietly right now (the
//     oldest among equals), and is dropped instead when it would be quieter still;
//   * a sound that does not loop and is out of earshot when played is never started.
//
// The listener's own entity (`Listener::entity`) is heard at its volume, centred, wherever its
// sources are: a player's own gun is not to one side of his head.
//
// Deterministic like the synth: the same plays and updates give the same steering calls.
//
// ge:summary Sounds in a world: hear (distance gain, pan) and Channels (entity channels, stealing)
// ge:keywords spatial 3d positional listener attenuation distance pan channel
// ge:keywords channel-limit steal replace entity loop source move update hear heard
// ge:skill audio
// ge:test tests/audio_channels_test.cpp
#pragma once

#include <cstdint>
#include <vector>

#include "audio/pcm.hpp"
#include "audio/synth.hpp"
#include "core/quat.hpp"

namespace ge::audio {

struct Listener {
  core::Vec3 at{0, 0, 0};
  core::Vec3 right{1, 0, 0};  // unit length
  int entity = -1;            // whose sounds are the listener's own (-1: nobody's)
};

struct Heard {
  float gain = 0;
  float pan = 0;
};

// How a source at `at` is heard (see the header). `width` scales the pan (1: a source square to
// the right is only in the right ear).
Heard hear(const Listener& listener, core::Vec3 at, float volume, float attenuation,
           float width = 1);

// A sound in the world, as a game plays it.
struct Emit {
  int entity = -1;  // who made it (-1: the world, never replaced)
  int channel = 0;  // the entity's channel: != 0 replaces, 0 stacks
  core::Vec3 at{0, 0, 0};
  float volume = 1;
  float attenuation = 0.001f;  // per world unit
  float pitch = 1;
  float echo = 0;
  Loop loop = Loop::file;
};

class Channels {
 public:
  // `limit` slots (at most kMaxSamples), played on the synth's `bus`; the pan's `width`.
  Channels(Synth& synth, int limit = 16, int bus = 0, float width = 1);

  // Play `pcm` (kept alive by the caller while it may sound). Returns the voice, or an invalid
  // id when it was out of earshot or quieter than every slot.
  SampleId play(const Pcm& pcm, const Emit& emit);
  // Move every sound `entity` is making (a rocket's flight, a player running while he fires).
  void move(int entity, core::Vec3 at);
  // Stop what `entity` plays on `channel` (0: everything it plays).
  void stop(int entity, int channel = 0, float fade_s = 0.02f);
  // The listener this frame: every sounding slot is heard again and steered over `glide_s`,
  // and slots whose voice has ended are freed.
  void update(const Listener& listener, float glide_s = 0.03f);

  int playing() const;  // slots taken
  int stolen() const { return stolen_; }
  int skipped() const { return skipped_; }  // plays dropped as out of earshot or too quiet

  struct Slot {
    SampleId voice;
    int entity = -1;
    int channel = 0;
    core::Vec3 at{0, 0, 0};
    float volume = 1, attenuation = 0;
    bool loops = false;
    uint64_t order = 0;
    Heard heard;
  };
  const std::vector<Slot>& slots() const { return slots_; }

 private:
  Heard heard_of(int entity, core::Vec3 at, float volume, float attenuation) const;

  Synth& synth_;
  int bus_;
  float width_;
  Listener listener_;
  std::vector<Slot> slots_;
  uint64_t order_ = 0;
  int stolen_ = 0, skipped_ = 0;
};

}  // namespace ge::audio
