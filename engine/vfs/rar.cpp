// RAR archives through the unRAR library's own interface (dll.hpp), and the stored RAR 5 writer.
// This file is the only one that includes unRAR; its macros (HANDLE, CALLBACK, ...) stay here.
//
// ge:summary RAR archives read through the unRAR library; the stored RAR 5 fixture writer
// ge:keywords rar unrar temp-file rar5-writer vint
// ge:skill vfs
// ge:test tests/vfs_rar_test.cpp
#include "vfs/rar.hpp"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <system_error>
#include <type_traits>

#include "core/inflate.hpp"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#define _UNIX  // NOLINT(bugprone-reserved-identifier): the library's own switch for its types
#endif
#include <dll.hpp>

namespace ge::vfs {
namespace {

constexpr uint8_t kRar4[] = {0x52, 0x61, 0x72, 0x21, 0x1A, 0x07, 0x00};
constexpr uint8_t kRar5[] = {0x52, 0x61, 0x72, 0x21, 0x1A, 0x07, 0x01, 0x00};

std::string error_name(int code) {
  switch (code) {
    case ERAR_NO_MEMORY:
      return "it needs more memory than there is";
    case ERAR_BAD_DATA:
      return "its data is damaged (a checksum fails)";
    case ERAR_BAD_ARCHIVE:
      return "it is not a whole RAR archive";
    case ERAR_UNKNOWN_FORMAT:
      return "its format is unknown";
    case ERAR_EOPEN:
    case ERAR_EREAD:
      return "it could not be read";
    case ERAR_MISSING_PASSWORD:
    case ERAR_BAD_PASSWORD:
      return "it is encrypted (no password can be given)";
    case ERAR_LARGE_DICT:
      return "it needs a dictionary over 4 GB";
    default:
      return "the unpacker stopped with code " + std::to_string(code);
  }
}

// The archive's bytes in a private temporary file for the library, deleted with the object.
struct TempFile {
  std::filesystem::path path;
  bool ok = false;
  explicit TempFile(std::span<const uint8_t> bytes) {
    static std::atomic<uint64_t> counter{0};
    std::error_code ec;
    const std::filesystem::path dir = std::filesystem::temp_directory_path(ec);
    if (ec) return;
    const auto now =
        static_cast<uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count());
    for (int attempt = 0; attempt < 64 && !ok; attempt++) {
      const uint64_t tag =
          now ^ (counter.fetch_add(1) * 0x9E3779B97F4A7C15ull) ^ reinterpret_cast<uintptr_t>(this);
      path = dir / ("ge-rar-" + std::to_string(tag) + ".rar");
#ifdef _WIN32
      FILE* f = _wfopen(path.c_str(), L"wbx");
#else
      FILE* f = std::fopen(path.c_str(), "wbx");
#endif
      if (f == nullptr) continue;
      ok = std::fwrite(bytes.data(), 1, bytes.size(), f) == bytes.size();
      ok = (std::fclose(f) == 0) && ok;
      if (!ok) std::filesystem::remove(path, ec);
    }
  }
  ~TempFile() {
    std::error_code ec;
    if (ok) std::filesystem::remove(path, ec);
  }
  TempFile(const TempFile&) = delete;
  TempFile& operator=(const TempFile&) = delete;
};

struct Sink {
  std::vector<uint8_t>* out = nullptr;
  size_t limit = 0;
  bool over = false;
};

int CALLBACK on_event(UINT msg, LPARAM user, LPARAM p1, LPARAM p2) {
  if (msg != UCM_PROCESSDATA) return -1;  // a password or the next volume: none can be given
  // The library hands its pointers over as integers (LPARAM), so they are cast back here.
  auto* sink = reinterpret_cast<Sink*>(user);  // NOLINT(performance-no-int-to-ptr)
  const auto n = static_cast<size_t>(p2);
  if (sink->out == nullptr) return 1;
  if (sink->out->size() + n > sink->limit) {
    sink->over = true;
    return -1;
  }
  const auto* p = reinterpret_cast<const uint8_t*>(p1);  // NOLINT(performance-no-int-to-ptr)
  sink->out->insert(sink->out->end(), p, p + n);
  return 1;
}

void put_utf8(std::string& s, uint32_t c) {
  if (c < 0x80) {
    s += static_cast<char>(c);
  } else if (c < 0x800) {
    s += static_cast<char>(0xC0 | (c >> 6));
    s += static_cast<char>(0x80 | (c & 0x3F));
  } else if (c < 0x10000) {
    s += static_cast<char>(0xE0 | (c >> 12));
    s += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
    s += static_cast<char>(0x80 | (c & 0x3F));
  } else {
    s += static_cast<char>(0xF0 | (c >> 18));
    s += static_cast<char>(0x80 | ((c >> 12) & 0x3F));
    s += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
    s += static_cast<char>(0x80 | (c & 0x3F));
  }
}

// A wide name (UTF-16 on Windows, UTF-32 elsewhere) as UTF-8 with '/' separators.
std::string utf8_name(const wchar_t* w) {
  std::string s;
  for (size_t i = 0; w[i] != 0; i++) {
    auto c = static_cast<uint32_t>(static_cast<std::make_unsigned_t<wchar_t>>(w[i]));
    if constexpr (sizeof(wchar_t) == 2) {
      const auto lo = static_cast<uint32_t>(static_cast<std::make_unsigned_t<wchar_t>>(w[i + 1]));
      if (c >= 0xD800 && c < 0xDC00 && lo >= 0xDC00 && lo < 0xE000) {
        c = 0x10000 + ((c - 0xD800) << 10) + (lo - 0xDC00);
        i++;
      }
    }
    put_utf8(s, c == '\\' ? '/' : c);
  }
  return s;
}

// The archive opened in `mode`; null with the reason in `error`.
HANDLE open_archive(const TempFile& file, unsigned mode, Sink* sink, std::string& error) {
  RAROpenArchiveDataEx o{};
#ifdef _WIN32
  std::wstring name = file.path.wstring();
  o.ArcNameW = name.data();
#else
  std::string name = file.path.string();
  o.ArcName = name.data();
#endif
  o.OpenMode = mode;
  o.Callback = on_event;
  o.UserData = reinterpret_cast<LPARAM>(sink);
  HANDLE h = RAROpenArchiveEx(&o);
  if (h == nullptr || o.OpenResult != ERAR_SUCCESS) {
    if (h != nullptr) RARCloseArchive(h);
    error = "rar: the archive does not open: " + error_name(static_cast<int>(o.OpenResult)) +
            "; next: download it again, or unpack it with a RAR tool and name the file inside";
    return nullptr;
  }
  if ((o.Flags & ROADF_VOLUME) != 0 && (o.Flags & ROADF_FIRSTVOLUME) == 0) {
    RARCloseArchive(h);
    error =
        "rar: this is a later volume of a set, not a whole archive; next: unpack the set's "
        "first volume with a RAR tool and name the file inside";
    return nullptr;
  }
  return h;
}

bool refuse_header(const RARHeaderDataEx& hd, const std::string& name, std::string& error) {
  if ((hd.Flags & (RHDF_SPLITBEFORE | RHDF_SPLITAFTER)) != 0) {
    error = "rar: '" + name + "' is split across the volumes of a set; next: unpack the whole " +
            "set with a RAR tool and name the file";
    return true;
  }
  if ((hd.Flags & RHDF_ENCRYPTED) != 0) {
    error = "rar: '" + name + "' is encrypted, and no password can be given; next: unpack it " +
            "with a RAR tool and name the file";
    return true;
  }
  return false;
}

void put_vint(std::vector<uint8_t>& o, uint64_t v) {
  while (v >= 0x80) {
    o.push_back(static_cast<uint8_t>(v | 0x80));
    v >>= 7;
  }
  o.push_back(static_cast<uint8_t>(v));
}

void put_u32(std::vector<uint8_t>& o, uint32_t v) {
  for (int i = 0; i < 4; i++) o.push_back(static_cast<uint8_t>(v >> (8 * i)));
}

// One header: its CRC-32, its size, then `body` (the type onward).
void put_header(std::vector<uint8_t>& o, const std::vector<uint8_t>& body) {
  std::vector<uint8_t> h;
  put_vint(h, body.size());
  h.insert(h.end(), body.begin(), body.end());
  put_u32(o, core::crc32(h.data(), h.size()));
  o.insert(o.end(), h.begin(), h.end());
}

}  // namespace

