// THE INSTRUMENT every sound is played on: a fixed-capacity DSP graph that renders 48 kHz stereo
// float. Nothing ships as an audio file; a sound is a list of numbers (`sound.hpp`), played here.
//
// THE GRAPH, per block of `kBlock` frames:
//   one-shots (`tone`, `noise`)  oscillators or a noise window -> drive -> filter -> envelope
//   layers (`layer`, `set`)      a steered source -> drive -> filter -> level
//        each -> pan -> its BUS's gain -> the mix, with a send into the shared ECHO
//   the send -> the ROOM's reverb (reverb.hpp), while a room is set
//   the mix + the echo + the room -> the LIMITER -> the output
//
// THE BUSES are the faders: every voice and layer is played on one of `kBuses`, and a bus's gain
// (`bus_gain`) is applied as it renders, so a fader moved mid-sound is heard at once and a bus at
// zero mutes its echo too. Which bus is which is the game's (its `mix_of`).
//
// THE ECHO is one filtered feedback delay every voice can send into, so overlapping sounds sit in
// the same place: outdoors what answers is a treeline a few hundred metres off, short and damped.
//
// THE ROOM is the space round the listener (`room`, a RoomSound: the walls' early echoes and a
// reverberant tail, room.hpp probes it from the level), fed by the same send; it also sets how
// much of the outdoor echo is heard, so a closed room silences the treeline.
//
// THE LIMITER keeps a stack of sounds out of the clip: a soft-knee compressor at -10 dBFS, ratio
// 12, 5 ms attack (slower would let a landing through; faster would distort the bass inside a
// cycle) and 250 ms release. Single sounds (-34 to -20 dBFS) pass untouched.
//
// THREADS: the game thread is the PRODUCER (`tone`, `noise`, `layer`, `set`, `stop`, `bus_gain`)
// and the render thread is the CONSUMER (`render`): calls travel through a single-producer
// single-consumer ring and are applied at the start of the next block, so `render` never locks
// and never allocates. Offline (a test, a lab) both are the same thread: call, then render. A
// full ring drops the call and counts it (`Stats::dropped`); a full voice pool steals the oldest
// one-shot (`Stats::stolen`).
//
// DETERMINISTIC: the same calls before the same `render`s give the same bytes on every platform
// (`dsp.hpp`), which is what makes a sound's buffer a golden and a lab digest.
//
// ge:summary The synth: one-shots, steered layers, buses, echo, limiter; the producer/render ring
// ge:keywords synth instrument dsp-graph render block one-shot layer bus fader echo delay
// ge:keywords limiter compressor ring spsc command producer consumer render-thread device
// ge:keywords voice-pool steal stats offline-render room reverb
// ge:skill audio
// ge:test tests/audio_synth_test.cpp
#pragma once

#include <cstdint>
#include <memory>

#include "audio/reverb.hpp"
#include "audio/voice.hpp"

namespace ge::audio {

inline constexpr int kBlock = 128;      // frames a block: the unit calls are applied at
inline constexpr int kBuses = 4;        // faders
inline constexpr int kMaxVoices = 96;   // one-shots sounding at once
inline constexpr int kMaxLayers = 48;   // layers built at once
inline constexpr int kRingSize = 1024;  // calls in flight between two blocks

// A layer, as `layer` handed it out. Invalid when the synth had no free slot.
struct LayerId {
  int16_t slot = -1;
  bool valid() const { return slot >= 0; }
};

// The shared echo's room.
struct EchoRoom {
  float delay_s = 0.21f;
  float feedback = 0.24f;
  float damp_hz = 1800;
};

struct Stats {
  uint64_t frames = 0;     // rendered since creation
  uint32_t voices = 0;     // one-shots sounding after the last block
  uint32_t layers = 0;     // layers built
  uint32_t dropped = 0;    // calls lost to a full ring
  uint32_t stolen = 0;     // one-shots cut short for a new one
  float reduction_db = 0;  // the limiter's gain reduction after the last block (<= 0)
};

class Synth {
 public:
  // `seed` draws the noise pools and every noise window's offset.
  explicit Synth(uint32_t seed = 1, EchoRoom room = {});
  ~Synth();
  Synth(const Synth&) = delete;
  Synth& operator=(const Synth&) = delete;

  // --- the producer (the game thread) ---
  void tone(const Tone& tone, int bus = 0);
  void noise(const Noise& noise, int bus = 0);
  // Build a layer, silent until steered. Invalid when all `kMaxLayers` are built.
  LayerId layer(const LayerSpec& spec, int bus = 0);
  // Steer it: every field of `target` is reached on the one-pole `glide_s` (a pitch on half
  // of it). A target within 1e-4 of the last one sent is not sent again.
  void set(LayerId id, const LayerTarget& target, float glide_s);
  // Fade it out over a few hundredths and free its slot.
  void stop(LayerId id);
  // A fader: the bus's gain, reached on `glide_s` (0: set at once, as before a sound plays).
  void bus_gain(int bus, float gain, float glide_s = 0.05f);
  // The listener's room, glided to over `glide_s` (0: at once). A synth starts with none.
  void room(const RoomSound& room, float glide_s = 0.3f);

  // --- the consumer (the render thread) ---
  // Render `frames` of interleaved stereo (L, R, L, R, ...) into `out`, applying every call
  // that arrived since the last block at the start of each block.
  void render(float* out, int frames);

  // The synth's clock: frames rendered over the rate, s. Safe from either thread.
  double now_s() const;
  Stats stats() const;

  struct Impl;

 private:
  std::unique_ptr<Impl> impl_;
};

}  // namespace ge::audio
