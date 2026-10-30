// THE EXPORT'S VIDEO CODEC, BY ITS WORD (docs/replays.md § The platform's encoders): one
// encoder and one decoder that are AV1 by the engine's own libaom (capture/codecs.hpp) or H.264
// and HEVC by the platform's encoder (capture/platform_video.hpp), so the export, the live
// capture and the labs name a codec and never which library codes it.
//
//   av1    libaom (GE_ENCODERS), every platform; plays in a .webm, an .mkv and an .mp4
//   h264   the platform's encoder; an .mp4 or an .mkv (a WebM cannot carry it)
//   hevc   the same, HEVC Main
//   auto   the platform's H.264 when this machine has an encoder for it, else AV1: the
//          hardware encoder first, the engine's own behind it
//
// Each names its Matroska codec id (V_AV1, V_MPEG4/ISO/AVC, V_MPEGH/ISO/HEVC), which the MP4
// muxer maps to its sample entry, and its codec private (av1C, avcC or hvcC).
//
// ge:summary The export's video codec by its word: AV1 by libaom, H.264 or HEVC by the platform
// ge:keywords codec av1 h264 hevc auto video-encoder video-decoder fallback hardware-encoder
// ge:keywords matroska-codec-id codec-private
// ge:skill replay
// ge:test tests/capture_platform_video_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "capture/codecs.hpp"
#include "capture/platform_video.hpp"

namespace ge::capture {

enum class VideoCodec : uint8_t { kAv1, kH264, kHevc };

// Whether `word` names a video codec (av1, h264, hevc or auto).
bool video_codec_word(std::string_view word);
// The Matroska codec id of `c`.
std::string_view matroska_codec_of(VideoCodec c);

struct VideoEncoderOptions {
  int width = 0, height = 0;  // both even
  double fps = 30;
  double key_seconds = 2;
  // AV1's: a constant quality and libaom's realtime speed (capture/codecs.hpp).
  int crf = 32;
  int speed = 9;
  int threads = 4;
  // The platform's: bits a second; 0 is its default (capture/platform_video.hpp).
  int64_t bitrate = 0;
};

class VideoEncoder {
 public:
  bool open(std::string_view word, const VideoEncoderOptions& o, std::string& error);
  // One I420 picture shown at `ms`; its packets are appended to `out`.
  bool encode(std::span<const uint8_t> i420, int64_t ms, std::vector<Packet>& out,
              std::string& error);
  bool flush(std::vector<Packet>& out, std::string& error);

  VideoCodec codec() const { return codec_; }
  std::string_view matroska_codec() const { return matroska_codec_of(codec_); }
  const std::vector<uint8_t>& codec_private() const { return private_; }
  // What codes the pictures: "libaom", or the platform's name and "hardware" or "software".
  std::string encoder() const;
  bool hardware() const { return codec_ != VideoCodec::kAv1 && platform_.hardware(); }

 private:
  VideoCodec codec_ = VideoCodec::kAv1;
  Av1Encoder av1_;
  PlatformEncoder platform_;
  std::vector<uint8_t> private_;
};

class VideoDecoder {
 public:
  // A track with Matroska codec id `codec`, its codec private and its size.
  bool open(std::string_view codec, std::span<const uint8_t> codec_private, int width, int height,
            std::string& error);
  bool decode(std::span<const uint8_t> packet, std::vector<uint8_t>& i420, int& width, int& height,
              bool& got, std::string& error);

 private:
  VideoCodec codec_ = VideoCodec::kAv1;
  Av1Decoder av1_;
  PlatformDecoder platform_;
};

}  // namespace ge::capture
