// The client's search path (files.hpp).
//
// ge:summary The client's search path over a game folder's loose files and numbered packs
// ge:keywords search-path pak open_game_dir find_file add_archive write_save read_save
// ge:skill vfs shooter
// ge:test tests/shooters_edge_listen_test.cpp
#include "shooters_edge/client/files.hpp"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <set>
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

std::vector<std::string> map_names(const SearchPath& path) {
  std::set<std::string> names;
  const auto take = [&](std::string_view p) {
    if (p.rfind("maps/", 0) != 0 || p.size() <= 9 || p.substr(p.size() - 4) != ".bsp") return;
    const std::string_view n = p.substr(5, p.size() - 9);
    if (n.find('/') == std::string_view::npos) names.emplace(n);
  };
  for (const SearchPath::Archive& a : path.archives)
    for (const ge::vfs::PakEntry& e : a.dir.entries) take(e.path);
  std::error_code ec;
  if (!path.folder.empty())
    for (const fs::directory_entry& e : fs::directory_iterator(fs::path(path.folder) / "maps", ec))
      if (e.is_regular_file(ec)) take("maps/" + e.path().filename().string());
  std::vector<std::string> out;
  if (names.erase("start") > 0) out.emplace_back("start");
  out.insert(out.end(), names.begin(), names.end());
  return out;
}

namespace {

// The save's file, or "" with `error` when the name or the folder will not do.
fs::path save_file(const SearchPath& path, const std::string& name, std::string& error) {
  bool ok = !name.empty() && name.size() <= 64;
  for (const char c : name)
    ok = ok && ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
                c == '_' || c == '-');
  if (!ok) {
    error = "the save's name `" + name + "` is not 1..64 letters, digits, _ or -; next: `save s1`";
    return {};
  }
  if (path.folder.empty()) {
    error = "no game folder to keep saves in; next: start the game with -basedir";
    return {};
  }
  return fs::path(path.folder) / "saves" / (name + ".save");
}

}  // namespace

bool write_save(const SearchPath& path, const std::string& name, std::span<const uint8_t> bytes,
                std::string& error) {
  const fs::path file = save_file(path, name, error);
  if (file.empty()) return false;
  std::error_code ec;
  fs::create_directories(file.parent_path(), ec);
  fs::path tmp = file;
  tmp += ".tmp";
  {
    std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
    f.write(reinterpret_cast<const char*>(bytes.data()),
            static_cast<std::streamsize>(bytes.size()));
    f.flush();
    if (!f) {
      error = "could not write " + tmp.string() +
              "; next: check the folder is writable and the "
              "disk has room";
      return false;
    }
  }
  fs::rename(tmp, file, ec);
  if (ec) {
    error = "could not put the save at " + file.string() + ": " + ec.message() +
            "; next: check the folder is writable";
    return false;
  }
  return true;
}

bool read_save(const SearchPath& path, const std::string& name, std::vector<uint8_t>& out,
               std::string& error) {
  const fs::path file = save_file(path, name, error);
  if (file.empty()) return false;
  if (!read_whole(file, out)) {
    error = "no save " + file.string() + "; next: `save " + name + "` first, or check the name";
    return false;
  }
  return true;
}

}  // namespace shooters_edge::client
