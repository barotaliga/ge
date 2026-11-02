// The engine's JPEG (engine/capture/jpeg.hpp): a frame read back by the decoder close to what went
// in at every quality and both chroma forms, smaller at a lower quality, an odd size padded and
// cut back, the same picture packed to the same bytes on every platform (the bytes' hash pinned),
// and files the decoder does not take refused with the reason.
//
// ge:summary Tests: the JPEG encoder and decoder: quality, chroma, odd sizes, pinned bytes,
// ge:summary refusals
// ge:keywords test jpeg jpg quality chroma 444 420 psnr pinned-bytes decode refuse progressive
// ge:skill replay testing
// ge:test tests/capture_jpeg_test.cpp
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include "capture/jpeg.hpp"
#include "check.hpp"

namespace gc = ge::capture;

namespace {

// A frame-like picture: a gradient, bands of colour, thin lines and a dither.
std::vector<uint8_t> frame(int w, int h) {
  std::vector<uint8_t> rgba(static_cast<size_t>(w) * h * 4);
  uint32_t s = 7;
  for (int y = 0; y < h; ++y)
    for (int x = 0; x < w; ++x) {
      uint8_t* p = &rgba[(static_cast<size_t>(y) * w + x) * 4];
      s = s * 1664525u + 1013904223u;
      const int n = static_cast<int>(s >> 29) - 4;
      const bool line = x % 23 == 0 || y % 17 == 0;
      p[0] = static_cast<uint8_t>(line ? 250 : 40 + x * 180 / w + n);
      p[1] = static_cast<uint8_t>(line ? 210 : 60 + y * 150 / h + n);
      p[2] = static_cast<uint8_t>(line ? 40 : (x / 16 % 2 ? 200 : 90) + n);
      p[3] = 255;
    }
  return rgba;
}

double psnr(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b) {
  double se = 0;
  size_t n = 0;
  for (size_t i = 0; i < a.size() && i < b.size(); ++i) {
    if (i % 4 == 3) continue;
    const double d = static_cast<double>(a[i]) - b[i];
    se += d * d;
    ++n;
  }
  return se == 0 ? 100 : 10 * std::log10(255.0 * 255.0 / (se / static_cast<double>(n)));
}

uint64_t fnv(const std::vector<uint8_t>& b) {
  uint64_t h = 0xcbf29ce484222325ull;
  for (const uint8_t v : b) h = (h ^ v) * 0x100000001b3ull;
  return h;
}

}  // namespace

TEST(a_frame_reads_back_close_at_each_quality_and_chroma) {
  const std::vector<uint8_t> rgba = frame(96, 64);
  for (const bool sub : {false, true}) {
    size_t last = 0;
    double last_psnr = 0;
    for (const int q : {50, 75, 90, 100}) {
      const std::vector<uint8_t> j = gc::encode_jpeg(96, 64, rgba, q, sub);
      std::string error;
      const auto back = gc::decode_jpeg(j, error);
      CHECK_MSG(back.has_value(), error);
      if (!back) continue;
      CHECK(back->width == 96 && back->height == 64);
      const double p = psnr(rgba, back->rgba);
      // The frame's one-pixel yellow lines are halved chroma's worst case: 4:2:0 keeps ~20 dB of
      // them whatever the quality (ffmpeg's decoder reads the same), a game's frame over 40.
      CHECK_MSG(p > (sub ? 18.0 : 26.0), "q" + std::to_string(q) + " psnr " + std::to_string(p));
      CHECK_MSG(j.size() > last && p > last_psnr,
                "q" + std::to_string(q) + " not larger and better");
      last = j.size();
      last_psnr = p;
    }
  }
}

TEST(full_chroma_keeps_coloured_lines_sharper_and_costs_bytes) {
  const std::vector<uint8_t> rgba = frame(96, 64);
  std::string error;
  const std::vector<uint8_t> full = gc::encode_jpeg(96, 64, rgba, 90, false);
  const std::vector<uint8_t> half = gc::encode_jpeg(96, 64, rgba, 90, true);
  CHECK(half.size() < full.size());
  const auto f = gc::decode_jpeg(full, error), h = gc::decode_jpeg(half, error);
  CHECK(f && h && psnr(rgba, f->rgba) > psnr(rgba, h->rgba));
}

TEST(an_odd_size_is_padded_and_cut_back) {
  const std::vector<uint8_t> rgba = frame(13, 7);
  for (const bool sub : {false, true}) {
    std::string error;
    const auto back = gc::decode_jpeg(gc::encode_jpeg(13, 7, rgba, 95, sub), error);
    CHECK_MSG(back && back->width == 13 && back->height == 7, error);
    if (back) CHECK(psnr(rgba, back->rgba) > (sub ? 16 : 30));
  }
}

TEST(the_same_picture_packs_to_the_same_bytes_everywhere) {
  // The DCT and the colour conversion are integers, so these hashes hold on every compiler and
  // CPU; a deliberate change to the encoder rewrites them and says why.
  const std::vector<uint8_t> rgba = frame(64, 48);
  const uint64_t full = fnv(gc::encode_jpeg(64, 48, rgba, 90, false));
  const uint64_t half = fnv(gc::encode_jpeg(64, 48, rgba, 75, true));
  CHECK_MSG(full == 12121397717135144061ull, "4:4:4 q90 hash " + std::to_string(full));
  CHECK_MSG(half == 9048056314399274146ull, "4:2:0 q75 hash " + std::to_string(half));
}

TEST(files_it_does_not_take_are_refused_with_the_reason) {
  std::string error;
  CHECK(!gc::decode_jpeg(std::vector<uint8_t>{1, 2, 3, 4}, error));
  CHECK(error.find("not a JPEG") != std::string::npos);
  // A progressive frame header (SOF2) where the baseline one would be.
  std::vector<uint8_t> j = gc::encode_jpeg(16, 16, frame(16, 16), 90, false);
  for (size_t i = 2; i + 1 < j.size(); ++i)
    if (j[i] == 0xFF && j[i + 1] == 0xC0) {
      j[i + 1] = 0xC2;
      break;
    }
  CHECK(!gc::decode_jpeg(j, error));
  CHECK_MSG(error.find("progressive") != std::string::npos, error);
  // Cut short inside the image data.
  std::vector<uint8_t> cut = gc::encode_jpeg(64, 48, frame(64, 48), 90, false);
  cut.resize(cut.size() / 2);
  CHECK(!gc::decode_jpeg(cut, error));
}
