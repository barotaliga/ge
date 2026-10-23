// The moon's face: the table decoded, the map sampled and uploaded with its mips, and the bake's
// filtering and encoding (render/moon.hpp says what the map is).
//
// ge:summary The moon's albedo table decoded, sampled, uploaded with mips, and baked
// ge:keywords render moon albedo base64 decode encode bake mipmap moon-map
// ge:skill render
// ge:test tests/render_moon_test.cpp
#include "render/moon.hpp"

#include <algorithm>
#include <cmath>

namespace ge::render {

extern const char* const kMoonTable[];  // render/moon_data.cpp: generated, nullptr-ended
extern const double kMoonTableScale;

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr char kAlphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

int sextet(char c) {
  if (c >= 'A' && c <= 'Z') return c - 'A';
  if (c >= 'a' && c <= 'z') return c - 'a' + 26;
  if (c >= '0' && c <= '9') return c - '0' + 52;
  if (c == '+') return 62;
  if (c == '/') return 63;
  return -1;
}

double linear_of_srgb(double c) {
  return c <= 0.04045 ? c / 12.92 : std::pow((c + 0.055) / 1.055, 2.4);
}

// The texel at column `x` (wrapping round the sphere) and row `y` (clamped at the poles).
double texel(const std::vector<uint8_t>& m, int x, int y) {
  x = ((x % kMoonMapWidth) + kMoonMapWidth) % kMoonMapWidth;
  y = std::clamp(y, 0, kMoonMapHeight - 1);
  return m[static_cast<size_t>(y) * kMoonMapWidth + static_cast<size_t>(x)];
}

}  // namespace

const std::vector<uint8_t>& moon_albedo() {
  static const std::vector<uint8_t> map = [] {
    std::string text;
    for (const char* const* line = kMoonTable; *line; ++line) text += *line;
    std::vector<uint8_t> m;
    if (!decode_moon_map(text, m) ||
        m.size() != static_cast<size_t>(kMoonMapWidth) * kMoonMapHeight) {
      m.assign(static_cast<size_t>(kMoonMapWidth) * kMoonMapHeight, 255);
    }
    return m;
  }();
  return map;
}

double moon_albedo_scale() { return kMoonTableScale; }

double albedo_at(double longitude, double latitude) {
  const std::vector<uint8_t>& m = moon_albedo();
  const double u = (longitude / 360.0 + 0.5) * kMoonMapWidth - 0.5;
  const double v = (0.5 - latitude / 180.0) * kMoonMapHeight - 0.5;
  const int x = static_cast<int>(std::floor(u)), y = static_cast<int>(std::floor(v));
  const double fx = u - x, fy = v - y;
  const double top = texel(m, x, y) * (1 - fx) + texel(m, x + 1, y) * fx;
  const double bottom = texel(m, x, y + 1) * (1 - fx) + texel(m, x + 1, y + 1) * fx;
  return (top * (1 - fy) + bottom * fy) / 255.0 * moon_albedo_scale();
}

MoonTexture create_moon_texture(gpu::Device& device) {
  const std::vector<uint8_t>& top = moon_albedo();
  uint32_t levels = 1;
  while ((kMoonMapHeight >> levels) >= 1) ++levels;
  MoonTexture t;
  wgpu::TextureDescriptor td{};
  td.label = "sky.moon";
  td.size = {static_cast<uint32_t>(kMoonMapWidth), static_cast<uint32_t>(kMoonMapHeight), 1};
  td.format = wgpu::TextureFormat::R8Unorm;
  td.mipLevelCount = levels;
  td.usage = wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::CopyDst;
  t.texture = device.device.CreateTexture(&td);
  // Each level the box filter of the one above (the map is linear light already).
  std::vector<double> level(top.begin(), top.end());
  uint32_t w = kMoonMapWidth, h = kMoonMapHeight;
  for (uint32_t mip = 0; mip < levels; ++mip) {
    // A row is padded to 256 bytes, as a copy's rows must be.
    const uint32_t row = (w + 255u) / 256u * 256u;
    std::vector<uint8_t> bytes(static_cast<size_t>(row) * h, 0);
    for (uint32_t y = 0; y < h; ++y) {
      for (uint32_t x = 0; x < w; ++x) {
        const double v = level[static_cast<size_t>(y) * w + x];
        bytes[static_cast<size_t>(y) * row + x] =
            static_cast<uint8_t>(std::lround(std::clamp(v, 0.0, 255.0)));
      }
    }
    wgpu::TexelCopyTextureInfo dst{};
    dst.texture = t.texture;
    dst.mipLevel = mip;
    wgpu::TexelCopyBufferLayout layout{};
    layout.bytesPerRow = row;
    layout.rowsPerImage = h;
    const wgpu::Extent3D size{w, h, 1};
    device.queue.WriteTexture(&dst, bytes.data(), bytes.size(), &layout, &size);
    if (mip + 1 == levels) break;
    const uint32_t nw = std::max(1u, w / 2), nh = std::max(1u, h / 2);
    std::vector<double> next(static_cast<size_t>(nw) * nh);
    for (uint32_t y = 0; y < nh; ++y) {
      for (uint32_t x = 0; x < nw; ++x) {
        const uint32_t x0 = std::min(2 * x, w - 1), x1 = std::min(2 * x + 1, w - 1);
        const uint32_t y0 = std::min(2 * y, h - 1), y1 = std::min(2 * y + 1, h - 1);
        next[static_cast<size_t>(y) * nw + x] =
            (level[static_cast<size_t>(y0) * w + x0] + level[static_cast<size_t>(y0) * w + x1] +
             level[static_cast<size_t>(y1) * w + x0] + level[static_cast<size_t>(y1) * w + x1]) /
            4;
      }
    }
    level.swap(next);
    w = nw;
    h = nh;
  }
  t.view = t.texture.CreateView();
  return t;
}

