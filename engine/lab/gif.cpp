// The shrink's box filter, the palette (exact, or the commonest five-bit buckets) and the GIF's
// LZW stream.
//
// ge:summary The GIF writer: the box-filter shrink, the 256-colour palette, the LZW codes
// ge:keywords gif lzw palette popularity bucket nearest box-filter sub-block clear-code
// ge:skill labs
// ge:test tests/lab_gif_test.cpp
#include "lab/gif.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <unordered_map>

namespace ge::lab {

namespace {

// A pixel laid over white, as 0xRRGGBB.
uint32_t over_white(const uint8_t* p) {
  const auto ch = [&](int i) {
    return static_cast<uint32_t>((p[i] * p[3] + 255 * (255 - p[3]) + 127) / 255);
  };
  return (ch(0) << 16) | (ch(1) << 8) | ch(2);
}

uint32_t bucket_of(uint32_t rgb) {
  return ((rgb >> 19) & 31) << 10 | ((rgb >> 11) & 31) << 5 | ((rgb >> 3) & 31);
}

struct Indexed {
  std::vector<uint32_t> palette;  // 0xRRGGBB, at most 256
  std::vector<uint8_t> pixels;    // one index a pixel
};

Indexed index_colours(const Picture& p) {
  Indexed out;
  const size_t n = static_cast<size_t>(p.width) * static_cast<size_t>(p.height);
  std::vector<uint32_t> rgb(n);
  for (size_t i = 0; i < n; ++i) rgb[i] = over_white(&p.rgba[i * 4]);
  out.pixels.resize(n);

  // Few enough colours: every one kept exactly.
  std::unordered_map<uint32_t, uint8_t> exact;
  for (size_t i = 0; i < n && exact.size() <= 256; ++i) {
    if (exact.size() < 256 || exact.count(rgb[i])) {
      exact.emplace(rgb[i], static_cast<uint8_t>(exact.size()));
    } else {
      exact.emplace(rgb[i], 0);  // a 257th: past exact
    }
  }
  if (exact.size() <= 256) {
    out.palette.resize(exact.size());
    for (const auto& [c, k] : exact) out.palette[k] = c;
    for (size_t i = 0; i < n; ++i) out.pixels[i] = exact[rgb[i]];
    return out;
  }

  // The commonest five-bit buckets, each the mean of its pixels.
  constexpr size_t kBuckets = 1u << 15;
  std::vector<uint32_t> count(kBuckets, 0);
  std::vector<std::array<uint64_t, 3>> sum(kBuckets, {0, 0, 0});
  for (size_t i = 0; i < n; ++i) {
    const uint32_t b = bucket_of(rgb[i]);
    ++count[b];
    sum[b][0] += (rgb[i] >> 16) & 255;
    sum[b][1] += (rgb[i] >> 8) & 255;
    sum[b][2] += rgb[i] & 255;
  }
  std::vector<uint32_t> order;
  for (uint32_t b = 0; b < kBuckets; ++b) {
    if (count[b] > 0) order.push_back(b);
  }
  const size_t keep = std::min<size_t>(256, order.size());
  std::partial_sort(order.begin(), order.begin() + static_cast<std::ptrdiff_t>(keep), order.end(),
                    [&](uint32_t a, uint32_t b) {
                      return count[a] > count[b] || (count[a] == count[b] && a < b);
                    });
  order.resize(keep);
  for (const uint32_t b : order) {
    const auto mean = [&](int c) { return static_cast<uint32_t>(sum[b][c] / count[b]); };
    out.palette.push_back((mean(0) << 16) | (mean(1) << 8) | mean(2));
  }
  // Every bucket a picture uses, to its nearest entry.
  std::vector<int> nearest(kBuckets, -1);
  for (size_t i = 0; i < n; ++i) {
    const uint32_t b = bucket_of(rgb[i]);
    if (nearest[b] < 0) {
      const int r = static_cast<int>((b >> 10) << 3 | 4),
                g = static_cast<int>(((b >> 5) & 31) << 3 | 4),
                bl = static_cast<int>((b & 31) << 3 | 4);
      int best = 0;
      long best_d = -1;
      for (size_t k = 0; k < out.palette.size(); ++k) {
        const int dr = r - static_cast<int>((out.palette[k] >> 16) & 255);
        const int dg = g - static_cast<int>((out.palette[k] >> 8) & 255);
        const int db = bl - static_cast<int>(out.palette[k] & 255);
        const long d = 2L * dr * dr + 4L * dg * dg + 3L * db * db;
        if (best_d < 0 || d < best_d) {
          best_d = d;
          best = static_cast<int>(k);
        }
      }
      nearest[b] = best;
    }
    out.pixels[i] = static_cast<uint8_t>(nearest[b]);
  }
  return out;
}

// Codes packed least significant bit first, cut into sub-blocks of at most 255 bytes.
struct BitWriter {
  std::vector<uint8_t> bytes;
  uint32_t acc = 0;
  int bits = 0;
  void put(uint32_t code, int size) {
    acc |= code << bits;
    bits += size;
    while (bits >= 8) {
      bytes.push_back(static_cast<uint8_t>(acc & 255));
      acc >>= 8;
      bits -= 8;
    }
  }
  void flush() {
    if (bits > 0) bytes.push_back(static_cast<uint8_t>(acc & 255));
    acc = 0;
    bits = 0;
  }
};

void put16(std::vector<uint8_t>& out, int v) {
  out.push_back(static_cast<uint8_t>(v & 255));
  out.push_back(static_cast<uint8_t>((v >> 8) & 255));
}

}  // namespace

Picture shrink(const Picture& picture, int width) {
  if (width <= 0 || picture.width <= width) return picture;
  const int height =
      std::max(1, static_cast<int>(static_cast<long>(picture.height) * width / picture.width));
  Picture out{width, height, std::vector<uint8_t>(static_cast<size_t>(width) * height * 4)};
  for (int y = 0; y < height; ++y) {
    const int y0 = y * picture.height / height,
              y1 = std::max(y0 + 1, (y + 1) * picture.height / height);
    for (int x = 0; x < width; ++x) {
      const int x0 = x * picture.width / width,
                x1 = std::max(x0 + 1, (x + 1) * picture.width / width);
      std::array<uint32_t, 4> s{0, 0, 0, 0};
      for (int sy = y0; sy < y1; ++sy) {
        for (int sx = x0; sx < x1; ++sx) {
          const uint8_t* p = &picture.rgba[(static_cast<size_t>(sy) * picture.width + sx) * 4];
          for (int c = 0; c < 4; ++c) s[c] += p[c];
        }
      }
      const uint32_t area = static_cast<uint32_t>((y1 - y0) * (x1 - x0));
      uint8_t* q = &out.rgba[(static_cast<size_t>(y) * width + x) * 4];
      for (int c = 0; c < 4; ++c) q[c] = static_cast<uint8_t>((s[c] + area / 2) / area);
    }
  }
  return out;
}

std::vector<uint8_t> encode_gif(const Picture& picture) {
  const Indexed ix = index_colours(picture);
  std::vector<uint8_t> out = {'G', 'I', 'F', '8', '9', 'a'};
  put16(out, picture.width);
  put16(out, picture.height);
  out.push_back(0xF7);  // a global table of 256 entries, 8 bits a channel
  out.push_back(0);     // the background is entry 0
  out.push_back(0);     // square pixels
  for (size_t k = 0; k < 256; ++k) {
    const uint32_t c = k < ix.palette.size() ? ix.palette[k] : 0;
    out.push_back(static_cast<uint8_t>((c >> 16) & 255));
    out.push_back(static_cast<uint8_t>((c >> 8) & 255));
    out.push_back(static_cast<uint8_t>(c & 255));
  }
  out.push_back(0x2C);  // the frame: at the origin, the whole screen, no local table
  put16(out, 0);
  put16(out, 0);
  put16(out, picture.width);
  put16(out, picture.height);
  out.push_back(0);

  // LZW over 8-bit symbols: clear 256, end 257. A code grows a bit when the next one would not
  // fit, as the reader's table (one entry behind) sees it; a full table starts over.
  constexpr uint32_t kClear = 256, kEnd = 257, kMax = 4096;
  out.push_back(8);
  BitWriter w;
  std::unordered_map<uint32_t, uint32_t> table;
  table.reserve(kMax);
  uint32_t next = kEnd + 1;
  int size = 9;
  w.put(kClear, size);
  if (!ix.pixels.empty()) {
    uint32_t cur = ix.pixels[0];
    for (size_t i = 1; i < ix.pixels.size(); ++i) {
      const uint32_t key = cur << 8 | ix.pixels[i];
      if (const auto it = table.find(key); it != table.end()) {
        cur = it->second;
        continue;
      }
      w.put(cur, size);
      table.emplace(key, next++);
      if (next > (1u << size) && size < 12) ++size;
      if (next == kMax) {
        w.put(kClear, size);
        table.clear();
        next = kEnd + 1;
        size = 9;
      }
      cur = ix.pixels[i];
    }
    w.put(cur, size);
    if (next + 1 > (1u << size) && size < 12 && next != kEnd + 1) ++size;
  }
  w.put(kEnd, size);
  w.flush();
  for (size_t at = 0; at < w.bytes.size(); at += 255) {
    const size_t len = std::min<size_t>(255, w.bytes.size() - at);
    out.push_back(static_cast<uint8_t>(len));
    out.insert(out.end(), w.bytes.begin() + static_cast<std::ptrdiff_t>(at),
               w.bytes.begin() + static_cast<std::ptrdiff_t>(at + len));
  }
  out.push_back(0);     // the frame's last sub-block
  out.push_back(0x3B);  // the trailer
  return out;
}

}  // namespace ge::lab
