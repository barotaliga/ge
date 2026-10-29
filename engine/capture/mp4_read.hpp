// A READER OF THE ENGINE'S OWN FRAGMENTED MP4 FILES, for the tests and the labs that check what
// the muxer wrote (capture/mp4.hpp): the brands, the tracks with their sample entry and its
// configuration box, the edit list's start, the duration in the `mehd`, every sample with its
// track, its time, its duration, whether it is a sync sample and where its bytes are, the
// fragments and the `mfra`'s seek points. It walks the boxes the muxer writes and skips any other;
// it is not a general demuxer (no sample tables, no `tfhd` defaults beyond the base at the moof),
// and a player never needs it.
//
// A FILE CUT SHORT reads to its last whole fragment: a box that runs past the end stops the walk
// and sets `cut`, and only the fragments whose `moof` and `mdat` are both whole give samples.
//
// ge:summary A reader of the engine's own fragmented MP4 files: tracks, samples, fragments, mfra
// ge:keywords mp4 fmp4 read parse demux boxes moof trun tfdt samples mfra tfra truncated test
// ge:skill replay
// ge:test tests/capture_mp4_test.cpp
#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace ge::capture {

struct Mp4Track {
  uint32_t id = 0;
  std::string handler;  // "vide" or "soun"
  std::string entry;    // the sample entry: av01, avc1, hvc1, Opus
  uint64_t timescale = 0;
  uint32_t width = 0, height = 0;   // video
  uint32_t channels = 0, rate = 0;  // audio
  std::string config_type;          // av1C, avcC, hvcC or dOps
  std::vector<uint8_t> config;      // its body
  int64_t edit_start = 0;           // the edit list's media time (0 with none)
};

struct Mp4Sample {
  uint32_t track = 0;
  int64_t t = 0;         // the decode time, in the track's timescale
  int64_t duration = 0;  // in the track's timescale
  int64_t ms = 0;        // t in milliseconds, rounded down
  bool key = false;
  size_t offset = 0, size = 0;  // the sample's bytes in the file
};

struct Mp4Seek {
  uint32_t track = 0;
  int64_t t = 0;
  int64_t moof = 0;  // the fragment's offset in the file
};

struct Mp4File {
  std::string major;
  std::vector<std::string> brands;
  uint64_t movie_timescale = 0;
  uint64_t fragment_duration = 0;  // the mehd's, in the movie's timescale
  std::vector<Mp4Track> tracks;
  std::vector<Mp4Sample> samples;  // in file order
  std::vector<int64_t> fragments;  // every whole fragment's moof offset
  std::vector<Mp4Seek> seeks;      // the mfra's
  bool cut = false;                // the file ends inside a box
};

// `bytes` read, or nothing with `error` naming the box where it went wrong.
std::optional<Mp4File> read_mp4(std::span<const uint8_t> bytes, std::string& error);

}  // namespace ge::capture
