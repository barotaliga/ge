// The pictures a map carries (plugins/brush/pictures.hpp): a 29 and a BSP2 map give every
// texture's four levels, each the fixture's pattern, as views into the file; a 38 or 46 map, a
// file too short and a record whose offset is out of the lump give no pixels.
//
// ge:summary Tests reading a 29 or BSP2 map's own miptex pictures, and refusing the rest
// ge:keywords test brush pictures miptex read_map_pictures carries_pictures
// ge:skill brush testing
#include <string>
#include <vector>

#include "brush/pictures.hpp"
#include "brush/read.hpp"
#include "brush/write.hpp"
#include "check.hpp"

namespace br = ge::brush;

namespace {

std::vector<uint8_t> written(br::Family f) {
  std::vector<uint8_t> bytes;
  std::string error;
  CHECK_MSG(br::write_bsp(br::sample_fixture(), f, &bytes, &error), error);
  return bytes;
}

}  // namespace

TEST(a_29_and_a_bsp2_map_give_every_texture_s_levels) {
  for (br::Family f : {br::Family::v29, br::Family::bsp2}) {
    const std::vector<uint8_t> file = written(f);
    CHECK(br::carries_pictures(file));
    const std::vector<br::MapPicture> pics = br::read_map_pictures(file);
    br::Map map;
    std::string error;
    CHECK_MSG(br::read_bsp(file, &map, &error), error);
    CHECK(!pics.empty() && pics.size() == map.textures.size());
    for (size_t k = 0; k < pics.size(); ++k) {
      const br::MapPicture& p = pics[k];
      CHECK(k < map.textures.size() && p.name == map.textures[k].name);
      CHECK(p.width == 64 && p.height == 64);
      for (int level = 0; level < 4; ++level) {
        CHECK(p.level[level] != nullptr);
        CHECK(p.level[level] >= file.data() && p.level[level] < file.data() + file.size());
      }
      // The fixture's pattern: two indices, 16 + 16 (k mod 12) and 6 more, in 8-texel cells.
      const auto a = uint8_t(16 + 16 * (k % 12)), b = uint8_t(a + 6);
      CHECK(p.level[0][0] == ((k % 2) != 0 ? a : b));
      CHECK(p.level[0][8] == ((k % 2) != 0 ? b : a));
    }
  }
}

TEST(other_families_and_damaged_files_give_no_pixels) {
  for (br::Family f : {br::Family::v38, br::Family::v46}) {
    CHECK(!br::carries_pictures(written(f)));
    CHECK(br::read_map_pictures(written(f)).empty());
  }
  CHECK(br::read_map_pictures(std::vector<uint8_t>{29, 0, 0}).empty());
  std::vector<uint8_t> file = written(br::Family::v29);
  // Point the first record's offset past the lump: that texture has no pixels, the rest do.
  const uint32_t ofs = uint32_t(file[20]) | uint32_t(file[21]) << 8 | uint32_t(file[22]) << 16 |
                       uint32_t(file[23]) << 24;
  file[ofs + 4] = 0xff;
  file[ofs + 5] = 0xff;
  file[ofs + 6] = 0xff;
  file[ofs + 7] = 0x7f;
  const std::vector<br::MapPicture> pics = br::read_map_pictures(file);
  CHECK(!pics.empty() && pics[0].width == 0 && pics[0].level[0] == nullptr);
  if (pics.size() > 1) CHECK(pics[1].width == 64);
}
