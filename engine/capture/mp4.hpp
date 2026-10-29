// THE ENGINE'S FRAGMENTED MP4 MUXER (docs/replays.md § MP4): the same two tracks the Matroska
// muxer writes (capture/matroska.hpp), in the ISO base media file format, for the codecs a WebM
// cannot carry (the platforms' H.264 and HEVC encoders) and for players and sites that take only
// an .mp4. The tracks are described with the same `MatroskaOptions` and Matroska's codec ids, so
// one export can pick its container by the file's name (capture/mux.hpp):
//
//   V_AV1               `av01` with `av1C` (the codec private is the av1C record, as in Matroska)
//   V_MPEG4/ISO/AVC     `avc1` with `avcC` (the codec private is the AVCDecoderConfigurationRecord)
//   V_MPEGH/ISO/HEVC    `hvc1` with `hvcC` (the HEVCDecoderConfigurationRecord)
//   A_OPUS              `Opus` with `dOps`, built from the OpusHead codec private; its pre-skip
//                       is also the track's edit list, so a player drops the encoder's delay
//
// Uncompressed pictures and float PCM have no MP4 sample entry players read, so they stay in .mkv.
//
// THE LAYOUT: `ftyp`, then a `moov` with every track and no samples (its `mvex` says the samples
// come in fragments; the `mehd`'s duration is patched on close), then FRAGMENTS, each a `moof`
// (per track a `traf`: `tfhd`, the decode time in `tfdt`, one `trun` of the samples' durations,
// sizes and flags) and the `mdat` of its samples, video first, and at the close an `mfra` with a
// `tfra` of every fragment's start, which a player seeks by. A fragment is written whole when the
// next one starts: at a video keyframe once the last has run `cluster_ms` or grown to
// `cluster_bytes` (with no video, by time alone). So a file cut short by a crash or a full disk
// plays to its last whole fragment: nothing written after it is needed.
//
// TIMES are milliseconds from the start, in rising order per track, as for the Matroska muxer.
// The video track counts milliseconds; the audio track counts samples at its rate. A sample's
// duration is the time to the next sample on its track, so the newest sample of each track waits
// for the next (or the close) before it goes into a fragment; the last sample of a track lasts as
// long as the one before it.
//
// ge:summary The fragmented MP4 muxer: moov, moof and mdat fragments, mfra; AV1, H.264, HEVC, Opus
// ge:keywords mp4 fmp4 fragmented-mp4 iso-bmff muxer moov moof mdat trun tfdt mfra tfra mehd
// ge:keywords av01 av1c avc1 avcc hvc1 hvcc opus dops edit-list elst pre-skip crash-safe
// ge:skill replay
// ge:test tests/capture_mp4_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "capture/matroska.hpp"
#include "capture/out_file.hpp"

namespace ge::capture {

// The MP4 sample entry for a Matroska codec id ("" when an MP4 cannot carry it).
std::string_view mp4_entry_of(std::string_view codec);
// Why `o` cannot be written as an MP4 ("" when it can).
std::string mp4_problem(const MatroskaOptions& o);

class Mp4Writer {
 public:
  Mp4Writer() = default;
  Mp4Writer(const Mp4Writer&) = delete;
  Mp4Writer& operator=(const Mp4Writer&) = delete;

  // Start a file at `path`, or with an empty path keep every byte in memory (`bytes()`).
  bool open(const std::string& path, const MatroskaOptions& o, std::string& error);
  // A sample at `ms`. The tracks are numbered video 1, audio 2 (audio 1 with no video).
  bool video(int64_t ms, bool key, std::span<const uint8_t> data, std::string& error);
  bool audio(int64_t ms, std::span<const uint8_t> data, std::string& error);
  // The samples still held, the `mfra`, and the `mehd`'s duration (`end_ms`) patched.
  bool close(int64_t end_ms, std::string& error);

  const std::vector<uint8_t>& bytes() const { return out_.bytes(); }
  int64_t written() const { return out_.written(); }
  int64_t fragments() const { return sequence_; }

 private:
  struct Sample {
    int64_t t = 0;          // in the track's timescale
    int64_t duration = -1;  // < 0: not known yet (the track's newest)
    bool key = true;
    size_t offset = 0, size = 0;  // in the track's `data`
  };
  struct Track {
    bool on = false;
    uint32_t id = 0;
    int64_t timescale = 1000;
    std::vector<Sample> samples;  // held, oldest first
    std::vector<uint8_t> data;
    int64_t last_ms = -1;
    int64_t last_duration = -1;  // the newest written sample's
  };
  struct Start {
    int64_t t;       // the fragment's first video sample, in its track's timescale
    int64_t offset;  // the moof's, from the file's start
  };
  bool add(Track& k, int64_t ms, bool key, std::span<const uint8_t> data, std::string& error);
  // A fragment of every sample whose duration is known.
  bool flush(std::string& error);
  size_t held_bytes() const;

  MatroskaOptions opt_;
  OutFile out_;
  bool open_ = false;
  Track video_, audio_;
  int64_t fragment_ms_ = -1;  // the open fragment's first video (or audio) time; < 0 none
  int64_t mehd_at_ = 0;       // the mehd's 8-byte duration
  int64_t sequence_ = 0;      // fragments written
  std::vector<Start> starts_;
};

}  // namespace ge::capture
