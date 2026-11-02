// Stills by format (engine/capture/still.hpp): the words, the fallbacks this build and machine
// take, and a frame packed as each format and read back by `decode_still` close to what went in
// (PNG exactly), smaller than the PNG for every lossy one; an encoder this machine lacks falls back
// and says so rather than failing.
//
// ge:summary Tests: stills as PNG, JPEG, AVIF and HEIC, read back, smaller, falling back
// ge:keywords test still png jpg avif heic fallback decode_still encode_still psnr size
// ge:skill replay testing
// ge:test tests/capture_still_test.cpp
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include "capture/codecs.hpp"
#include "capture/still.hpp"
#include "check.hpp"

namespace gc = ge::capture;

namespace {

// A frame-like picture: gradients with a rendered frame's fine grain (a flat gradient is a PNG's
// best case and no game's frame).
std::vector<uint8_t> frame(int w, int h) {
  std::vector<uint8_t> rgba(static_cast<size_t>(w) * h * 4);
  uint32_t s = 11;
  for (int y = 0; y < h; ++y)
    for (int x = 0; x < w; ++x) {
      uint8_t* p = &rgba[(static_cast<size_t>(y) * w + x) * 4];
      s = s * 1664525u + 1013904223u;
      const int n = static_cast<int>(s >> 29) - 4;
      p[0] = static_cast<uint8_t>(60 + x * 150 / w + n);
      p[1] = static_cast<uint8_t>(90 + y * 120 / h + n);
      p[2] = static_cast<uint8_t>((y < h / 2 ? 220 : 120 + (x * y) % 40) + n);
      p[3] = 255;
    }
  return rgba;
}

double psnr(const std::vector<uint8_t>& a, int aw, const std::vector<uint8_t>& b, int bw, int bh) {
  double se = 0;
  size_t n = 0;
  for (int y = 0; y < bh; ++y)
    for (int x = 0; x < bw; ++x)
      for (int c = 0; c < 3; ++c) {
        const double d = static_cast<double>(a[(static_cast<size_t>(y) * aw + x) * 4 + c]) -
                         b[(static_cast<size_t>(y) * bw + x) * 4 + c];
        se += d * d;
        ++n;
      }
  return se == 0 ? 100 : 10 * std::log10(255.0 * 255.0 / (se / static_cast<double>(n)));
}

}  // namespace

TEST(the_words_and_the_fallbacks) {
  gc::StillFormat f{};
  CHECK(gc::still_format_of("jpeg", f) && f == gc::StillFormat::kJpg);
  CHECK(gc::still_format_of("avif", f) && gc::still_extension(f) == "avif");
  CHECK(!gc::still_format_of("bmp", f));
  CHECK(gc::still_writable(gc::StillFormat::kPng) == gc::StillFormat::kPng);
  CHECK(gc::still_writable(gc::StillFormat::kJpg) == gc::StillFormat::kJpg);
  CHECK(gc::still_writable(gc::StillFormat::kAvif) ==
        (gc::encoders_built() ? gc::StillFormat::kAvif : gc::StillFormat::kJpg));
  CHECK(gc::still_writable(gc::StillFormat::kHeic) != gc::StillFormat::kPng);
}

TEST(each_format_reads_back_close_and_the_lossy_ones_are_smaller) {
  const int w = 128, h = 72;
  const std::vector<uint8_t> rgba = frame(w, h);
  size_t png_size = 0;
  for (const gc::StillFormat asked : {gc::StillFormat::kPng, gc::StillFormat::kJpg,
                                      gc::StillFormat::kAvif, gc::StillFormat::kHeic}) {
    std::vector<uint8_t> bytes;
    gc::StillFormat used{};
    std::string note, error;
    CHECK(gc::encode_still({asked, 90, false}, w, h, rgba, bytes, used, note));
    CHECK_MSG(used == asked, std::string(gc::still_extension(asked)) + ": " + note);
    const auto back = gc::decode_still(bytes, error);
    CHECK_MSG(back.has_value(), std::string(gc::still_extension(used)) + ": " + error);
    if (!back) continue;
    CHECK(back->width == w && back->height == h);
    const double p = psnr(rgba, w, back->rgba, back->width, back->height);
    if (used == gc::StillFormat::kPng) {
      png_size = bytes.size();
      CHECK(p == 100);
    } else {
      CHECK_MSG(p > 34, std::string(gc::still_extension(used)) + " psnr " + std::to_string(p));
      CHECK_MSG(bytes.size() < png_size, std::string(gc::still_extension(used)) + " not smaller");
    }
  }
}

TEST(an_odd_size_loses_its_last_row_and_column_only_in_heif) {
  const std::vector<uint8_t> rgba = frame(33, 17);
  std::vector<uint8_t> bytes;
  gc::StillFormat used{};
  std::string note, error;
  CHECK(gc::encode_still({gc::StillFormat::kAvif, 80, true}, 33, 17, rgba, bytes, used, note));
  const auto back = gc::decode_still(bytes, error);
  CHECK_MSG(back.has_value(), error);
  if (!back) return;
  if (used == gc::StillFormat::kAvif)
    CHECK(back->width == 32 && back->height == 16);
  else
    CHECK(back->width == 33 && back->height == 17);
}
