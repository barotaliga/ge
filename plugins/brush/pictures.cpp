// The pictures a 29 or BSP2 map carries (pictures.hpp).
//
// ge:summary Reads a 29 or BSP2 map's own miptex pictures as views into the file
// ge:keywords pictures miptex mips read_map_pictures
// ge:skill brush
// ge:test tests/brush_pictures_test.cpp
#include "brush/pictures.hpp"

#include "brush/bytes.hpp"

namespace ge::brush {

using detail::Cursor;

bool carries_pictures(std::span<const uint8_t> file) {
  if (file.size() < 4) return false;
  const uint32_t first = uint32_t(file[0]) | uint32_t(file[1]) << 8 | uint32_t(file[2]) << 16 |
                         uint32_t(file[3]) << 24;
  return first == 29 || first == 0x32505342u;  // version 29, or the magic "BSP2"
}

std::vector<MapPicture> read_map_pictures(std::span<const uint8_t> file) {
  std::vector<MapPicture> out;
  if (!carries_pictures(file)) return out;
  Cursor head{file};
  head.u32();
  for (int i = 0; i < 2; ++i) head.u32(), head.u32();  // lumps 0 and 1
  const uint32_t ofs = head.u32(), len = head.u32();
  if (head.bad || ofs > file.size() || len > file.size() - ofs || len < 4) return out;
  const std::span<const uint8_t> lump = file.subspan(ofs, len);
  Cursor c{lump};
  const uint32_t count = c.u32();
  for (uint32_t i = 0; i < count && !c.bad; ++i) {
    const int32_t at = c.i32();
    MapPicture m;
    if (at >= 0 && uint32_t(at) + 40 <= len) {
      Cursor r{lump};
      r.at = size_t(at);
      m.name = r.name(16);
      m.width = r.u32();
      m.height = r.u32();
      for (int k = 0; k < 4; ++k) {
        const uint32_t o = r.u32();
        const uint64_t bytes = uint64_t(m.width >> k) * (m.height >> k);
        if (o != 0 && uint64_t(at) + o + bytes <= len) m.level[k] = lump.data() + at + o;
      }
      if (!m.level[0]) m.width = m.height = 0;
    }
    out.push_back(std::move(m));
  }
  return out;
}

}  // namespace ge::brush
