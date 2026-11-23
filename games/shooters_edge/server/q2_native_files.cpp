// THE FILES A HOSTED GAME LIBRARY READS AND WRITES through the extended interface's file
// extension (`FILESYSTEM_API_V1`, porting/shooters-edge/q2-game-api-ex-facts.md § 4): loose files
// under the game's folder and then the base folder for reading, the game's folder for writing.
// - A path is made safe first: backslashes become slashes, `.` and empty parts go, `..` takes the
//   part before it and never climbs above the folder; an empty path is "Invalid quake path".
// - Open modes: read, write (made or emptied), append (made, at its end), read-write (an existing
//   file, at its end); `exclusive` refuses a file already there; gzip and raw deflate are not
//   offered (not supported). Read is answered with the length, the others with the position.
// - Handles are 1 up to 1024; a call on a handle not open, or of the wrong way, is a bad handle;
//   a file keeps the first error it met and answers it again.
// - `load_file` reads a whole file into a block of the game's memory under its tag, a zero byte
//   after it; with no buffer it only answers the length.
// - `list_files` lists a folder in every search folder: by extensions (`.bsp;.ent`, any case) or,
//   by filter, by wildcards on the path below it (then into folders, as `recursive` and
//   `dirs only` do, 8 deep); dot files skipped; names sorted, each once (records with extra info:
//   sorted only).
// - Errors are negative: a system error's number, or the engine's own codes from `kFsErrBase`,
//   which `error_string` names.
// Packs are not searched: a game reading its own files out of a pack gets "not found" (the
// ledger's partial).
//
// ge:summary The hosted game library's file extension: open, read, write, seek, lines, lists
// ge:keywords game-library filesystem-api filesystem_api_v1 open_file load_file list_files
// ge:keywords getgameapiex extended-game-interface
// ge:skill shooter mod
// ge:test tests/shooters_edge_q2_native_test.cpp
#include <algorithm>
#include <cctype>
#include <cerrno>
#include <chrono>
#include <climits>
#include <csetjmp>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iterator>
#include <string>
#include <system_error>
#include <vector>

#include "server/macros.hpp"
#include "shooters_edge/server/q2_native_host.hpp"

