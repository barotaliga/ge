// The client's search path (files.hpp).
//
// ge:summary The client's search path over a game folder's loose files and numbered packs
// ge:keywords search-path pak open_game_dir find_file add_archive write_save read_save mod
// ge:skill vfs shooter
// ge:test tests/shooters_edge_files_test.cpp
#include "shooters_edge/client/files.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <set>
#include <system_error>

#include "classicenhance/profile.hpp"
#include "save/file.hpp"
#include "shooters_edge/client/install_configs.hpp"
#include "shooters_edge/client/load_order.hpp"
#include "vfs/zip.hpp"

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
            ". Replace it with an undamaged copy, or move it out of the game folder.";
    return false;
  }
  return true;
}

bool open_game_dir(const std::string& basedir, const std::string& game, SearchPath& out,
                   std::string& error, const ge::classicenhance::Profile& content) {
  out = SearchPath{};
  const fs::path root = basedir.empty() ? "." : basedir;
  const std::string base(content.base_dir);
  std::error_code ec;
  if (!fs::is_directory(root / game, ec)) {
    error = "the game folder " + (root / game).string() +
            " does not exist. Start the game with -basedir <the folder that holds " + base + "/" +
            ge::classicenhance::pack_name(content, 0) + ">.";
    return false;
  }
  std::string why;
  const LoadOrder& order = load_order_of(content);
  out.loose_first = !order.layout.archives_over_loose;
  // The archive at the install's root, under everything (the sequel's rerelease keeps one).
  if (const fs::path at = root / std::string(order.root_archive);
      !order.root_archive.empty() && fs::is_regular_file(at, ec)) {
    std::vector<uint8_t> bytes;
    if (!read_whole(at, bytes) || !add_archive(out, at.string(), std::move(bytes), error))
      return false;
  }
  // The base game under a mod: its files are the mod's unless the mod has its own.
  if (game != base && fs::is_directory(root / base, ec) &&
      !ge::vfs::add_game(&out.vfs, root.string(), base, &why, order.layout)) {
    error =
        why + ". Replace the damaged archive, or move it out of " + (root / base).string() + ".";
    return false;
  }
  // The bundled client's folder between them, as its client plays it (install_configs.hpp).
  if (const fs::path client = client_folder(root, content);
      !client.empty() && client.filename() != game &&
      !ge::vfs::add_game(&out.vfs, root.string(), client.filename().string(), &why, order.layout)) {
    error = why + ". Replace the damaged archive, or move it out of " + client.string() + ".";
    return false;
  }
  if (!ge::vfs::add_game(&out.vfs, root.string(), game, &why, order.layout)) {
    error =
        why + ". Replace the damaged archive, or move it out of " + (root / game).string() + ".";
    return false;
  }
  out.folder = (root / game).string();
  return true;
}

std::span<const uint8_t> find_file(const SearchPath& path, std::string_view name,
                                   std::vector<uint8_t>& store) {
  // A loose file in the game folder, as it is on disk now (one written since the mount): first
  // where loose files win, else only when no archive holds the name.
  const auto loose = [&]() -> std::span<const uint8_t> {
    if (path.folder.empty()) return {};
    const fs::path at = fs::path(path.folder) / std::string(name);
    std::error_code ec;
    if (fs::is_regular_file(at, ec) && read_whole(at, store)) return store;
    return {};
  };
  if (path.loose_first)
    if (const std::span<const uint8_t> b = loose(); !b.empty()) return b;
  const ge::vfs::FileRef* ref = ge::vfs::find(path.vfs, name);
  if (ref == nullptr) return path.loose_first ? std::span<const uint8_t>{} : loose();
  if (!path.loose_first && path.vfs.sources[ref->source].kind == ge::vfs::SourceKind::directory)
    if (const std::span<const uint8_t> b = loose(); !b.empty()) return b;
  if (const std::span<const uint8_t> v = ge::vfs::view(path.vfs, *ref); !v.empty()) return v;
  std::string why;
  if (!ge::vfs::read(path.vfs, *ref, &store, &why)) return {};
  return store;
}

