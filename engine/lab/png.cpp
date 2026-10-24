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

#include "assets/inflate.hpp"

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
  static const uint8_t kSignature[8] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n'};
  if (bytes.size() < 8 || !std::equal(kSignature, kSignature + 8, bytes.begin())) return {};
  uint32_t width = 0, height = 0;
  int channels = 0;
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
      const uint8_t depth = data[8], colour = data[9], interlace = data[12];
      channels = colour == 6 ? 4 : colour == 2 ? 3 : colour == 4 ? 2 : colour == 0 ? 1 : 0;
      if (depth != 8 || channels == 0 || interlace != 0) return {};
    } else if (std::equal(type, type + 4, "IDAT")) {
      z.insert(z.end(), data, data + len);
    } else if (std::equal(type, type + 4, "IEND")) {
      break;
    }
    at += 12 + len;
  }
  if (width == 0 || height == 0 || width > 1u << 15 || height > 1u << 15) return {};
  const auto raw = inflate_zlib(z);
  const size_t stride = size_t{width} * static_cast<size_t>(channels);
  if (!raw || raw->size() != (stride + 1) * height) return {};
  // Undo each row's filter in place, against the row above (zeros above the first).
  std::vector<uint8_t> rows(stride * height);
  const auto bpp = static_cast<size_t>(channels);
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
  Picture p{static_cast<int>(width), static_cast<int>(height), {}};
  p.rgba.resize(size_t{width} * height * 4);
  for (size_t i = 0; i < size_t{width} * height; i++) {
    const uint8_t* s = &rows[i * bpp];
    uint8_t* d = &p.rgba[i * 4];
    if (channels >= 3) {
      d[0] = s[0], d[1] = s[1], d[2] = s[2];
    } else {
      d[0] = d[1] = d[2] = s[0];
    }
    d[3] = channels == 4 ? s[3] : channels == 2 ? s[1] : 255;
  }
  return p;
}

std::optional<Picture> read_png(const std::string& path) {
  std::ifstream f(path, std::ios::binary);
  if (!f) return {};
  const std::vector<uint8_t> bytes{std::istreambuf_iterator<char>(f),
                                   std::istreambuf_iterator<char>()};
  return decode_png(bytes);
}

}  // namespace ge::lab
