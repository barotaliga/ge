// The PNG reader's chunks, palette and row filters; the nearest texel; sRGB's transfer.
//
// ge:summary The PNG reader's chunks, palette and filters, the nearest texel, sRGB to linear
// ge:keywords png ihdr plte trns idat iend filter paeth palette bit-depth texel srgb
// ge:skill assets
// ge:test tests/assets_texture_test.cpp
#include "assets/image.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>

#include "assets/inflate.hpp"

namespace ge::assets {

namespace {

uint32_t get32(const uint8_t* p) {
  return (uint32_t{p[0]} << 24) | (uint32_t{p[1]} << 16) | (uint32_t{p[2]} << 8) | p[3];
}

int paeth(int a, int b, int c) {
  const int p = a + b - c;
  const int pa = std::abs(p - a), pb = std::abs(p - b), pc = std::abs(p - c);
  if (pa <= pb && pa <= pc) return a;
  return pb <= pc ? b : c;
}

}  // namespace

uint32_t crc32(const uint8_t* bytes, size_t n, uint32_t crc) {
  crc = ~crc;
  for (size_t i = 0; i < n; i++) {
    crc ^= bytes[i];
    for (int k = 0; k < 8; k++) crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1u)));
  }
  return ~crc;
}

std::optional<Image> decode_png(std::span<const uint8_t> bytes) {
  static const uint8_t kSignature[8] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n'};
  if (bytes.size() < 8 || !std::equal(kSignature, kSignature + 8, bytes.begin())) return {};
  uint32_t width = 0, height = 0;
  int channels = 0, depth = 0;
  bool indexed = false;
  std::vector<uint8_t> palette;      // rgba a colour
  std::vector<uint8_t> grey_or_rgb;  // the colour type's own transparency, unused but read
  std::vector<uint8_t> z;
  size_t at = 8;
  while (at + 12 <= bytes.size()) {
    const uint32_t len = get32(&bytes[at]);
    if (len > bytes.size() - at - 12) return {};
    const uint8_t* type = &bytes[at + 4];
    const uint8_t* data = type + 4;
    if (crc32(type, len + 4) != get32(data + len)) return {};
    if (std::equal(type, type + 4, "IHDR")) {
      if (len < 13) return {};
      width = get32(data);
      height = get32(data + 4);
      depth = data[8];
      const uint8_t colour = data[9], interlace = data[12];
      indexed = colour == 3;
      channels = colour == 6   ? 4
                 : colour == 2 ? 3
                 : colour == 4 ? 2
                 : colour == 0 ? 1
                 : colour == 3 ? 1
                               : 0;
      const bool depth_ok = indexed ? (depth == 1 || depth == 2 || depth == 4 || depth == 8)
                                    : depth == 8;
      if (!depth_ok || channels == 0 || interlace != 0) return {};
    } else if (std::equal(type, type + 4, "PLTE")) {
      if (len % 3 != 0) return {};
      palette.clear();
      for (uint32_t i = 0; i + 2 < len; i += 3) palette.insert(palette.end(), {data[i], data[i + 1], data[i + 2], 255});
    } else if (std::equal(type, type + 4, "tRNS")) {
      if (indexed) {
        for (uint32_t i = 0; i < len && size_t{i} * 4 + 3 < palette.size(); ++i)
          palette[size_t{i} * 4 + 3] = data[i];
      }
    } else if (std::equal(type, type + 4, "IDAT")) {
      z.insert(z.end(), data, data + len);
    } else if (std::equal(type, type + 4, "IEND")) {
      break;
    }
    at += 12 + len;
  }
  if (width == 0 || height == 0 || width > 1u << 15 || height > 1u << 15) return {};
  if (indexed && palette.empty()) return {};
  const auto raw = inflate_zlib(z);
  // A row's bytes: whole bytes for 8 bits a channel, packed indices below that.
  const size_t stride = (size_t{width} * static_cast<size_t>(channels) * depth + 7) / 8;
  if (!raw || raw->size() != (stride + 1) * height) return {};
  // Undo each row's filter in place, against the row above (zeros above the first). The filter's
  // byte step is a pixel's bytes, at least one.
  std::vector<uint8_t> rows(stride * height);
  const size_t bpp = std::max<size_t>(1, static_cast<size_t>(channels) * depth / 8);
  for (size_t y = 0; y < height; y++) {
    const uint8_t filter = (*raw)[y * (stride + 1)];
    const uint8_t* in = &(*raw)[y * (stride + 1) + 1];
    uint8_t* out = &rows[y * stride];
    const uint8_t* up = y > 0 ? out - stride : nullptr;
    for (size_t i = 0; i < stride; i++) {
      const int a = i >= bpp ? out[i - bpp] : 0;
      const int b = up ? up[i] : 0;
      const int c = up && i >= bpp ? up[i - bpp] : 0;
      int v = in[i];
      switch (filter) {
        case 0:
          break;
        case 1:
          v += a;
          break;
        case 2:
          v += b;
          break;
        case 3:
          v += (a + b) / 2;
          break;
        case 4:
          v += paeth(a, b, c);
          break;
        default:
          return {};
      }
      out[i] = static_cast<uint8_t>(v);
    }
  }
  Image p{static_cast<int>(width), static_cast<int>(height), {}};
  p.rgba.resize(size_t{width} * height * 4);
  for (size_t y = 0; y < height; y++) {
    const uint8_t* row = &rows[y * stride];
    for (size_t x = 0; x < width; x++) {
      uint8_t* d = &p.rgba[(y * width + x) * 4];
      if (indexed) {
        const size_t bit = x * static_cast<size_t>(depth);
        const int shift = 8 - depth - static_cast<int>(bit % 8);
        const size_t i = (row[bit / 8] >> shift) & ((1u << depth) - 1);
        if (i * 4 + 3 >= palette.size()) return {};
        std::copy(&palette[i * 4], &palette[i * 4] + 4, d);
        continue;
      }
      const uint8_t* s = &row[x * static_cast<size_t>(channels)];
      if (channels >= 3) {
        d[0] = s[0], d[1] = s[1], d[2] = s[2];
      } else {
        d[0] = d[1] = d[2] = s[0];
      }
      d[3] = channels == 4 ? s[3] : channels == 2 ? s[1] : 255;
    }
  }
  return p;
}

const uint8_t* texel(const Image& image, float u, float v) {
  const float fu = u - std::floor(u), fv = v - std::floor(v);
  const int x = std::min(image.width - 1, static_cast<int>(fu * static_cast<float>(image.width)));
  const int y =
      std::min(image.height - 1, static_cast<int>(fv * static_cast<float>(image.height)));
  return &image.rgba[(size_t(y) * size_t(image.width) + size_t(x)) * 4];
}

float srgb_to_linear(uint8_t c) {
  const float s = static_cast<float>(c) / 255.0f;
  return s <= 0.04045f ? s / 12.92f : std::pow((s + 0.055f) / 1.055f, 2.4f);
}

}  // namespace ge::assets
