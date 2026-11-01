// The client's search path (files.hpp).
//
// ge:summary The client's search path over a game folder's loose files and numbered packs
// ge:keywords search-path pak open_game_dir find_file add_archive write_save read_save mod
// ge:skill vfs shooter
// ge:test tests/shooters_edge_files_test.cpp
#include "shooters_edge/client/files.hpp"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <set>
#include <system_error>

#include "classicenhance/profile.hpp"
#include "save/file.hpp"

namespace shooters_edge::client {

namespace fs = std::filesystem;

namespace {

bool read_whole(const fs::path& p, std::vector<uint8_t>& out) {
  std::ifstream f(p, std::ios::binary);
  if (!f) return false;
  out.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
  return true;
}

}  // namespace

bool add_archive(SearchPath& path, const std::string& name, std::vector<uint8_t> bytes,
                 std::string& error) {
  std::string why;
  if (!ge::vfs::add_archive(&path.vfs, name, std::move(bytes), "", &why)) {
    error = name + " is not an archive the game can read: " + why +
            "; next: replace it with an undamaged copy, or move it out of the game folder";
    return false;
  }
  return true;
}

bool open_game_dir(const std::string& basedir, const std::string& game, SearchPath& out,
                   std::string& error) {
  out = SearchPath{};
  const fs::path root = basedir.empty() ? "." : basedir;
  const std::string base(ge::classicenhance::classic_profile().base_dir);
  std::error_code ec;
  if (!fs::is_directory(root / game, ec)) {
    error = "the game folder " + (root / game).string() +
            " does not exist; next: pass -basedir <the folder that holds " + base + "/" +
            ge::classicenhance::pack_name(ge::classicenhance::classic_profile(), 0) + ">";
    return false;
  }
  std::string why;
  // The base game under a mod: its files are the mod's unless the mod has its own.
  if (game != base && fs::is_directory(root / base, ec) &&
      !ge::vfs::add_game(&out.vfs, root.string(), base, &why)) {
    error =
        why + "; next: replace the damaged archive, or move it out of " + (root / base).string();
    return false;
  }
  if (!ge::vfs::add_game(&out.vfs, root.string(), game, &why)) {
    error =
        why + "; next: replace the damaged archive, or move it out of " + (root / game).string();
    return false;
  }
  out.folder = (root / game).string();
  return true;
}

std::span<const uint8_t> find_file(const SearchPath& path, std::string_view name,
                                   std::vector<uint8_t>& store) {
  // A loose file in the game folder first, as it is on disk now (one written since the mount).
  if (!path.folder.empty()) {
    const fs::path loose = fs::path(path.folder) / std::string(name);
    std::error_code ec;
    if (fs::is_regular_file(loose, ec) && read_whole(loose, store)) return store;
  }
  const ge::vfs::FileRef* ref = ge::vfs::find(path.vfs, name);
  if (ref == nullptr) return {};
  if (const std::span<const uint8_t> v = ge::vfs::view(path.vfs, *ref); !v.empty()) return v;
  std::string why;
  if (!ge::vfs::read(path.vfs, *ref, &store, &why)) return {};
  return store;
}

std::vector<std::string> map_names(const SearchPath& path) {
  const ge::classicenhance::AssetKind& maps = ge::classicenhance::classic_profile().maps;
  std::set<std::string> names;
  for (const ge::vfs::Listed& l : ge::vfs::list(path.vfs)) {
    const std::string_view n = ge::classicenhance::asset_name(maps, l.name);
    if (!n.empty() && n.find('/') == std::string_view::npos) names.emplace(n);
  }
  std::vector<std::string> out;
  if (names.erase(std::string(ge::classicenhance::classic_profile().start_map)) > 0)
    out.emplace_back(ge::classicenhance::classic_profile().start_map);
  out.insert(out.end(), names.begin(), names.end());
  return out;
}

std::vector<std::string> archive_paths(const SearchPath& path) {
  std::vector<std::string> out;
  for (const ge::vfs::Source& s : path.vfs.sources)
    if (s.kind != ge::vfs::SourceKind::directory) out.push_back(s.path);
  return out;
}

std::vector<std::span<const uint8_t>> content_packs(const SearchPath& path) {
  const ge::classicenhance::Profile& p = ge::classicenhance::classic_profile();
  // A numbered pack by its file name ("pak3.pak", any case); a stand-in pack is not one.
  const auto numbered = [&p](const std::string& at) {
    std::string n = fs::path(at).filename().string();
    for (char& c : n) c = c >= 'A' && c <= 'Z' ? char(c - 'A' + 'a') : c;
    if (n.size() <= p.pack_stem.size() + p.pack_ext.size() || !n.starts_with(p.pack_stem) ||
        !n.ends_with(p.pack_ext))
      return false;
    const std::string_view digits = std::string_view(n).substr(
        p.pack_stem.size(), n.size() - p.pack_stem.size() - p.pack_ext.size());
    for (const char c : digits)
      if (c < '0' || c > '9') return false;
    return true;
  };
  std::vector<std::span<const uint8_t>> out;
  const std::string* game = nullptr;
  for (const ge::vfs::Source& s : path.vfs.sources) {
    if (s.kind != ge::vfs::SourceKind::pack || !numbered(s.path)) continue;
    if (game == nullptr) game = &s.game;
    if (s.game != *game) break;
    out.emplace_back(s.bytes);
  }
  return out;
}

namespace {

// The save's file, or "" with `error` when the name or the folder will not do.
fs::path save_file(const SearchPath& path, const std::string& name, bool classic,
                   std::string& error) {
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
  return classic ? fs::path(path.folder) / (name + ".sav")
                 : fs::path(path.folder) / "saves" / (name + ".save");
}

}  // namespace

bool write_save(const SearchPath& path, const std::string& name, std::span<const uint8_t> bytes,
                std::string& error, bool classic) {
  const fs::path file = save_file(path, name, classic, error);
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
               std::string& error, bool classic) {
  const fs::path file = save_file(path, name, classic, error);
  if (file.empty()) return false;
  if (!read_whole(file, out)) {
    error = "no save " + file.string() + "; next: `save " + name + "` first, or check the name";
    return false;
  }
  return true;
}

bool classic_save_name(std::string& name) {
  constexpr std::string_view kExt = ".sav";
  if (name.size() <= kExt.size() || name.compare(name.size() - kExt.size(), kExt.size(), kExt) != 0)
    return false;
  name.resize(name.size() - kExt.size());
  return true;
}

std::string save_label(const SearchPath& path, const std::string& name) {
  std::string error;
  const fs::path ours = save_file(path, name, false, error);
  if (ours.empty()) return {};
  {
    std::ifstream f(ours, std::ios::binary);
    std::vector<uint8_t> head(ge::save::kPeekBytes);
    if (f) {
      f.read(reinterpret_cast<char*>(head.data()), static_cast<std::streamsize>(head.size()));
      head.resize(static_cast<size_t>(f.gcount()));
      ge::save::Header h;
      ge::save::Meta m;
      if (ge::save::peek(head, h, m, error) && !m.title.empty()) return m.title;
    }
  }
  std::ifstream f(save_file(path, name, true, error));
  std::string line;
  if (!f || !std::getline(f, line) || !std::getline(f, line)) return {};
  if (!line.empty() && line.back() == '\r') line.pop_back();
  for (char& c : line) c = c == '_' ? ' ' : c;
  return line;
}

}  // namespace shooters_edge::client
