// The client's search path (files.hpp).
//
// ge:summary The client's search path over a game folder's loose files and numbered packs
// ge:keywords search-path pak open_game_dir find_file add_archive
// ge:skill vfs shooter
// ge:test tests/shooters_edge_listen_test.cpp
#include "shooters_edge/client/files.hpp"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <system_error>

namespace shooters_edge::client {

namespace fs = std::filesystem;

namespace {

bool read_whole(const fs::path& p, std::vector<uint8_t>& out) {
  std::ifstream f(p, std::ios::binary);
  if (!f) return false;
  out.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
  return true;
}

// `dir/name` when it exists as written, else the entry whose name matches it ignoring case (a
// pack copied off a disc can be PAK0.PAK).
fs::path find_ci(const fs::path& dir, const std::string& name) {
  std::error_code ec;
  if (fs::exists(dir / name, ec)) return dir / name;
  for (const fs::directory_entry& e : fs::directory_iterator(dir, ec)) {
    std::string n = e.path().filename().string();
    if (n.size() != name.size()) continue;
    bool same = true;
    for (size_t i = 0; i < n.size() && same; i++) {
      const char a = static_cast<char>(n[i] >= 'A' && n[i] <= 'Z' ? n[i] + 32 : n[i]);
      same = a == name[i];
    }
    if (same) return e.path();
  }
  return {};
}

}  // namespace

bool add_archive(SearchPath& path, std::string name, std::vector<uint8_t> bytes,
                 std::string& error) {
  SearchPath::Archive a;
  a.path = std::move(name);
  a.bytes = std::move(bytes);
  std::string why;
  if (!ge::vfs::read_pak(a.bytes, &a.dir, &why)) {
    error = a.path + " is not a pack the game can read: " + why +
            "; next: replace it with an undamaged copy, or move it out of the game folder";
    return false;
  }
  path.archives.push_back(std::move(a));
  return true;
}

bool open_game_dir(const std::string& basedir, const std::string& game, SearchPath& out,
                   std::string& error) {
  out = SearchPath{};
  const fs::path dir = fs::path(basedir.empty() ? "." : basedir) / game;
  std::error_code ec;
  if (!fs::is_directory(dir, ec)) {
    error = "the game folder " + dir.string() +
            " does not exist; next: pass -basedir <the folder that holds " + game + "/pak0.pak>";
    return false;
  }
  out.folder = dir.string();
  for (int n = 0;; n++) {
    const fs::path p = find_ci(dir, "pak" + std::to_string(n) + ".pak");
    if (p.empty()) break;
    std::vector<uint8_t> bytes;
    if (!read_whole(p, bytes)) {
      error = "cannot read " + p.string() + "; next: check its permissions";
      return false;
    }
    if (!add_archive(out, p.string(), std::move(bytes), error)) return false;
  }
  return true;
}

std::span<const uint8_t> find_file(const SearchPath& path, std::string_view name,
                                   std::vector<uint8_t>& store) {
  if (!path.folder.empty()) {
    const fs::path loose = fs::path(path.folder) / std::string(name);
    std::error_code ec;
    if (fs::is_regular_file(loose, ec) && read_whole(loose, store)) return store;
  }
  for (size_t i = path.archives.size(); i-- > 0;) {
    const SearchPath::Archive& a = path.archives[i];
    if (const ge::vfs::PakEntry* e = ge::vfs::pak_find(a.dir, name))
      return ge::vfs::pak_file(a.bytes, *e);
  }
  return {};
}

}  // namespace shooters_edge::client
