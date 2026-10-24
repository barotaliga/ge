// THE TRANSPORT: a replay's controls, a video player's, because that is the instrument every
// player already knows (docs/replays.md § Keyframes and the seek). Play and pause; the end of the
// replay pauses there and never leaves it (play at the end starts again from the first step);
// four speeds; back and forward `skip_s`; a frame at a time while paused; the scrubber's seek.
//
// It is pure state over the replay's steps: the app hands `tick` each frame's seconds and draws
// the step it answers, and the replay's `Player` (player.hpp) steps the run there. Nothing here
// steps a run or reads a clock, so a test drives it with numbers.
//
// THE PLAYHEAD is a step count with a fraction (at 0.25 speed and 60 frames a second a frame moves
// half a step); `at` is its whole part, the step the run should have taken. The length may grow
// (`set_length`), as a live run's log does under an instant replay.
//
// ge:summary The replay's transport: play, pause, speeds, skip, frame step, seek, the playhead
// ge:keywords replay transport play pause toggle speed speeds faster slower skip back forward
// ge:keywords frame-step scrubber seek playhead tick end-pauses video-player controls
// ge:skill replay
// ge:test tests/replay_transport_test.cpp
#pragma once

#include <array>
#include <cstdint>

namespace ge::replay {

// The speeds, slowest first; a transport opens at 1.
inline constexpr std::array<double, 4> kSpeeds{0.25, 0.5, 1, 2};
// Back and forward, s of the run.
inline constexpr double kSkipS = 5;
// One frame while paused, s of the run: a thirtieth, a frame of broadcast video.
inline constexpr double kFrameS = 1.0 / 30;

class Transport {
 public:
  // A replay of `length` steps at `hz`, paused on its first step.
  Transport(double hz, int64_t length);

  void play();
  void pause() { playing_ = false; }
  void toggle();
  bool playing() const { return playing_; }

  // One speed faster (dir > 0) or slower (dir < 0), held at the ends of `kSpeeds`.
  void faster(int dir);
  double speed() const { return kSpeeds[speed_]; }

  // The playhead put on `step`, held inside 0..length; playing or paused as it was.
  void seek(int64_t step);
  // `kSkipS` back (dir < 0) or forward (dir > 0).
  void skip(int dir);
  // One frame back or forward (at least one step); a playing transport pauses first.
  void frame(int dir);

  // `dt_s` seconds of the app's clock have passed: a playing playhead moves on by
  // dt_s * speed * hz steps and pauses at the end. Returns `at()`.
  int64_t tick(double dt_s);
  int64_t at() const;
  double position() const { return head_; }
  int64_t length() const { return length_; }
  bool at_end() const { return head_ >= static_cast<double>(length_); }
  // The replay now holds `length` steps (a live log grew); the playhead is held inside it.
  void set_length(int64_t length);

 private:
  double hz_;
  int64_t length_;
  double head_ = 0;
  bool playing_ = false;
  int speed_ = 2;  // 1x
};

}  // namespace ge::replay
