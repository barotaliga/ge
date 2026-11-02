// The JPEG encoder and decoder (jpeg.hpp): the standard tables, the fixed-point DCT, the Huffman
// coder and the marker segments.
//
// ge:summary JPEG's tables, the fixed-point DCT both ways, Huffman coding, markers, YCbCr
// ge:keywords jpeg dct idct huffman zigzag quant-table marker sof0 dht dqt sos ycbcr
// ge:skill replay
// ge:test tests/capture_jpeg_test.cpp
#include "capture/jpeg.hpp"

#include <algorithm>
#include <array>
#include <cstddef>

namespace ge::capture {

namespace {

// The orthonormal 8x8 DCT's basis, times 2^13: kDct[u][x] = a(u) cos((2x + 1) u pi / 16), a(0) =
// sqrt(1/8), a(u) = 1/2. Integers, so a block transforms to the same coefficients everywhere.
constexpr int32_t kDct[8][8] = {
    {2896, 2896, 2896, 2896, 2896, 2896, 2896, 2896},
    {4017, 3406, 2276, 799, -799, -2276, -3406, -4017},
    {3784, 1567, -1567, -3784, -3784, -1567, 1567, 3784},
    {3406, -799, -4017, -2276, 2276, 4017, 799, -3406},
    {2896, -2896, -2896, 2896, 2896, -2896, -2896, 2896},
    {2276, -4017, 799, 3406, -3406, -799, 4017, -2276},
    {1567, -3784, 3784, -1567, -1567, 3784, -3784, 1567},
    {799, -2276, 3406, -4017, 4017, -3406, 2276, -799},
};

// The zigzag: the n-th coefficient coded is the block's kZigzag[n]-th, row by row.
constexpr uint8_t kZigzag[64] = {0,  1,  8,  16, 9,  2,  3,  10, 17, 24, 32, 25, 18, 11, 4,  5,
                                 12, 19, 26, 33, 40, 48, 41, 34, 27, 20, 13, 6,  7,  14, 21, 28,
                                 35, 42, 49, 56, 57, 50, 43, 36, 29, 22, 15, 23, 30, 37, 44, 51,
                                 58, 59, 52, 45, 38, 31, 39, 46, 53, 60, 61, 54, 47, 55, 62, 63};

// The standard quantization tables at quality 50, row by row.
constexpr uint8_t kLumaQ[64] = {16,  11,  10,  16,  24, 40, 51,  61,  12,  12,  14,  19,  26,
                                58,  60,  55,  14,  13, 16, 24,  40,  57,  69,  56,  14,  17,
                                22,  29,  51,  87,  80, 62, 18,  22,  37,  56,  68,  109, 103,
                                77,  24,  35,  55,  64, 81, 104, 113, 92,  49,  64,  78,  87,
                                103, 121, 120, 101, 72, 92, 95,  98,  112, 100, 103, 99};
constexpr uint8_t kChromaQ[64] = {17, 18, 24, 47, 99, 99, 99, 99, 18, 21, 26, 66, 99, 99, 99, 99,
                                  24, 26, 56, 99, 99, 99, 99, 99, 47, 66, 99, 99, 99, 99, 99, 99,
                                  99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
                                  99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99};

// The standard Huffman tables: the count of codes of each length 1..16, then the symbols.
constexpr uint8_t kDcLumaBits[16] = {0, 1, 5, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0};
constexpr uint8_t kDcChromaBits[16] = {0, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0};
constexpr uint8_t kDcValues[12] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
constexpr uint8_t kAcLumaBits[16] = {0, 2, 1, 3, 3, 2, 4, 3, 5, 5, 4, 4, 0, 0, 1, 0x7d};
constexpr uint8_t kAcLumaValues[162] = {
    0x01, 0x02, 0x03, 0x00, 0x04, 0x11, 0x05, 0x12, 0x21, 0x31, 0x41, 0x06, 0x13, 0x51, 0x61,
    0x07, 0x22, 0x71, 0x14, 0x32, 0x81, 0x91, 0xa1, 0x08, 0x23, 0x42, 0xb1, 0xc1, 0x15, 0x52,
    0xd1, 0xf0, 0x24, 0x33, 0x62, 0x72, 0x82, 0x09, 0x0a, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x25,
    0x26, 0x27, 0x28, 0x29, 0x2a, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x43, 0x44, 0x45,
    0x46, 0x47, 0x48, 0x49, 0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x63, 0x64,
    0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7a, 0x83,
    0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8a, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99,
    0x9a, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6,
    0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xd2, 0xd3,
    0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8,
    0xe9, 0xea, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa};
constexpr uint8_t kAcChromaBits[16] = {0, 2, 1, 2, 4, 4, 3, 4, 7, 5, 4, 4, 0, 1, 2, 0x77};
constexpr uint8_t kAcChromaValues[162] = {
    0x00, 0x01, 0x02, 0x03, 0x11, 0x04, 0x05, 0x21, 0x31, 0x06, 0x12, 0x41, 0x51, 0x07, 0x61,
    0x71, 0x13, 0x22, 0x32, 0x81, 0x08, 0x14, 0x42, 0x91, 0xa1, 0xb1, 0xc1, 0x09, 0x23, 0x33,
    0x52, 0xf0, 0x15, 0x62, 0x72, 0xd1, 0x0a, 0x16, 0x24, 0x34, 0xe1, 0x25, 0xf1, 0x17, 0x18,
    0x19, 0x1a, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x43, 0x44,
    0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x63,
    0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7a,
    0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8a, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97,
    0x98, 0x99, 0x9a, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4,
    0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca,
    0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7,
    0xe8, 0xe9, 0xea, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa};

int32_t round_shift(int64_t v, int n) {
  return static_cast<int32_t>((v + (int64_t{1} << (n - 1))) >> n);
}

// `in` (row by row, centred on 0) to its DCT coefficients, row by row.
void forward_dct(const int32_t in[64], int32_t out[64]) {
  int32_t t[64];  // the rows transformed, times 2^3
  for (int y = 0; y < 8; ++y)
    for (int u = 0; u < 8; ++u) {
      int64_t s = 0;
      for (int x = 0; x < 8; ++x) s += int64_t{kDct[u][x]} * in[y * 8 + x];
      t[y * 8 + u] = round_shift(s, 10);
    }
  for (int v = 0; v < 8; ++v)
    for (int u = 0; u < 8; ++u) {
      int64_t s = 0;
      for (int y = 0; y < 8; ++y) s += int64_t{kDct[v][y]} * t[y * 8 + u];
      out[v * 8 + u] = round_shift(s, 16);
    }
}

// The coefficients back to the block, row by row, centred on 0.
void inverse_dct(const int32_t in[64], int32_t out[64]) {
  int32_t t[64];  // the columns transformed, times 2^3
  for (int y = 0; y < 8; ++y)
    for (int u = 0; u < 8; ++u) {
      int64_t s = 0;
      for (int v = 0; v < 8; ++v) s += int64_t{kDct[v][y]} * in[v * 8 + u];
      t[y * 8 + u] = round_shift(s, 10);
    }
  for (int y = 0; y < 8; ++y)
    for (int x = 0; x < 8; ++x) {
      int64_t s = 0;
      for (int u = 0; u < 8; ++u) s += int64_t{kDct[u][x]} * t[y * 8 + u];
      out[y * 8 + x] = round_shift(s, 16);
    }
}

// The standard table scaled to `quality` (1..100), row by row.
std::array<uint8_t, 64> scaled_table(const uint8_t base[64], int quality) {
  const int scale = quality < 50 ? 5000 / quality : 200 - quality * 2;
  std::array<uint8_t, 64> q{};
  for (int i = 0; i < 64; ++i)
    q[i] = static_cast<uint8_t>(std::clamp((base[i] * scale + 50) / 100, 1, 255));
  return q;
}

struct Code {
  uint16_t bits = 0;
  uint8_t length = 0;
};

// The canonical codes of a table: by length, then in the symbols' order.
std::array<Code, 256> canonical(const uint8_t counts[16], const uint8_t* values) {
  std::array<Code, 256> codes{};
  uint16_t code = 0;
  int k = 0;
  for (int len = 1; len <= 16; ++len) {
    for (int i = 0; i < counts[len - 1]; ++i)
      codes[values[k++]] = {code++, static_cast<uint8_t>(len)};
    code = static_cast<uint16_t>(code << 1);
  }
  return codes;
}

class BitWriter {
 public:
  explicit BitWriter(std::vector<uint8_t>& out) : out_(out) {}
  void put(uint32_t bits, int length) {
    acc_ = (acc_ << length) | (bits & ((1u << length) - 1));
    n_ += length;
    while (n_ >= 8) {
      const auto b = static_cast<uint8_t>(acc_ >> (n_ - 8));
      out_.push_back(b);
      if (b == 0xFF) out_.push_back(0);  // a 0xFF in the data is followed by a 0 (not a marker)
      n_ -= 8;
    }
    acc_ &= (1u << n_) - 1;
  }
  void finish() {  // the last byte filled with ones
    if (n_ > 0) put(0x7F, 8 - n_);
  }

