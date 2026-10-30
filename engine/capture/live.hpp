// THE LIVE CAPTURE (docs/replays.md § The live capture): the window and its sound recorded while
// the game is played, into the same Matroska or WebM file the export writes, with no frame of
// the game waiting for it.
//
// THREE THREADS MEET HERE, and none waits on another:
// - THE AUDIO DEVICE'S THREAD taps the mix (`audio`, right after the synth renders the pull):
//   the samples go into a lock-free ring, and the tap's count of samples IS THE CAPTURE'S CLOCK.
// - THE GAME'S THREAD stamps each presented frame on that clock (`stamp`: the samples tapped
//   so far, carried forward by the wall clock since the last tap, never backwards) and, once
//   the frame's readback arrives (gpu/readback_ring.hpp), hands the picture in (`picture`). It
//   takes a lock for a swap of buffers; with `queue` pictures already waiting the picture is
//   DROPPED and counted, never queued without bound.
// - THE CAPTURE'S OWN WORKER converts each picture to I420 (crop or pad to the file's size if
//   the window was resized), encodes it (the video by `codec`, capture/video.hpp: "av1" the
//   default, the platform's "h264" or "hevc", or "auto"; the sound Opus; raw I420 and float PCM
//   with ""), cuts the sound into 20 ms packets, and writes both tracks
//   INTERLEAVED BY TIME (interleave.hpp): a sound packet goes in once a picture at or after it
//   has been encoded, or once it is half a second older than the last frame stamped with no
//   picture before it still waiting (a frame still in the readback ring is a few frames old at
//   most), a picture once the sound has been cut past it.
//
// The frame rate is variable: each picture keeps the time it was presented at. `fps` is the
// track's nominal rate (its default duration and AV1's keyframe spacing). With no sound (`sound`
// false, or a silent game) the clock is the wall clock since `open`.
//
// If the worker stalls longer than the ring holds (`audio_seconds`), the samples that did not
// fit are LOST and counted, and the worker writes as much silence where it notices them, so the
// sound keeps its length and the sync holds to within the ring.
//
// ge:summary The live capture: the mix tapped, frames stamped on its clock, encoded off-thread
// ge:keywords live-capture record window mix tap audio-thread ring lock-free stamp clock worker
// ge:keywords drop av1 opus webm interleave variable-frame-rate no-stall readback
// ge:skill replay
// ge:test tests/capture_live_test.cpp
#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "capture/codecs.hpp"
#include "capture/interleave.hpp"
#include "capture/mux.hpp"
#include "capture/video.hpp"
#include "lab/raster.hpp"

namespace ge::capture {

struct LiveOptions {
  std::string file;           // "" keeps the file in memory (`memory()` after close)
  int width = 0, height = 0;  // the file's size; rounded down to even
  double fps = 60;            // nominal: the track's default duration, AV1's keyframe spacing
  bool sound = true;          // an audio track off the tap
  std::string codec = "av1";  // av1, h264, hevc or auto (video.hpp), or "" (I420 and PCM, mkv)
  std::string container;      // "mp4" or "mkv"; "" by the file's name (mux.hpp)
  int crf = 32;
  int speed = 10;  // libaom's realtime speed, 7..11
  int threads = 4;
  int audio_bitrate = 128000;
  int queue = 3;               // pictures waiting for the worker before one is dropped
  double audio_seconds = 4.0;  // the tap's ring
};

struct LiveStats {
  int64_t pictures = 0;       // written
  int64_t dropped = 0;        // handed in while `queue` were waiting
  int64_t audio_frames = 0;   // tapped
  int64_t lost_frames = 0;    // tapped while the ring was full (written as silence)
  int64_t last_ms = 0;        // the last picture's time
  double encode_ms_max = 0;   // the worker's slowest picture (convert and encode)
  double encode_ms_mean = 0;  // and its mean
  int64_t bytes = 0;          // the file's, once closed
};

class Live {
 public:
  Live() = default;
  ~Live();
  Live(const Live&) = delete;
  Live& operator=(const Live&) = delete;

