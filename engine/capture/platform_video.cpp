// The platform's encoders' shared parts (platform_video.hpp): NV12 to and from I420, and the
// stubs a build without a platform encoder answers with. Each platform's encoder is its own file
// (videotoolbox.cpp).
//
// ge:summary platform_video's NV12 conversions and the stubs for a build with no platform encoder
// ge:keywords nv12 i420 stride chroma interleave stub no-platform-encoder
// ge:skill replay
// ge:test tests/capture_platform_video_test.cpp
#include "capture/platform_video.hpp"

#include <cstring>

namespace ge::capture {

void i420_to_nv12(std::span<const uint8_t> i420, int width, int height, uint8_t* y, size_t y_stride,
                  uint8_t* uv, size_t uv_stride) {
  const auto w = static_cast<size_t>(width), h = static_cast<size_t>(height);
  const uint8_t* src = i420.data();
  for (size_t r = 0; r < h; r++) std::memcpy(y + r * y_stride, src + r * w, w);
  const uint8_t* u = src + w * h;
  const uint8_t* v = u + (w / 2) * (h / 2);
  for (size_t r = 0; r < h / 2; r++) {
    uint8_t* row = uv + r * uv_stride;
    for (size_t c = 0; c < w / 2; c++) {
      row[2 * c] = u[r * (w / 2) + c];
      row[2 * c + 1] = v[r * (w / 2) + c];
    }
  }
}

void nv12_to_i420(const uint8_t* y, size_t y_stride, const uint8_t* uv, size_t uv_stride, int width,
                  int height, std::vector<uint8_t>& i420) {
  const auto w = static_cast<size_t>(width), h = static_cast<size_t>(height);
  i420.resize(w * h * 3 / 2);
  for (size_t r = 0; r < h; r++) std::memcpy(i420.data() + r * w, y + r * y_stride, w);
  uint8_t* u = i420.data() + w * h;
  uint8_t* v = u + (w / 2) * (h / 2);
  for (size_t r = 0; r < h / 2; r++) {
    const uint8_t* row = uv + r * uv_stride;
    for (size_t c = 0; c < w / 2; c++) {
      u[r * (w / 2) + c] = row[2 * c];
      v[r * (w / 2) + c] = row[2 * c + 1];
    }
  }
}

#if !(defined(__APPLE__) && GE_ENCODERS)

namespace {
constexpr const char* kNoPlatformEncoder =
    "this build has no platform video encoder (VideoToolbox on macOS and iOS is the one so far, "
    "with GE_ENCODERS on); record with --codec=av1, or --codec=auto to fall back to it (skill: "
    "replay)";
}  // namespace

std::string_view platform_encoder_name() { return ""; }

struct PlatformEncoder::Impl {};
PlatformEncoder::PlatformEncoder() = default;
PlatformEncoder::~PlatformEncoder() = default;
bool PlatformEncoder::open(PlatformCodec, const PlatformVideoOptions&, std::string& error) {
  error = kNoPlatformEncoder;
  return false;
}
bool PlatformEncoder::encode(std::span<const uint8_t>, int64_t, std::vector<Packet>&,
                             std::string& error) {
  error = kNoPlatformEncoder;
  return false;
}
bool PlatformEncoder::flush(std::vector<Packet>&, std::string&) { return true; }

struct PlatformDecoder::Impl {};
PlatformDecoder::PlatformDecoder() = default;
PlatformDecoder::~PlatformDecoder() = default;
bool PlatformDecoder::open(PlatformCodec, std::span<const uint8_t>, int, int, std::string& error) {
  error = kNoPlatformEncoder;
  return false;
}
bool PlatformDecoder::decode(std::span<const uint8_t>, std::vector<uint8_t>&, int&, int&, bool& got,
                             std::string& error) {
  got = false;
  error = kNoPlatformEncoder;
  return false;
}

#endif

}  // namespace ge::capture