std::span<const uint8_t> game_palette(const SearchPath& path, std::vector<uint8_t>& store,
                                      const ge::classicenhance::Profile& content) {
  return ge::classicenhance::palette_of(content, find_file(path, content.palette, store));
}

std::span<const uint8_t> map_lit(const SearchPath& path, std::string_view map,
                                 std::vector<uint8_t>& store, std::string* found,
                                 const ge::classicenhance::Profile& content) {
  std::string beside = ge::classicenhance::asset_path(content.maps, map);
  if (const size_t dot = beside.rfind('.'); dot != std::string::npos) beside.resize(dot);
  beside += ".lit";
  const std::string kept = "lits/" + std::string(map) + ".lit";
  const std::string under = "maps/lits/" + std::string(map) + ".lit";
  for (const std::string& at : {under, beside, kept}) {
    const std::span<const uint8_t> bytes = find_file(path, at, store);
    if (bytes.empty()) continue;
    if (found != nullptr) *found = at;
    return bytes;
  }
  return {};
}

bool sky_box_files(const SearchPath& path, std::string_view name, SkyBoxFiles* out,
                   std::string* missing) {
  static constexpr const char* kSuffixes[6] = {"rt", "bk", "lf", "ft", "up", "dn"};
  if (name.empty() || name.find_first_of("./\\") != std::string_view::npos) {
    if (missing != nullptr) *missing = kSuffixes[0];
    return false;
  }
  const std::string n(name);
  for (size_t i = 0; i < 6; ++i) {
    const std::string sfx = kSuffixes[i];
    bool found = false;
    for (const char* form : {"env/@", "gfx/env/@", "env/@_", "gfx/env/@_"}) {
      // The folder, the name, an underscore or none, then the suffix.
      std::string base(form);
      base.replace(base.find('@'), 1, n);
      base.append(sfx);
      for (const char* ext : {".tga", ".png", ".jpg", ".pcx"}) {
        std::vector<uint8_t> store;
        std::string at = base;
        at.append(ext);
        const std::span<const uint8_t> bytes = find_file(path, at, store);
        if (bytes.empty()) continue;
        out->bytes[i].assign(bytes.begin(), bytes.end());
        out->paths[i] = at;
        found = true;
        break;
      }
      if (found) break;
    }
    if (!found) {
      if (missing != nullptr) *missing = sfx;
      return false;
    }
  }
  return true;
}

std::vector<std::string> map_names(const SearchPath& path,
                                   const ge::classicenhance::Profile& content) {
  const ge::classicenhance::AssetKind& maps = content.maps;
  std::set<std::string> names;
  for (const ge::vfs::Listed& l : ge::vfs::list(path.vfs)) {
    const std::string_view n = ge::classicenhance::asset_name(maps, l.name);
    if (!n.empty() && n.find('/') == std::string_view::npos) names.emplace(n);
  }
  std::vector<std::string> out;
  if (names.erase(std::string(content.start_map)) > 0) out.emplace_back(content.start_map);
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
  const auto name_of = [](const std::string& at) {
    std::string n = fs::path(at).filename().string();
    for (char& c : n) c = c >= 'A' && c <= 'Z' ? char(c - 'A' + 'a') : c;
    return n;
  };
  // A numbered pack by its file name ("pak3.pak", any case); a stand-in pack is not one.
  const auto numbered = [&p](const std::string& n) {
    if (n.size() <= p.pack_stem.size() + p.pack_ext.size() || !n.starts_with(p.pack_stem) ||
        !n.ends_with(p.pack_ext))
      return false;
    const std::string_view digits = std::string_view(n).substr(
        p.pack_stem.size(), n.size() - p.pack_stem.size() - p.pack_ext.size());
    for (const char c : digits)
      if (c < '0' || c > '9') return false;
    return true;
  };
  if (path.vfs.sources.empty()) return {};
  // The base game's folder: the first mounted.
  const std::string& game = path.vfs.sources.front().game;
  std::vector<std::span<const uint8_t>> out;
  bool first = false;  // its first pack among them
  std::span<const uint8_t> zip;
  for (const ge::vfs::Source& s : path.vfs.sources) {
    if (s.game != game) break;
    const std::string n = name_of(s.path);
    if (s.kind == ge::vfs::SourceKind::pack && numbered(n)) {
      out.emplace_back(s.bytes);
      first = first || n == ge::classicenhance::pack_name(p, 0);
    } else if (s.kind == ge::vfs::SourceKind::zip && n == ge::classicenhance::zip_name(p, 0)) {
      zip = s.bytes;
    }
  }
  // No first pack: the content zip standing in for it (`pak0.pk3`) is the first, as the pipeline
  // takes it, keyed by the zip's own bytes.
  if (!first && !zip.empty()) out.insert(out.begin(), zip);
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
    error = "\"" + name + "\" can't be a save's name: use 1 to 64 letters, digits, _ or -";
    return {};
  }
  if (path.folder.empty()) {
    error = "There is no game folder to keep saves in (start the game with -basedir).";
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
              ". Check that the folder is writable and the disk has room.";
      return false;
    }
  }
  fs::rename(tmp, file, ec);
  if (ec) {
    error = "could not put the save at " + file.string() + ": " + ec.message() +
            ". Check that the folder is writable.";
    return false;
  }
  return true;
}

