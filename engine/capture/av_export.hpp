// A RUN EXPORTED WITH ITS SOUND (docs/replays.md § Video with sound): the pictures a recording
// draws (play/record.hpp, `--format=mkv`) written into the engine's own Matroska file
// (capture/matroska.hpp) beside the run's sound, rendered offline in lockstep by the synth.
//
// THE TWO CLOCKS ARE ONE. Picture k is shown at k / fps seconds and shows the run after the step
// the frame clock picks for it (play/record.hpp's `frame_steps`); a step's events sound at the
// sample its end falls on, (at - from) * rate / hz rounded, where `at` is the steps taken. So the
// export renders the synth up to each step's sample before it hands the step's events to the
// game's sound (`stepped`), and a click and a flash on the same step land within a frame of each
// other however slowly the export runs: nothing is timed by a wall clock. Each picture's sound
// (the samples from its time to the next picture's) is written just before the next picture, so
// the file interleaves the tracks by time.
//
// THE TRACKS: by default the video as uncompressed I420 (capture/yuv.hpp) and the sound as 32-bit
// float stereo at the synth's rate, a `matroska` file. With a `codec` (capture/video.hpp: "av1", or
// the platform's "h264" or "hevc", or "auto") the video is coded and the sound Opus
// (capture/codecs.hpp), a `webm` file for AV1 in Matroska: then the blocks are INTERLEAVED BY TIME,
// since an Opus packet covers 20 ms whatever the frame rate, so a picture waits until no packet
// still to come can start before it. The game's sound comes from its `SoundSpec` (play/game.hpp);
// with none the file has no audio track.
//
// A SOUND THE CALLER PLAYS (`AvExportOptions::synth`): an export of something that is not a play
// contract's run (a recorded game played back, games/shooters_edge/client/demo_video.hpp) hands
// the export its own synth instead of a `SoundSpec`. The caller plays its events on that synth and
// calls `sound_to(sample)` first, so each event starts on the sample its time falls on; the export
// renders the synth between, exactly as it does for a run's steps, and pictures go in through the
// run-less `frame`.
//
// ge:summary A run exported with its sound: pictures as I420, the synth rendered on the step clock
// ge:keywords export av-export mkv matroska sound audio sync lockstep click flash frame-clock
// ge:keywords sample step-sample offline-render synth i420 pcm av1 opus webm interleave
// ge:skill replay
// ge:test tests/capture_av_export_test.cpp
#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "audio/synth.hpp"
#include "capture/codecs.hpp"
#include "capture/interleave.hpp"
#include "capture/mux.hpp"
#include "capture/video.hpp"
#include "lab/raster.hpp"
#include "play/game.hpp"

namespace ge::capture {

struct AvExportOptions {
  std::string file;           // "" keeps the file in memory (`bytes()`)
  int width = 0, height = 0;  // the pictures' size, both even
  double fps = 30;
  double hz = 120;                         // the run's steps a second
  int64_t from = 0;                        // the step picture 0 shows
  const play::SoundSpec* sound = nullptr;  // null: no audio track (unless `synth`)
  audio::Synth* synth = nullptr;  // the caller's own synth, played by the caller (with no `sound`)
  uint32_t seed = 1;              // the synth's noise
  std::string codec;              // "" uncompressed; av1, h264, hevc or auto (video.hpp)
  std::string container;          // "mp4" or "mkv"; "" by the file's name (mux.hpp)
  int crf = 32;                   // av1: 0..63, lower is better and larger
  int speed = 9;                  // av1: libaom's realtime speed, 7..11
  int threads = 4;                // av1: the encoder's threads
  int audio_bitrate = 128000;     // av1: Opus's bits a second
};

// The sample step `at`'s events sound at: (at - from) * rate / hz, rounded.
int64_t step_sample(int64_t at, int64_t from, double hz, int rate);
// The sample picture `k` starts at: k * rate / fps, rounded.
int64_t frame_sample(int64_t k, double fps, int rate);
// The millisecond picture `k` is shown at: k * 1000 / fps, rounded.
int64_t frame_ms(int64_t k, double fps);

class AvExport {
 public:
  bool open(const AvExportOptions& o, std::string& error);
  // After each step the run takes once open: `at` is the steps it has taken.
  void stepped(const play::Run& run, int64_t at);
  // The next picture (`p` at the open size) of `run` as it stands.
  bool frame(const play::Run& run, const lab::Picture& p, std::string& error);
  // The next picture of an export whose sound the caller plays (`AvExportOptions::synth`).
  bool frame(const lab::Picture& p, std::string& error);
  // The caller's synth rendered up to `sample` (from the export's start): call it before playing
  // an event that starts there. A sample already rendered renders nothing.
  void sound_to(int64_t sample) { render_to(sample); }
  // The last picture's sound, and the file closed.
  bool close(std::string& error);

  bool has_sound() const { return synth_ != nullptr; }
  int64_t frames() const { return frames_; }
  int64_t samples() const { return emitted_; }
  int64_t bytes() const { return mux_.written(); }
  const std::vector<uint8_t>& memory() const { return mux_.bytes(); }
  std::string_view doc_type() const { return doc_type_; }
  // The video track's codec id as the container names it ("V_UNCOMPRESSED" when none).
  std::string_view video_codec() const {
    return coded_ ? video_.matroska_codec() : std::string_view("V_UNCOMPRESSED");
  }

 private:
  void render_to(int64_t sample);
  bool emit_sound(int64_t to, std::string& error);
  // The packets held, in time order: all of them when `all`, else every one nothing still to be
  // encoded can come before.
  bool interleave(bool all, std::string& error);

  AvExportOptions opt_;
  Mux mux_;
  std::string doc_type_;
  std::unique_ptr<audio::Synth> owned_;  // the run's synth (a `SoundSpec`'s)
  audio::Synth* synth_ = nullptr;        // the one rendered: `owned_` or the caller's
  std::unique_ptr<play::Sound> sound_;
  std::vector<float> pending_;  // rendered, not yet written: samples from `emitted_` on
  int64_t rendered_ = 0;        // samples rendered
  int64_t emitted_ = 0;         // samples written
  int64_t frames_ = 0;
  std::vector<uint8_t> yuv_;
  std::vector<uint8_t> pcm_;
  bool coded_ = false;  // a codec named
  VideoEncoder video_;
  OpusEncoder opus_;
  Interleave held_;  // encoded, not yet written
};

}  // namespace ge::capture
