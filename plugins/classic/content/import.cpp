// A classic game copied into its data folder (import.hpp).
//
// ge:summary Walks a folder or disc image, finds the game, copies its files and music into data/
// ge:keywords import copy install data-folder disc image music steam volumes
// ge:skill classic vfs shooter
// ge:test tests/classic_content_import_test.cpp
#include "classic/content/import.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <set>
#include <span>
#include <string>

#include "assets/sha256.hpp"
#include "vfs/disc.hpp"
#include "vfs/lha.hpp"
#include "vfs/pak.hpp"
#include "vfs/unpack.hpp"
#include "vfs/zip.hpp"

namespace ge::classic::content {

namespace fs = std::filesystem;

namespace {

std::string lower(std::string_view s) {
  std::string o(s);
  for (char& c : o) c = char(std::tolower(static_cast<unsigned char>(c)));
  return o;
}

bool read_whole(const fs::path& p, std::vector<uint8_t>* out, std::string* error) {
  std::ifstream in(p, std::ios::binary);
  if (!in) {
    *error = "could not read " + p.string() + "; check that it is there and can be read";
    return false;
  }
  in.seekg(0, std::ios::end);
  out->resize(size_t(in.tellg()));
  in.seekg(0);
  in.read(reinterpret_cast<char*>(out->data()), std::streamsize(out->size()));
  if (!in) {
    *error = "could not read all of " + p.string() + "; the disc or drive may be failing";
    return false;
  }
  return true;
}

bool write_whole(const fs::path& p, std::span<const uint8_t> bytes, std::string* error) {
  std::error_code ec;
  fs::create_directories(p.parent_path(), ec);
  const fs::path part = p.string() + ".part";
  {
    std::ofstream o(part, std::ios::binary);
    o.write(reinterpret_cast<const char*>(bytes.data()), std::streamsize(bytes.size()));
    if (!o) {
      *error = "could not write " + p.string() +
               "; check that the game's data folder can be written and the disk has room";
      return false;
    }
  }
  fs::rename(part, p, ec);
  if (ec) {
    *error = "could not put " + p.string() + " in place (" + ec.message() + ")";
    return false;
  }
  return true;
}

size_t depth(std::string_view p) { return size_t(std::count(p.begin(), p.end(), '/')); }

std::string parent_of(std::string_view p) {
  const size_t s = p.rfind('/');
  return s == std::string_view::npos ? "" : std::string(p.substr(0, s));
}

std::string last_part(std::string_view p) {
  const size_t s = p.rfind('/');
  return std::string(s == std::string_view::npos ? p : p.substr(s + 1));
}

std::string ext_of(std::string_view p) {
  return lower(fs::path(std::string(p)).extension().string());
}

// A file's size and last write time, as a disc image's or an archive's stamp.
std::string stamp_of(const std::string& path) {
  std::error_code ec;
  return std::to_string(static_cast<unsigned long long>(fs::file_size(path, ec))) + " " +
         std::to_string(
             static_cast<long long>(fs::last_write_time(path, ec).time_since_epoch().count()));
}

// The largest release archive read whole into memory: past any game's zip or 7z of its disc.
constexpr uint64_t kArchiveLimit = uint64_t(1) << 31;

// The release's words by key, as the game reads them (files.cpp `localized_text`).
constexpr std::string_view kWordsFile = "localization/loc_english.txt";

// The folders a game reads loose files from.
bool content_folder(std::string_view first) {
  static const char* kFolders[] = {"maps",    "sound", "music",    "progs",       "models",
                                   "gfx",     "pics",  "textures", "env",         "sprites",
                                   "players", "skins", "video",    "localization"};
  const std::string f = lower(first);
  return std::any_of(std::begin(kFolders), std::end(kFolders),
                     [&](const char* k) { return f == k; });
}

bool archive_name(std::string_view name) {
  const std::string e = lower(fs::path(std::string(name)).extension().string());
  return e == ".pak" || e == ".pk3" || e == ".pkz";
}

bool music_folder(std::string_view name) {
  const std::string f = lower(name);
  return f == "music" || f == "cdtracks" || f == "soundtrack";
}

// An installer's LHA archives in the tree, their files joined to it under the archive's folder.
void open_installers(Tree* t) {
  std::vector<TreeFile> added;
  for (const TreeFile& f : t->files) {
    const std::string n = lower(last_part(f.path));
    const bool named = n.rfind("resource.", 0) == 0 || n.ends_with(".lha") || n.ends_with(".lzh");
    if (!named || f.size > (uint64_t(400) << 20)) continue;
    auto bytes = std::make_shared<std::vector<uint8_t>>();
    std::string error;
    std::vector<vfs::LhaEntry> entries;
    if (!f.read(bytes.get(), &error) || !vfs::read_lha(*bytes, &entries, &error)) continue;
    const std::string at = parent_of(f.path);
    for (const vfs::LhaEntry& e : entries) {
      if (e.method == 'd') continue;
      std::string name = e.name;
      std::replace(name.begin(), name.end(), '\\', '/');
      added.push_back({at.empty() ? name : std::string(at).append("/").append(name), e.size, 0,
                       // A reader that throws (out of memory) ends the import.
                       // NOLINTNEXTLINE(bugprone-exception-escape)
                       [bytes, e](std::vector<uint8_t>* out, std::string* err) {
                         return vfs::lha_unpack(*bytes, e, out, err);
                       }});
    }
  }
  t->files.insert(t->files.end(), added.begin(), added.end());
}

bool same_file(const fs::path& p, std::span<const uint8_t> bytes) {
  std::error_code ec;
  if (!fs::exists(p, ec) || fs::file_size(p, ec) != bytes.size()) return false;
  std::vector<uint8_t> have;
  std::string error;
  return read_whole(p, &have, &error) && assets::sha256(have) == assets::sha256(bytes);
}

// The source's fingerprint: where it is and every file's path, size and last write time (a disc
// image's own size and time), as the SHA-256 of that list in path order.
std::string fingerprint(const Tree& tree) {
  std::error_code ec;
  std::vector<std::string> lines;
  lines.reserve(tree.files.size());
  for (const TreeFile& f : tree.files)
    lines.push_back(f.path + "\t" + std::to_string(f.size) + "\t" + std::to_string(f.stamp));
  std::sort(lines.begin(), lines.end());
  std::string all =
      fs::weakly_canonical(tree.source, ec).generic_string() + "\n" + tree.image_stamp + "\n";
  for (const std::string& l : lines) all += l + "\n";
  return assets::sha256_hex(std::span(reinterpret_cast<const uint8_t*>(all.data()), all.size()));
}

constexpr const char* kTakenFile = "taken.txt";

// The taken file of a take-in: `taken.txt` when the source held the base game, else
// `taken-<pack folders>.txt`, so taking a pack in alone never sets the base game's claim aside.
std::string taken_name(const std::vector<const classicenhance::MissionPack*>& packs, bool base) {
  if (base || packs.empty()) return kTakenFile;
  std::string n = "taken";
  for (const classicenhance::MissionPack* m : packs) n += "-" + std::string(m->folder);
  return n + ".txt";
}

// Whether a taken file in `dir` says this source (`print`) was taken in whole (with its music when
// `music`) and every file it put in place is still there at its size; with the profile `game`,
// the packs it took in (`*packs`) and whether it held the base game (`*base`).
bool taken_before(const fs::path& dir, const std::string& print, bool music,
                  const classicenhance::Profile& game,
                  std::vector<const classicenhance::MissionPack*>* packs, bool* base) {
  std::error_code ec;
  for (auto it = fs::directory_iterator(dir, ec); !ec && it != fs::directory_iterator();
       it.increment(ec)) {
    const std::string n = it->path().filename().string();
    if (!n.starts_with("taken") || !n.ends_with(".txt")) continue;
    std::ifstream in(it->path());
    std::string line;
    if (!in || !std::getline(in, line) || line != "taken 2") continue;
    bool print_ok = false, music_ok = !music, any = false, files_ok = true;
    std::vector<const classicenhance::MissionPack*> found;
    bool held_base = true;
    while (files_ok && std::getline(in, line)) {
      if (line.starts_with("fingerprint ")) {
        print_ok = line.substr(12) == print;
      } else if (line.starts_with("music ")) {
        music_ok = music_ok || line.substr(6) == "1";
      } else if (line.starts_with("base ")) {
        held_base = line.substr(5) == "1";
      } else if (line.starts_with("pack ")) {
        if (const auto* m = classicenhance::find_pack(game, line.substr(5))) found.push_back(m);
      } else if (line.starts_with("file ")) {
        const size_t sp = line.find(' ', 5);
        if (sp == std::string::npos) {
          files_ok = false;
          break;
        }
        const uint64_t size = std::strtoull(line.c_str() + 5, nullptr, 10);
        const fs::path f = dir / line.substr(sp + 1);
        files_ok = fs::is_regular_file(f, ec) && fs::file_size(f, ec) == size;
        any = true;
      }
    }
    if (print_ok && music_ok && any && files_ok) {
      *packs = found;
      *base = held_base;
      return true;
    }
  }
  return false;
}

// The taken file written for a finished take-in (import.hpp § Taken in once); a failure only
// means the next launch takes the game in again.
void write_taken(const fs::path& dir, const std::string& from, const std::string& print, bool music,
                 const ImportReport& r) {
  std::error_code ec;
  std::string text = "taken 2\nsource " + fs::weakly_canonical(from, ec).generic_string() +
                     "\nfingerprint " + print + "\nmusic " + (music ? "1" : "0") + "\nbase " +
                     (r.base ? "1" : "0") + "\n";
  for (const classicenhance::MissionPack* m : r.packs)
    text += "pack " + std::string(m->folder) + "\n";
  std::vector<std::string> files = r.copied;
  files.insert(files.end(), r.same.begin(), r.same.end());
  for (const StoredTrack& t : r.tracks)
    files.push_back((r.music_folder.empty() ? "" : r.music_folder + "/") + "music/" + t.file);
  for (const std::string& f : files) {
    const uint64_t size = fs::file_size(dir / f, ec);
    if (!ec) text += "file " + std::to_string(size) + " " + f + "\n";
  }
  std::string error;
  write_whole(dir / taken_name(r.packs, r.base),
              std::span(reinterpret_cast<const uint8_t*>(text.data()), text.size()), &error);
}

// A release zip listed: its entries read out of it on demand, the archive held in memory.
bool open_zip(const std::string& path, Tree* out, std::string* error) {
  std::error_code ec;
  if (fs::file_size(path, ec) > kArchiveLimit) {
    *error = path + " is larger than a zip this reads whole (2 GiB); unpack it and give its folder";
    return false;
  }
  auto bytes = std::make_shared<std::vector<uint8_t>>();
  if (!read_whole(path, bytes.get(), error)) return false;
  vfs::Zip zip;
  std::string why;
  if (!vfs::read_zip(*bytes, &zip, &why)) {
    *error = path + " is not a zip this reads (" + why + "); unpack it and give its folder";
    return false;
  }
  for (const vfs::ZipEntry& e : zip.entries) {
    std::string name = e.name;
    std::replace(name.begin(), name.end(), '\\', '/');
    if (name.empty() || name.back() == '/') continue;
    // A reader that throws (out of memory) ends the import.
    // NOLINTNEXTLINE(bugprone-exception-escape)
    out->files.push_back({name, e.size, 0, [bytes, e](std::vector<uint8_t>* b, std::string* err) {
                            return vfs::zip_unpack(*bytes, e, b, err);
                          }});
  }
  out->image_stamp = stamp_of(path);
  return true;
}

// A disc image read from the tree at `path`.
bool open_image(const std::string& path, Tree* out, std::string* error) {
  auto disc = std::make_shared<vfs::Disc>();
  if (!vfs::open_disc(path, disc.get(), error)) return false;
  for (const vfs::DiscFile& f : disc->files) {
    // A reader that throws (out of memory) ends the import.
    // NOLINTNEXTLINE(bugprone-exception-escape)
    out->files.push_back({f.path, f.size, 0, [disc, f](std::vector<uint8_t>* b, std::string* e) {
                            return disc->read_file(f, b, e);
                          }});
  }
  out->disc = disc;
  return true;
}

// The one disc image a 7z holds, unpacked into `scratch` and read as the tree.
bool open_seven(const std::string& path, const std::string& scratch, Tree* out,
                std::string* error) {
  std::error_code ec;
  if (scratch.empty()) {
    *error = path + " is a 7z; give a folder to unpack its disc image into, or unpack it yourself";
    return false;
  }
  if (fs::file_size(path, ec) > kArchiveLimit) {
    *error = path + " is larger than a 7z this reads whole (2 GiB); unpack it and give its image";
    return false;
  }
  std::vector<uint8_t> bytes;
  if (!read_whole(path, &bytes, error)) return false;
  vfs::Unpacked one;
  std::string why;
  const auto image = [](std::string_view name) {
    const std::string e = ext_of(name);
    return e == ".mdf" || e == ".iso" || e == ".img" || e == ".bin";
  };
  if (!vfs::unpack_one(std::move(bytes), one, why, image)) {
    *error = "no one disc image to read in " + path + " (" + why +
             "); unpack it and give the game's folder or its image";
    return false;
  }
  const fs::path to = fs::path(scratch) / ("unpacked" + ext_of(one.name));
  if (!write_whole(to, one.bytes, error)) return false;
  out->unpacked = to.string();
  return open_image(to.string(), out, error);
}

// Whether the folder `l` (lower case, ending in '/') is a base or multiplayer folder of a profile.
bool base_like(std::string_view folder) {
  for (const classicenhance::Profile& p : classicenhance::kProfiles)
    if (folder == lower(p.base_dir) ||
        (!p.multiplayer_dir.empty() && folder == lower(p.multiplayer_dir)))
      return true;
  return false;
}

// Whether `dir` lies inside the data folder (a pack there is taken in already).
bool inside_data(const fs::path& dir, const std::string& data_root) {
  if (data_root.empty()) return false;
  std::error_code ec;
  const std::string d = fs::weakly_canonical(dir, ec).generic_string();
  const std::string r = fs::weakly_canonical(data_root, ec).generic_string();
  return d == r || d.starts_with(r + "/");
}

}  // namespace

bool open_tree(const std::string& path, Tree* out, std::string* error, const std::string& scratch) {
  *out = Tree();
  out->source = path;
  std::error_code ec;
  const std::string ext = ext_of(path);
  if (fs::is_regular_file(path, ec) && vfs::is_disc_image(path)) {
    if (!open_image(path, out, error)) return false;
    out->image_stamp = stamp_of(path);
  } else if (fs::is_regular_file(path, ec) && ext == ".zip") {
    if (!open_zip(path, out, error)) return false;
  } else if (fs::is_regular_file(path, ec) && ext == ".7z") {
    if (!open_seven(path, scratch, out, error)) return false;
    out->image_stamp = stamp_of(path);
  } else if (fs::is_directory(path, ec)) {
    const fs::path top(path);
    for (auto it = fs::recursive_directory_iterator(
             top, fs::directory_options::skip_permission_denied, ec);
         it != fs::recursive_directory_iterator(); it.increment(ec)) {
      if (ec) break;
      if (it.depth() >= 6) it.disable_recursion_pending();
      if (!it->is_regular_file(ec)) continue;
      const fs::path p = it->path();
      out->files.push_back(
          {p.lexically_relative(top).generic_string(), it->file_size(ec),
           static_cast<int64_t>(it->last_write_time(ec).time_since_epoch().count()),
           // A reader that throws (out of memory) ends the import.
           // NOLINTNEXTLINE(bugprone-exception-escape)
           [p](std::vector<uint8_t>* b, std::string* e) { return read_whole(p, b, e); }});
    }
  } else {
    *error = "there is no folder, disc image or release archive at " + path +
             "; give a game's folder, a mounted disc, its .iso, .cue or .mdf, or its .zip or .7z";
    return false;
  }
  std::string prefix;
  if (find_game(*out, &prefix) == nullptr) open_installers(out);
  return true;
}

const classicenhance::Profile* find_game(const Tree& tree, std::string* prefix) {
  const classicenhance::Profile* best = nullptr;
  size_t best_depth = ~size_t(0);
  for (const classicenhance::Profile& p : classicenhance::kProfiles) {
    const std::string base = lower(p.base_dir) + "/";
    const std::string firsts[] = {base + classicenhance::pack_name(p, 0),
                                  base + classicenhance::zip_name(p, 0)};
    for (const TreeFile& f : tree.files) {
      const std::string l = lower(f.path);
      for (const std::string& want : firsts) {
        if (!l.ends_with(want)) continue;
        const size_t at = l.size() - want.size();
        if (at != 0 && l[at - 1] != '/') continue;
        if (depth(l) < best_depth) {
          best_depth = depth(l);
          best = &p;
          *prefix = f.path.substr(0, at);
        }
      }
    }
  }
  return best;
}

std::vector<FoundPack> find_packs(const Tree& tree, const classicenhance::Profile* game,
                                  const PackIdentifier& identify) {
  std::vector<FoundPack> out;
  const auto have = [&](const classicenhance::MissionPack* m) {
    return std::any_of(out.begin(), out.end(), [&](const FoundPack& f) { return f.pack == m; });
  };
  std::set<std::string> folders;  // every folder in the tree, lower case, '/' at the end
  for (const TreeFile& f : tree.files) {
    std::string d = lower(parent_of(f.path));
    while (!d.empty()) {
      if (!folders.insert(d + "/").second) break;
      d = parent_of(d);
    }
  }
  std::vector<const TreeFile*> unnamed;  // first packs in no pack's folder
  for (const TreeFile& f : tree.files) {
    const std::string l = lower(f.path);
    const std::string name = last_part(l);
    std::string folder = last_part(parent_of(l));
    if (folder.empty()) {  // the pack's own folder pointed at: its name is the source's
      std::string src = fs::path(tree.source).generic_string();
      while (!src.empty() && src.back() == '/') src.pop_back();
      folder = lower(last_part(src));
    }
    if (name != "pak0.pak" || base_like(folder)) continue;
    // In a folder named after a pack: the game whose pack it is, told by the base folder beside
    // it, then by its digest, then by the one game with a pack of that name.
    std::vector<std::pair<const classicenhance::Profile*, const classicenhance::MissionPack*>> by;
    for (const classicenhance::Profile& p : classicenhance::kProfiles)
      if (const auto* m = classicenhance::find_pack(p, folder)) by.emplace_back(&p, m);
    if (by.empty()) {
      unnamed.push_back(&f);
      continue;
    }
    const std::string above = parent_of(parent_of(l));
    const std::string up = above.empty() ? "" : above + "/";
    const classicenhance::Profile* pick = nullptr;
    const classicenhance::MissionPack* pack = nullptr;
    for (const auto& [p, m] : by) {
      if (folders.contains(up + lower(p->base_dir) + "/") || (game == p && by.size() > 1)) {
        pick = p;
        pack = m;
        break;
      }
    }
    if (pick == nullptr && by.size() > 1) {
      std::vector<uint8_t> bytes;
      std::string why;
      if (f.read(&bytes, &why)) {
        const classicenhance::Profile* g = nullptr;
        if (const auto* m = identify(bytes.size(), assets::sha256_hex(bytes), &g)) {
          pick = g;
          pack = m;
        }
      }
    }
    if (pick == nullptr && by.size() == 1) {
      pick = by[0].first;
      pack = by[0].second;
    }
    if (pick == nullptr || have(pack)) continue;
    const std::string root = parent_of(l);
    out.push_back({pick, pack, root.empty() ? "" : root + "/", "", "", "", true});
  }
  // Under another folder's name (a disc's `Data/all/pak0.pak`): known by its digest, its other
  // files from the shallowest folder named after it, its patches to the base game from a
  // `patch/<base folder>/` folder.
  for (const TreeFile* f : unnamed) {
    if (f->size > kArchiveLimit) continue;
    std::vector<uint8_t> bytes;
    std::string why;
    if (!f->read(&bytes, &why)) continue;
    const classicenhance::Profile* g = nullptr;
    const classicenhance::MissionPack* m = identify(bytes.size(), assets::sha256_hex(bytes), &g);
    if (m == nullptr || have(m)) continue;
    FoundPack found{g, m, "", f->path, "", "", false};
    for (const std::string& d : folders) {
      const std::string name = last_part(d.substr(0, d.size() - 1));
      if (name == m->folder && (found.loose.empty() || depth(d) < depth(found.loose)))
        found.loose = d;
      const std::string patch = "patch/" + lower(g->base_dir) + "/";
      if (d.ends_with(patch) && (d.size() == patch.size() || d[d.size() - patch.size() - 1] == '/'))
        found.patch = d;
    }
    out.push_back(found);
  }
  if (game != nullptr)
    out.erase(
        std::remove_if(out.begin(), out.end(), [&](const FoundPack& f) { return f.game != game; }),
        out.end());
  return out;
}

bool import_game(const std::string& from, const std::string& data_root, ImportReport* out,
                 std::string* error, const ImportOptions& opts) {
  *out = ImportReport();
  Tree tree;
  const fs::path scratch = fs::path(data_root) / ".unpacking";
  struct Cleanup {  // a 7z's image unpacked for the take-in goes with it
    const Tree& t;
    ~Cleanup() {
      std::error_code ec;
      if (t.unpacked.empty()) return;
      fs::remove(t.unpacked, ec);
      fs::remove(fs::path(t.unpacked).parent_path(), ec);  // the scratch folder, when empty
    }
  } cleanup{tree};
  if (!open_tree(from, &tree, error, scratch.string())) return false;
  tree.source = from;
  std::string prefix;
  const classicenhance::Profile* game = find_game(tree, &prefix);
  const PackIdentifier identify =
      opts.identify ? opts.identify : PackIdentifier(classicenhance::known_pack);
  // Taken in before and unchanged since: nothing to read (import.hpp § Taken in once). Only the
  // base game's folder and the packs' folders named after them are known before the packs are
  // read, so a source of packs alone is checked against every game's taken files.
  const std::string print = fingerprint(tree);
  for (const classicenhance::Profile& p : classicenhance::kProfiles) {
    if (game != nullptr && game != &p) continue;
    const fs::path dir = fs::path(data_root) / std::string(p.id);
    if (taken_before(dir, print, opts.music, p, &out->packs, &out->base) &&
        out->base == (game != nullptr)) {
      out->game = &p;
      out->game_dir = dir.string();
      out->unchanged = true;
      return true;
    }
  }
  out->packs.clear();
  const std::vector<FoundPack> packs = find_packs(tree, game, identify);
  if (game == nullptr && packs.empty()) {
    *error = "no game this knows is in " + from + " (looked for " +
             classicenhance::first_pack_path(classicenhance::q1_profile()) +
             ", the other games' first packs and their mission packs at any depth); point at the "
             "game's own folder, its disc, its disc image or its release archive";
    return false;
  }
  out->base = game != nullptr;
  if (game == nullptr) game = packs[0].game;
  out->game = game;
  for (const FoundPack& f : packs)
    if (f.game == game) out->packs.push_back(f.pack);
  const fs::path dir = fs::path(data_root) / std::string(game->id);
  out->game_dir = dir.string();
  {  // a take-in that stops half way leaves no claim behind
    std::error_code ec;
    fs::remove(dir / taken_name(out->packs, out->base), ec);
  }
  const auto say = [&](const std::string& s) {
    if (opts.progress) opts.progress(s);
  };
  const std::string lp = lower(prefix);
  // Each folder copied: its place in the tree (lower case, '/' at the end) and its name in the
  // data folder; `archives` takes the archives at its top too, not only its content folders.
  struct Root {
    std::string at, name;
    bool archives;
  };
  std::vector<Root> roots;
  if (out->base) {
    roots.push_back({lp + lower(game->base_dir) + "/", std::string(game->base_dir), true});
    if (!game->multiplayer_dir.empty())
      roots.push_back(
          {lp + lower(game->multiplayer_dir) + "/", std::string(game->multiplayer_dir), true});
  }
  std::vector<std::pair<std::string, const TreeFile*>> singles;  // a pack under another name
  for (const FoundPack& f : packs) {
    if (f.game != game) continue;
    const std::string folder(f.pack->folder);
    if (f.named) roots.push_back({f.root, folder, true});
    if (!f.loose.empty()) roots.push_back({f.loose, folder, false});
    if (!f.patch.empty()) roots.push_back({f.patch, std::string(game->base_dir), false});
    if (!f.pack_file.empty())
      for (const TreeFile& t : tree.files)
        if (t.path == f.pack_file)
          singles.emplace_back(folder + "/" + classicenhance::pack_name(*game, 0), &t);
  }
  const auto put = [&](const std::string& rel, const std::vector<uint8_t>& bytes) {
    say("copying " + rel);
    if (same_file(dir / rel, bytes)) {
      out->same.push_back(rel);
      return true;
    }
    if (!write_whole(dir / rel, bytes, error)) return false;
    out->copied.push_back(rel);
    return true;
  };
  std::vector<std::pair<std::string, std::vector<uint8_t>>> base_packs;  // for music
  for (const TreeFile& f : tree.files) {
    const std::string l = lower(f.path);
    for (const Root& root : roots) {
      if (!l.starts_with(root.at)) continue;
      const std::string rest = l.substr(root.at.size());
      const size_t slash = rest.find('/');
      const bool keep = slash == std::string::npos ? root.archives && archive_name(rest)
                                                   : content_folder(rest.substr(0, slash));
      if (!keep) continue;
      const std::string rel = root.name + "/" + rest;
      std::vector<uint8_t> bytes;
      if (!f.read(&bytes, error) || !put(rel, bytes)) return false;
      if (out->base && root.at == roots[0].at && ext_of(rest) == ".pak")
        base_packs.emplace_back(rel, std::move(bytes));
      break;
    }
  }
  for (const auto& [rel, f] : singles) {
    std::vector<uint8_t> bytes;
    if (!f->read(&bytes, error) || !put(rel, bytes)) return false;
  }
  if (!out->base)
    out->notes.push_back(std::string(game->what) + " itself was not in " + from +
                         "; its mission packs play only over it, so take it in too");
  // Another add-on beside the base folder (a third party's pack, a mod): named and left.
  for (const TreeFile& f : tree.files) {
    if (!out->base) break;
    const std::string l = lower(f.path);
    if (!l.starts_with(lp) || last_part(l) != classicenhance::pack_name(*game, 0)) continue;
    const std::string folder = parent_of(l.substr(lp.size()));
    if (folder.empty() || folder.find('/') != std::string::npos || base_like(folder) ||
        std::any_of(packs.begin(), packs.end(),
                    [&](const FoundPack& p) { return p.named && p.root == lp + folder + "/"; }))
      continue;
    out->notes.push_back(parent_of(f.path).substr(prefix.size()) + " beside " +
                         std::string(game->base_dir) + " is not one of " + std::string(game->what) +
                         "'s mission packs this game plays; it was left where it is");
  }
  // A release that prints its words by key (progs/localized.hpp) keeps their table in its own
  // archives beside the base folder (`*.kpf`): that one file is taken out of the first that has
  // it and kept loose in the base folder, where the game reads it.
  for (const TreeFile& f : tree.files) {
    if (!out->base) break;
    const std::string l = lower(f.path);
    if (!l.starts_with(lp) || l.find('/', lp.size()) != std::string::npos || ext_of(l) != ".kpf")
      continue;
    std::vector<uint8_t> bytes;
    std::string why;
    vfs::Zip zip;
    if (!f.read(&bytes, &why) || !vfs::read_zip(bytes, &zip, &why)) continue;
    const vfs::ZipEntry* e = vfs::zip_find(zip, kWordsFile);
    std::vector<uint8_t> text;
    if (e == nullptr || !vfs::zip_unpack(bytes, *e, &text, &why)) continue;
    if (!put(std::string(game->base_dir) + "/" + std::string(kWordsFile), text)) return false;
    break;
  }
  if (!opts.music) {
    write_taken(dir, from, print, false, *out);
    return true;
  }
  // The music: the base game's into `<data>/<id>/music`; a disc of mission packs alone, its
  // tracks into the pack's folder (`<data>/<id>/<pack>/music`), which the pack plays first. A
  // disc carrying two packs and one set of tracks (Quake's two packs on one disc, whose maps both
  // ask for tracks 2..9) plays those tracks for either pack, as the disc did: each pack keeps them.
  if (!out->base && !out->packs.empty()) out->music_folder = std::string(out->packs[0]->folder);
  const std::string music_dir =
      out->music_folder.empty() ? out->game_dir : (dir / out->music_folder).string();
  const auto keep_track = [&](int n, const Samples& s, MusicSource kind, const std::string& sha,
                              const std::string& what) {
    StoredTrack t;
    std::string why;
    if (!store_track(music_dir, game->id, n, s, kind, sha, &t, &why)) {
      out->notes.push_back(what + ": " + why);
      return;
    }
    out->tracks.push_back(t);
  };
  // The disc's own audio tracks first: the best source there is.
  if (tree.disc && (out->base || !out->music_folder.empty())) {
    for (const vfs::DiscTrack& t : tree.disc->tracks) {
      if (t.kind != vfs::TrackKind::audio) continue;
      say("taking track " + std::to_string(t.number) + " off the disc");
      std::vector<int16_t> pcm;
      std::string why;
      if (!tree.disc->read_audio(t, &pcm, &why)) {
        out->notes.push_back("disc track " + std::to_string(t.number) + ": " + why);
        continue;
      }
      Samples s;
      s.data.assign(pcm.begin(), pcm.end());
      keep_track(t.number, s, MusicSource::disc, "", "disc track " + std::to_string(t.number));
    }
  }
  // Music files: in a music folder at the game's top or beside its base folder, and in its packs.
  struct Found {
    std::string path;
    MusicSource kind;
    std::function<bool(std::vector<uint8_t>*, std::string*)> read;
  };
  std::vector<Found> found;
  const std::string base = lower(game->base_dir);
  for (const TreeFile& f : tree.files) {
    if (!out->base) break;
    MusicSource kind;
    if (!music_source_of(f.path, &kind)) continue;
    const std::string folder = parent_of(f.path);
    if (!music_folder(last_part(folder))) continue;
    const std::string above = lower(parent_of(folder));
    const std::string top = lp.empty() ? "" : lp.substr(0, lp.size() - 1);
    if (above != top && last_part(above) != base) continue;  // another game's or a mod's music
    found.push_back({f.path, kind, f.read});
  }
  for (const auto& [name, bytes] : base_packs) {
    vfs::Pak pak;
    std::string why;
    if (!vfs::read_pak(bytes, &pak, &why)) continue;
    for (const vfs::PakEntry& e : pak.entries) {
      MusicSource kind;
      if (!music_source_of(e.path, &kind) || !lower(e.path).starts_with("music/")) continue;
      const std::span<const uint8_t> in = vfs::pak_file(bytes, e);
      std::vector<uint8_t> copy(in.begin(), in.end());
      found.push_back({name + ":" + e.path, kind, [copy](std::vector<uint8_t>* b, std::string*) {
                         *b = copy;
                         return true;
                       }});
    }
  }
  std::stable_sort(found.begin(), found.end(),
                   [](const Found& a, const Found& b) { return a.kind < b.kind; });
  std::vector<std::string> names;
  names.reserve(found.size());
  for (const Found& f : found) names.push_back(last_part(f.path));
  const std::vector<int> numbers = track_numbers(names);
  for (size_t i = 0; i < found.size(); i++) {
    if (numbers[i] == 0) {
      out->notes.push_back(found[i].path + ": its name gives no track number; left out");
      continue;
    }
    say("taking track " + std::to_string(numbers[i]) + " from " + found[i].path);
    std::vector<uint8_t> bytes;
    Samples s;
    std::string why;
    if (!found[i].read(&bytes, &why) || !decode_music(bytes, found[i].kind, &s, &why)) {
      out->notes.push_back(found[i].path + ": " + why);
      continue;
    }
    keep_track(numbers[i], s, found[i].kind, assets::sha256_hex(bytes), found[i].path);
  }
  std::stable_sort(out->tracks.begin(), out->tracks.end(),
                   [](const StoredTrack& a, const StoredTrack& b) { return a.track < b.track; });
  for (size_t i = 1; i < out->packs.size() && !out->base && !out->tracks.empty(); i++) {
    const fs::path to = dir / std::string(out->packs[i]->folder) / "music";
    std::error_code ec;
    fs::create_directories(to, ec);
    fs::copy(fs::path(music_dir) / "music", to,
             fs::copy_options::overwrite_existing | fs::copy_options::recursive, ec);
    if (ec)
      out->notes.push_back(std::string(out->packs[i]->name) + " kept no music: " + ec.message());
  }
  write_taken(dir, from, print, true, *out);
  return true;
}

std::vector<Offer> find_offers(const std::vector<std::string>& roots,
                               const std::string& data_root) {
  std::vector<Offer> out;
  std::error_code ec;
  // The sources games were taken in from look for packs too: a pack beside a base game taken in
  // before packs were known.
  std::vector<std::string> all = roots;
  for (const classicenhance::Profile& g : classicenhance::kProfiles) {
    const fs::path dir = fs::path(data_root) / std::string(g.id);
    for (const fs::directory_entry& e : fs::directory_iterator(dir, ec)) {
      const std::string n = e.path().filename().string();
      if (!n.starts_with("taken") || !n.ends_with(".txt")) continue;
      std::ifstream in(e.path());
      for (std::string line; std::getline(in, line);)
        if (line.starts_with("source ") && fs::is_directory(line.substr(7), ec) &&
            std::find(all.begin(), all.end(), line.substr(7)) == all.end())
          all.push_back(line.substr(7));
    }
  }
  const auto taken = [&](const classicenhance::Profile& g, const classicenhance::MissionPack& m) {
    return fs::is_regular_file(fs::path(data_root) / std::string(g.id) / std::string(m.folder) /
                                   classicenhance::pack_name(g, 0),
                               ec);
  };
  const auto offered = [&](const classicenhance::MissionPack* m) {
    return std::any_of(out.begin(), out.end(), [&](const Offer& o) { return o.pack == m; });
  };
  const auto child = [&](const fs::path& dir, std::string_view name) {
    for (const fs::directory_entry& e : fs::directory_iterator(dir, ec))
      if (lower(e.path().filename().string()) == name) return e.path();
    return fs::path();
  };
  // A disc image or a zip that holds a pack: its listing read by seeks, never the whole file.
  using Listed = std::vector<std::pair<std::string, uint64_t>>;
  const auto offer_listed = [&](const fs::path& file, const Listed& names) {
    const auto has_folder = [&](std::string_view folder) {
      const std::string seg = "/" + lower(folder) + "/";
      return std::any_of(names.begin(), names.end(), [&](const auto& n) {
        return ("/" + lower(n.first)).find(seg) != std::string::npos;
      });
    };
    for (const auto& [n, size] : names) {
      const std::string l = lower(n);
      const std::string folder = last_part(parent_of(l));
      for (const classicenhance::Profile& g : classicenhance::kProfiles) {
        if (last_part(l) != lower(classicenhance::pack_name(g, 0))) continue;
        bool other_base = false;
        for (const classicenhance::Profile& o : classicenhance::kProfiles)
          other_base = other_base || (&o != &g && has_folder(o.base_dir));
        const bool named_here = !other_base || has_folder(g.base_dir);
        for (const classicenhance::MissionPack& m : g.packs) {
          if (size != m.size && (folder != lower(m.folder) || !named_here)) continue;
          if (offered(&m) || taken(g, m)) continue;
          out.push_back({file.string(), &g, &m});
        }
      }
    }
  };
  const auto look_inside = [&](const fs::path& file) {
    const std::string e = lower(file.extension().string());
    Listed names;
    std::string why;
    if (e == ".zip") {
      std::vector<vfs::ZipEntry> entries;
      if (!vfs::list_zip_file(file.string(), &entries, &why)) return;
      for (const vfs::ZipEntry& z : entries) {
        std::string n = z.name;
        std::replace(n.begin(), n.end(), '\\', '/');
        names.emplace_back(std::move(n), z.size);
      }
    } else {
      // A track's .bin beside its cue sheet is read through the sheet.
      if (e == ".bin" && fs::exists(fs::path(file).replace_extension(".cue"), ec)) return;
      vfs::Disc disc;
      if (!vfs::open_disc(file.string(), &disc, &why)) return;
      for (const vfs::DiscFile& f : disc.files) names.emplace_back(f.path, f.size);
    }
    offer_listed(file, names);
  };
  int seen = 0, looked = 0;
  for (const std::string& r : all) {
    const fs::path root(r);
    if (!data_root.empty() && fs::weakly_canonical(root, ec) == fs::weakly_canonical(data_root, ec))
      continue;
    for (auto it = fs::recursive_directory_iterator(
             root, fs::directory_options::skip_permission_denied, ec);
         it != fs::recursive_directory_iterator() && seen < kOfferFolders; it.increment(ec)) {
      if (ec) break;
      if (it.depth() >= 3) it.disable_recursion_pending();
      if (it->is_regular_file(ec)) {
        const std::string e = lower(it->path().extension().string());
        if ((e == ".zip" || vfs::is_disc_image(it->path().string())) &&
            it->file_size(ec) >= kOfferImageBytes && looked < kOfferImages &&
            !inside_data(it->path().parent_path(), data_root)) {
          looked++;
          look_inside(it->path());
        }
        continue;
      }
      if (!it->is_directory(ec)) continue;
      seen++;
      const fs::path dir = it->path();
      const std::string name = lower(dir.filename().string());
      if (!inside_data(dir, data_root)) {
        for (const classicenhance::Profile& g : classicenhance::kProfiles) {
          const classicenhance::MissionPack* m = classicenhance::find_pack(g, name);
          if (m == nullptr || offered(m) || taken(g, *m)) continue;
          if (child(dir, classicenhance::pack_name(g, 0)).empty()) continue;
          // The pack beside this game's base folder: the install; another game's base folder
          // beside it: not this game's pack.
          const bool with_base = !child(dir.parent_path(), lower(g.base_dir)).empty();
          bool other_base = false;
          for (const classicenhance::Profile& o : classicenhance::kProfiles)
            other_base =
                other_base || (&o != &g && !child(dir.parent_path(), lower(o.base_dir)).empty());
          if (other_base && !with_base) continue;
          out.push_back({(with_base ? dir.parent_path() : dir).string(), &g, m});
        }
        const fs::path first = child(dir, "pak0.pak");
        const uint64_t size = first.empty() ? 0 : fs::file_size(first, ec);
        for (const classicenhance::Profile& g : classicenhance::kProfiles)
          for (const classicenhance::MissionPack& m : g.packs) {
            if (size != m.size || offered(&m) || taken(g, m) || base_like(name)) continue;
            fs::path top = dir;  // the folder under the root that holds it
            while (top.has_parent_path() && top.parent_path() != root && top != root)
              top = top.parent_path();
            out.push_back({top.string(), &g, &m});
          }
      }
    }
  }
  return out;
}

std::vector<std::string> store_folders() {
  std::vector<fs::path> roots, out;
  const char* home = std::getenv("HOME");
  const fs::path h = home != nullptr ? fs::path(home) : fs::path();
  std::vector<fs::path> steam;
  if (!h.empty()) {
    steam.push_back(h / "Library/Application Support/Steam");
    steam.push_back(h / ".steam/steam");
    steam.push_back(h / ".local/share/Steam");
  }
  steam.push_back("C:/Program Files (x86)/Steam");
  std::error_code ec;
  for (const fs::path& s : steam) {
    if (!fs::is_directory(s / "steamapps", ec)) continue;
    roots.push_back(s / "steamapps/common");
    // The other libraries: each `"path"  "<folder>"` line of libraryfolders.vdf.
    std::ifstream vdf(s / "steamapps/libraryfolders.vdf");
    for (std::string line; std::getline(vdf, line);) {
      const size_t k = line.find("\"path\"");
      if (k == std::string::npos) continue;
      const size_t a = line.find('"', k + 6),
                   b = a == std::string::npos ? a : line.find('"', a + 1);
      if (b == std::string::npos) continue;
      std::string p = line.substr(a + 1, b - a - 1);
      for (size_t at; (at = p.find("\\\\")) != std::string::npos;) p.replace(at, 2, "/");
      roots.push_back(fs::path(p) / "steamapps/common");
    }
  }
  roots.push_back("C:/GOG Games");
  roots.push_back("C:/Program Files (x86)/GOG Galaxy/Games");
  roots.push_back("/Volumes");
  if (const char* user = std::getenv("USER"); user != nullptr) {
    roots.push_back(fs::path("/media") / user);
    roots.push_back(fs::path("/run/media") / user);
  }
#if defined(_WIN32)
  for (char d = 'D'; d <= 'Z'; d++) roots.push_back(std::string(1, d) + ":/");
#endif
  std::set<std::string> seen;
  std::vector<std::string> result;
  for (const fs::path& r : roots) {
    if (!fs::is_directory(r, ec)) continue;
    const std::string s = fs::weakly_canonical(r, ec).generic_string();
    if (seen.insert(s).second) result.push_back(s);
  }
  return result;
}

}  // namespace ge::classic::content