namespace shooters_edge::server {

namespace {

namespace fs = std::filesystem;
using native::NativeFilesApi;

constexpr size_t kMostFiles = 1024;
constexpr int64_t kMostLoad = 0x8001000;
constexpr int kMostDepth = 8;

enum FsErr : uint8_t { kUnspecified = 0, kInvalidPath = 3, kEndOfFile = 4, kNotRegular = 6 };

int fs_err(int k) { return -(native::kFsErrBase + k); }

const char* const kErrText[] = {
    "Unspecified error",     "Unknown file format",    "Invalid file format",
    "Invalid quake path",    "Unexpected end of file", "File too small",
    "Not a regular file",    "Decompression overrun",  "String truncation avoided",
    "Infinite loop avoided", "Library error",          "Out of slots",
    "Inflate failed",        "Deflate failed",         "Coherency check failed",
    "Bad compression method"};

// The path made safe (the header's rules); false when nothing is left of it.
bool clean_path(const char* path, std::string& out) {
  out.clear();
  if (path == nullptr) return false;
  std::vector<std::string> parts;
  std::string part;
  const auto take = [&] {
    if (part == "..") {
      if (!parts.empty()) parts.pop_back();
    } else if (!part.empty() && part != ".") {
      parts.push_back(part);
    }
    part.clear();
  };
  for (const char* p = path; *p != 0; p++) {
    const char c = *p == '\\' ? '/' : *p;
    if (c == '/')
      take();
    else
      part += c;
  }
  take();
  for (const std::string& s : parts) out += (out.empty() ? "" : "/") + s;
  if (out.empty()) return false;
  return std::none_of(out.begin(), out.end(),
                      [](char c) { return static_cast<unsigned char>(c) < 32; });
}

// The folders a read searches, by the mode's path bits.
std::vector<fs::path> search(const NativeHost& h, uint32_t mode) {
  if ((mode & native::kFsPathBase) != 0 && (mode & native::kFsPathGame) == 0) return {h.base_dir};
  if ((mode & native::kFsPathGame) != 0 && (mode & native::kFsPathBase) == 0) return {h.write_dir};
  return h.folders;
}

NativeHost::OpenFile* handle_of(NativeHost& h, int32_t handle) {
  if (handle < 1 || size_t(handle) > h.files.size()) return nullptr;
  NativeHost::OpenFile& f = h.files[size_t(handle) - 1];
  return f.file != nullptr ? &f : nullptr;
}

int32_t new_handle(NativeHost& h, std::FILE* file, bool write) {
  size_t i = 0;
  while (i < h.files.size() && h.files[i].file != nullptr) i++;
  if (i == h.files.size()) h.files.emplace_back();
  h.files[i] = {file, write, 0};
  return static_cast<int32_t>(i + 1);
}

int64_t fs_open(const char* path, int32_t* handle, uint32_t mode) {
  NativeHost& h = *native_current();
  if (handle != nullptr) *handle = 0;
  if (handle == nullptr) return -EINVAL;
  std::string rel;
  if (!clean_path(path, rel)) return fs_err(kInvalidPath);
  if ((mode & (native::kFsFlagGzip | native::kFsFlagDeflate)) != 0) return -ENOSYS;
  size_t open = 0;
  for (const auto& f : h.files) open += f.file != nullptr ? 1 : 0;
  if (open >= kMostFiles) return -EMFILE;
  std::error_code ec;
  const uint32_t how = mode & native::kFsModeMask;
  if (how == native::kFsModeRead) {
    for (const fs::path& dir : search(h, mode)) {
      fs::path p = dir / rel;
      if (!fs::exists(p, ec)) {
        std::string lower = rel;
        for (char& c : lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        p = dir / lower;
        if (lower == rel || !fs::exists(p, ec)) continue;
      }
      if (!fs::is_regular_file(p, ec)) return fs_err(kNotRegular);
      std::FILE* f = std::fopen(p.string().c_str(), "rb");
      if (f == nullptr) return -errno;
      const int64_t size = static_cast<int64_t>(fs::file_size(p, ec));
      *handle = new_handle(h, f, false);
      return size;
    }
    return -ENOENT;
  }
  const fs::path base = (mode & native::kFsPathBase) != 0 && (mode & native::kFsPathGame) == 0
                            ? h.base_dir
                            : h.write_dir;
  if (base.empty()) return -ENOENT;
  const fs::path p = base / rel;
  if ((mode & native::kFsFlagExclusive) != 0 && fs::exists(p, ec)) return -EEXIST;
  fs::create_directories(p.parent_path(), ec);
  const char* fmode = how == native::kFsModeWrite    ? "wb"
                      : how == native::kFsModeAppend ? "ab"
                                                     : "r+b";
  std::FILE* f = std::fopen(p.string().c_str(), fmode);
  if (f == nullptr) return -errno;
  std::fseek(f, 0, SEEK_END);
  const int64_t at = how == native::kFsModeWrite ? 0 : static_cast<int64_t>(std::ftell(f));
  *handle = new_handle(h, f, true);
  return at;
}

int fs_close(int32_t handle) {
  NativeHost& h = *native_current();
  NativeHost::OpenFile* f = handle_of(h, handle);
  if (f == nullptr) return -EBADF;
  const int kept = f->error;
  const int closed = std::fclose(f->file);
  *f = {};
  if (kept != 0) return kept;
  return closed == 0 ? 0 : fs_err(kUnspecified);
}

int fs_load(const char* path, void** buffer, uint32_t flags, uint32_t tag) {
  NativeHost& h = *native_current();
  if ((flags & native::kFsModeMask) != 0) return -EINVAL;
  if (tag > 65535) {
    h.error_text = "PF_LoadFile: bad tag";
    h.dead = true;
    if (h.jump != nullptr) std::longjmp(*h.jump, 1);  // NOLINT(cert-err52-cpp): the interface's
    std::abort();
  }
  int32_t handle = 0;
  const int64_t size = fs_open(path, &handle, flags);
  if (size < 0) return static_cast<int>(size);
  NativeHost::OpenFile* f = handle_of(h, handle);
  if (size > kMostLoad) {
    fs_close(handle);
    return -EFBIG;
  }
  if (buffer == nullptr) {
    fs_close(handle);
    return static_cast<int>(size);
  }
  auto* block = static_cast<char*>(std::calloc(size_t(size) + 1, 1));
  if (block == nullptr) {
    fs_close(handle);
    return -ENOMEM;
  }
  const size_t got = std::fread(block, 1, size_t(size), f->file);
  fs_close(handle);
  if (got != size_t(size)) {
    std::free(block);
    return fs_err(kEndOfFile);
  }
  h.blocks[block] = static_cast<int>(tag);
  *buffer = block;
  return static_cast<int>(size);
}

int fs_read(void* buffer, size_t length, int32_t handle) {
  NativeHost& h = *native_current();
  NativeHost::OpenFile* f = handle_of(h, handle);
  if (f == nullptr || f->write) return -EBADF;
  if (length > size_t(INT_MAX)) return -EINVAL;
  if (f->error != 0) return f->error;
  if (length == 0) return 0;
  const size_t got = std::fread(buffer, 1, length, f->file);
  if (got < length && std::ferror(f->file) != 0) return f->error = fs_err(kUnspecified);
  return static_cast<int>(got);
}

int fs_write(const void* buffer, size_t length, int32_t handle) {
  NativeHost& h = *native_current();
  NativeHost::OpenFile* f = handle_of(h, handle);
  if (f == nullptr || !f->write) return -EBADF;
  if (length > size_t(INT_MAX)) return -EINVAL;
  if (f->error != 0) return f->error;
  if (std::fwrite(buffer, 1, length, f->file) != length) return f->error = fs_err(kUnspecified);
  return static_cast<int>(length);
}

int fs_flush(int32_t handle) {
  NativeHost& h = *native_current();
  NativeHost::OpenFile* f = handle_of(h, handle);
  if (f == nullptr || !f->write) return -EBADF;
  return std::fflush(f->file) == 0 ? 0 : fs_err(kUnspecified);
}

int64_t fs_tell(int32_t handle) {
  NativeHost& h = *native_current();
  NativeHost::OpenFile* f = handle_of(h, handle);
  if (f == nullptr) return -EBADF;
  const long at = std::ftell(f->file);
  return at < 0 ? -EINVAL : static_cast<int64_t>(at);
}

int fs_seek(int32_t handle, int64_t offset, int whence) {
  NativeHost& h = *native_current();
  NativeHost::OpenFile* f = handle_of(h, handle);
  if (f == nullptr) return -EBADF;
  if (whence != SEEK_SET && whence != SEEK_CUR && whence != SEEK_END) return -EINVAL;
  if (offset < LONG_MIN || offset > LONG_MAX) return -EOVERFLOW;
  return std::fseek(f->file, static_cast<long>(offset), whence) == 0 ? 0 : -EINVAL;
}

int fs_read_line(int32_t handle, char* buffer, size_t size) {
  NativeHost& h = *native_current();
  NativeHost::OpenFile* f = handle_of(h, handle);
  if (f == nullptr || f->write) return -EBADF;
  if (size == 0 || size > size_t(INT_MAX) || buffer == nullptr) return -EINVAL;
  if (f->error != 0) return f->error;
  if (std::fgets(buffer, static_cast<int>(size), f->file) == nullptr) {
    buffer[0] = 0;
    return std::ferror(f->file) != 0 ? (f->error = fs_err(kUnspecified)) : 0;
  }
  return static_cast<int>(std::strlen(buffer));
}

// Whether `name` (below the listed folder) passes the filter.
bool passes(const std::string& name, const char* filter, bool by_filter) {
  if (filter == nullptr || filter[0] == 0) return true;
  std::string want(filter);
  size_t at = 0;
  while (at <= want.size()) {
    size_t end = want.find(';', at);
    if (end == std::string::npos) end = want.size();
    const std::string one = want.substr(at, end - at);
    at = end + 1;
    if (one.empty()) continue;
    if (by_filter) {
      if (ge::server::wildcard_match(one, name)) return true;
    } else if (name.size() >= one.size()) {
      bool same = true;
      for (size_t k = 0; k < one.size() && same; k++)
        same = std::tolower(static_cast<unsigned char>(name[name.size() - one.size() + k])) ==
               std::tolower(static_cast<unsigned char>(one[k]));
      if (same) return true;
    }
  }
  return false;
}

struct Found {
  std::string name;
  int64_t size = 0, mtime = 0;
};

void walk(const fs::path& dir, const std::string& below, int depth, bool deep, bool dirs_only,
          const char* filter, bool by_filter, std::vector<Found>& out) {
  std::error_code ec;
  for (fs::directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec)) {
    const std::string leaf = it->path().filename().string();
    if (leaf.empty() || leaf[0] == '.') continue;
    std::string name = below;
    if (!name.empty()) name += '/';
    name += leaf;
    const bool folder = it->is_directory(ec);
    if (folder && deep && depth < kMostDepth)
      walk(it->path(), name, depth + 1, deep, dirs_only, filter, by_filter, out);
    if (folder != dirs_only || (!folder && !it->is_regular_file(ec))) continue;
    if (!passes(name, filter, by_filter)) continue;
    Found f;
    f.name = name;
    if (!folder) f.size = static_cast<int64_t>(it->file_size(ec));
    // Unix seconds: the file's age on its own clock taken from the system clock's now (no
    // clock_cast: Apple's standard library has none).
    const auto age = fs::file_time_type::clock::now() - it->last_write_time(ec);
    f.mtime = static_cast<int64_t>(
        std::chrono::duration_cast<std::chrono::seconds>(
            (std::chrono::system_clock::now() -
             std::chrono::duration_cast<std::chrono::system_clock::duration>(age))
                .time_since_epoch())
            .count());
    out.push_back(std::move(f));
  }
}

void** fs_list(const char* path, const char* filter, uint32_t flags, int* count) {
  NativeHost& h = *native_current();
  if (count != nullptr) *count = 0;
  const uint32_t list_bits = flags & 0xFF00u;
  const bool dirs_only = (flags & native::kFsSearchDirsOnly) != 0;
  if (dirs_only && list_bits != native::kFsSearchDirsOnly) return nullptr;
  std::string rel;
  if (path != nullptr && !clean_path(path, rel)) rel.clear();
  const bool by_filter = (flags & native::kFsSearchByFilter) != 0;
  const bool deep = by_filter || dirs_only || (flags & native::kFsSearchRecursive) != 0;
  std::vector<Found> found;
  for (const fs::path& dir : search(h, flags))
    walk(rel.empty() ? dir : dir / rel, "", 0, deep, dirs_only, filter, by_filter, found);
  if (found.empty()) return nullptr;
  const bool info = (flags & native::kFsSearchExtraInfo) != 0;
  std::sort(found.begin(), found.end(),
            [](const Found& a, const Found& b) { return a.name < b.name; });
  if (!info)
    found.erase(std::unique(found.begin(), found.end(),
                            [](const Found& a, const Found& b) { return a.name == b.name; }),
                found.end());
  auto** list = static_cast<void**>(std::calloc(found.size() + 1, sizeof(void*)));
  if (list == nullptr) return nullptr;
  for (size_t i = 0; i < found.size(); i++) {
    std::string name = found[i].name;
    if ((flags & native::kFsSearchStripExt) != 0) {
      const size_t dot = name.find_last_of('.');
      const size_t slash = name.find_last_of('/');
      if (dot != std::string::npos && (slash == std::string::npos || dot > slash)) name.resize(dot);
    }
    if ((flags & native::kFsSearchSavePath) != 0 && !rel.empty()) name.insert(0, rel + '/');
    if (info) {
      auto* rec = static_cast<native::NativeFileInfo*>(
          std::calloc(1, offsetof(native::NativeFileInfo, name) + name.size() + 1));
      if (rec == nullptr) break;
      rec->size = found[i].size;
      rec->ctime = found[i].mtime;
      rec->mtime = found[i].mtime;
      std::memcpy(rec->name, name.c_str(), name.size() + 1);
      list[i] = rec;
    } else {
      auto* text = static_cast<char*>(std::calloc(name.size() + 1, 1));
      if (text == nullptr) break;
      std::memcpy(text, name.c_str(), name.size() + 1);
      list[i] = text;
    }
    if (count != nullptr) *count = static_cast<int>(i + 1);
  }
  return list;
}

void fs_free_list(void** list) {
  if (list == nullptr) return;
  for (void** p = list; *p != nullptr; p++) std::free(*p);
  std::free(reinterpret_cast<void*>(list));
}

const char* fs_error_string(int error) {
  if (error >= 0) return "Success";
  if (error > -native::kFsErrBase) return std::strerror(-error);
  const int k = -error - native::kFsErrBase;
  return k < static_cast<int>(std::size(kErrText)) ? kErrText[k] : kErrText[0];
}

const NativeFilesApi kFiles = {fs_open,      fs_close, fs_load,      fs_read,
                               fs_write,     fs_flush, fs_tell,      fs_seek,
                               fs_read_line, fs_list,  fs_free_list, fs_error_string};

}  // namespace

const native::NativeFilesApi* native_files_api() { return &kFiles; }

void native_close_files(NativeHost& h) {
  for (NativeHost::OpenFile& f : h.files)
    if (f.file != nullptr) std::fclose(f.file);
  h.files.clear();
}

}  // namespace shooters_edge::server
