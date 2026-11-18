// Files fetched from a server kept in a partial of the archive they came from (partial.hpp).
//
// ge:summary The partial archive: the origin, the partial kept, the whole put together
// ge:keywords vfs partial-pack partial download origin assemble rename atomic-write
// ge:skill vfs
// ge:test tests/vfs_partial_test.cpp
#include "vfs/partial.hpp"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <utility>
#include <vector>

#include "vfs/pak.hpp"
#include "vfs/zip.hpp"

namespace ge::vfs {
namespace {

namespace fs = std::filesystem;

constexpr const char* kMeta = ".partial";
constexpr uint32_t kMaxEntries = 1u << 20;
constexpr size_t kHeader = 12, kEntry = 64, kPath = 56;

struct Held {
  uint32_t index = 0, offset = 0;
  std::string path;
  std::vector<uint8_t> bytes;
};

struct Partial {
  uint32_t count = 0, dir = 0;
  uint64_t size = 0;
  std::vector<Held> held;  // by entry
};

bool fail(std::string* error, const std::string& what) {
  if (error) *error = "partial archive: " + what;
  return false;
}

bool ends_with_any_case(std::string_view s, std::string_view end) {
  if (s.size() < end.size()) return false;
  for (size_t i = 0; i < end.size(); ++i) {
    char c = s[s.size() - end.size() + i];
    if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    if (c != end[i]) return false;
  }
  return true;
}

bool plain_file_name(std::string_view n) {
  return !n.empty() && n[0] != '.' && n.find_first_of("/\\:") == std::string_view::npos &&
         n.find("..") == std::string_view::npos;
}

bool plain_path(std::string_view p) {
  return !p.empty() && p[0] != '/' && p[0] != '.' && p.find("..") == std::string_view::npos &&
         p.find_first_of("\\:\n") == std::string_view::npos && p != kMeta;
}

std::vector<uint8_t> read_whole(const fs::path& p) {
  std::ifstream in(p, std::ios::binary);
  if (!in) return {};
  return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

bool write_atomic(const fs::path& p, std::span<const uint8_t> bytes, std::string* error) {
  fs::path tmp = p;
  tmp += ".tmp";
  {
    std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
    out.write(reinterpret_cast<const char*>(bytes.data()), std::streamsize(bytes.size()));
    if (!out)
      return fail(error, "could not write " + tmp.string() + ": check the folder is writable");
  }
  std::error_code ec;
  fs::rename(tmp, p, ec);
  if (ec) {
    fs::remove(tmp, ec);
    return fail(error, "could not rename onto " + p.string() + ": check the folder is writable");
  }
  return true;
}

std::string meta_text(const ArchiveOrigin& o, const Partial& p) {
  std::string t = "archive " + o.archive + "\ncount " + std::to_string(p.count) + "\nsize " +
                  std::to_string(p.size) + "\ndir " + std::to_string(p.dir) + "\n";
  for (const Held& h : p.held)
    t += "held " + std::to_string(h.index) + " " + std::to_string(h.offset) + " " + h.path + "\n";
  return t;
}

// The partial on disk read back; false (start again) when it is missing or does not read.
bool load(const fs::path& file, bool pack, Partial* p) {
  const std::vector<uint8_t> bytes = read_whole(file);
  if (bytes.empty()) return false;
  std::vector<std::pair<std::string, std::vector<uint8_t>>> files;
  std::string error;
  if (pack) {
    Pak pak;
    if (!read_pak(bytes, &pak, &error)) return false;
    for (const PakEntry& e : pak.entries) {
      const std::span<const uint8_t> b = pak_file(bytes, e);
      files.emplace_back(e.path, std::vector<uint8_t>(b.begin(), b.end()));
    }
  } else {
    Zip zip;
    if (!read_zip(bytes, &zip, &error)) return false;
    for (const ZipEntry& e : zip.entries) {
      std::vector<uint8_t> b;
      if (!zip_unpack(bytes, e, &b, &error)) return false;
      files.emplace_back(e.name, std::move(b));
    }
  }
  const auto meta =
      std::find_if(files.begin(), files.end(), [](const auto& f) { return f.first == kMeta; });
  if (meta == files.end()) return false;
  std::istringstream in(std::string(meta->second.begin(), meta->second.end()));
  std::string line;
  while (std::getline(in, line)) {
    std::istringstream w(line);
    std::string key;
    w >> key;
    if (key == "count") w >> p->count;
    if (key == "size") w >> p->size;
    if (key == "dir") w >> p->dir;
    if (key != "held") continue;
    Held h;
    w >> h.index >> h.offset;
    w.get();
    std::getline(w, h.path);
    const auto f =
        std::find_if(files.begin(), files.end(), [&h](const auto& x) { return x.first == h.path; });
    if (!w.fail() && f != files.end()) {
      h.bytes = f->second;
      p->held.push_back(std::move(h));
    }
  }
  return p->count != 0;
}

void put32(std::vector<uint8_t>& b, size_t at, uint32_t v) {
  for (int k = 0; k < 4; ++k) b[at + size_t(k)] = uint8_t(v >> (8 * k));
}

// The pack laid out as the server's was: every file at its offset, the directory where it was
// (bytes no file covers, and a name's bytes after its end, are zero: the server's may hold
// leftovers there). False when the offsets do not hold together (the caller writes the files in
// entry order).
bool lay_out_pack(const Partial& p, std::vector<uint8_t>* out) {
  const uint64_t dir_end = uint64_t(p.dir) + uint64_t(p.count) * kEntry;
  if (p.size > 0xffffffffu || dir_end > p.size || p.dir < kHeader) return false;
  // Every file and the directory inside the archive, none over another.
  std::vector<std::pair<uint64_t, uint64_t>> spans{{p.dir, dir_end}};
  for (const Held& h : p.held) spans.emplace_back(h.offset, h.offset + uint64_t(h.bytes.size()));
  std::sort(spans.begin(), spans.end());
  uint64_t end = kHeader;
  for (const auto& [from, to] : spans) {
    if (from < end || to > p.size) return false;
    end = std::max(end, to);
  }
  out->assign(static_cast<size_t>(p.size), 0);
  const char magic[4] = {'P', 'A', 'C', 'K'};
  std::copy(magic, magic + 4, out->begin());
  put32(*out, 4, p.dir);
  put32(*out, 8, p.count * uint32_t(kEntry));
  for (const Held& h : p.held) {
    std::copy(h.bytes.begin(), h.bytes.end(), out->begin() + h.offset);
    const size_t at = p.dir + size_t(h.index) * kEntry;
    std::copy(h.path.begin(), h.path.end(), out->begin() + static_cast<std::ptrdiff_t>(at));
    put32(*out, at + kPath, h.offset);
    put32(*out, at + kPath + 4, uint32_t(h.bytes.size()));
  }
  return true;
}

// THE FAST WAY for a pack: the new file written over the old `.partial` entry and directory at the
// partial's end, then the entry and the directory after it, and the header pointed at them; only
// the directory and the `.partial` entry are read. False (nothing written) when the partial is not
// one this archive's, already holds the entry or the path, or this file would complete it: the
// caller then rewrites the partial whole.
bool append_to_pack(const fs::path& partial, const ArchiveOrigin& o, std::string_view path,
                    std::span<const uint8_t> bytes, Kept* out) {
  std::fstream f(partial, std::ios::in | std::ios::out | std::ios::binary);
  if (!f) return false;
  f.seekg(0, std::ios::end);
  const auto end = static_cast<uint64_t>(f.tellg());
  uint8_t head[kHeader] = {};
  f.seekg(0);
  if (end < kHeader || !f.read(reinterpret_cast<char*>(head), kHeader) || head[0] != 'P' ||
      head[1] != 'A' || head[2] != 'C' || head[3] != 'K')
    return false;
  const std::span<const uint8_t> h(head, kHeader);
  const uint32_t dir =
      uint32_t(h[4]) | uint32_t(h[5]) << 8 | uint32_t(h[6]) << 16 | uint32_t(h[7]) << 24;
  const uint32_t len =
      uint32_t(h[8]) | uint32_t(h[9]) << 8 | uint32_t(h[10]) << 16 | uint32_t(h[11]) << 24;
  if (len == 0 || len % kEntry != 0 || uint64_t(dir) + len != end) return false;
  std::vector<uint8_t> entries(len);
  f.seekg(dir);
  if (!f.read(reinterpret_cast<char*>(entries.data()), len)) return false;
  const auto u32 = [&entries](size_t at) {
    return uint32_t(entries[at]) | uint32_t(entries[at + 1]) << 8 |
           uint32_t(entries[at + 2]) << 16 | uint32_t(entries[at + 3]) << 24;
  };
  const size_t last = len - kEntry;  // the `.partial` entry is always the last
  if (std::string_view(reinterpret_cast<const char*>(&entries[last])) != kMeta) return false;
  const uint32_t meta_at = u32(last + kPath), meta_len = u32(last + kPath + 4);
  if (uint64_t(meta_at) + meta_len != dir) return false;
  std::string meta(meta_len, '\0');
  f.seekg(meta_at);
  if (!f.read(meta.data(), meta_len)) return false;
  Partial p;
  std::istringstream in(meta);
  std::string line;
  uint32_t held = 0;
  while (std::getline(in, line)) {
    std::istringstream w(line);
    std::string key;
    w >> key;
    if (key == "count") w >> p.count;
    if (key == "size") w >> p.size;
    if (key == "dir") w >> p.dir;
    if (key != "held") continue;
    uint32_t index = 0, offset = 0;
    std::string name;
    w >> index >> offset;
    w.get();
    std::getline(w, name);
    if (index == o.index || name == path) return false;
    held++;
  }
  if (p.count != o.count || p.size != o.size || p.dir != o.dir_offset || held + 1 >= p.count ||
      held != last / kEntry)
    return false;
  meta += "held " + std::to_string(o.index) + " " + std::to_string(o.offset) + " " +
          std::string(path) + "\n";
  // The new file where the old entry was, the entry after it, then the directory.
  const uint64_t new_meta = uint64_t(meta_at) + bytes.size();
  const uint64_t new_dir = new_meta + meta.size();
  if (new_dir + len + kEntry > 0xffffffffu) return false;
  std::vector<uint8_t> tail(entries.begin(), entries.begin() + static_cast<std::ptrdiff_t>(last));
  tail.resize(last + 2 * kEntry, 0);
  std::copy(path.begin(), path.end(), tail.begin() + static_cast<std::ptrdiff_t>(last));
  put32(tail, last + kPath, meta_at);
  put32(tail, last + kPath + 4, uint32_t(bytes.size()));
  const std::string_view mname = kMeta;
  std::copy(mname.begin(), mname.end(), tail.begin() + static_cast<std::ptrdiff_t>(last + kEntry));
  put32(tail, last + kEntry + kPath, uint32_t(new_meta));
  put32(tail, last + kEntry + kPath + 4, uint32_t(meta.size()));
  f.seekp(meta_at);
  f.write(reinterpret_cast<const char*>(bytes.data()), std::streamsize(bytes.size()));
  f.write(meta.data(), std::streamsize(meta.size()));
  f.write(reinterpret_cast<const char*>(tail.data()), std::streamsize(tail.size()));
  std::vector<uint8_t> hdr(head, head + kHeader);
  put32(hdr, 4, uint32_t(new_dir));
  put32(hdr, 8, uint32_t(tail.size()));
  f.seekp(0);
  f.write(reinterpret_cast<const char*>(hdr.data()), kHeader);
  f.flush();
  if (!f) return false;
  *out = Kept{partial.string(), false, held + 1, p.count};
  return true;
}

}  // namespace

bool archive_origin(const SearchPath& sp, const FileRef& file, ArchiveOrigin* out) {
  if (file.source >= sp.sources.size()) return false;
  const Source& s = sp.sources[file.source];
  if (s.kind == SourceKind::directory) return false;
  *out = ArchiveOrigin{};
  out->archive = fs::path(s.path).filename().string();
  // A partial a server is itself filling is no archive to fill from: its files go loose.
  if (out->archive.find(".partial.") != std::string::npos) return false;
  out->folder = s.game;
  out->index = file.entry;
  out->size = s.bytes.size();
  if (s.kind == SourceKind::pack) {
    if (file.entry >= s.pak.entries.size() || s.bytes.size() < kHeader) return false;
    out->count = static_cast<uint32_t>(s.pak.entries.size());
    out->offset = s.pak.entries[file.entry].offset;
    out->dir_offset = uint32_t(s.bytes[4]) | uint32_t(s.bytes[5]) << 8 |
                      uint32_t(s.bytes[6]) << 16 | uint32_t(s.bytes[7]) << 24;
  } else {
    if (file.entry >= s.zip.entries.size()) return false;
    out->count = static_cast<uint32_t>(s.zip.entries.size());
  }
  return true;
}

std::string partial_name(std::string_view archive) {
  const size_t dot = archive.rfind('.');
  if (!plain_file_name(archive) || dot == std::string_view::npos || dot == 0) return {};
  return std::string(archive.substr(0, dot)) + ".partial" + std::string(archive.substr(dot));
}

bool keep_in_partial(const std::string& dir, const ArchiveOrigin& o, std::string_view path,
                     std::span<const uint8_t> bytes, Kept* out, std::string* error) {
  const std::string part = partial_name(o.archive);
  if (part.empty())
    return fail(error, "the archive's name \"" + o.archive + "\" is not a file name");
  if (o.count == 0 || o.count > kMaxEntries || o.index >= o.count)
    return fail(error, "entry " + std::to_string(o.index) + " of " + std::to_string(o.count) +
                           " in " + o.archive + " does not hold together");
  if (!plain_path(path)) return fail(error, "the path \"" + std::string(path) + "\" is not plain");
  const bool pack = ends_with_any_case(o.archive, ".pak");
  if (pack && path.size() >= kPath)
    return fail(error, "the path \"" + std::string(path) + "\" is longer than a pack stores");
  const fs::path folder(dir), whole = folder / o.archive, partial = folder / part;
  std::error_code ec;
  if (fs::exists(whole, ec))
    return fail(error, o.archive + " is already in " + dir + ": the file is kept loose");
  fs::create_directories(folder, ec);
  if (pack && append_to_pack(partial, o, path, bytes, out)) return true;

  Partial p;
  if (!load(partial, pack, &p) || p.count != o.count || p.size != o.size ||
      (pack && p.dir != o.dir_offset))
    p = Partial{o.count, pack ? o.dir_offset : 0, o.size, {}};
  std::erase_if(p.held, [&](const Held& h) { return h.index == o.index || h.path == path; });
  p.held.push_back({o.index, o.offset, std::string(path), {bytes.begin(), bytes.end()}});
  std::sort(p.held.begin(), p.held.end(),
            [](const Held& a, const Held& b) { return a.index < b.index; });
  *out = Kept{};
  out->held = static_cast<uint32_t>(p.held.size());
  out->count = p.count;

  std::vector<uint8_t> file;
  if (p.held.size() == p.count) {
    out->whole = true;
    out->on_disk = whole.string();
    if (!pack || !lay_out_pack(p, &file)) {
      std::vector<std::pair<std::string, std::vector<uint8_t>>> files;
      files.reserve(p.held.size());
      for (Held& h : p.held) files.emplace_back(h.path, std::move(h.bytes));
      if (!pack)
        file = write_stored_zip(files);
      else if (!write_pak(files, &file, error))
        return false;
    }
    if (!write_atomic(whole, file, error)) return false;
    fs::remove(partial, ec);
    return true;
  }
  out->on_disk = partial.string();
  const std::string meta = meta_text(o, p);
  std::vector<std::pair<std::string, std::vector<uint8_t>>> files;
  files.reserve(p.held.size() + 1);
  for (const Held& h : p.held) files.emplace_back(h.path, h.bytes);
  files.emplace_back(kMeta, std::vector<uint8_t>(meta.begin(), meta.end()));
  if (!pack)
    file = write_stored_zip(files);
  else if (!write_pak(files, &file, error))
    return false;
  return write_atomic(partial, file, error);
}

void write_origin(std::string_view path, const ArchiveOrigin& origin, std::vector<uint8_t>* out) {
  for (const std::string_view t :
       {path, std::string_view(origin.archive), std::string_view(origin.folder)}) {
    out->insert(out->end(), t.begin(), t.end());
    out->push_back(0);
  }
  for (const uint32_t v : {origin.count, origin.index, static_cast<uint32_t>(origin.size),
                           origin.offset, origin.dir_offset})
    for (int k = 0; k < 4; ++k) out->push_back(static_cast<uint8_t>(v >> (8 * k)));
}

bool read_origin(std::span<const uint8_t> bytes, size_t* at, std::string* path,
                 ArchiveOrigin* out) {
  size_t i = *at;
  std::string* strings[3] = {path, &out->archive, &out->folder};
  std::string got[3];
  for (std::string& t : got) {
    while (i < bytes.size() && bytes[i] != 0) t.push_back(static_cast<char>(bytes[i++]));
    if (i >= bytes.size()) return false;
    ++i;
  }
  if (bytes.size() - i < 20) return false;
  uint32_t v[5];
  for (uint32_t& x : v) {
    x = 0;
    for (int k = 0; k < 4; ++k) x |= static_cast<uint32_t>(bytes[i++]) << (8 * k);
  }
  for (int k = 0; k < 3; ++k) *strings[k] = std::move(got[k]);
  out->count = v[0];
  out->index = v[1];
  out->size = v[2];
  out->offset = v[3];
  out->dir_offset = v[4];
  *at = i;
  return true;
}

}  // namespace ge::vfs
