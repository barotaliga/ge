// THE ROOM'S REVERB: what the space round the listener does to the synth's shared echo send, in
// two parts, both stereo:
//
//   EARLY   up to kEarlyTaps single reflections off the nearest surfaces, each a delay (the way
//           to the wall and back), a gain (the spreading and the surface's absorption) and a
//           side (where the wall is), read off one line, each damped by its wall
//   LATE    a feedback delay network of kLines lines mixed by a Householder matrix (every line
//           feeds every other, so the echoes thicken into a smooth tail), each line's length
//           the room's mean free path scaled by a spread of mutually prime ratios, each loop's
//           gain set so its energy falls 60 dB in `decay_s` (10^(-3 L / (rate decay_s))) and
//           a one-pole lowpass at `damp_hz` in each loop, so the highs die first as air and soft
//           surfaces take them; fed `predelay_ms` after the sound, the gap before the first
//           wave comes back; its level normalised so `wet` 1 is a tail with the send's energy
//
// A ROOM CHANGES SMOOTHLY: every number glides on `glide_s` (a one-pole every kControl frames),
// the lines' lengths too, read with linear interpolation, so walking from a corridor into a hall
// opens the tail without a click. The early taps cross-fade the same way.
//
// AT REST (`wet` 0, no taps, `echo` 1, as a synth starts) nothing here runs and the synth's bits
// are what they were before rooms existed. Deterministic like the rest of the synth: float
// arithmetic, every coefficient through core/libm.
//
// ge:summary The room's reverb: early reflections off the nearest walls and a damped FDN tail
// ge:keywords audio reverb room fdn feedback-delay-network householder early-reflections rt60
// ge:keywords decay-time predelay damping mean-free-path tail room-sound reverb-glide
// ge:skill audio
// ge:test tests/audio_reverb_test.cpp
#pragma once

#include <array>
#include <cstdint>
#include <vector>

namespace ge::audio {

inline constexpr int kEarlyTaps = 8;
inline constexpr int kLines = 8;
inline constexpr float kSoundSpeed = 343.0f;  // m/s, in air at 20 C
inline constexpr float kMaxPathM = 60.0f;     // the longest mean free path a line holds, m
inline constexpr float kMaxEarlyMs = 400.0f;  // the latest reflection, ms
inline constexpr float kMaxPredelayMs = 200.0f;

// One reflection: after `delay_ms`, `gain` of the send, on the `pan` side (-1 left .. 1 right),
// its highs cut over `damp_hz` by the surface.
struct EarlyTap {
  float delay_ms = 0;
  float gain = 0;
  float pan = 0;
  float damp_hz = 8000;
};

// What the listener's room does to the echo send.
struct RoomSound {
  float wet = 0;           // the late tail's level against the send; 0: no tail
  float decay_s = 1.0f;    // RT60: the tail falls 60 dB in this long
  float size_m = 10;       // the mean free path, m: how far apart the tail's echoes are
  float damp_hz = 6000;    // the tail's highs die first above this
  float predelay_ms = 10;  // the tail starts this long after the sound
  float echo = 1;          // the synth's outdoor echo's share (EchoRoom): 0 in a closed room
  int taps = 0;            // early reflections in `tap`
  std::array<EarlyTap, kEarlyTaps> tap{};
};

// The reverb's state: lines allocated once, at the longest they can be; `process` allocates
// nothing.
class Reverb {
 public:
  Reverb();
  // The room to glide to over `glide_s` (0: at once, as before a sound plays).
  void set(const RoomSound& room, float glide_s);
  // True while anything is heard or could be (a tail still ringing after the room went dry).
  bool active() const { return active_; }
  // The outdoor echo's share now (gliding).
  float echo_share() const { return echo_; }
  // `n` frames of the send `in` into the stereo `l`, `r` (added to them), n <= the block.
  void process(const float* in, float* l, float* r, int n);

 private:
  void control();  // the glides and the coefficients, every kControl frames

  RoomSound target_{};
  float glide_ = 1;
  bool active_ = false;
  int quiet_ = 0;  // control steps the output has been silent, once the target is dry
  // the gliding numbers
  float wet_ = 0, decay_ = 1, size_ = 10, damp_ = 6000, predelay_ = 10, echo_ = 1;
  std::array<float, kEarlyTaps> tap_delay_{}, tap_gain_{}, tap_pan_{}, tap_damp_{};
  // the coefficients
  std::array<float, kLines> length_{}, loop_gain_{};
  float loop_coeff_ = 1, norm_ = 1;
  std::array<float, kEarlyTaps> tap_coeff_{};
  int ctl_ = 0;
  // the lines
  std::vector<float> early_;  // the send, kept for the taps and the predelay
  int early_write_ = 0;
  std::array<std::vector<float>, kLines> line_{};
  int line_write_ = 0;
  std::array<float, kLines> lp_{};
  std::array<float, kEarlyTaps> tap_lp_{};
  float peak_ = 0;
};

}  // namespace ge::audio
