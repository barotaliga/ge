// The version 38 family's wall textures (plugins/brush/wal.hpp): a file written reads back with its
// head and four levels as views; a short, empty or cut file is refused with the file named; a 38
// map's textures are found by their names, a missing one named in the faults.
//
// ge:summary Tests reading and writing wall texture files and a 38 map's pictures from them
// ge:keywords test brush wal read_wal write_wal wal_pictures wal_path
// ge:skill brush testing
#include <string>
#include <vector>

#include "brush/read.hpp"
#include "brush/wal.hpp"
#include "brush/write.hpp"
#include "check.hpp"

namespace br = ge::brush;

namespace {

std::vector<uint8_t> pattern(uint32_t w, uint32_t h) {
  std::vector<uint8_t> px(size_t(w) * h);
  for (uint32_t y = 0; y < h; ++y)
    for (uint32_t x = 0; x < w; ++x)
      px[size_t(y) * w + x] = uint8_t((x / 8 + y / 8) % 2 ? 40 : 200);
  return px;
}

br::WalHead named(const std::string& name) {
  br::WalHead h;
  h.name = name;
  return h;
}

}  // namespace

TEST(a_written_wall_texture_reads_back) {
  const br::WalHead head{
      .name = "e1u1/metal", .next = "e1u1/metal2", .flags = 0x8, .contents = 0x20, .value = 7};
  const std::vector<uint8_t> file = br::write_wal(head, 64, 32, pattern(64, 32));
  CHECK(file.size() == br::kWalHead + size_t{64 * 32 + 32 * 16 + 16 * 8 + 8 * 4});
  br::MapPicture p;
  br::WalHead back;
  std::string error;
  CHECK_MSG(br::read_wal(file, "t.wal", &p, &error, &back), error);
  CHECK(p.name == "e1u1/metal" && p.width == 64 && p.height == 32);
  CHECK(back.next == "e1u1/metal2" && back.flags == 0x8 && back.contents == 0x20 &&
        back.value == 7);
  for (const uint8_t* level : p.level)
    CHECK(level >= file.data() && level < file.data() + file.size());
  CHECK(p.level[0][0] == 200 && p.level[0][8] == 40);
  CHECK(p.level[1][0] == 200 && p.level[1][4] == 40);  // level 1: every other texel
  CHECK(br::wal_path("e1u1/metal") == "textures/e1u1/metal.wal");
}

TEST(a_damaged_wall_texture_is_refused_with_its_name) {
  br::MapPicture p;
  std::string error;
  CHECK(!br::read_wal(std::vector<uint8_t>(40, 0), "a.wal", &p, &error));
  CHECK(error.find("a.wal") != std::string::npos && error.find("next:") != std::string::npos);
  std::vector<uint8_t> file = br::write_wal(named("x"), 16, 16, pattern(16, 16));
  file.resize(br::kWalHead + 100);  // the first level cut
  CHECK(!br::read_wal(file, "b.wal", &p, &error));
  CHECK(error.find("b.wal") != std::string::npos && p.width == 0);
  std::vector<uint8_t> empty(br::kWalHead, 0);  // a size of 0
  CHECK(!br::read_wal(empty, "c.wal", &p, &error));
}

TEST(a_38_map_s_textures_come_from_their_files) {
  std::vector<uint8_t> bytes;
  std::string error;
  CHECK_MSG(br::write_bsp(br::sample_fixture(), br::Family::v38, &bytes, &error), error);
  br::Map map;
  CHECK_MSG(br::read_bsp(bytes, &map, &error), error);
  CHECK(map.textures.size() >= 2);
  std::vector<std::vector<uint8_t>> files(map.textures.size());
  std::vector<std::span<const uint8_t>> views(map.textures.size());
  for (size_t i = 1; i < files.size(); ++i) {  // the first texture's file is missing
    files[i] = br::write_wal(named(map.textures[i].name), 32, 32, pattern(32, 32));
    views[i] = files[i];
  }
  std::vector<std::string> faults;
  const std::vector<br::MapPicture> pics = br::wal_pictures(map, views, &faults);
  CHECK(pics.size() == map.textures.size());
  CHECK(pics[0].width == 0 && pics[0].name == map.textures[0].name);
  CHECK(faults.size() == 1 && faults[0].find(br::wal_path(map.textures[0].name)) == 0);
  for (size_t i = 1; i < pics.size(); ++i)
    CHECK(pics[i].width == 32 && pics[i].level[0] != nullptr);
}