bool is_rar(std::span<const uint8_t> b) {
  return (b.size() >= sizeof kRar5 && std::memcmp(b.data(), kRar5, sizeof kRar5) == 0) ||
         (b.size() >= sizeof kRar4 && std::memcmp(b.data(), kRar4, sizeof kRar4) == 0);
}

bool read_rar(std::span<const uint8_t> bytes, std::vector<RarEntry>& out, std::string& error) {
  out.clear();
  TempFile file(bytes);
  if (!file.ok) {
    error =
        "rar: no temporary file could be written for the unpacker; next: free disk space in "
        "the temporary folder and try again";
    return false;
  }
  Sink sink;
  HANDLE h = open_archive(file, RAR_OM_LIST, &sink, error);
  if (h == nullptr) return false;
  RARHeaderDataEx hd{};
  int r = 0;
  while ((r = RARReadHeaderEx(h, &hd)) == ERAR_SUCCESS) {
    RarEntry e;
    e.name = utf8_name(hd.FileNameW);
    e.size = (uint64_t{hd.UnpSizeHigh} << 32) | hd.UnpSize;
    e.dir = (hd.Flags & RHDF_DIRECTORY) != 0;
    e.crc = !e.dir && hd.HashType == RAR_HASH_CRC32 ? hd.FileCRC : 0;
    if (refuse_header(hd, e.name, error)) {
      RARCloseArchive(h);
      return false;
    }
    out.push_back(std::move(e));
    r = RARProcessFile(h, RAR_SKIP, nullptr, nullptr);
    if (r != ERAR_SUCCESS) break;
  }
  RARCloseArchive(h);
  if (r != ERAR_END_ARCHIVE) {
    error = "rar: the archive's list stops after " + std::to_string(out.size()) +
            " entries: " + error_name(r) +
            "; next: download it again, or unpack it with a RAR tool and name the file inside";
    return false;
  }
  return true;
}