 private:
  std::vector<uint8_t>& out_;
  uint32_t acc_ = 0;
  int n_ = 0;
};

// The bits that say `v`: its length (the category) and the bits themselves, a negative as v - 1.
int category(int v) {
  int a = v < 0 ? -v : v, n = 0;
  while (a) {
    ++n;
    a >>= 1;
  }
  return n;
}

void put_value(BitWriter& w, int v, int n) {
  if (n > 0) w.put(static_cast<uint32_t>(v < 0 ? v + (1 << n) - 1 : v), n);
}

void put16(std::vector<uint8_t>& out, int v) {
  out.push_back(static_cast<uint8_t>(v >> 8));
  out.push_back(static_cast<uint8_t>(v));
}

void segment(std::vector<uint8_t>& out, uint8_t marker, const std::vector<uint8_t>& body) {
  out.push_back(0xFF);
  out.push_back(marker);
  put16(out, static_cast<int>(body.size()) + 2);
  out.insert(out.end(), body.begin(), body.end());
}

void huffman_table(std::vector<uint8_t>& body, uint8_t cls_id, const uint8_t counts[16],
                   const uint8_t* values) {
  body.push_back(cls_id);
  int n = 0;
  for (int i = 0; i < 16; ++i) {
    body.push_back(counts[i]);
    n += counts[i];
  }
  body.insert(body.end(), values, values + n);
}

struct Coder {
  std::array<Code, 256> dc, ac;
  std::array<uint8_t, 64> q;
  int last_dc = 0;
};

void code_block(BitWriter& w, Coder& c, const int32_t in[64]) {
  int32_t f[64];
  forward_dct(in, f);
  int z[64];
  for (int i = 0; i < 64; ++i) {
    const int32_t v = f[kZigzag[i]], q = c.q[kZigzag[i]];
    z[i] = v >= 0 ? (v + q / 2) / q : -((-v + q / 2) / q);
  }
  const int diff = z[0] - c.last_dc;
  c.last_dc = z[0];
  const int dn = category(diff);
  w.put(c.dc[dn].bits, c.dc[dn].length);
  put_value(w, diff, dn);
  int run = 0;
  for (int i = 1; i < 64; ++i) {
    if (z[i] == 0) {
      ++run;
      continue;
    }
    for (; run >= 16; run -= 16) w.put(c.ac[0xF0].bits, c.ac[0xF0].length);
    const int n = category(z[i]);
    const int sym = (run << 4) | n;
    w.put(c.ac[sym].bits, c.ac[sym].length);
    put_value(w, z[i], n);
    run = 0;
  }
  if (run > 0) w.put(c.ac[0].bits, c.ac[0].length);
}

}  // namespace

std::vector<uint8_t> encode_jpeg(int width, int height, std::span<const uint8_t> rgba, int quality,
                                 bool subsample) {
  quality = std::clamp(quality, 1, 100);
  const int mcu = subsample ? 16 : 8;
  const int pw = (width + mcu - 1) / mcu * mcu, ph = (height + mcu - 1) / mcu * mcu;
  // The planes at the padded size, the last row and column repeated: JFIF's full-range YCbCr.
  std::vector<int16_t> yp(static_cast<size_t>(pw) * ph), cbp(yp.size()), crp(yp.size());
  for (int y = 0; y < ph; ++y)
    for (int x = 0; x < pw; ++x) {
      const uint8_t* p =
          &rgba[(static_cast<size_t>(std::min(y, height - 1)) * width + std::min(x, width - 1)) *
                4];
      const int r = p[0], g = p[1], b = p[2];
      const size_t i = static_cast<size_t>(y) * pw + x;
      yp[i] = static_cast<int16_t>((19595 * r + 38470 * g + 7471 * b + 32768) >> 16);
      cbp[i] =
          static_cast<int16_t>((-11059 * r - 21709 * g + 32768 * b + (128 << 16) + 32767) >> 16);
      crp[i] = static_cast<int16_t>((32768 * r - 27439 * g - 5329 * b + (128 << 16) + 32767) >> 16);
    }

  std::vector<uint8_t> out{0xFF, 0xD8};
  segment(out, 0xE0, {'J', 'F', 'I', 'F', 0, 1, 1, 0, 0, 1, 0, 1, 0, 0});
  const std::array<uint8_t, 64> lq = scaled_table(kLumaQ, quality),
                                cq = scaled_table(kChromaQ, quality);
  std::vector<uint8_t> dqt;
  for (int t = 0; t < 2; ++t) {
    dqt.push_back(static_cast<uint8_t>(t));
    for (int i = 0; i < 64; ++i) dqt.push_back((t == 0 ? lq : cq)[kZigzag[i]]);
  }
  segment(out, 0xDB, dqt);
  const uint8_t luma_sampling = subsample ? 0x22 : 0x11;
  segment(out, 0xC0,
          {8, static_cast<uint8_t>(height >> 8), static_cast<uint8_t>(height),
           static_cast<uint8_t>(width >> 8), static_cast<uint8_t>(width), 3, 1, luma_sampling, 0, 2,
           0x11, 1, 3, 0x11, 1});
  std::vector<uint8_t> dht;
  huffman_table(dht, 0x00, kDcLumaBits, kDcValues);
  huffman_table(dht, 0x10, kAcLumaBits, kAcLumaValues);
  huffman_table(dht, 0x01, kDcChromaBits, kDcValues);
  huffman_table(dht, 0x11, kAcChromaBits, kAcChromaValues);
  segment(out, 0xC4, dht);
  segment(out, 0xDA, {3, 1, 0x00, 2, 0x11, 3, 0x11, 0, 63, 0});

  Coder luma{canonical(kDcLumaBits, kDcValues), canonical(kAcLumaBits, kAcLumaValues), lq};
  Coder cb{canonical(kDcChromaBits, kDcValues), canonical(kAcChromaBits, kAcChromaValues), cq};
  Coder cr = cb;
  BitWriter w(out);
  int32_t block[64];
  const auto take = [&](const std::vector<int16_t>& plane, int bx, int by) {
    for (int y = 0; y < 8; ++y)
      for (int x = 0; x < 8; ++x)
        block[y * 8 + x] = plane[static_cast<size_t>(by + y) * pw + bx + x] - 128;
  };
  // A chroma block over a 16x16 MCU: the mean of each 2x2.
  const auto take_half = [&](const std::vector<int16_t>& plane, int bx, int by) {
    for (int y = 0; y < 8; ++y)
      for (int x = 0; x < 8; ++x) {
        const size_t i = static_cast<size_t>(by + 2 * y) * pw + static_cast<size_t>(bx + 2 * x);
        block[y * 8 + x] =
            ((plane[i] + plane[i + 1] + plane[i + pw] + plane[i + pw + 1] + 2) >> 2) - 128;
      }
  };
  for (int my = 0; my < ph; my += mcu)
    for (int mx = 0; mx < pw; mx += mcu) {
      if (subsample) {
        for (int k = 0; k < 4; ++k) {
          take(yp, mx + (k & 1) * 8, my + (k >> 1) * 8);
          code_block(w, luma, block);
        }
        take_half(cbp, mx, my);
        code_block(w, cb, block);
        take_half(crp, mx, my);
        code_block(w, cr, block);
      } else {
        take(yp, mx, my);
        code_block(w, luma, block);
        take(cbp, mx, my);
        code_block(w, cb, block);
        take(crp, mx, my);
        code_block(w, cr, block);
      }
    }
  w.finish();
  out.push_back(0xFF);
  out.push_back(0xD9);
  return out;
}

namespace {

struct Table {                // a Huffman table for decoding: the codes by length
  int32_t max_code[18] = {};  // the largest code of each length, -1 when none
  int32_t offset[17] = {};    // the first symbol of each length, less its first code
  uint8_t values[256] = {};
  bool set = false;
};

class BitReader {
 public:
  BitReader(std::span<const uint8_t> b, size_t at) : b_(b), at_(at) {}
  int bit() {
    if (n_ == 0) {
      uint8_t v = 0;
      if (at_ < b_.size()) {
        v = b_[at_];
        if (v == 0xFF) {
          const uint8_t next = at_ + 1 < b_.size() ? b_[at_ + 1] : 0;
          if (next == 0) {
            at_ += 2;
          } else {
            v = 0;  // a marker: the data has ended, read zeros
            ended_ = true;
          }
        } else {
          ++at_;
        }
      } else {
        ended_ = true;
      }
      byte_ = v;
      n_ = 8;
    }
    --n_;
    return (byte_ >> n_) & 1;
  }
  int bits(int n) {
    int v = 0;
    for (int i = 0; i < n; ++i) v = (v << 1) | bit();
    return v;
  }
  bool ended() const { return ended_; }

