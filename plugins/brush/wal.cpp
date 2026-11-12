// The version 38 family's wall texture files (wal.hpp).
//
// ge:summary Reads and writes wall texture files: the 100-byte head and four mip levels
// ge:keywords wal read_wal write_wal wal_path mips
// ge:skill brush
// ge:test tests/brush_wal_test.cpp
#include "brush/wal.hpp"

#include <algorithm>

#include "brush/bytes.hpp"

namespace ge::brush {

using detail::Cursor;

bool read_wal(std::span<const uint8_t> bytes, std::string_view where, MapPicture* out,
              std::string* error, WalHead* head) {
  *out = {};
  const auto fail = [&](const std::string& why) {
    if (error) *error = std::string(where) + ": " + why;
    *out = {};
    return false;
  };
  if (bytes.size() < kWalHead)
    return fail("a wall texture of " + std::to_string(bytes.size()) +
                " bytes, shorter than its 100-byte head; next: check the file is a .wal");
  Cursor c{bytes};
  WalHead h;
  h.name = c.name(32);
  const uint32_t w = c.u32(), ht = c.u32();
  uint32_t ofs[4];
  for (uint32_t& o : ofs) o = c.u32();
  h.next = c.name(32);
  h.flags = c.u32();
  h.contents = c.u32();
  h.value = c.i32();
  if (w == 0 || ht == 0 || w > 4096 || ht > 4096)
    return fail("a wall texture of " + std::to_string(w) + " x " + std::to_string(ht) +
                " texels, outside 1 to 4096; next: check the file is a .wal");
  out->name = h.name;
  out->width = w;
  out->height = ht;
  for (int k = 0; k < 4; ++k) {
    const uint64_t n = uint64_t(std::max(1u, w >> k)) * std::max(1u, ht >> k);
    if (ofs[k] < kWalHead || ofs[k] > bytes.size() || n > bytes.size() - ofs[k]) {
      if (k == 0)
        return fail("its first level (" + std::to_string(w) + " x " + std::to_string(ht) +
                    " at byte " + std::to_string(ofs[0]) + ") runs past the file's " +
                    std::to_string(bytes.size()) + " bytes; next: check the file is whole");
      break;  // a smaller level missing: the atlas makes its own
    }
    out->level[k] = bytes.data() + ofs[k];
  }
  if (head) *head = std::move(h);
  return true;
}

std::string wal_path(std::string_view name) {
  std::string p = "textures/";
  p.append(name).append(".wal");
  return p;
}

std::vector<MapPicture> wal_pictures(const Map& map,
                                     std::span<const std::span<const uint8_t>> files,
                                     std::vector<std::string>* faults) {
  std::vector<MapPicture> out(map.textures.size());
  for (size_t i = 0; i < out.size(); ++i) {
    const std::string& name = map.textures[i].name;
    const std::string path = wal_path(name);
    const std::span<const uint8_t> bytes = i < files.size() ? files[i] : std::span<const uint8_t>{};
    std::string why;
    if (bytes.empty()) {
      why = path + ": not in the game's files";
    } else if (read_wal(bytes, path, &out[i], &why)) {
      out[i].name = name;  // the map's name for it: the head's may differ in case or be empty
      continue;
    }
    out[i] = {};
    out[i].name = name;
    if (faults) faults->push_back(why);
  }
  return out;
}

std::vector<uint8_t> write_wal(const WalHead& head, uint32_t width, uint32_t height,
                               std::span<const uint8_t> indices) {
  std::vector<uint8_t> out(kWalHead, 0);
  const auto put32 = [&out](size_t at, uint32_t v) {
    for (int i = 0; i < 4; ++i) out[at + size_t(i)] = uint8_t(v >> (8 * i));
  };
  std::copy_n(head.name.begin(), std::min<size_t>(head.name.size(), 31), out.begin());
  put32(32, width);
  put32(36, height);
  std::copy_n(head.next.begin(), std::min<size_t>(head.next.size(), 31), out.begin() + 56);
  put32(88, head.flags);
  put32(92, head.contents);
  put32(96, uint32_t(head.value));
  for (uint32_t k = 0; k < 4; ++k) {
    put32(40 + 4 * k, uint32_t(out.size()));
    const uint32_t s = 1u << k, w = std::max(1u, width >> k), h = std::max(1u, height >> k);
    for (uint32_t y = 0; y < h; ++y)
      for (uint32_t x = 0; x < w; ++x) {
        const size_t at = size_t(y) * s * width + size_t(x) * s;
        out.push_back(at < indices.size() ? indices[at] : uint8_t(0));
      }
  }
  return out;
}

}  // namespace ge::brush
