// The search path (search.hpp): mounting game directories and archives, the folded-name index,
// reading a winner.
//
// ge:summary The search path's mounts, its folded-name hash index and its readers
// ge:keywords vfs search-path mount index hash fold read view list pk3dir
// ge:skill vfs
// ge:test tests/vfs_search_test.cpp
#include "vfs/search.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <system_error>
#include <utility>

namespace ge::vfs {
namespace {

namespace fs = std::filesystem;

char fold(char c) {
  if (c == '\\') return '/';
  return c >= 'A' && c <= 'Z' ? char(c - 'A' + 'a') : c;
}

uint64_t hash_of(std::string_view name) {
  uint64_t h = 0xcbf29ce484222325ull;
  for (char c : name) {
    h ^= uint8_t(fold(c));
    h *= 0x100000001b3ull;
  }
  return h;
}

bool folded_equal(std::string_view stored, std::string_view query) {
  if (stored.size() != query.size()) return false;
  for (size_t i = 0; i < stored.size(); ++i) {
    if (stored[i] != fold(query[i])) return false;
  }
  return true;
}

// The slot `name` sits in, or the empty slot it would take.
size_t probe(const SearchPath& sp, std::string_view name, uint64_t h) {
  const size_t mask = sp.slots.size() - 1;
  for (size_t at = size_t(h) & mask;; at = (at + 1) & mask) {
    const SearchPath::Slot& s = sp.slots[at];
    if (!s.used) return at;
    if (s.hash == h && folded_equal({sp.names.data() + s.name_at, s.name_len}, name)) return at;
  }
}

void grow(SearchPath& sp) {
  std::vector<SearchPath::Slot> old = std::move(sp.slots);
  sp.slots.assign(old.empty() ? 64 : old.size() * 2, SearchPath::Slot{});
  for (const SearchPath::Slot& s : old) {
    if (!s.used) continue;
    const std::string_view name{sp.names.data() + s.name_at, s.name_len};
    sp.slots[probe(sp, name, s.hash)] = s;
  }
}

// Files the source `src` holds become the winners of their names.
void index_source(SearchPath& sp, uint32_t src) {
  const Source& s = sp.sources[src];
  const auto add = [&](std::string_view name, uint32_t entry, uint64_t size) {
    if (name.empty()) return;
    if ((sp.files + 1) * 2 > sp.slots.size()) grow(sp);
    const uint64_t h = hash_of(name);
    SearchPath::Slot& slot = sp.slots[probe(sp, name, h)];
    if (!slot.used) {
      slot.used = true;
      slot.hash = h;
      slot.name_at = uint32_t(sp.names.size());
      slot.name_len = uint32_t(name.size());
      for (char c : name) sp.names.push_back(fold(c));
      ++sp.files;
    }
    slot.ref = FileRef{src, entry, size};
  };
  switch (s.kind) {
    case SourceKind::pack:
      for (size_t i = 0; i < s.pak.entries.size(); ++i) {
        add(s.pak.entries[i].path, uint32_t(i), s.pak.entries[i].size);
      }
      break;
    case SourceKind::zip:
      for (size_t i = 0; i < s.zip.entries.size(); ++i) {
        const ZipEntry& e = s.zip.entries[i];
        if (!e.name.empty() && (e.name.back() == '/' || e.name.back() == '\\')) continue;
        add(e.name, uint32_t(i), e.size);
      }
      break;
    case SourceKind::directory:
      for (size_t i = 0; i < s.files.size(); ++i) add(s.files[i], uint32_t(i), s.file_sizes[i]);
      break;
  }
}

std::string lower(std::string s) {
  for (char& c : s) c = fold(c);
  return s;
}

bool ends_with(const std::string& s, std::string_view tail) {
  return s.size() >= tail.size() && s.compare(s.size() - tail.size(), tail.size(), tail) == 0;
}

// `pak<digits>.pak`'s number, or -1.
int64_t pak_number(const std::string& folded) {
  if (folded.size() < 8 || folded.size() > 13 || folded.compare(0, 3, "pak") != 0 ||
      !ends_with(folded, ".pak")) {
    return -1;
  }
  int64_t n = 0;
  for (size_t i = 3; i < folded.size() - 4; ++i) {
    if (folded[i] < '0' || folded[i] > '9') return -1;
    n = n * 10 + (folded[i] - '0');
  }
  return n;
}

bool read_disk(const std::string& path, std::vector<uint8_t>* out, std::string* error) {
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    if (error) *error = "vfs: " + path + " cannot be opened; next: check it exists and is readable";
    return false;
  }
  out->assign(std::istreambuf_iterator<char>(in), {});
  return true;
}

// Every file under `dir` ('/' separated, relative) in name order; with `skip_archives`, not the
// game directory's own packs, pk3s and what is inside its pk3dirs.
bool walk(const fs::path& dir, bool skip_archives, std::vector<std::string>* files,
          std::vector<uint64_t>* sizes, std::string* error) {
  std::error_code ec;
  std::vector<std::pair<std::string, uint64_t>> found;
  fs::recursive_directory_iterator it(dir, ec), end;
  for (; !ec && it != end; it.increment(ec)) {
    const fs::path rel = it->path().lexically_relative(dir);
    const std::string top = lower(rel.begin()->string());
    if (skip_archives && it->is_directory(ec) && it.depth() == 0 && ends_with(top, ".pk3dir")) {
      it.disable_recursion_pending();
      continue;
    }
    if (!it->is_regular_file(ec)) continue;
    if (skip_archives && it.depth() == 0 && (pak_number(top) >= 0 || ends_with(top, ".pk3"))) {
      continue;
    }
    found.emplace_back(rel.generic_string(), uint64_t(it->file_size(ec)));
  }
  if (ec) {
    if (error)
      *error = "vfs: " + dir.string() + ": " + ec.message() + "; next: check the directory";
    return false;
  }
  std::sort(found.begin(), found.end());
  for (auto& [name, size] : found) {
    files->push_back(std::move(name));
    sizes->push_back(size);
  }
  return true;
}

}  // namespace