 private:
  std::span<const uint8_t> b_;
  size_t at_;
  uint8_t byte_ = 0;
  int n_ = 0;
  bool ended_ = false;
};

int decode_symbol(BitReader& r, const Table& t) {
  int32_t code = 0;
  for (int len = 1; len <= 16; ++len) {
    code = (code << 1) | r.bit();
    if (code <= t.max_code[len]) return t.values[t.offset[len] + code];
  }
  return -1;
}

int extend(int v, int n) { return n == 0 ? 0 : v < (1 << (n - 1)) ? v - (1 << n) + 1 : v; }

struct Component {
  int id = 0, h = 1, v = 1, tq = 0, td = 0, ta = 0;
  int pred = 0;
  std::vector<uint8_t> plane;  // the component's samples at its own size, padded to its blocks
  int stride = 0;
};

}  // namespace

std::optional<lab::Picture> decode_jpeg(std::span<const uint8_t> b, std::string& error) {
  if (b.size() < 4 || b[0] != 0xFF || b[1] != 0xD8) {
    error = "not a JPEG (no start-of-image marker)";
    return std::nullopt;
  }
  int qt[4][64] = {};
  Table dc[4], ac[4];
  std::vector<Component> comps;
  int width = 0, height = 0;
  size_t at = 2;
  while (at + 4 <= b.size()) {
    if (b[at] != 0xFF) {
      error = "a marker was expected at byte " + std::to_string(at);
      return std::nullopt;
    }
    const uint8_t m = b[at + 1];
    const size_t len = static_cast<size_t>(b[at + 2]) << 8 | b[at + 3];
    if (len < 2 || at + 2 + len > b.size()) {
      error = "a segment runs past the file's end at byte " + std::to_string(at);
      return std::nullopt;
    }
    const std::span<const uint8_t> s = b.subspan(at + 4, len - 2);
    if (m == 0xDB) {
      for (size_t i = 0; i + 65 <= s.size(); i += 65) {
        if (s[i] >> 4 != 0) {
          error = "a 16-bit quantization table (only 8-bit is read)";
          return std::nullopt;
        }
        for (int k = 0; k < 64; ++k) qt[s[i] & 3][kZigzag[k]] = s[i + 1 + k];
      }
    } else if (m == 0xC4) {
      for (size_t i = 0; i + 17 <= s.size();) {
        Table& t = (s[i] >> 4) ? ac[s[i] & 3] : dc[s[i] & 3];
        int code = 0, k = 0;
        for (int bits = 1; bits <= 16; ++bits) {
          const int n = s[i + bits];
          t.offset[bits] = k - code;
          t.max_code[bits] = n ? code + n - 1 : -1;
          code = (code + n) << 1;
          k += n;
        }
        if (k > 256 || i + 17 + k > s.size()) {
          error = "a Huffman table runs past its segment";
          return std::nullopt;
        }
        std::copy_n(&s[i + 17], k, t.values);
        t.set = true;
        i += 17 + static_cast<size_t>(k);
      }
    } else if (m == 0xC0 || m == 0xC1) {
      if (s.size() < 6 || s[0] != 8) {
        error = "not an 8-bit image";
        return std::nullopt;
      }
      height = s[1] << 8 | s[2];
      width = s[3] << 8 | s[4];
      const int n = s[5];
      if (n < 1 || n > 3 || s.size() < 6 + 3u * n || width == 0 || height == 0) {
        error = "the frame header names " + std::to_string(n) + " components, 1..3 are read";
        return std::nullopt;
      }
      for (int i = 0; i < n; ++i) {
        Component c;
        c.id = s[6 + 3 * i];
        c.h = std::max(1, s[7 + 3 * i] >> 4);
        c.v = std::max(1, s[7 + 3 * i] & 15);
        c.tq = s[8 + 3 * i] & 3;
        comps.push_back(std::move(c));
      }
    } else if ((m >= 0xC2 && m <= 0xCF && m != 0xC4 && m != 0xC8 && m != 0xCC)) {
      error = "a progressive, lossless or arithmetic JPEG (only baseline is read)";
      return std::nullopt;
    } else if (m == 0xDD) {
      if (s.size() >= 2 && (s[0] << 8 | s[1]) != 0) {
        error = "restart markers (not read)";
        return std::nullopt;
      }
    } else if (m == 0xDA) {
      if (comps.empty() || s.empty() || s[0] != comps.size()) {
        error = "a scan before its frame, or a scan of some components only";
        return std::nullopt;
      }
      for (size_t i = 0; i < comps.size(); ++i) {
        for (Component& c : comps)
          if (c.id == s[1 + 2 * i]) {
            c.td = s[2 + 2 * i] >> 4;
            c.ta = s[2 + 2 * i] & 3;
          }
      }
      int hmax = 1, vmax = 1;
      for (const Component& c : comps) {
        hmax = std::max(hmax, c.h);
        vmax = std::max(vmax, c.v);
      }
      if (hmax > 2 || vmax > 2) {
        error = "a sampling over 2x2";
        return std::nullopt;
      }
      for (const Component& c : comps)
        if (!dc[c.td].set || !ac[c.ta].set) {
          error = "a scan names a Huffman table the file did not define";
          return std::nullopt;
        }
      const int mw = 8 * hmax, mh = 8 * vmax;
      const int mcus_x = (width + mw - 1) / mw, mcus_y = (height + mh - 1) / mh;
      for (Component& c : comps) {
        c.stride = mcus_x * c.h * 8;
        c.plane.assign(static_cast<size_t>(c.stride) * mcus_y * c.v * 8, 0);
      }
      BitReader r(b, at + 2 + len);
      int32_t coef[64], px[64];
      for (int my = 0; my < mcus_y; ++my)
        for (int mx = 0; mx < mcus_x; ++mx)
          for (Component& c : comps)
            for (int by = 0; by < c.v; ++by)
              for (int bx = 0; bx < c.h; ++bx) {
                std::fill(coef, coef + 64, 0);
                const int dn = decode_symbol(r, dc[c.td]);
                if (dn < 0 || dn > 11) {
                  error = "a damaged DC code";
                  return std::nullopt;
                }
                c.pred += extend(r.bits(dn), dn);
                coef[0] = c.pred * qt[c.tq][0];
                for (int k = 1; k < 64;) {
                  const int sym = decode_symbol(r, ac[c.ta]);
                  if (sym < 0) {
                    error = "a damaged AC code";
                    return std::nullopt;
                  }
                  if (sym == 0) break;
                  k += sym >> 4;
                  const int n = sym & 15;
                  if (k > 63) {
                    error = "a run past the block's end";
                    return std::nullopt;
                  }
                  coef[kZigzag[k]] = extend(r.bits(n), n) * qt[c.tq][kZigzag[k]];
                  ++k;
                }
                inverse_dct(coef, px);
                const int x0 = (mx * c.h + bx) * 8, y0 = (my * c.v + by) * 8;
                for (int y = 0; y < 8; ++y)
                  for (int x = 0; x < 8; ++x)
                    c.plane[static_cast<size_t>(y0 + y) * c.stride + x0 + x] =
                        static_cast<uint8_t>(std::clamp(px[y * 8 + x] + 128, 0, 255));
              }
      if (r.ended()) {
        error = "the image data ended early";
        return std::nullopt;
      }
      lab::Picture p;
      p.width = width;
      p.height = height;
      p.rgba.resize(static_cast<size_t>(width) * height * 4);
      const auto sample = [&](const Component& c, int x, int y) {
        return int{c.plane[static_cast<size_t>(y * c.v / vmax) * c.stride + x * c.h / hmax]};
      };
      for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x) {
          uint8_t* o = &p.rgba[(static_cast<size_t>(y) * width + x) * 4];
          const int Y = sample(comps[0], x, y);
          if (comps.size() < 3) {
            o[0] = o[1] = o[2] = static_cast<uint8_t>(Y);
          } else {
            const int cb = sample(comps[1], x, y) - 128, cr = sample(comps[2], x, y) - 128;
            o[0] = static_cast<uint8_t>(std::clamp(Y + ((91881 * cr + 32768) >> 16), 0, 255));
            o[1] = static_cast<uint8_t>(
                std::clamp(Y - ((22554 * cb + 46802 * cr + 32768) >> 16), 0, 255));
            o[2] = static_cast<uint8_t>(std::clamp(Y + ((116130 * cb + 32768) >> 16), 0, 255));
          }
          o[3] = 255;
        }
      return p;
    }
    at += 2 + len;
  }
  error = "no image data (no start-of-scan before the file's end)";
  return std::nullopt;
}

}  // namespace ge::capture
