// The TGA reader and writer (engine/assets/tga.hpp): a 32-bit picture with alpha comes back
// exactly through both written forms; a bottom-up 24-bit file is turned the right way up and
// opaque; the 16-bit, grey, colour-mapped and packed forms read; and a damaged or unknown file
// gives nothing with a reason that says what to do.
//
// ge:summary Tests the TGA reader and writer: alpha kept, row order, every form, refusals
// ge:keywords test assets tga decode_tga encode_tga alpha rle colour-map grey
// ge:skill assets testing
#include <string>
#include <vector>

#include "assets/tga.hpp"
#include "check.hpp"

namespace as = ge::assets;

namespace {

as::Image sample() {
  as::Image img;
  img.width = 5;
  img.height = 3;
  for (int y = 0; y < 3; ++y)
    for (int x = 0; x < 5; ++x) {
      const bool run = y == 1;  // a row of one colour, for the packed form's runs
      img.rgba.push_back(run ? 200 : uint8_t(x * 40));
      img.rgba.push_back(run ? 10 : uint8_t(y * 60));
      img.rgba.push_back(run ? 30 : uint8_t(x + y));
      img.rgba.push_back(run ? 128 : uint8_t(x == 2 ? 0 : 255));  // see-through texels kept
    }
  return img;
}

std::vector<uint8_t> header(uint8_t type, uint16_t w, uint16_t h, uint8_t depth, uint8_t desc) {
  std::vector<uint8_t> b(18, 0);
  b[2] = type;
  b[12] = uint8_t(w), b[13] = uint8_t(w >> 8), b[14] = uint8_t(h), b[15] = uint8_t(h >> 8);
  b[16] = depth;
  b[17] = desc;
  return b;
}

}  // namespace

TEST(a_32_bit_picture_with_alpha_comes_back_exactly) {
  const as::Image img = sample();
  for (bool packed : {false, true}) {
    const std::vector<uint8_t> file = as::encode_tga(img, packed);
    std::string why;
    const auto back = as::decode_tga(file, &why);
    CHECK_MSG(back.has_value(), why);
    if (!back) continue;
    CHECK(back->width == 5 && back->height == 3);
    CHECK(back->rgba == img.rgba);
  }
  CHECK(as::encode_tga(img, true).size() < as::encode_tga(img, false).size());
}

TEST(a_bottom_up_24_bit_file_is_turned_the_right_way_up) {
  std::vector<uint8_t> b = header(2, 1, 2, 24, 0);
  b.insert(b.end(), {3, 2, 1});     // the bottom row: blue 3, green 2, red 1
  b.insert(b.end(), {30, 20, 10});  // the top row
  const auto img = as::decode_tga(b);
  CHECK(img.has_value());
  if (!img) return;
  CHECK(img->rgba == (std::vector<uint8_t>{10, 20, 30, 255, 1, 2, 3, 255}));
}

TEST(the_16_bit_grey_and_colour_mapped_forms_read) {
  std::vector<uint8_t> b = header(2, 2, 1, 16, 0x20);
  b.insert(b.end(), {0x1f, 0x80, 0x00, 0x7c});  // blue with its alpha bit; red without
  auto img = as::decode_tga(b);
  CHECK(img && img->rgba == (std::vector<uint8_t>{0, 0, 255, 255, 255, 0, 0, 0}));

  b = header(11, 3, 1, 8, 0x20);  // grey, packed: a run of 3
  b.insert(b.end(), {0x82, 77});
  img = as::decode_tga(b);
  CHECK(img &&
        img->rgba == (std::vector<uint8_t>{77, 77, 77, 255, 77, 77, 77, 255, 77, 77, 77, 255}));

  b = header(1, 2, 1, 8, 0x20);
  b[1] = 1;            // a colour map
  b[3] = 0, b[4] = 0;  // from entry 0
  b[5] = 2, b[6] = 0;  // two entries
  b[7] = 24;
  b.insert(b.end(), {0, 0, 255, 255, 0, 0});  // red, blue (stored BGR)
  b.insert(b.end(), {1, 0});
  img = as::decode_tga(b);
  CHECK(img && img->rgba == (std::vector<uint8_t>{0, 0, 255, 255, 255, 0, 0, 255}));
  b.back() = 7;  // an index past the map
  std::string why;
  CHECK(!as::decode_tga(b, &why));
  CHECK_MSG(why.find("past the map") != std::string::npos, why);
}

TEST(a_damaged_or_unknown_file_gives_nothing_with_a_reason) {
  std::string why;
  CHECK(!as::decode_tga(std::vector<uint8_t>{1, 2, 3}, &why));
  CHECK_MSG(
      why.find("18-byte header") != std::string::npos && why.find("next:") != std::string::npos,
      why);
  CHECK(!as::decode_tga(header(4, 1, 1, 24, 0), &why));
  CHECK_MSG(why.find("image type 4") != std::string::npos, why);
  CHECK(!as::decode_tga(header(2, 1, 1, 12, 0), &why));
  std::vector<uint8_t> short_px = header(2, 2, 2, 32, 0);
  short_px.insert(short_px.end(), {1, 2, 3, 4});
  CHECK(!as::decode_tga(short_px, &why));
  CHECK_MSG(why.find("run out") != std::string::npos, why);
  std::vector<uint8_t> long_run = header(10, 2, 1, 24, 0);
  long_run.insert(long_run.end(), {0x85, 1, 2, 3});  // a run of 6 in a picture of 2
  CHECK(!as::decode_tga(long_run, &why));
  CHECK_MSG(why.find("passes the picture's end") != std::string::npos, why);
}