double linear_luminance(double r, double g, double b) {
  return 0.2126 * linear_of_srgb(r) + 0.7152 * linear_of_srgb(g) + 0.0722 * linear_of_srgb(b);
}

bool bake_moon_map(const std::vector<double>& linear, int width, int height,
                   std::vector<uint8_t>& bytes, double& scale) {
  if (width != 2 * height || height < kMoonMapHeight || height % kMoonMapHeight != 0 ||
      linear.size() != static_cast<size_t>(width) * static_cast<size_t>(height)) {
    return false;
  }
  // The box filter down to the map's size, in linear light.
  const int k = height / kMoonMapHeight;
  std::vector<double> lin(static_cast<size_t>(kMoonMapWidth) * kMoonMapHeight);
  double most = 0;
  for (int y = 0; y < kMoonMapHeight; ++y) {
    for (int x = 0; x < kMoonMapWidth; ++x) {
      double sum = 0;
      for (int j = 0; j < k; ++j) {
        for (int i = 0; i < k; ++i) {
          sum += linear[static_cast<size_t>(y * k + j) * static_cast<size_t>(width) +
                        static_cast<size_t>(x * k + i)];
        }
      }
      const double v = sum / (k * k);
      lin[static_cast<size_t>(y) * kMoonMapWidth + x] = v;
      most = std::max(most, v);
    }
  }
  if (most <= 0) return false;
  bytes.resize(lin.size());
  for (size_t i = 0; i < lin.size(); ++i) {
    bytes[i] = static_cast<uint8_t>(std::lround(lin[i] / most * 255.0));
  }
  // The near side's mean as a full moon shows it: each texel by the disc's area it covers
  // (cos latitude for the sphere's, cos latitude cos longitude for its slant to the eye).
  double sum = 0, weight = 0;
  for (int y = 0; y < kMoonMapHeight; ++y) {
    const double lat = (0.5 - (y + 0.5) / kMoonMapHeight) * kPi;
    for (int x = 0; x < kMoonMapWidth; ++x) {
      const double lon = ((x + 0.5) / kMoonMapWidth - 0.5) * 2 * kPi;
      const double w = std::cos(lat) * std::cos(lat) * std::cos(lon);
      if (w <= 0) continue;
      sum += w * bytes[static_cast<size_t>(y) * kMoonMapWidth + x];
      weight += w;
    }
  }
  scale = 255.0 * weight / sum;
  return true;
}

std::string encode_moon_map(const std::vector<uint8_t>& bytes) {
  std::string out;
  for (size_t i = 0; i < bytes.size(); i += 3) {
    const uint32_t a = bytes[i];
    const uint32_t b = i + 1 < bytes.size() ? bytes[i + 1] : 0;
    const uint32_t c = i + 2 < bytes.size() ? bytes[i + 2] : 0;
    const uint32_t v = (a << 16) | (b << 8) | c;
    out += kAlphabet[(v >> 18) & 63];
    out += kAlphabet[(v >> 12) & 63];
    out += i + 1 < bytes.size() ? kAlphabet[(v >> 6) & 63] : '=';
    out += i + 2 < bytes.size() ? kAlphabet[v & 63] : '=';
  }
  return out;
}

bool decode_moon_map(std::string_view base64, std::vector<uint8_t>& out) {
  out.clear();
  uint32_t acc = 0;
  int bits = 0;
  for (const char c : base64) {
    if (c == '=') break;
    const int v = sextet(c);
    if (v < 0) return false;
    acc = (acc << 6) | static_cast<uint32_t>(v);
    bits += 6;
    if (bits >= 8) {
      bits -= 8;
      out.push_back(static_cast<uint8_t>((acc >> bits) & 0xff));
    }
  }
  return true;
}

}  // namespace ge::render
