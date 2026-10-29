// The PCX reader and writer (engine/assets/pcx.hpp): an image round-trips with its indices and
// palette, padding and runs longer than a line read right, a run crossing into the next line is
// decoded as one stream, bytes at 0xC0 and above survive as runs of one, and damaged files are
// refused with the byte and the next step.
//
// ge:summary Tests for the PCX reader and writer: round trips, runs, padding, refusals
// ge:keywords test pcx decode_pcx encode_pcx rle palette skin padding refusal
// ge:skill assets testing
#include <string>
#include <vector>

#include "assets/pcx.hpp"
#include "check.hpp"

namespace {

using namespace ge::assets;

IndexedImage pattern(int w, int h) {
  IndexedImage img;
  img.width = w;
  img.height = h;
  img.indices.resize(static_cast<size_t>(w) * static_cast<size_t>(h));
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      // Long flat runs, then every value 0..255 (the top two bits set included).
      const int v = x < w / 2 ? (y / 3) * 16 : (x * 7 + y * 13) & 255;
      img.indices[static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x)] =
          static_cast<uint8_t>(v);
    }
  }
  img.palette.resize(768);
  for (size_t i = 0; i < 768; ++i) img.palette[i] = static_cast<uint8_t>(i * 5 % 256);
  return img;
}

}  // namespace

TEST(an_image_round_trips_with_its_indices_and_palette) {
  for (const int w : {1, 7, 64, 129, 296}) {
    const IndexedImage a = pattern(w, 37);
    const std::vector<uint8_t> file = encode_pcx(a);
    IndexedImage b;
    std::string err;
    CHECK_MSG(decode_pcx(file, "test.pcx", &b, &err), err);
    CHECK(b.width == w && b.height == 37);
    CHECK_MSG(b.indices == a.indices, "indices differ at width " + std::to_string(w));
    CHECK(b.palette == a.palette);
  }
}

TEST(a_run_may_cross_into_the_next_line) {
  // A 4 x 2 image, 4 bytes a line, written as one run of 8.
  std::vector<uint8_t> file = encode_pcx(pattern(4, 2));
  file.resize(128);
  file.push_back(0xC8);  // a run of 8
  file.push_back(42);
  IndexedImage img;
  std::string err;
  CHECK_MSG(decode_pcx(file, "cross.pcx", &img, &err), err);
  for (const uint8_t v : img.indices) CHECK(v == 42);
  CHECK(img.palette.empty());
}

TEST(the_colours_come_through_the_palette) {
  IndexedImage img = pattern(8, 2);
  const std::vector<uint8_t> rgba = pcx_rgba(img);
  const size_t i = 5;  // a pixel
  const size_t c = size_t{img.indices[i]} * 3;
  CHECK(rgba[i * 4] == img.palette[c] && rgba[i * 4 + 2] == img.palette[c + 2]);
  CHECK(rgba[i * 4 + 3] == 255);
  std::vector<uint8_t> grey(768);
  for (size_t k = 0; k < 768; ++k) grey[k] = static_cast<uint8_t>(k / 3);
  const std::vector<uint8_t> other = pcx_rgba(img, grey);
  CHECK(other[i * 4 + 1] == img.indices[i]);  // a game's own palette wins
}

TEST(damaged_files_are_refused_with_the_byte_and_the_next_step) {
  const std::vector<uint8_t> good = encode_pcx(pattern(16, 16));
  IndexedImage img;
  std::string err;
  CHECK(
      !decode_pcx(std::vector<uint8_t>(good.begin(), good.begin() + 60), "short.pcx", &img, &err));
  CHECK_MSG(err.find("short.pcx:") == 0 && err.find("at byte 60") != std::string::npos &&
                err.find("next:") != std::string::npos,
            err);
  std::vector<uint8_t> bad = good;
  bad[0] = 'P';
  CHECK(!decode_pcx(bad, "png.pcx", &img, &err) && err.find("not a PCX") != std::string::npos);
  bad = good;
  bad[3] = 4;
  CHECK(!decode_pcx(bad, "four.pcx", &img, &err) && err.find("4 bits") != std::string::npos);
  bad = std::vector<uint8_t>(good.begin(), good.begin() + 140);
  CHECK(!decode_pcx(bad, "cut.pcx", &img, &err) && err.find("the pixels end") != std::string::npos);
}
