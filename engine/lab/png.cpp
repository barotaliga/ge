// The PNG chunks, the stored-block zlib stream and the two checksums; the reader's chunks and
// row filters.
//
// ge:summary The PNG writer's chunks, stored deflate and checksums; the reader
// ge:keywords png crc32 adler32 zlib stored-deflate chunk ihdr idat iend decode filter paeth
// ge:skill labs
// ge:test tests/lab_raster_test.cpp
#include "lab/png.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>

#include "assets/image.hpp"
#include "lab/deflate.hpp"

namespace ge::lab {

namespace {

void put32(std::vector<uint8_t>& out, uint32_t v) {
  out.push_back(static_cast<uint8_t>(v >> 24));
  out.push_back(static_cast<uint8_t>(v >> 16));
  out.push_back(static_cast<uint8_t>(v >> 8));
  out.push_back(static_cast<uint8_t>(v));
}

void chunk(std::vector<uint8_t>& out, const char* type, const std::vector<uint8_t>& data) {
  put32(out, static_cast<uint32_t>(data.size()));
  const size_t start = out.size();
  out.insert(out.end(), type, type + 4);
  out.insert(out.end(), data.begin(), data.end());
  put32(out, crc32(&out[start], out.size() - start));
}

// The PNG Paeth predictor: whichever of left, up and up-left is nearest left + up - up-left.
int paeth(int a, int b, int c) {
  const int p = a + b - c, pa = std::abs(p - a), pb = std::abs(p - b), pc = std::abs(p - c);
  return pa <= pb && pa <= pc ? a : pb <= pc ? b : c;
}

}  // namespace

std::vector<uint8_t> encode_png(const Picture& picture) {
  const size_t stride = static_cast<size_t>(picture.width) * 4;
  // The raw image: each scanline led by filter type 0 (none).
  std::vector<uint8_t> raw;
  raw.reserve((stride + 1) * picture.height);
  for (int y = 0; y < picture.height; y++) {
    raw.push_back(0);
    const uint8_t* row = &picture.rgba[y * stride];
    raw.insert(raw.end(), row, row + stride);
  }

  // zlib: the header, stored blocks of at most 65535 bytes, the Adler-32 of the raw bytes.
  std::vector<uint8_t> z{0x78, 0x01};
  size_t at = 0;
  do {
    const size_t len = std::min<size_t>(65535, raw.size() - at);
    z.push_back(at + len == raw.size() ? 1 : 0);
    z.push_back(static_cast<uint8_t>(len));
    z.push_back(static_cast<uint8_t>(len >> 8));
    z.push_back(static_cast<uint8_t>(~len));
    z.push_back(static_cast<uint8_t>(~len >> 8));
    z.insert(z.end(), raw.begin() + static_cast<std::ptrdiff_t>(at),
             raw.begin() + static_cast<std::ptrdiff_t>(at + len));
    at += len;
  } while (at < raw.size());
  uint32_t a = 1, b = 0;
  for (const uint8_t byte : raw) {
    a = (a + byte) % 65521;
    b = (b + a) % 65521;
  }
  put32(z, (b << 16) | a);

  std::vector<uint8_t> out{0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n'};
  std::vector<uint8_t> ihdr;
  put32(ihdr, static_cast<uint32_t>(picture.width));
  put32(ihdr, static_cast<uint32_t>(picture.height));
  ihdr.insert(ihdr.end(), {8, 6, 0, 0, 0});  // 8 bits, RGBA, deflate, adaptive, no interlace
  chunk(out, "IHDR", ihdr);
  chunk(out, "IDAT", z);
  chunk(out, "IEND", {});
  return out;
}

bool write_png(const std::string& path, const Picture& picture) {
  return write_file(path, encode_png(picture));
}

bool write_file(const std::string& path, std::span<const uint8_t> bytes) {
  bool ok = false;
  {
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f) return false;
    f.write(reinterpret_cast<const char*>(bytes.data()),
            static_cast<std::streamsize>(bytes.size()));
    f.close();
    ok = !f.fail();
  }
  if (!ok) std::remove(path.c_str());
  return ok;
}

namespace {

// `height` rows of `width` pixels of `channels` bytes each as a PNG of `color_type` (0 grey, 2
// RGB), each row under the filter (none, sub, up, paeth) whose bytes sum smallest as signed
// values, the usual guess at what deflates best; `pixel(x, y, c)` gives a pixel's channel.
template <class Pixel>
std::vector<uint8_t> encode_filtered(int width, int height, int channels, uint8_t color_type,
                                     Pixel&& pixel) {
  const size_t bpp = static_cast<size_t>(channels);
  const size_t row_bytes = static_cast<size_t>(width) * bpp;
  std::vector<uint8_t> raw;
  raw.reserve((row_bytes + 1) * static_cast<size_t>(height));
  std::vector<uint8_t> row(row_bytes), up(row_bytes, 0), trial(row_bytes), best(row_bytes);
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x)
      for (int c = 0; c < channels; ++c) row[x * bpp + c] = pixel(x, y, c);
    long best_cost = -1;
    uint8_t best_type = 0;
    for (const uint8_t type : {uint8_t{0}, uint8_t{1}, uint8_t{2}, uint8_t{4}}) {
      long cost = 0;
      for (size_t i = 0; i < row_bytes; ++i) {
        const int a = i >= bpp ? row[i - bpp] : 0, b = y > 0 ? up[i] : 0,
                  c = i >= bpp && y > 0 ? up[i - bpp] : 0;
        const int pred = type == 0 ? 0 : type == 1 ? a : type == 2 ? b : paeth(a, b, c);
        trial[i] = static_cast<uint8_t>(row[i] - pred);
        cost += trial[i] < 128 ? trial[i] : 256 - trial[i];
      }
      if (best_cost < 0 || cost < best_cost) {
        best_cost = cost;
        best_type = type;
        best.swap(trial);
      }
    }
    raw.push_back(best_type);
    raw.insert(raw.end(), best.begin(), best.end());
    up.swap(row);
  }
  std::vector<uint8_t> png{0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n'};
  std::vector<uint8_t> ihdr;
  put32(ihdr, static_cast<uint32_t>(width));
  put32(ihdr, static_cast<uint32_t>(height));
  ihdr.insert(ihdr.end(), {8, color_type, 0, 0, 0});  // 8-bit, deflate, adaptive, no interlace
  chunk(png, "IHDR", ihdr);
  chunk(png, "IDAT", deflate_zlib(raw));
  chunk(png, "IEND", {});
  return png;
}

}  // namespace

std::vector<uint8_t> encode_png_grey(int width, int height, std::span<const uint8_t> grey) {
  const size_t w = static_cast<size_t>(width);
  return encode_filtered(width, height, 1, 0,
                         [&](int x, int y, int) { return grey[static_cast<size_t>(y) * w + x]; });
}

std::vector<uint8_t> encode_png_rgb(int width, int height, std::span<const uint8_t> rgba) {
  const size_t w = static_cast<size_t>(width);
  return encode_filtered(width, height, 3, 2, [&](int x, int y, int c) {
    return rgba[(static_cast<size_t>(y) * w + static_cast<size_t>(x)) * 4 + c];
  });
}

std::optional<Picture> decode_png(std::span<const uint8_t> bytes) {
  auto image = assets::decode_png(bytes);
  if (!image) return {};
  return Picture{image->width, image->height, std::move(image->rgba)};
}

std::optional<Picture> read_png(const std::string& path) {
  std::ifstream f(path, std::ios::binary);
  if (!f) return {};
  const std::vector<uint8_t> bytes{std::istreambuf_iterator<char>(f),
                                   std::istreambuf_iterator<char>()};
  return decode_png(bytes);
}

}  // namespace ge::lab
