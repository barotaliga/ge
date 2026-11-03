// A still packed by its format (still.hpp): the words, the fallbacks, the encoders driven for one
// picture, and the decode by the file's first bytes.
//
// ge:summary Stills packed by format: PNG, the JPEG writer, AV1 or HEVC boxed as HEIF, fallbacks
// ge:keywords still encode decode fallback avif heic jpg png crop-even quality-map
// ge:skill replay
// ge:test tests/capture_still_test.cpp
#include "capture/still.hpp"

#include <algorithm>
#include <cstring>

#include "capture/codecs.hpp"
#include "capture/heif.hpp"
#include "capture/jpeg.hpp"
#include "capture/platform_video.hpp"
#include "capture/video.hpp"
#include "capture/yuv.hpp"
#include "lab/png.hpp"

namespace ge::capture {

bool still_format_of(std::string_view word, StillFormat& out) {
  if (word == "png")
    out = StillFormat::kPng;
  else if (word == "jpg" || word == "jpeg")
    out = StillFormat::kJpg;
  else if (word == "avif")
    out = StillFormat::kAvif;
  else if (word == "heic")
    out = StillFormat::kHeic;
  else
    return false;
  return true;
}

std::string_view still_extension(StillFormat f) {
  switch (f) {
    case StillFormat::kPng:
      return "png";
    case StillFormat::kJpg:
      return "jpg";
    case StillFormat::kAvif:
      return "avif";
    case StillFormat::kHeic:
      return "heic";
  }
  return "png";
}

StillFormat still_writable(StillFormat f) {
  if (f == StillFormat::kHeic && !platform_encoder_name().empty()) return f;
  if (f == StillFormat::kHeic) f = StillFormat::kAvif;
  if (f == StillFormat::kAvif && !encoders_built()) f = StillFormat::kJpg;
  return f;
}

namespace {

// One picture through the video encoder `word` (av1 or hevc) at the still's quality, boxed.
bool encode_heif(const StillOptions& o, int width, int height, std::span<const uint8_t> rgba,
                 std::vector<uint8_t>& out, std::string& error) {
  const bool avif = o.format == StillFormat::kAvif;
  // 4:2:0 needs even sides: an odd last row or column is cut off.
  lab::Picture p;
  p.width = width & ~1;
  p.height = height & ~1;
  if (p.width < 2 || p.height < 2) {
    error = "the picture is smaller than 2x2";
    return false;
  }
  p.rgba.resize(static_cast<size_t>(p.width) * p.height * 4);
  for (int y = 0; y < p.height; ++y)
    std::memcpy(&p.rgba[static_cast<size_t>(y) * p.width * 4],
                &rgba[static_cast<size_t>(y) * width * 4], static_cast<size_t>(p.width) * 4);
  std::vector<uint8_t> i420;
  to_i420(p, i420);
  const int q = std::clamp(o.quality, 1, 100);
  VideoEncoderOptions v;
  v.width = p.width;
  v.height = p.height;
  v.still = true;
  v.crf = ((100 - q) * 63 + 50) / 100;
  v.speed = 7;  // realtime's slowest: the smallest file, still well under a second at 1440p
  v.quality = q / 100.0;
  VideoEncoder enc;
  if (!enc.open(avif ? "av1" : "hevc", v, error)) return false;
  std::vector<Packet> packets;
  if (!enc.encode(i420, 0, packets, error) || !enc.flush(packets, error)) return false;
  if (packets.empty()) {
    error = "the encoder gave no picture";
    return false;
  }
  HeifStill s;
  s.kind = avif ? HeifKind::kAvif : HeifKind::kHeic;
  s.width = p.width;
  s.height = p.height;
  // The AV1 codec private is the av1C record with the sequence header after it, as av1C carries.
  s.config = enc.codec_private();
  s.data = std::move(packets.front().data);
  out = write_heif(s);
  return true;
}

}  // namespace

bool encode_still(const StillOptions& o, int width, int height, std::span<const uint8_t> rgba,
                  std::vector<uint8_t>& out, StillFormat& used, std::string& note) {
  note.clear();
  if (width <= 0 || height <= 0 || rgba.size() < static_cast<size_t>(width) * height * 4) {
    note = "the picture has no pixels";
    return false;
  }
  StillOptions at = o;
  at.format = still_writable(o.format);
  if (at.format != o.format)
    note = std::string(still_extension(o.format)) + " cannot be written by this build or machine";
  for (;;) {
    std::string why;
    if (at.format == StillFormat::kPng) {
      out = lab::encode_png_rgb(width, height, rgba);
    } else if (at.format == StillFormat::kJpg) {
      out = encode_jpeg(width, height, rgba, o.quality, o.subsample);
    } else if (!encode_heif(at, width, height, rgba, out, why)) {
      note = std::string(still_extension(at.format)) + " was refused: " + why;
      at.format =
          at.format == StillFormat::kHeic ? still_writable(StillFormat::kAvif) : StillFormat::kJpg;
      continue;
    }
    used = at.format;
    if (!note.empty()) note += ", so it is " + std::string(still_extension(used));
    return true;
  }
}

std::optional<lab::Picture> decode_still(std::span<const uint8_t> bytes, std::string& error) {
  if (bytes.size() >= 8 && bytes[0] == 0x89 && bytes[1] == 'P') {
    auto p = lab::decode_png(bytes);
    if (!p) error = "the PNG does not read";
    return p;
  }
  if (bytes.size() >= 2 && bytes[0] == 0xFF && bytes[1] == 0xD8) return decode_jpeg(bytes, error);
  HeifStill s;
  if (!read_heif(bytes, s, error)) return std::nullopt;
  VideoDecoder dec;
  const bool avif = s.kind == HeifKind::kAvif;
  if (!dec.open(avif ? matroska_codec_of(VideoCodec::kAv1) : matroska_codec_of(VideoCodec::kHevc),
                s.config, s.width, s.height, error))
    return std::nullopt;
  std::vector<uint8_t> i420;
  int w = 0, h = 0;
  bool got = false;
  if (!dec.decode(s.data, i420, w, h, got, error)) return std::nullopt;
  if (!got) {
    error = "the decoder gave no picture";
    return std::nullopt;
  }
  return from_i420(i420.data(), w, h);
}

}  // namespace ge::capture