bool add_archive(SearchPath* sp, const std::string& name, std::vector<uint8_t> bytes,
                 const std::string& game, std::string* error) {
  Source s;
  s.path = name;
  s.game = game;
  s.bytes = std::move(bytes);
  std::string why;
  const bool pack = s.bytes.size() >= 4 && s.bytes[0] == 'P' && s.bytes[1] == 'A' &&
                    s.bytes[2] == 'C' && s.bytes[3] == 'K';
  if (pack) {
    s.kind = SourceKind::pack;
    if (!read_pak(s.bytes, &s.pak, &why)) {
      if (error) *error = "vfs: " + name + ": " + why;
      return false;
    }
  } else {
    s.kind = SourceKind::zip;
    if (!read_zip(s.bytes, &s.zip, &why)) {
      if (error) {
        *error = "vfs: " + name + ": not a pack, and as a zip: " + why +
                 "; next: fetch the archive again";
      }
      return false;
    }
  }
  sp->sources.push_back(std::move(s));
  index_source(*sp, uint32_t(sp->sources.size() - 1));
  return true;
}

bool add_directory(SearchPath* sp, const std::string& dir, const std::string& game,
                   std::string* error) {
  Source s;
  s.kind = SourceKind::directory;
  s.path = dir;
  s.game = game;
  if (!walk(fs::path(dir), false, &s.files, &s.file_sizes, error)) return false;
  sp->sources.push_back(std::move(s));
  index_source(*sp, uint32_t(sp->sources.size() - 1));
  return true;
}

