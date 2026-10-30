// THE PLATFORM'S OWN VIDEO ENCODERS (docs/replays.md § The platform's encoders, M23d4): H.264
// and HEVC by the encoder the operating system ships, on its media engine when the machine has
// one, so a capture costs the game's frame almost nothing. Each platform is one file that alone
// includes its SDK; on a platform with none (or GE_ENCODERS off) every `open` answers so and the
// export falls back to the engine's own AV1 (capture/video.hpp's `auto`).
//
//   macOS, iOS    VideoToolbox (capture/videotoolbox.cpp): H.264 High and HEVC Main, the
//                 hardware encoder asked for and the system's software encoder taken without one
//   later         Media Foundation on Windows, MediaCodec on Android, VA-API on Linux (M23d4)
//
// THE STREAM is what a muxer takes as is: every packet is one picture's NAL units, each led by its
// 4-byte length (the form Matroska's V_MPEG4/ISO/AVC and V_MPEGH/ISO/HEVC and MP4's avc1 and hvc1
// carry), with no B-frames (pictures come out in the order they went in, so decode order is
// presentation order and neither container needs a composition offset), a keyframe every
// `key_seconds`, at an average bitrate. `encode` waits for its picture's packet, so a packet
// never comes out a call late. The track's codec private (the `avcC` or `hvcC` record) is known
// at open: a black picture is encoded once on a session of the same settings and its record kept;
// a keyframe later carrying another record is an error, since the file has already named one.
//
// THE DECODER is for the tests and the labs that read a file back; a player never needs it.
//
// ge:summary The platform's H.264 and HEVC encoders and decoders (VideoToolbox), or stubs
// ge:keywords h264 hevc avc hvc avcc hvcc videotoolbox hardware-encoder media-engine nal
// ge:keywords platform-encoder software-fallback bitrate keyframe no-b-frames decoder
// ge:skill replay
// ge:test tests/capture_platform_video_test.cpp
#pragma once

#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "capture/codecs.hpp"

namespace ge::capture {

enum class PlatformCodec : uint8_t { kH264, kHevc };

// The platform's encoder's name ("videotoolbox"), or "" when this build has none.
std::string_view platform_encoder_name();

struct PlatformVideoOptions {
  int width = 0, height = 0;  // both even
  double fps = 30;
  int64_t bitrate = 0;  // bits a second; 0: 0.1 bit a pixel a frame (2.8 Mbit/s at 720p30)
  double key_seconds = 2;
};

class PlatformEncoder {
 public:
  PlatformEncoder();
  ~PlatformEncoder();
  PlatformEncoder(const PlatformEncoder&) = delete;
  PlatformEncoder& operator=(const PlatformEncoder&) = delete;

  bool open(PlatformCodec c, const PlatformVideoOptions& o, std::string& error);
  // One I420 picture (the open size) shown at `ms`; its packet is appended to `out`.
  bool encode(std::span<const uint8_t> i420, int64_t ms, std::vector<Packet>& out,
              std::string& error);
  bool flush(std::vector<Packet>& out, std::string& error);
  // The `avcC` or `hvcC` record (empty before open).
  const std::vector<uint8_t>& codec_private() const { return config_; }
  // Whether the pictures go through the machine's media engine rather than the processor.
  bool hardware() const { return hardware_; }

  struct Impl;

 private:
  std::unique_ptr<Impl> impl_;
  std::vector<uint8_t> config_;
  bool hardware_ = false;
};

class PlatformDecoder {
 public:
  PlatformDecoder();
  ~PlatformDecoder();
  PlatformDecoder(const PlatformDecoder&) = delete;
  PlatformDecoder& operator=(const PlatformDecoder&) = delete;

  // A track of `c` with its codec private `config` at `width` x `height`.
  bool open(PlatformCodec c, std::span<const uint8_t> config, int width, int height,
            std::string& error);
  // One packet; the picture it completes replaces `i420` and sets `width` and `height`, and
  // `got` says whether one did.
  bool decode(std::span<const uint8_t> packet, std::vector<uint8_t>& i420, int& width, int& height,
              bool& got, std::string& error);

  struct Impl;

 private:
  std::unique_ptr<Impl> impl_;
};

// NV12 (the luma plane, then one plane of interleaved chroma) to I420 and back, row by row with
// each plane's own stride: the layout the platforms' encoders take and their decoders give.
void i420_to_nv12(std::span<const uint8_t> i420, int width, int height, uint8_t* y, size_t y_stride,
                  uint8_t* uv, size_t uv_stride);
void nv12_to_i420(const uint8_t* y, size_t y_stride, const uint8_t* uv, size_t uv_stride, int width,
                  int height, std::vector<uint8_t>& i420);

}  // namespace ge::capture