bool rar_unpack(std::span<const uint8_t> bytes, size_t index, std::vector<uint8_t>& out,
                std::string& error, size_t limit) {
  out.clear();
  TempFile file(bytes);
  if (!file.ok) {
    error =
        "rar: no temporary file could be written for the unpacker; next: free disk space in "
        "the temporary folder and try again";
    return false;
  }
  Sink sink;
  sink.limit = limit;
  HANDLE h = open_archive(file, RAR_OM_EXTRACT, &sink, error);
  if (h == nullptr) return false;
  RARHeaderDataEx hd{};
  for (size_t i = 0;; i++) {
    int r = RARReadHeaderEx(h, &hd);
    if (r != ERAR_SUCCESS) {
      RARCloseArchive(h);
      error = "rar: entry " + std::to_string(index) + " is not in the archive (" + error_name(r) +
              "); next: list the archive and name a file in it";
      return false;
    }
    const std::string name = utf8_name(hd.FileNameW);
    if (i < index) {
      // A solid archive's files share one stream: an earlier file is unpacked (to nowhere) so
      // the stream reaches this one; a plain archive just skips.
      r = RARProcessFile(h, (hd.Flags & RHDF_SOLID) != 0 ? RAR_TEST : RAR_SKIP, nullptr, nullptr);
      if (r == ERAR_SUCCESS) continue;
      RARCloseArchive(h);
      error = "rar: '" + name + "' before the file does not unpack: " + error_name(r) +
              "; next: download it again, or unpack it with a RAR tool and name the file inside";
      return false;
    }
    if (refuse_header(hd, name, error)) {
      RARCloseArchive(h);
      return false;
    }
    sink.out = &out;
    r = RARProcessFile(h, RAR_TEST, nullptr, nullptr);
    RARCloseArchive(h);
    if (sink.over || r != ERAR_SUCCESS) out.clear();
    if (sink.over) {
      error = "rar: '" + name + "' unpacks to more than " + std::to_string(limit) +
              " bytes; next: unpack it with a RAR tool and name the file inside";
      return false;
    }
    if (r != ERAR_SUCCESS) {
      error = "rar: '" + name + "' does not unpack: " + error_name(r) +
              "; next: download it again, or unpack it with a RAR tool and name the file inside";
      return false;
    }
    return true;
  }
}

std::vector<uint8_t> write_rar(
    const std::vector<std::pair<std::string, std::vector<uint8_t>>>& files) {
  std::vector<uint8_t> o(std::begin(kRar5), std::end(kRar5));
  put_header(o, {1, 0, 0});  // the main header: type 1, no flags, no archive flags
  for (const auto& [name, data] : files) {
    const bool dir = !name.empty() && name.back() == '/';
    const std::string stored = dir ? name.substr(0, name.size() - 1) : name;
    std::vector<uint8_t> h;
    put_vint(h, 2);            // a file header
    put_vint(h, dir ? 0 : 2);  // the data area follows (none for a directory)
    if (!dir) put_vint(h, data.size());
    put_vint(h, dir ? 1 : 4);  // a directory, or a file with its CRC-32
    put_vint(h, dir ? 0 : data.size());
    put_vint(h, dir ? 0x10 : 0x20);  // attributes: a directory or an archived file
    if (!dir) put_u32(h, core::crc32(data.data(), data.size()));
    put_vint(h, 0);  // version 0, not solid, method 0: stored
    put_vint(h, 0);  // made on Windows (the attributes above are its kind)
    put_vint(h, stored.size());
    h.insert(h.end(), stored.begin(), stored.end());
    put_header(o, h);
    if (!dir) o.insert(o.end(), data.begin(), data.end());
  }
  put_header(o, {5, 0, 0});  // the end of the archive: type 5, no flags, the last volume
  return o;
}

}  // namespace ge::vfs
