// THE EXPORT'S ENCODERS (docs/replays.md § Compressed video): AV1 for the pictures (libaom) and
// Opus for the sound (libopus), wrapped so nothing else in the engine includes either. Each
// encoder hands back PACKETS: the bytes of one block of the Matroska file with its time, and for
// video whether a player can start decoding there. The decoders exist for the tests and the labs
// that read a file back and measure what the encoding lost; a player never needs them.
//
// AV1 runs in libaom's realtime mode with no lookahead (a picture in, its packet out), at a
// constant quality (`crf`, 0..63, lower is better and larger) and a keyframe every `key_seconds`,
// so a cluster can start every few seconds. Each packet loses its temporal delimiter, as
// Matroska's AV1 mapping asks, and the track's codec private is the `av1C` record built from the
// encoder's sequence header (`av1_codec_private`).
//
// OPUS takes 48 kHz stereo floats and cuts 20 ms packets (960 samples a channel); packet n is
// stamped n * 20 ms. Its encoder delays the sound by `preskip` samples, which the file names as
// the track's CodecDelay so a player drops them; the codec private is the `OpusHead` record.
//
// A build without the encoders (GE_ENCODERS=OFF, cmake/Codecs.cmake) compiles all of this to
// stubs whose `open` answers so (`encoders_built()` is false).
//
// ge:summary The export's AV1 (libaom) and Opus (libopus) encoders and decoders, as packets
// ge:keywords av1 opus libaom libopus encoder decoder codec compress webm packet av1c opushead
// ge:keywords preskip codec-delay keyframe crf realtime temporal-delimiter psnr
// ge:skill replay
// ge:test tests/capture_codecs_test.cpp
#pragma once

#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace ge::capture {

// Whether this build has the encoders (GE_ENCODERS).
bool encoders_built();

struct Packet {
  std::vector<uint8_t> data;
  int64_t ms = 0;
  bool key = true;
};

struct Av1Options {
  int width = 0, height = 0;  // both even
  double fps = 30;
  int crf = 32;   // 0..63
  int speed = 9;  // libaom's realtime speed, 7..11: higher is faster and larger
  int threads = 4;
  double key_seconds = 2;
};

class Av1Encoder {
 public:
  Av1Encoder();
  ~Av1Encoder();
  Av1Encoder(const Av1Encoder&) = delete;
  Av1Encoder& operator=(const Av1Encoder&) = delete;

  bool open(const Av1Options& o, std::string& error);
  // One I420 picture (capture/yuv.hpp's layout, the open size) shown at `ms`; its packets are
  // appended to `out`.
  bool encode(std::span<const uint8_t> i420, int64_t ms, std::vector<Packet>& out,
              std::string& error);
  // Whatever the encoder still holds.
  bool flush(std::vector<Packet>& out, std::string& error);
  // The sequence header OBU (empty before open).
  const std::vector<uint8_t>& sequence_header() const { return sequence_header_; }

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
  std::vector<uint8_t> sequence_header_;
  double fps_ = 30;
};

// Matroska's AV1 codec private: the `av1C` record (8-bit 4:2:0, the profile and level read from
// `sequence_header`) followed by the sequence header itself.
std::vector<uint8_t> av1_codec_private(std::span<const uint8_t> sequence_header);

class Av1Decoder {
 public:
  Av1Decoder();
  ~Av1Decoder();
  Av1Decoder(const Av1Decoder&) = delete;
  Av1Decoder& operator=(const Av1Decoder&) = delete;

  bool open(std::string& error);
  // One packet; a picture it completes replaces `i420` (width * height * 3 / 2 bytes) and sets
  // `width` and `height`, and `got` says whether one did.
  bool decode(std::span<const uint8_t> packet, std::vector<uint8_t>& i420, int& width, int& height,
              bool& got, std::string& error);

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

constexpr int kOpusRate = 48000;
constexpr int kOpusPacketSamples = 960;  // 20 ms a channel
constexpr int64_t kOpusSeekPreRollNs = 80'000'000;

class OpusEncoder {
 public:
  OpusEncoder();
  ~OpusEncoder();
  OpusEncoder(const OpusEncoder&) = delete;
  OpusEncoder& operator=(const OpusEncoder&) = delete;

  bool open(int bitrate, std::string& error);
  // `frames` interleaved stereo frames at 48 kHz; every packet they complete goes to `out`.
  bool push(const float* stereo, size_t frames, std::vector<Packet>& out, std::string& error);
  // Silence until every sample pushed (and the encoder's delay) is in a packet.
  bool finish(std::vector<Packet>& out, std::string& error);
  int preskip() const { return preskip_; }
  // The time the next packet will be stamped with.
  int64_t next_ms() const { return packets_ * 20; }
  // The `OpusHead` record.
  std::vector<uint8_t> codec_private() const;

 private:
  bool encode_one(std::vector<Packet>& out, std::string& error);

  struct Impl;
  std::unique_ptr<Impl> impl_;
  std::vector<float> held_;  // pushed, not yet in a packet
  int64_t pushed_ = 0;       // frames pushed
  int64_t packets_ = 0;
  int preskip_ = 0;
};

class OpusDecoder {
 public:
  OpusDecoder();
  ~OpusDecoder();
  OpusDecoder(const OpusDecoder&) = delete;
  OpusDecoder& operator=(const OpusDecoder&) = delete;

  bool open(std::string& error);
  // One packet's interleaved stereo frames appended to `out`.
  bool decode(std::span<const uint8_t> packet, std::vector<float>& out, std::string& error);

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace ge::capture
