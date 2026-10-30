// The wad archive and palette pictures (engine/vfs/wad.hpp): pictures written into a wad read
// back by name in any case, byte for byte, as views into the wad; a raw lump reads at the size
// the caller names; a picture whose pixels run short, a wad cut short, a lump past the end, an
// empty name or no magic is refused with a message naming what is wrong.
//
// ge:summary Tests: the wad archive and palette pictures round trip and what they refuse
// ge:keywords test vfs wad lump picture lmp indexed round-trip refuse damaged truncated
// ge:skill vfs testing
#include <string>
#include <vector>

#include "check.hpp"
#include "vfs/wad.hpp"

namespace vfs = ge::vfs;

namespace {

vfs::IndexedPicture picture(int w, int h) {
  vfs::IndexedPicture p;
  p.width = w, p.height = h;
  for (int i = 0; i < w * h; ++i) p.indices.push_back(uint8_t(i * 7));
  return p;
}

std::vector<uint8_t> sample() {
  std::vector<uint8_t> out;
  std::string error;
  const bool ok =
      vfs::write_wad({{"NUM_0", vfs::kPictureLump, vfs::write_picture(picture(24, 24))},
                      {"conchars", vfs::kRawLump, std::vector<uint8_t>(size_t{128} * 128, 3)},
                      {"sbar", vfs::kPictureLump, vfs::write_picture(picture(320, 24))}},
                     &out, &error);
  CHECK_MSG(ok, error);
  return out;
}

bool refused(const std::vector<uint8_t>& bytes, const std::string& word) {
  vfs::Wad wad;
  std::string error;
  if (vfs::read_wad(bytes, &wad, &error)) return false;
  return error.find(word) != std::string::npos;
}

void put(std::vector<uint8_t>& b, size_t at, uint32_t v) {
  for (int k = 0; k < 4; ++k) b[at + size_t(k)] = uint8_t(v >> (8 * k));
}

}  // namespace

TEST(pictures_written_into_a_wad_read_back_by_name_in_any_case) {
  const std::vector<uint8_t> bytes = sample();
  vfs::Wad wad;
  std::string error;
  CHECK_MSG(vfs::read_wad(bytes, &wad, &error), error);
  CHECK(wad.entries.size() == 3);
  const vfs::WadEntry* e = vfs::wad_find(wad, "num_0");
  CHECK(e != nullptr && e->type == vfs::kPictureLump && !e->compressed);
  if (e) {
    const auto lump = vfs::wad_lump(bytes, *e);
    CHECK(lump.data() == bytes.data() + e->offset);
    vfs::IndexedPicture p;
    CHECK_MSG(vfs::read_picture(lump, &p, &error), error);
    CHECK(p.width == 24 && p.height == 24 && p.indices == picture(24, 24).indices);
  }
  const vfs::WadEntry* c = vfs::wad_find(wad, "CONCHARS");
  CHECK(c != nullptr && c->type == vfs::kRawLump);
  if (c) {
    vfs::IndexedPicture p;
    CHECK(vfs::read_raw_picture(vfs::wad_lump(bytes, *c), 128, 128, &p, &error));
    CHECK(p.indices.size() == size_t{128} * 128 && p.indices[0] == 3);
    CHECK(!vfs::read_raw_picture(vfs::wad_lump(bytes, *c), 128, 129, &p, &error));
    CHECK(error.find("needs") != std::string::npos);
  }
  CHECK(vfs::wad_find(wad, "none") == nullptr);
}

TEST(a_damaged_wad_or_picture_is_refused_naming_the_damage) {
  const std::vector<uint8_t> good = sample();
  CHECK(refused({'W', 'A', 'D'}, "magic"));
  std::vector<uint8_t> b = good;
  b[3] = '3';
  CHECK(refused(b, "magic"));
  CHECK(refused(std::vector<uint8_t>(good.begin(), good.end() - 1), "runs past"));
  b = good;
  const size_t dir = good.size() - size_t(3) * 32;
  put(b, dir, uint32_t(good.size()));
  put(b, dir + 4, 1);
  CHECK(refused(b, "lies past the end"));
  b = good;
  b[dir + 16] = 0;
  CHECK(refused(b, "empty name"));
  vfs::IndexedPicture p;
  std::string error;
  std::vector<uint8_t> pic = vfs::write_picture(picture(4, 4));
  pic.pop_back();
  CHECK(!vfs::read_picture(pic, &p, &error) && error.find("needs 16") != std::string::npos);
  CHECK(!vfs::read_picture(std::vector<uint8_t>(5), &p, &error));
  std::vector<uint8_t> out;
  CHECK(!vfs::write_wad({{std::string(16, 'x'), vfs::kPictureLump, {}}}, &out, &error));
  CHECK(error.find("15") != std::string::npos);
}