bool add_game(SearchPath* sp, const std::string& root, const std::string& game,
              std::string* error) {
  const fs::path dir = fs::path(root) / game;
  std::error_code ec;
  if (!fs::is_directory(dir, ec)) {
    if (error) {
      *error = "vfs: the game directory " + dir.string() +
               " is not there; next: check the root and the game's name";
    }
    return false;
  }
  std::vector<std::pair<int64_t, std::string>> packs;
  std::vector<std::pair<std::string, std::string>> others;  // folded name, name on disk
  for (const fs::directory_entry& e : fs::directory_iterator(dir, ec)) {
    const std::string name = e.path().filename().string(), folded = lower(name);
    if (e.is_regular_file(ec) && pak_number(folded) >= 0) {
      packs.emplace_back(pak_number(folded), name);
    } else if ((e.is_regular_file(ec) && ends_with(folded, ".pk3")) ||
               (e.is_directory(ec) && ends_with(folded, ".pk3dir"))) {
      others.emplace_back(folded, name);
    }
  }
  std::sort(packs.begin(), packs.end());
  std::sort(others.begin(), others.end());
  for (const auto& [n, name] : packs) {
    std::vector<uint8_t> bytes;
    if (!read_disk((dir / name).string(), &bytes, error)) return false;
    if (!add_archive(sp, (dir / name).string(), std::move(bytes), game, error)) return false;
  }
  for (const auto& [folded, name] : others) {
    const fs::path at = dir / name;
    if (ends_with(folded, ".pk3dir")) {
      if (!add_directory(sp, at.string(), game, error)) return false;
      continue;
    }
    std::vector<uint8_t> bytes;
    if (!read_disk(at.string(), &bytes, error)) return false;
    if (!add_archive(sp, at.string(), std::move(bytes), game, error)) return false;
  }
  Source loose;
  loose.kind = SourceKind::directory;
  loose.path = dir.string();
  loose.game = game;
  if (!walk(dir, true, &loose.files, &loose.file_sizes, error)) return false;
  sp->sources.push_back(std::move(loose));
  index_source(*sp, uint32_t(sp->sources.size() - 1));
  return true;
}

const FileRef* find(const SearchPath& sp, std::string_view path) {
  if (sp.slots.empty() || path.empty()) return nullptr;
  const SearchPath::Slot& s = sp.slots[probe(sp, path, hash_of(path))];
  return s.used ? &s.ref : nullptr;
}

std::span<const uint8_t> view(const SearchPath& sp, const FileRef& file) {
  const Source& s = sp.sources[file.source];
  if (s.kind == SourceKind::pack) return pak_file(s.bytes, s.pak.entries[file.entry]);
  if (s.kind == SourceKind::zip) {
    const ZipEntry& e = s.zip.entries[file.entry];
    // The reader checked the packed bytes lie inside the archive; a stored entry whose two sizes
    // disagree is damaged, and `read` refuses it on its checksum.
    if (e.method == 0 && e.packed == e.size) {
      return std::span<const uint8_t>(s.bytes).subspan(e.data, e.packed);
    }
  }
  return {};
}

bool read(const SearchPath& sp, const FileRef& file, std::vector<uint8_t>* out,
          std::string* error) {
  const Source& s = sp.sources[file.source];
  switch (s.kind) {
    case SourceKind::pack: {
      const std::span<const uint8_t> b = view(sp, file);
      out->assign(b.begin(), b.end());
      return true;
    }
    case SourceKind::zip: {
      std::string why;
      if (!zip_unpack(s.bytes, s.zip.entries[file.entry], out, &why)) {
        if (error) *error = "vfs: " + s.path + ": " + why;
        return false;
      }
      return true;
    }
    case SourceKind::directory:
      return read_disk((fs::path(s.path) / s.files[file.entry]).string(), out, error);
  }
  return false;
}

std::string_view stored_name(const SearchPath& sp, const FileRef& file) {
  const Source& s = sp.sources[file.source];
  switch (s.kind) {
    case SourceKind::pack:
      return s.pak.entries[file.entry].path;
    case SourceKind::zip:
      return s.zip.entries[file.entry].name;
    case SourceKind::directory:
      return s.files[file.entry];
  }
  return {};
}

std::vector<Listed> list(const SearchPath& sp) {
  std::vector<Listed> out;
  out.reserve(sp.files);
  for (const SearchPath::Slot& s : sp.slots) {
    if (s.used) out.push_back({{sp.names.data() + s.name_at, s.name_len}, s.ref});
  }
  std::sort(out.begin(), out.end(),
            [](const Listed& a, const Listed& b) { return a.name < b.name; });
  return out;
}

}  // namespace ge::vfs
