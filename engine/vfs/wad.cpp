// The wad archive's and the palette picture's readers and writers (wad.hpp).
//
// ge:summary The wad archive's checked directory, palette pictures and their fixture writers
// ge:keywords vfs wad lump picture lmp indexed read write refuse bounds
// ge:skill vfs
// ge:test tests/vfs_wad_test.cpp
#include "vfs/wad.hpp"

#include <cstddef>

namespace ge::vfs {
namespace {

constexpr size_t kHeader = 12, kEntry = 32, kName = 16;

uint32_t u32(std::span<const uint8_t> b, size_t at) {
  return uint32_t(b[at]) | uint32_t(b[at + 1]) << 8 | uint32_t(b[at + 2]) << 16 |
         uint32_t(b[at + 3]) << 24;
}

void put(std::vector<uint8_t>* out, size_t at, uint32_t v) {
  for (int k = 0; k < 4; ++k) (*out)[at + size_t(k)] = uint8_t(v >> (8 * k));
}

bool refuse(std::string* error, const std::string& what) {
  if (error)
    *error = what + ". The file is damaged or not classic content: fetch it again (skill: vfs)";
  return false;
}

char lower(char c) { return c >= 'A' && c <= 'Z' ? char(c - 'A' + 'a') : c; }

}  // namespace

bool read_wad(std::span<const uint8_t> bytes, Wad* out, std::string* error) {
  out->entries.clear();
  if (bytes.size() < kHeader || bytes[0] != 'W' || bytes[1] != 'A' || bytes[2] != 'D' ||
      bytes[3] != '2') {
    return refuse(error, "wad: no WAD2 magic in the first 4 bytes");
  }
  const uint32_t count = u32(bytes, 4), dir = u32(bytes, 8);
  if (dir > bytes.size() || count > (bytes.size() - dir) / kEntry) {
    return refuse(error, "wad: the directory (" + std::to_string(count) + " entries at " +
                             std::to_string(dir) + ") runs past the file's " +
                             std::to_string(bytes.size()) + " bytes");
  }
  out->entries.reserve(count);
  for (size_t i = 0; i < count; ++i) {
    const size_t at = dir + i * kEntry;
    size_t n = 0;
    while (n < kName && bytes[at + 16 + n] != 0) ++n;
    if (n == 0) return refuse(error, "wad: entry " + std::to_string(i) + " has an empty name");
    WadEntry e;
    e.name.assign(reinterpret_cast<const char*>(&bytes[at + 16]), n);
    e.offset = u32(bytes, at);
    e.size = u32(bytes, at + 4);
    e.type = bytes[at + 12];
    e.compressed = bytes[at + 13] != 0;
    if (e.offset > bytes.size() || e.size > bytes.size() - e.offset) {
      return refuse(error,
                    "wad: lump " + std::to_string(i) + " (" + e.name + ") lies past the end");
    }
    out->entries.push_back(std::move(e));
  }
  return true;
}

const WadEntry* wad_find(const Wad& wad, std::string_view name) {
  for (const WadEntry& e : wad.entries) {
    if (e.name.size() != name.size()) continue;
    bool same = true;
    for (size_t k = 0; k < name.size() && same; ++k) same = lower(e.name[k]) == lower(name[k]);
    if (same) return &e;
  }
  return nullptr;
}

std::span<const uint8_t> wad_lump(std::span<const uint8_t> bytes, const WadEntry& entry) {
  return bytes.subspan(entry.offset, entry.size);
}

bool read_raw_picture(std::span<const uint8_t> bytes, int width, int height, IndexedPicture* out,
                      std::string* error) {
  if (width <= 0 || height <= 0 || width > 4096 || height > 4096) {
    return refuse(error, "picture: a size of " + std::to_string(width) + "x" +
                             std::to_string(height) + " is out of range (1..4096)");
  }
  const size_t n = size_t(width) * size_t(height);
  if (bytes.size() < n) {
    return refuse(error, "picture: " + std::to_string(width) + "x" + std::to_string(height) +
                             " needs " + std::to_string(n) + " bytes, the lump holds " +
                             std::to_string(bytes.size()));
  }
  out->width = width;
  out->height = height;
  out->indices.assign(bytes.begin(), bytes.begin() + std::ptrdiff_t(n));
  return true;
}

bool read_picture(std::span<const uint8_t> bytes, IndexedPicture* out, std::string* error) {
  if (bytes.size() < 8) {
    return refuse(error, "picture: " + std::to_string(bytes.size()) +
                             " bytes is too short for its 8-byte header");
  }
  const uint32_t w = u32(bytes, 0), h = u32(bytes, 4);
  if (w > 4096 || h > 4096) {
    return refuse(error, "picture: a size of " + std::to_string(w) + "x" + std::to_string(h) +
                             " is out of range (1..4096)");
  }
  return read_raw_picture(bytes.subspan(8), int(w), int(h), out, error);
}

std::vector<uint8_t> write_picture(const IndexedPicture& picture) {
  std::vector<uint8_t> out(8);
  put(&out, 0, uint32_t(picture.width));
  put(&out, 4, uint32_t(picture.height));
  out.insert(out.end(), picture.indices.begin(), picture.indices.end());
  return out;
}

bool write_wad(const std::vector<WadLump>& lumps, std::vector<uint8_t>* out, std::string* error) {
  out->assign(kHeader, 0);
  (*out)[0] = 'W', (*out)[1] = 'A', (*out)[2] = 'D', (*out)[3] = '2';
  std::vector<uint32_t> at;
  for (const WadLump& l : lumps) {
    if (l.name.empty() || l.name.size() >= kName) {
      return refuse(error, "wad: the lump name '" + l.name + "' is empty or over 15 bytes");
    }
    at.push_back(uint32_t(out->size()));
    out->insert(out->end(), l.bytes.begin(), l.bytes.end());
  }
  const size_t dir = out->size();
  put(out, 4, uint32_t(lumps.size()));
  put(out, 8, uint32_t(dir));
  out->resize(dir + lumps.size() * kEntry, 0);
  for (size_t i = 0; i < lumps.size(); ++i) {
    const size_t e = dir + i * kEntry;
    put(out, e, at[i]);
    put(out, e + 4, uint32_t(lumps[i].bytes.size()));
    put(out, e + 8, uint32_t(lumps[i].bytes.size()));
    (*out)[e + 12] = lumps[i].type;
    for (size_t k = 0; k < lumps[i].name.size(); ++k)
      (*out)[e + 16 + k] = uint8_t(lumps[i].name[k]);
  }
  return true;
}

}  // namespace ge::vfs
