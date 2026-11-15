// FLAC, THE LOSSLESS AUDIO FILE, WRITTEN AND READ: how a game's music is kept once it is taken off
// a disc or out of an install (the shooter content layer's soundtrack.hpp), so the samples a disc
// holds come back bit for bit, and played (music.hpp).
//
// - **The writer** (`flac_encode`) takes integer samples (interleaved, 4 to 24 bits, 1 to 8
//   channels) and writes blocks of 4096: for each, every fixed predictor (orders 0 to 4) on each
//   channel, and for stereo the four channel layouts (left and right, left and side, side and
//   right, mid and side), the smallest kept; the residual Rice-coded in partitions, each with its
//   own parameter. No linear prediction: a CD track keeps about 60 to 70% of its size, a few
//   percent above the reference encoder at its best, in a fraction of the time. Tags go into a
//   Vorbis comment block. The stream's MD5 is left unset (the store keeps its own SHA-256).
// - **The reader** (`flac_decode`) reads any FLAC stream: constant, verbatim, fixed and linear
//   prediction subframes, both Rice parameter widths and their escapes, wasted bits, every block
//   size, rate and depth code; each frame's CRC-16 is checked and a frame that fails is refused
//   with its number.
//
// ge:summary Writes and reads FLAC: lossless audio, fixed predictors, Rice partitions, tags, CRCs
// ge:keywords flac lossless audio codec encode decode rice fixed-predictor lpc stereo mid-side
// ge:keywords vorbis-comment tags crc8 crc16 streaminfo flac_encode flac_decode music soundtrack
// ge:skill audio
// ge:test tests/audio_flac_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace ge::audio {

struct FlacInfo {
  int rate = 0;
  int channels = 0;
  int bits = 0;                                           // bits per sample
  uint64_t frames = 0;                                    // samples per channel
  std::vector<std::pair<std::string, std::string>> tags;  // the Vorbis comment's fields, in order
};

// `samples` (interleaved, `channels` of them, each fitting in `bits`) as a FLAC file, with `tags`
// (NAME, value) in its Vorbis comment.
std::vector<uint8_t> flac_encode(std::span<const int32_t> samples, int channels, int rate, int bits,
                                 const std::vector<std::pair<std::string, std::string>>& tags = {});
// The FLAC file in `bytes` read into `*samples` (interleaved) and `*info`; false with the frame and
// the reason in `*error` when it does not read.
bool flac_decode(std::span<const uint8_t> bytes, std::vector<int32_t>* samples, FlacInfo* info,
                 std::string* error);

}  // namespace ge::audio
