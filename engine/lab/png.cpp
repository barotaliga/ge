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

int paeth(int a, int b, int c) {
  const int p = a + b - c;
  const int pa = std::abs(p - a), pb = std::abs(p - b), pc = std::abs(p - c);
  if (pa <= pb && pa <= pc) return a;
  return pb <= pc ? b : c;
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

std::vector<uint8_t> encode_png_grey(int width, int height, std::span<const uint8_t> grey) {
  const size_t w = static_cast<size_t>(width);
  // Each row under the filter (none, sub, up, paeth) whose bytes sum smallest as signed values:
  // the usual guess at what deflates best.
  std::vector<uint8_t> raw;
  raw.reserve((w + 1) * height);
  std::vector<uint8_t> trial(w), best(w);
  for (int y = 0; y < height; ++y) {
    const uint8_t* row = &grey[y * w];
    const uint8_t* up = y > 0 ? &grey[(y - 1) * w] : nullptr;
    long best_cost = -1;
    uint8_t best_type = 0;
    for (const uint8_t type : {uint8_t{0}, uint8_t{1}, uint8_t{2}, uint8_t{4}}) {
      long cost = 0;
      for (size_t x = 0; x < w; ++x) {
        const int a = x > 0 ? row[x - 1] : 0, b = up ? up[x] : 0, c = x > 0 && up ? up[x - 1] : 0;
        const int pred = type == 0 ? 0 : type == 1 ? a : type == 2 ? b : paeth(a, b, c);
        trial[x] = static_cast<uint8_t>(row[x] - pred);
        cost += trial[x] < 128 ? trial[x] : 256 - trial[x];
      }
      if (best_cost < 0 || cost < best_cost) {
        best_cost = cost;
        best_type = type;
        best.swap(trial);
      }
    }
    raw.push_back(best_type);
    raw.insert(raw.end(), best.begin(), best.end());
  }
  std::vector<uint8_t> png{0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n'};
  std::vector<uint8_t> ihdr;
  put32(ihdr, static_cast<uint32_t>(width));
  put32(ihdr, static_cast<uint32_t>(height));
  ihdr.insert(ihdr.end(), {8, 0, 0, 0, 0});  // 8-bit grey, deflate, adaptive, no interlace
  chunk(png, "IHDR", ihdr);
  chunk(png, "IDAT", deflate_zlib(raw));
  chunk(png, "IEND", {});
  return png;
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
