// The export's video codec by its word (video.hpp).
//
// ge:summary VideoEncoder and VideoDecoder: a word to libaom or the platform's encoder
// ge:keywords codec word auto fallback av1 h264 hevc open encode decode
// ge:skill replay
// ge:test tests/capture_platform_video_test.cpp
#include "capture/video.hpp"

namespace ge::capture {

bool video_codec_word(std::string_view word) {
  return word == "av1" || word == "h264" || word == "hevc" || word == "auto";
}

std::string_view matroska_codec_of(VideoCodec c) {
  switch (c) {
    case VideoCodec::kH264:
      return "V_MPEG4/ISO/AVC";
    case VideoCodec::kHevc:
      return "V_MPEGH/ISO/HEVC";
    case VideoCodec::kAv1:
      break;
  }
  return "V_AV1";
}

bool VideoEncoder::open(std::string_view word, const VideoEncoderOptions& o, std::string& error) {
  if (!video_codec_word(word)) {
    error = "no video codec `" + std::string(word) + "`; the codecs are av1, h264, hevc and auto";
    return false;
  }
  if (word != "av1") {
    PlatformVideoOptions p;
    p.width = o.width;
    p.height = o.height;
    p.fps = o.fps;
    p.bitrate = o.bitrate;
    p.key_seconds = o.key_seconds;
    const PlatformCodec pc = word == "hevc" ? PlatformCodec::kHevc : PlatformCodec::kH264;
    std::string why;
    if (platform_.open(pc, p, why)) {
      codec_ = pc == PlatformCodec::kHevc ? VideoCodec::kHevc : VideoCodec::kH264;
      private_ = platform_.codec_private();
      return true;
    }
    if (word != "auto") {
      error = why;
      return false;
    }
  }
  Av1Options a;
  a.width = o.width;
  a.height = o.height;
  a.fps = o.fps;
  a.crf = o.crf;
  a.speed = o.speed;
  a.threads = o.threads;
  a.key_seconds = o.key_seconds;
  if (!av1_.open(a, error)) return false;
  codec_ = VideoCodec::kAv1;
  private_ = av1_codec_private(av1_.sequence_header());
  return true;
}

bool VideoEncoder::encode(std::span<const uint8_t> i420, int64_t ms, std::vector<Packet>& out,
                          std::string& error) {
  return codec_ == VideoCodec::kAv1 ? av1_.encode(i420, ms, out, error)
                                    : platform_.encode(i420, ms, out, error);
}

bool VideoEncoder::flush(std::vector<Packet>& out, std::string& error) {
  return codec_ == VideoCodec::kAv1 ? av1_.flush(out, error) : platform_.flush(out, error);
}

std::string VideoEncoder::encoder() const {
  if (codec_ == VideoCodec::kAv1) return "libaom";
  return std::string(platform_encoder_name()) +
         (platform_.hardware() ? " (hardware)" : " (software)");
}

bool VideoDecoder::open(std::string_view codec, std::span<const uint8_t> codec_private, int width,
                        int height, std::string& error) {
  if (codec == "V_AV1") {
    codec_ = VideoCodec::kAv1;
    return av1_.open(error);
  }
  if (codec == "V_MPEG4/ISO/AVC" || codec == "V_MPEGH/ISO/HEVC") {
    codec_ = codec == "V_MPEGH/ISO/HEVC" ? VideoCodec::kHevc : VideoCodec::kH264;
    return platform_.open(codec_ == VideoCodec::kHevc ? PlatformCodec::kHevc : PlatformCodec::kH264,
                          codec_private, width, height, error);
  }
  error = "no decoder for the video codec `" + std::string(codec) + "`";
  return false;
}

bool VideoDecoder::decode(std::span<const uint8_t> packet, std::vector<uint8_t>& i420, int& width,
                          int& height, bool& got, std::string& error) {
  return codec_ == VideoCodec::kAv1 ? av1_.decode(packet, i420, width, height, got, error)
                                    : platform_.decode(packet, i420, width, height, got, error);
}

}  // namespace ge::capture