bool read_save(const SearchPath& path, const std::string& name, std::vector<uint8_t>& out,
               std::string& error, bool classic) {
  const fs::path file = save_file(path, name, classic, error);
  if (file.empty()) return false;
  if (!read_whole(file, out)) {
    error = "There is no save called \"" + name + "\" (" + file.string() + ").";
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

std::shared_ptr<const ge::progs::LocalizedText> localized_text(const SearchPath& path) {
  constexpr std::string_view kFile = "localization/loc_english.txt";
  std::vector<uint8_t> store;
  const std::span<const uint8_t> loose = find_file(path, kFile, store);
  auto table = std::make_shared<ge::progs::LocalizedText>();
  if (!loose.empty()) {
    ge::progs::read_localized(
        std::string_view(reinterpret_cast<const char*>(loose.data()), loose.size()), *table);
    return table;
  }
  if (path.folder.empty()) return nullptr;
  // The release's own archives (`*.kpf`, zips) beside the game folder: large and unchanging under a
  // running game, so read once, kept by the folder.
  const fs::path beside = fs::path(path.folder).parent_path();
  static std::string cached_folder;
  static std::shared_ptr<const ge::progs::LocalizedText> cached;
  if (cached_folder == beside.string()) return cached;
  cached_folder = beside.string();
  cached = nullptr;
  std::error_code ec;
  std::vector<fs::path> archives;
  for (const fs::directory_entry& e : fs::directory_iterator(beside, ec)) {
    std::string ext = e.path().extension().string();
    for (char& c : ext) c = c >= 'A' && c <= 'Z' ? static_cast<char>(c + 32) : c;
    if (ext == ".kpf" && e.is_regular_file(ec)) archives.push_back(e.path());
  }
  std::sort(archives.begin(), archives.end());
  for (const fs::path& kpf : archives) {
    std::ifstream in(kpf, std::ios::binary);
    const std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(in)),
                                     std::istreambuf_iterator<char>());
    ge::vfs::Zip zip;
    std::string error;
    if (!ge::vfs::read_zip(bytes, &zip, &error)) continue;
    const ge::vfs::ZipEntry* e = ge::vfs::zip_find(zip, kFile);
    std::vector<uint8_t> text;
    if (e == nullptr || !ge::vfs::zip_unpack(bytes, *e, &text, &error)) continue;
    ge::progs::read_localized(
        std::string_view(reinterpret_cast<const char*>(text.data()), text.size()), *table);
    break;
  }
  if (table->words.empty()) return nullptr;
  cached = table;
  return cached;
}

}  // namespace shooters_edge::client
