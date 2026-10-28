// The zip reader: the end-of-central-directory record searched for in the file's last 64 KB
// (its comment may follow it), each central record checked against the file, each local header
// read for where the data really starts.
//
// ge:summary The zip reader and the stored-zip writer for tests
// ge:keywords zip central directory local header eocd deflate crc32
// ge:skill vfs
// ge:test tests/vfs_zip_test.cpp
#include "vfs/zip.hpp"

#include "assets/image.hpp"
#include "assets/inflate.hpp"

namespace ge::vfs {

namespace {

uint16_t u16(std::span<const uint8_t> b, size_t at) { return uint16_t(b[at] | (b[at + 1] << 8)); }
uint32_t u32(std::span<const uint8_t> b, size_t at) {
  return uint32_t(b[at]) | (uint32_t(b[at + 1]) << 8) | (uint32_t(b[at + 2]) << 16) |
         (uint32_t(b[at + 3]) << 24);
}
void put16(std::vector<uint8_t>& o, uint32_t v) {
  o.push_back(uint8_t(v));
  o.push_back(uint8_t(v >> 8));
}
void put32(std::vector<uint8_t>& o, uint32_t v) {
  put16(o, v & 0xffff);
  put16(o, v >> 16);
}

char fold(char c) {
  if (c == '\\') return '/';
  return c >= 'A' && c <= 'Z' ? char(c - 'A' + 'a') : c;
}

}  // namespace

bool same_name(std::string_view a, std::string_view b) {
  if (a.size() != b.size()) return false;
  for (size_t i = 0; i < a.size(); i++) {
    if (fold(a[i]) != fold(b[i])) return false;
  }
  return true;
}

bool read_zip(std::span<const uint8_t> bytes, Zip* out, std::string* error) {
  out->entries.clear();
  if (bytes.size() < 22) {
    *error = "zip: " + std::to_string(bytes.size()) + " bytes is too short for an end record";
    return false;
  }
  size_t end = bytes.size();
  const size_t lowest = bytes.size() > 22 + 65535 ? bytes.size() - 22 - 65535 : 0;
  for (size_t at = bytes.size() - 22;; at--) {
    if (u32(bytes, at) == 0x06054b50) {
      end = at;
      break;
    }
    if (at == lowest) break;
  }
  if (end == bytes.size()) {
    *error = "zip: no end-of-central-directory record in the last 64 KB; not a zip archive";
    return false;
  }
  if (u16(bytes, end + 4) != 0 || u16(bytes, end + 6) != 0) {
    *error = "zip: a spanned (multi-disk) archive; join its parts first";
    return false;
  }
  const uint32_t count = u16(bytes, end + 10);
  const uint32_t dir_size = u32(bytes, end + 12);
  const uint32_t dir_at = u32(bytes, end + 16);
  if (count == 0xffff || dir_at == 0xffffffff) {
    *error = "zip: a zip64 archive, which this reader does not take";
    return false;
  }
  if (uint64_t(dir_at) + dir_size > end) {
    *error = "zip: the central directory (offset " + std::to_string(dir_at) + ", " +
             std::to_string(dir_size) + " bytes) runs past the end record";
    return false;
  }
  size_t at = dir_at;
  for (uint32_t i = 0; i < count; i++) {
    const std::string where = "zip: central record " + std::to_string(i) + " at byte " +
                              std::to_string(at);
    if (at + 46 > end || u32(bytes, at) != 0x02014b50) {
      *error = where + " is not a central record";
      return false;
    }
    ZipEntry e;
    const uint16_t flags = u16(bytes, at + 8);
    e.method = u16(bytes, at + 10);
    e.crc = u32(bytes, at + 16);
    e.packed = u32(bytes, at + 20);
    e.size = u32(bytes, at + 24);
    const size_t name_len = u16(bytes, at + 28);
    const size_t extra_len = u16(bytes, at + 30);
    const size_t comment_len = u16(bytes, at + 32);
    const uint32_t local = u32(bytes, at + 42);
    if (at + 46 + name_len + extra_len + comment_len > end) {
      *error = where + " has a name or extra field past the directory";
      return false;
    }
    e.name.assign(reinterpret_cast<const char*>(bytes.data() + at + 46), name_len);
    if ((flags & 1) != 0) {
      *error = where + " ('" + e.name + "') is encrypted";
      return false;
    }
    if (e.method != 0 && e.method != 8) {
      *error = where + " ('" + e.name + "') uses method " + std::to_string(e.method) +
               "; only stored (0) and deflated (8) are read";
      return false;
    }
    if (uint64_t(local) + 30 > bytes.size() || u32(bytes, local) != 0x04034b50) {
      *error = where + " ('" + e.name + "') points at byte " + std::to_string(local) +
               ", which is not a local header";
      return false;
    }
    const uint64_t data = uint64_t(local) + 30 + u16(bytes, local + 26) + u16(bytes, local + 28);
    if (data + e.packed > dir_at) {
      *error = where + " ('" + e.name + "') has data running into the central directory";
      return false;
    }
    e.data = uint32_t(data);
    out->entries.push_back(std::move(e));
    at += 46 + name_len + extra_len + comment_len;
  }
  return true;
}

const ZipEntry* zip_find(const Zip& zip, std::string_view name) {
  for (const ZipEntry& e : zip.entries) {
    if (same_name(e.name, name)) return &e;
  }
  return nullptr;
}

bool zip_unpack(std::span<const uint8_t> bytes, const ZipEntry& entry, std::vector<uint8_t>* out,
                std::string* error) {
  const std::span<const uint8_t> packed = bytes.subspan(entry.data, entry.packed);
  if (entry.method == 0) {
    if (entry.packed != entry.size) {
      *error = "zip: stored entry '" + entry.name + "' has different packed and unpacked sizes";
      return false;
    }
    out->assign(packed.begin(), packed.end());
  } else {
    auto bytes_out = assets::inflate_raw(packed, entry.size);
    if (!bytes_out) {
      *error = "zip: entry '" + entry.name + "' does not inflate to its " +
               std::to_string(entry.size) + " bytes; the archive is damaged: fetch it again";
      return false;
    }
    *out = std::move(*bytes_out);
  }
  if (assets::crc32(out->data(), out->size()) != entry.crc) {
    *error = "zip: entry '" + entry.name +
             "' fails its CRC-32; the archive is damaged: fetch it again";
    return false;
  }
  return true;
}

uint32_t crc32_of(std::span<const uint8_t> bytes) {
  return assets::crc32(bytes.data(), bytes.size());
}

std::vector<uint8_t> write_stored_zip(
    const std::vector<std::pair<std::string, std::vector<uint8_t>>>& files) {
  std::vector<uint8_t> o;
  std::vector<uint32_t> locals;
  for (const auto& [name, data] : files) {
    locals.push_back(uint32_t(o.size()));
    put32(o, 0x04034b50);
    put16(o, 10);
    put16(o, 0);
    put16(o, 0);
    put32(o, 0);
    put32(o, assets::crc32(data.data(), data.size()));
    put32(o, uint32_t(data.size()));
    put32(o, uint32_t(data.size()));
    put16(o, uint32_t(name.size()));
    put16(o, 0);
    o.insert(o.end(), name.begin(), name.end());
    o.insert(o.end(), data.begin(), data.end());
  }
  const uint32_t dir_at = uint32_t(o.size());
  for (size_t i = 0; i < files.size(); i++) {
    const auto& [name, data] = files[i];
    put32(o, 0x02014b50);
    put16(o, 20);
    put16(o, 10);
    put16(o, 0);
    put16(o, 0);
    put32(o, 0);
    put32(o, assets::crc32(data.data(), data.size()));
    put32(o, uint32_t(data.size()));
    put32(o, uint32_t(data.size()));
    put16(o, uint32_t(name.size()));
    put16(o, 0);
    put16(o, 0);
    put16(o, 0);
    put16(o, 0);
    put32(o, 0);
    put32(o, locals[i]);
    o.insert(o.end(), name.begin(), name.end());
  }
  const uint32_t dir_size = uint32_t(o.size()) - dir_at;
  put32(o, 0x06054b50);
  put16(o, 0);
  put16(o, 0);
  put16(o, uint32_t(files.size()));
  put16(o, uint32_t(files.size()));
  put32(o, dir_size);
  put32(o, dir_at);
  put16(o, 0);
  return o;
}

}  // namespace ge::vfs