  // Start: the file opened, the worker running. `now_ns` is the clock the stamps are read on.
  bool open(const LiveOptions& o, uint64_t now_ns, std::string& error);
  bool is_open() const { return open_; }

  // THE AUDIO THREAD: `frames` interleaved stereo frames at 48 kHz just rendered. Never blocks
  // or allocates.
  void audio(const float* stereo, int frames, uint64_t now_ns);

  // THE GAME'S THREAD: the file's time of a frame presented at `now_ns`, ms.
  int64_t stamp(uint64_t now_ns);
  // THE GAME'S THREAD: the picture stamped `ms`. Its storage is taken and a spare buffer swapped
  // back into `p` (nothing allocated once warm). False when it was dropped (`p` is kept).
  bool picture(lab::Picture& p, int64_t ms);

  // Everything still held encoded and written, the file closed, the worker stopped.
  bool close(std::string& error);

  LiveStats stats() const;
  const std::vector<uint8_t>& memory() const { return mux_.bytes(); }
  std::string_view doc_type() const { return doc_type_; }
  // What codes the pictures (video.hpp), "" uncompressed.
  std::string encoder() const { return coded_ ? video_.encoder() : std::string(); }

 private:
  struct Pending {
    lab::Picture picture;
    int64_t ms = 0;
  };
  void work();
  bool drain_audio(bool all, std::string& error);
  bool encode_picture(Pending& p, std::string& error);
  bool cut_audio(const float* stereo, size_t frames, std::string& error);
  bool write(bool all, std::string& error);

  LiveOptions opt_;
  bool open_ = false;
  bool coded_ = false;
  std::string doc_type_;
  Mux mux_;
  VideoEncoder video_;
  OpusEncoder opus_;
  Interleave held_;

  // The tap's ring (single producer: the audio thread; single consumer: the worker).
  std::vector<float> ring_;  // a power of two of stereo frames, two floats each
  size_t ring_frames_ = 0;
  std::atomic<uint64_t> written_{0};  // frames put into the ring
  std::atomic<uint64_t> read_{0};     // frames taken out
  std::atomic<uint64_t> tapped_{0};   // frames tapped (the clock), the lost ones included
  std::atomic<uint64_t> lost_{0};     // frames that did not fit, not yet written as silence
  std::atomic<uint64_t> lost_total_{0};
  // The clock's last anchor: the frames tapped before the last tap, and when it came (a seqlock).
  std::atomic<uint32_t> anchor_seq_{0};
  std::atomic<uint64_t> anchor_frames_{0};
  std::atomic<uint64_t> anchor_ns_{0};
  std::atomic<bool> tapped_once_{false};
  std::atomic<bool> tapping_{false};  // the tap takes samples (open, not closing)
  uint64_t open_ns_ = 0;
  int64_t last_stamp_ = -1;
  std::atomic<int64_t> stamped_ms_{INT64_MIN};  // the last stamp handed out

  // The pictures handed in and the spares (the game's thread and the worker, under `mu_`).
  mutable std::mutex mu_;
  std::condition_variable wake_;
  std::deque<Pending> pending_;
  std::vector<std::vector<uint8_t>> spares_;
  int64_t dropped_ = 0;
  bool closing_ = false;
  std::thread worker_;

  // The worker's own.
  std::vector<float> pcm_hold_;  // tapped, not yet cut into a 20 ms packet
  int64_t cut_frames_ = 0;       // frames cut into packets (raw PCM's clock)
  int64_t newest_picture_ms_ = INT64_MIN;
  int64_t waiting_ms_ = INT64_MAX;  // the oldest picture still waiting (INT64_MAX: none)
  int64_t stamped_at_ = INT64_MIN;  // the last stamp when `waiting_ms_` was read
  int64_t pictures_ = 0;
  lab::Picture fitted_;
  std::vector<uint8_t> yuv_;
  double encode_ms_max_ = 0, encode_ms_sum_ = 0;
  std::string worker_error_;
  std::atomic<int64_t> stats_pictures_{0}, stats_last_ms_{0};
};

}  // namespace ge::capture
