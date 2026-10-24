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
  const std::vector<uint8_t> bytes = encode_png(picture);
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
