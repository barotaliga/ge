// A demo's content pack, made and served (content.hpp).
//
// ge:summary A demo's or a map's content pack made from the viewer folder's packs, served by hash
// ge:keywords hub content pack demo files zstd pak cache
// ge:skill net
// ge:test tests/hub_content_test.cpp
#include "hub/content.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <mutex>
#include <set>
#include <utility>
#include <vector>

#include "hub/archive.hpp"
#include "mod/sha256.hpp"
#include "vfs/pak.hpp"
#include "vfs/search.hpp"
#include "vfs/zstd.hpp"

namespace ge::hub {

namespace {

namespace fs = std::filesystem;

constexpr int kContentMaxAgeS = 365 * 24 * 3600;  // a pack's name is its hash: it never changes

bool is_hex64(std::string_view s) {
  return s.size() == 64 && std::all_of(s.begin(), s.end(), [](char c) {
           return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
         });
}

bool read_file(const fs::path& p, std::vector<uint8_t>& out) {
  std::ifstream f(p, std::ios::binary);
  if (!f) return false;
  out.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
  return true;
}

bool write_file(const fs::path& p, std::span<const uint8_t> bytes) {
  std::error_code ec;
  fs::create_directories(p.parent_path(), ec);
  const fs::path tmp = p.string() + ".part";
  {
    std::ofstream f(tmp, std::ios::binary);
    if (!f) return false;
    f.write(reinterpret_cast<const char*>(bytes.data()),
            static_cast<std::streamsize>(bytes.size()));
    if (!f) return false;
  }
  fs::rename(tmp, p, ec);
  return !ec;
}

std::string lower(std::string_view s) {
  std::string out(s);
  for (char& c : out)
    if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
  return out;
}

}  // namespace

std::string content_path(std::string_view sha) {
  return "content/" + std::string(sha) + ".pak.zst";
}

namespace {

// A pack made before: the index names it. False when there is none (or its pack is gone).
bool read_index(const Site& site, const fs::path& index, ContentPack& out) {
  std::ifstream f(index);
  std::string sha;
  uint64_t raw = 0;
  size_t files = 0, missing = 0;
  std::error_code ec;
  if (!(f >> sha >> raw >> files >> missing) || !is_hex64(sha) ||
      !fs::is_regular_file(fs::path(site.archive->dir) / content_path(sha), ec))
    return false;
  out.sha = sha;
  out.bytes = fs::file_size(fs::path(site.archive->dir) / content_path(sha), ec);
  out.raw = raw;
  out.files = files;
  out.missing = missing;
  return true;
}

// The viewer folder's packs mounted in the order the viewer mounts them.
bool mount_viewer_packs(const Site& site, vfs::SearchPath& sp, std::string& error) {
  std::vector<fs::path> packs;
  std::error_code ec;
  if (!site.viewer_dir.empty()) {
    for (fs::directory_iterator it(site.viewer_dir, ec), end; !ec && it != end; it.increment(ec)) {
      const std::string ext = lower(it->path().extension().string());
      if (it->is_regular_file(ec) && (ext == ".pak" || ext == ".pk3")) packs.push_back(it->path());
    }
  }
  std::sort(packs.begin(), packs.end());
  for (const fs::path& p : packs) {
    std::vector<uint8_t> bytes;
    std::string why;
    if (!read_file(p, bytes) ||
        !vfs::add_archive(&sp, p.filename().string(), std::move(bytes), "", &why)) {
      error = "the pack " + p.filename().string() + " beside the viewer does not read: " + why;
      return false;
    }
  }
  if (packs.empty()) {
    error = "no pack beside the viewer to take the files from";
    return false;
  }
  return true;
}

// The pack of `names` (a name ending in '/' takes the folder) out of `sp`, kept under its hash and
// pointed at by `index`.
bool make_pack(const Site& site, const HubSpec& spec, const vfs::SearchPath& sp,
               const std::vector<std::string>& names, const fs::path& index, ContentPack& out,
               std::string& error) {
  // The names asked for, the folders' files too.
  std::set<std::string> want;
  std::vector<std::string> folders;
  for (const std::string& n : names) {
    if (n.empty()) continue;
    if (n.back() == '/')
      folders.push_back(lower(n));
    else
      want.insert(lower(n));
  }
  if (!folders.empty()) {
    for (const vfs::Listed& l : vfs::list(sp)) {
      for (const std::string& f : folders)
        if (l.name.rfind(f, 0) == 0) want.insert(std::string(l.name));
    }
  }
  // Each name read once; what a file found names in turn (`file_needs`) joins the walk.
  std::vector<std::pair<std::string, std::vector<uint8_t>>> files;
  std::vector<std::string> todo(want.rbegin(), want.rend());
  std::vector<std::string> more;
  while (!todo.empty()) {
    const std::string n = std::move(todo.back());
    todo.pop_back();
    const vfs::FileRef* ref = vfs::find(sp, n);
    std::vector<uint8_t> bytes;
    std::string why;
    if (ref == nullptr || !vfs::read(sp, *ref, &bytes, &why)) {
      out.missing++;
      continue;
    }
    if (spec.file_needs) {
      more.clear();
      spec.file_needs(n, bytes, more);
      for (std::string& m : more) {
        m = lower(m);
        if (!m.empty() && m.back() != '/' && want.insert(m).second) todo.push_back(m);
      }
    }
    files.emplace_back(std::string(vfs::stored_name(sp, *ref)), std::move(bytes));
  }
  std::sort(files.begin(), files.end(),  // by name, as the names asked for were walked
            [](const auto& a, const auto& b) { return lower(a.first) < lower(b.first); });
  std::vector<uint8_t> pak;
  if (!vfs::write_pak(files, &pak, &error)) return false;
  const std::vector<uint8_t> packed = vfs::zstd_pack(pak, vfs::ZstdLevel{});
  out.sha = mod::sha256_hex(packed.data(), packed.size());
  out.bytes = packed.size();
  out.raw = pak.size();
  out.files = files.size();
  std::error_code ec;
  const fs::path file = fs::path(site.archive->dir) / content_path(out.sha);
  if (!fs::is_regular_file(file, ec) && !write_file(file, packed)) {
    error = "the content pack could not be written under " + file.parent_path().string();
    return false;
  }
  const std::string line = out.sha + " " + std::to_string(out.raw) + " " +
                           std::to_string(out.files) + " " + std::to_string(out.missing) + "\n";
  write_file(index, {reinterpret_cast<const uint8_t*>(line.data()), line.size()});
  return true;
}

}  // namespace

bool demo_content(const Site& site, const HubSpec& spec, std::string_view demo_sha,
                  ContentPack& out, std::string& error) {
  out = {};
  if (!spec.demo_files) {
    error = "the game names no demo's files";
    return false;
  }
  if (site.archive == nullptr || site.archive->dir.empty() || !is_hex64(demo_sha)) {
    error = "no archive holds the demo " + std::string(demo_sha);
    return false;
  }
  const fs::path index = fs::path(site.archive->dir) / "content" / (std::string(demo_sha) + ".txt");
  if (read_index(site, index, out)) return true;
  std::vector<uint8_t> demo;
  if (!read_file(fs::path(site.archive->dir) / archive_path(demo_sha), demo)) {
    error = "the demo " + std::string(demo_sha) + " is not in the archive";
    return false;
  }
  std::vector<std::string> names;
  if (!spec.demo_files(demo, names, error)) return false;
  vfs::SearchPath sp;
  if (!mount_viewer_packs(site, sp, error)) return false;
  return make_pack(site, spec, sp, names, index, out, error);
}

bool map_name_ok(std::string_view map) {
  if (map.empty() || map.size() > kMostMapName) return false;
  return std::all_of(map.begin(), map.end(), [](char c) {
    return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-';
  });
}

bool map_content(const Site& site, const HubSpec& spec, std::string_view map, ContentPack& out,
                 std::string& error) {
  out = {};
  if (!spec.map_files) {
    error = "the game names no map's files";
    return false;
  }
  if (!map_name_ok(map)) {
    error = "'" + std::string(map) + "' is not a map's name (lower-case letters, digits, _ and -)";
    return false;
  }
  if (site.archive == nullptr || site.archive->dir.empty()) {
    error = "this hub keeps no archive to keep the map's pack in (--archive=<dir>)";
    return false;
  }
  const fs::path index =
      fs::path(site.archive->dir) / "content" / ("map-" + std::string(map) + ".txt");
  if (read_index(site, index, out)) return true;
  vfs::SearchPath sp;
  if (!mount_viewer_packs(site, sp, error)) return false;
  if (vfs::find(sp, "maps/" + std::string(map) + ".bsp") == nullptr) {
    error = "no pack beside the viewer holds the map " + std::string(map);
    return false;
  }
  std::vector<std::string> names;
  if (!spec.map_files(map, sp, names, error)) return false;
  return make_pack(site, spec, sp, names, index, out, error);
}

std::vector<std::string> viewer_maps(const Site& site) {
  // The packs are read again only when they change (they are tens of megabytes): the folder, each
  // pack's name, size and time.
  std::string key = site.viewer_dir;
  std::error_code ec;
  if (!site.viewer_dir.empty()) {
    std::vector<std::string> parts;
    for (fs::directory_iterator it(site.viewer_dir, ec), end; !ec && it != end; it.increment(ec)) {
      const std::string ext = lower(it->path().extension().string());
      if (!it->is_regular_file(ec) || (ext != ".pak" && ext != ".pk3")) continue;
      parts.push_back(it->path().filename().string() + ":" + std::to_string(it->file_size(ec)) +
                      ":" +
                      std::to_string(static_cast<long long>(
                          it->last_write_time(ec).time_since_epoch().count())));
    }
    std::sort(parts.begin(), parts.end());
    for (const std::string& p : parts) key += "|" + p;
  }
  static std::mutex lock;
  static std::string read_for;
  static std::vector<std::string> maps;
  const std::lock_guard<std::mutex> hold(lock);
  if (read_for == key) return maps;
  maps.clear();
  read_for = key;
  vfs::SearchPath sp;
  std::string error;
  if (!mount_viewer_packs(site, sp, error)) return maps;
  for (const vfs::Listed& l : vfs::list(sp)) {
    const std::string n = lower(std::string(l.name));
    constexpr std::string_view kDir = "maps/", kExt = ".bsp";
    if (n.size() <= kDir.size() + kExt.size() || n.rfind(kDir, 0) != 0 ||
        n.compare(n.size() - kExt.size(), kExt.size(), kExt) != 0)
      continue;
    const std::string name = n.substr(kDir.size(), n.size() - kDir.size() - kExt.size());
    if (map_name_ok(name)) maps.push_back(name);
  }
  std::sort(maps.begin(), maps.end());
  maps.erase(std::unique(maps.begin(), maps.end()), maps.end());
  return maps;
}

bool content_answer(std::string_view path, const Site& site, Answer& out) {
  constexpr std::string_view kPrefix = "/content/", kExt = ".pak.zst";
  if (path.rfind(kPrefix, 0) != 0) return false;
  const std::string_view name = path.substr(kPrefix.size());
  out = {};
  out.type = "text/plain; charset=utf-8";
  const bool shaped =
      name.size() == 64 + kExt.size() && name.substr(64) == kExt && is_hex64(name.substr(0, 64));
  std::vector<uint8_t> bytes;
  if (!shaped || site.archive == nullptr || site.archive->dir.empty() ||
      !read_file(fs::path(site.archive->dir) / content_path(name.substr(0, 64)), bytes)) {
    out.code = 404;
    out.body = "no such content pack: a pack is /content/<64 hex>.pak.zst, made for a demo\n";
    return true;
  }
  out.type = "application/octet-stream";
  out.body.assign(bytes.begin(), bytes.end());
  out.max_age_s = kContentMaxAgeS;
  return true;
}

}  // namespace ge::hub
