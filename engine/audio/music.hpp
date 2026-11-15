// MUSIC: a game's soundtrack played from its kept files. A track is a stereo recording (a FLAC
// file, as the content layer keeps a game's music) decoded off the game thread and played on the
// synth as two sampled voices, the left channel hard left and the right hard right, sent in the
// same frame so they start on the same sample. Any game with music uses it whole; the game adds
// which file a track number names and the fader it plays on.
//
// THE PLAYER (`MusicPlayer`) is driven from the game thread:
//   `play(path, loop)`  decodes the file on a worker thread; the next `frame` that finds it ready
//                       starts it, fading the one before out. The same file asked again while it
//                       plays (a level restarted) carries on rather than starting over.
//   `stop`, `pause`, `resume`  as a CD player's buttons: pause keeps the place (the voices are
//                       stopped and played again from the frame they reached).
//   `frame(synth)`      once a frame: starts what finished decoding, steers the gain, frees a
//                       recording no voice reads any more (`Synth::reads`).
// A file that fails to decode is reported once by `take_error` and nothing plays.
//
// THE LEVEL: `gain` is the player's fader (the game's music volume); the voices play at
// `kMusicLevel` of it, a mastered track's peaks kept under the limiter's knee beside the game's
// own sounds.
//
// ge:summary Music: a stereo track decoded off the game thread and played as two sampled voices
// ge:keywords music soundtrack track play loop stop pause resume cd bgmvolume flac stereo decode
// ge:keywords worker-thread music-player
// ge:skill audio
// ge:test tests/audio_music_test.cpp
#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <thread>
#include <vector>

#include "audio/pcm.hpp"
#include "audio/synth.hpp"

namespace ge::audio {

inline constexpr float kMusicLevel = 0.5f;  // -6 dB: a mastered track's peaks under the limiter

// A decoded track: one Pcm a channel (a mono file plays the same samples on both).
struct MusicTrack {
  std::string path;
  Pcm left, right;
};

// Decode a FLAC file's bytes into a track. On failure returns false with `error` naming `where`.
bool music_from_flac(std::span<const uint8_t> bytes, const std::string& where, MusicTrack* out,
                     std::string* error);

class MusicPlayer {
 public:
  MusicPlayer() = default;
  MusicPlayer(const MusicPlayer&) = delete;
  MusicPlayer& operator=(const MusicPlayer&) = delete;
  ~MusicPlayer();

  void play(const std::string& path, bool loop);
  void stop();
  void pause();
  void resume();
  void gain(float g) { gain_ = g; }
  void frame(Synth& synth);

  // The file playing (or being decoded), "" when none; and whether it is paused or loops.
  const std::string& playing() const { return want_; }
  bool paused() const { return paused_; }
  bool loops() const { return loop_; }
  // Whether the voices are sounding (a decode may still be running when `playing` is set).
  bool sounding() const { return now_ != nullptr; }
  // A decode's failure, once; "" when there is none.
  std::string take_error();

 private:
  struct Job {
    std::string path;
    std::thread worker;
    std::atomic<bool> done{false};
    std::unique_ptr<MusicTrack> track;
    std::string error;
  };
  void start(Synth& synth, int64_t from);
  void silence(Synth& synth, float fade_s);
  int64_t position(const Synth& synth) const;

  std::string want_;
  bool loop_ = false, paused_ = false;
  bool cut_ = false;        // the voices playing are no longer wanted: stop them next frame
  bool pause_now_ = false;  // pause or resume asked, applied next frame
  float gain_ = 1, sent_gain_ = -1;
  int bus_ = 1;
  std::vector<std::unique_ptr<Job>> jobs_;  // decodes running or finished, oldest first
  std::unique_ptr<MusicTrack> now_;
  std::vector<std::unique_ptr<MusicTrack>> retired_;  // fading out until no voice reads them
  SampleId left_, right_;
  double started_at_ = 0;      // the synth's clock when the voices started
  int64_t started_frame_ = 0;  // the frame of the recording they started from
  int64_t paused_frame_ = 0;
  std::string error_;

 public:
  // The synth bus the music plays on (the game's music fader); before the first `frame`.
  void bus(int b) { bus_ = b; }
};

}  // namespace ge::audio
