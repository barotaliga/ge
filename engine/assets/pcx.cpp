// The PCX reader's header checks and runs, the writer, and the palette's colours.
//
// ge:summary The PCX reader's header and runs, the writer, the colours through a palette
// ge:keywords pcx header rle runs scanline padding palette decode encode
// ge:skill assets
// ge:test tests/assets_pcx_test.cpp
#include "assets/pcx.hpp"

namespace ge::assets {

namespace {

constexpr size_t kHeader = 128;
constexpr int kMaxSide = 4096;

int get16(const uint8_t* p) { return p[0] | (p[1] << 8); }

void put16(std::vector<uint8_t>& out, size_t at, int v) {
  out[at] = static_cast<uint8_t>(v & 0xFF);
  out[at + 1] = static_cast<uint8_t>((v >> 8) & 0xFF);
}

bool fail(std::string* error, std::string_view where, const std::string& what, size_t at,
          const char* next) {
  if (error) {
    *error =
        std::string(where) + ": " + what + " at byte " + std::to_string(at) + "; next: " + next;
  }
  return false;
}

}  // namespace

bool decode_pcx(std::span<const uint8_t> bytes, std::string_view where, IndexedImage* out,
                std::string* error) {
  const char* redo = "save it again as an 8-bit paletted PCX (one plane, run-length encoded)";
  if (bytes.size() < kHeader) {
    return fail(
        error, where,
        "the file is " + std::to_string(bytes.size()) + " bytes, shorter than the 128-byte header",
        bytes.size(), "the file is cut short; replace it from its source");
  }
  const uint8_t* h = bytes.data();
  if (h[0] != 0x0A) {
    return fail(error, where, "not a PCX (the first byte is " + std::to_string(h[0]) + ", not 10)",
                0, "check the file is the skin and not another kind of picture");
  }
  if (h[2] != 1) {
    return fail(error, where, "encoding " + std::to_string(h[2]) + " (only 1, runs, is read)", 2,
                redo);
  }
  if (h[3] != 8 || h[65] != 1) {
    return fail(error, where,
                std::to_string(h[3]) + " bits in " + std::to_string(h[65]) +
                    " planes (only 8 bits in 1 plane is read)",
                3, redo);
  }
  const int x0 = get16(h + 4), y0 = get16(h + 6), x1 = get16(h + 8), y1 = get16(h + 10);
  const int w = x1 - x0 + 1, ht = y1 - y0 + 1, line = get16(h + 66);
  if (w <= 0 || ht <= 0 || w > kMaxSide || ht > kMaxSide) {
    return fail(error, where,
                "a window of " + std::to_string(w) + " x " + std::to_string(ht) +
                    " pixels (1 to 4096 a side is read)",
                4, redo);
  }
  if (line < w) {
    return fail(error, where,
                std::to_string(line) + " bytes a line for " + std::to_string(w) + " pixels", 66,
                redo);
  }
  IndexedImage img;
  img.width = w;
  img.height = ht;
  img.indices.resize(static_cast<size_t>(w) * static_cast<size_t>(ht));
  // The lines as one stream: a run may cross from one line into the next.
  const size_t total = static_cast<size_t>(line) * static_cast<size_t>(ht);
  size_t at = kHeader, n = 0;
  while (n < total) {
    if (at >= bytes.size()) {
      return fail(
          error, where,
          "the pixels end after " + std::to_string(n) + " of " + std::to_string(total) + " bytes",
          at, "the file is cut short; replace it from its source");
    }
    uint8_t b = bytes[at++];
    size_t count = 1;
    if ((b & 0xC0) == 0xC0) {
      count = b & 0x3F;
      if (at >= bytes.size()) {
        return fail(error, where, "a run with no value after it", at,
                    "the file is cut short; replace it from its source");
      }
      b = bytes[at++];
    }
    for (size_t k = 0; k < count && n < total; ++k, ++n) {
      const size_t x = n % static_cast<size_t>(line), y = n / static_cast<size_t>(line);
      if (x < static_cast<size_t>(w)) img.indices[y * static_cast<size_t>(w) + x] = b;
    }
  }
  // The palette: the marker 12 and 768 bytes, at the very end.
  if (bytes.size() >= kHeader + 769 && bytes[bytes.size() - 769] == 0x0C) {
    img.palette.assign(bytes.end() - 768, bytes.end());
  }
  *out = std::move(img);
  return true;
}

std::vector<uint8_t> encode_pcx(const IndexedImage& image) {
  const int w = image.width, ht = image.height;
  const int line = w + (w & 1);  // even, as readers expect
  std::vector<uint8_t> out(kHeader, 0);
  out[0] = 0x0A;
  out[1] = 5;
  out[2] = 1;
  out[3] = 8;
  put16(out, 8, w - 1);
  put16(out, 10, ht - 1);
  put16(out, 12, 72);
  put16(out, 14, 72);
  out[65] = 1;
  put16(out, 66, line);
  put16(out, 68, 1);
  out.reserve(kHeader + static_cast<size_t>(line) * static_cast<size_t>(ht) * 2 + 769);
  for (int y = 0; y < ht; ++y) {
    const uint8_t* row = &image.indices[static_cast<size_t>(y) * static_cast<size_t>(w)];
    int x = 0;
    while (x < line) {
      const uint8_t v = x < w ? row[x] : 0;
      int run = 1;
      while (x + run < line && run < 63 && (x + run < w ? row[x + run] : 0) == v) ++run;
      if (run > 1 || (v & 0xC0) == 0xC0) out.push_back(static_cast<uint8_t>(0xC0 | run));
      out.push_back(v);
      x += run;
    }
  }
  if (image.palette.size() == 768) {
    out.push_back(0x0C);
    out.insert(out.end(), image.palette.begin(), image.palette.end());
  }
  return out;
}

std::vector<uint8_t> pcx_rgba(const IndexedImage& image, std::span<const uint8_t> palette) {
  const std::span<const uint8_t> pal = palette.size() == 768 ? palette : image.palette;
  std::vector<uint8_t> rgba(image.indices.size() * 4, 255);
  for (size_t i = 0; i < image.indices.size(); ++i) {
    const size_t c = size_t{image.indices[i]} * 3;
    for (size_t k = 0; k < 3; ++k)
      rgba[i * 4 + k] = pal.size() == 768 ? pal[c + k] : image.indices[i];
  }
  return rgba;
}

}  // namespace ge::assets
