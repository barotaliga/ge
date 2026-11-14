// QOI's ops, each way.
//
// ge:summary QOI's encoder and decoder over 8-bit RGBA
// ge:keywords qoi encode decode run index diff luma rgba
// ge:skill save
// ge:test tests/save_picture_test.cpp
#include "save/qoi.hpp"

#include <array>

namespace ge::save {

namespace {

constexpr uint8_t kOpIndex = 0x00, kOpDiff = 0x40, kOpLuma = 0x80, kOpRun = 0xc0;
constexpr uint8_t kOpRgb = 0xfe, kOpRgba = 0xff, kMask = 0xc0;
constexpr size_t kHeader = 14;
constexpr std::array<uint8_t, 8> kEnd{0, 0, 0, 0, 0, 0, 0, 1};
// Pictures larger than this many pixels are refused (a save's picture is a few hundred wide).
constexpr uint64_t kMostPixels = 1ull << 26;

struct Px {
  uint8_t r = 0, g = 0, b = 0, a = 255;
  bool operator==(const Px&) const = default;
};

int hash(const Px& p) { return (p.r * 3 + p.g * 5 + p.b * 7 + p.a * 11) % 64; }

void put32(std::vector<uint8_t>& out, uint32_t v) {
  for (int s = 24; s >= 0; s -= 8) out.push_back(static_cast<uint8_t>(v >> s));
}

uint32_t get32(const uint8_t* p) {
  return (uint32_t{p[0]} << 24) | (uint32_t{p[1]} << 16) | (uint32_t{p[2]} << 8) | p[3];
}

}  // namespace

void qoi_encode(int width, int height, std::span<const uint8_t> rgba, std::vector<uint8_t>& out) {
  out.clear();
  const size_t n = static_cast<size_t>(width) * static_cast<size_t>(height);
  out.reserve(kHeader + n + kEnd.size());
  out.insert(out.end(), {'q', 'o', 'i', 'f'});
  put32(out, static_cast<uint32_t>(width));
  put32(out, static_cast<uint32_t>(height));
  out.push_back(4);
  out.push_back(0);
  std::array<Px, 64> seen;
  seen.fill(Px{0, 0, 0, 0});  // the table starts zeroed, alpha too
  Px prev;
  int run = 0;
  for (size_t i = 0; i < n; i++) {
    const Px px{rgba[i * 4], rgba[i * 4 + 1], rgba[i * 4 + 2], rgba[i * 4 + 3]};
    if (px == prev) {
      if (++run == 62 || i + 1 == n) {
        out.push_back(static_cast<uint8_t>(kOpRun | (run - 1)));
        run = 0;
      }
      continue;
    }
    if (run > 0) {
      out.push_back(static_cast<uint8_t>(kOpRun | (run - 1)));
      run = 0;
    }
    const int h = hash(px);
    if (seen[static_cast<size_t>(h)] == px) {
      out.push_back(static_cast<uint8_t>(kOpIndex | h));
    } else {
      seen[static_cast<size_t>(h)] = px;
      if (px.a == prev.a) {
        const auto dr = static_cast<int8_t>(px.r - prev.r);
        const auto dg = static_cast<int8_t>(px.g - prev.g);
        const auto db = static_cast<int8_t>(px.b - prev.b);
        const int dr_g = dr - dg, db_g = db - dg;
        if (dr > -3 && dr < 2 && dg > -3 && dg < 2 && db > -3 && db < 2) {
          out.push_back(static_cast<uint8_t>(kOpDiff | (dr + 2) << 4 | (dg + 2) << 2 | (db + 2)));
        } else if (dg > -33 && dg < 32 && dr_g > -9 && dr_g < 8 && db_g > -9 && db_g < 8) {
          out.push_back(static_cast<uint8_t>(kOpLuma | (dg + 32)));
          out.push_back(static_cast<uint8_t>((dr_g + 8) << 4 | (db_g + 8)));
        } else {
          out.insert(out.end(), {kOpRgb, px.r, px.g, px.b});
        }
      } else {
        out.insert(out.end(), {kOpRgba, px.r, px.g, px.b, px.a});
      }
    }
    prev = px;
  }
  out.insert(out.end(), kEnd.begin(), kEnd.end());
}

bool qoi_decode(std::span<const uint8_t> qoi, int& width, int& height, std::vector<uint8_t>& rgba,
                std::string& problem) {
  if (qoi.size() < kHeader + kEnd.size() || qoi[0] != 'q' || qoi[1] != 'o' || qoi[2] != 'i' ||
      qoi[3] != 'f') {
    problem = "the picture is not a QOI file (no qoif header)";
    return false;
  }
  const uint32_t w = get32(&qoi[4]), h = get32(&qoi[8]);
  if (w == 0 || h == 0 || uint64_t{w} * h > kMostPixels || (qoi[12] != 3 && qoi[12] != 4)) {
    problem = "the picture's header says " + std::to_string(w) + "x" + std::to_string(h) +
              " with " + std::to_string(qoi[12]) + " channels, which no save's picture is";
    return false;
  }
  width = static_cast<int>(w);
  height = static_cast<int>(h);
  const size_t n = size_t{w} * h;
  rgba.resize(n * 4);
  std::array<Px, 64> seen;
  seen.fill(Px{0, 0, 0, 0});  // the table starts zeroed, alpha too
  Px px;
  size_t at = kHeader;
  const size_t end = qoi.size() - kEnd.size();
  int run = 0;
  for (size_t i = 0; i < n; i++) {
    if (run > 0) {
      run--;
    } else {
      if (at >= end) {
        problem = "the picture's pixels stop at " + std::to_string(i) + " of " + std::to_string(n);
        return false;
      }
      const uint8_t op = qoi[at++];
      if (op == kOpRgb || op == kOpRgba) {
        const size_t need = op == kOpRgb ? 3 : 4;
        if (at + need > end) {
          problem = "the picture ends inside a pixel";
          return false;
        }
        px.r = qoi[at];
        px.g = qoi[at + 1];
        px.b = qoi[at + 2];
        if (op == kOpRgba) px.a = qoi[at + 3];
        at += need;
      } else if ((op & kMask) == kOpIndex) {
        px = seen[op];
      } else if ((op & kMask) == kOpDiff) {
        px.r = static_cast<uint8_t>(px.r + ((op >> 4) & 3) - 2);
        px.g = static_cast<uint8_t>(px.g + ((op >> 2) & 3) - 2);
        px.b = static_cast<uint8_t>(px.b + (op & 3) - 2);
      } else if ((op & kMask) == kOpLuma) {
        if (at >= end) {
          problem = "the picture ends inside a pixel";
          return false;
        }
        const uint8_t b2 = qoi[at++];
        const int dg = (op & 0x3f) - 32;
        px.r = static_cast<uint8_t>(px.r + dg - 8 + ((b2 >> 4) & 0x0f));
        px.g = static_cast<uint8_t>(px.g + dg);
        px.b = static_cast<uint8_t>(px.b + dg - 8 + (b2 & 0x0f));
      } else {
        run = op & 0x3f;
      }
      seen[static_cast<size_t>(hash(px))] = px;
    }
    rgba[i * 4] = px.r;
    rgba[i * 4 + 1] = px.g;
    rgba[i * 4 + 2] = px.b;
    rgba[i * 4 + 3] = px.a;
  }
  for (size_t k = 0; k < kEnd.size(); k++)
    if (qoi[end + k] != kEnd[k]) {
      problem = "the picture's end marker is missing";
      return false;
    }
  return true;
}

}  // namespace ge::save
