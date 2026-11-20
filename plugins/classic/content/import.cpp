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

#include "assets/sha256.hpp"
#include "vfs/lha.hpp"
#include "vfs/pak.hpp"
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
      added.push_back({at.empty() ? name : std::string(at).append("/").append(name), e.size,
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

}  // namespace

bool open_tree(const std::string& path, Tree* out, std::string* error) {
  *out = Tree();
  out->source = path;
  std::error_code ec;
  if (fs::is_regular_file(path, ec) && vfs::is_disc_image(path)) {
    auto disc = std::make_shared<vfs::Disc>();
    if (!vfs::open_disc(path, disc.get(), error)) return false;
    for (const vfs::DiscFile& f : disc->files) {
      out->files.push_back({f.path, f.size, [disc, f](std::vector<uint8_t>* b, std::string* e) {
                              return disc->read_file(f, b, e);
                            }});
    }
    out->disc = disc;
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
           [p](std::vector<uint8_t>* b, std::string* e) { return read_whole(p, b, e); }});
    }
  } else {
    *error = "there is no folder or disc image at " + path +
             "; give a game's folder, a mounted disc, or its .iso or .cue";
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

bool import_game(const std::string& from, const std::string& data_root, ImportReport* out,
                 std::string* error, const ImportOptions& opts) {
  *out = ImportReport();
  Tree tree;
  if (!open_tree(from, &tree, error)) return false;
  std::string prefix;
  const classicenhance::Profile* game = find_game(tree, &prefix);
  if (game == nullptr) {
    *error = "no game this knows is in " + from + " (looked for " +
             classicenhance::first_pack_path(classicenhance::q1_profile()) +
             " and the other games' first packs at any depth); point at the game's own folder, "
             "its disc or its disc image";
    return false;
  }
  out->game = game;
  const fs::path dir = fs::path(data_root) / std::string(game->id);
  out->game_dir = dir.string();
  const auto say = [&](const std::string& s) {
    if (opts.progress) opts.progress(s);
  };
  const std::string lp = lower(prefix);
  std::vector<std::pair<std::string, std::string>> roots = {
      {lp + lower(game->base_dir) + "/", std::string(game->base_dir)}};
  if (!game->multiplayer_dir.empty())
    roots.emplace_back(lp + lower(game->multiplayer_dir) + "/", std::string(game->multiplayer_dir));
  std::vector<std::pair<std::string, std::vector<uint8_t>>> packs;  // the base folder's, for music
  for (const TreeFile& f : tree.files) {
    const std::string l = lower(f.path);
    for (const auto& [root, name] : roots) {
      if (!l.starts_with(root)) continue;
      const std::string rest = l.substr(root.size());
      const size_t slash = rest.find('/');
      const bool keep =
          slash == std::string::npos ? archive_name(rest) : content_folder(rest.substr(0, slash));
      if (!keep) continue;
      const std::string rel = std::string(name).append("/").append(rest);
      say("copying " + rel);
      std::vector<uint8_t> bytes;
      if (!f.read(&bytes, error)) return false;
      if (same_file(dir / rel, bytes)) {
        out->same.push_back(rel);
      } else {
        if (!write_whole(dir / rel, bytes, error)) return false;
        out->copied.push_back(rel);
      }
      if (root == roots[0].first && lower(fs::path(rest).extension().string()) == ".pak")
        packs.emplace_back(rel, std::move(bytes));
    }
  }
  // A release that prints its words by key (progs/localized.hpp) keeps their table in its own
  // archives beside the base folder (`*.kpf`): that one file is taken out of the first that has
  // it and kept loose in the base folder, where the game reads it.
  for (const TreeFile& f : tree.files) {
    const std::string l = lower(f.path);
    if (!l.starts_with(lp) || l.find('/', lp.size()) != std::string::npos ||
        lower(fs::path(l).extension().string()) != ".kpf")
      continue;
    std::vector<uint8_t> bytes;
    std::string why;
    vfs::Zip zip;
    if (!f.read(&bytes, &why) || !vfs::read_zip(bytes, &zip, &why)) continue;
    const vfs::ZipEntry* e = vfs::zip_find(zip, kWordsFile);
    std::vector<uint8_t> text;
    if (e == nullptr || !vfs::zip_unpack(bytes, *e, &text, &why)) continue;
    const std::string rel = std::string(game->base_dir) + "/" + std::string(kWordsFile);
    say("copying " + rel);
    if (same_file(dir / rel, text)) {
      out->same.push_back(rel);
    } else {
      if (!write_whole(dir / rel, text, error)) return false;
      out->copied.push_back(rel);
    }
    break;
  }
  if (!opts.music) return true;
  const auto keep_track = [&](int n, const Samples& s, MusicSource kind, const std::string& sha,
                              const std::string& what) {
    StoredTrack t;
    std::string why;
    if (!store_track(out->game_dir, game->id, n, s, kind, sha, &t, &why)) {
      out->notes.push_back(what + ": " + why);
      return;
    }
    out->tracks.push_back(t);
  };
  // The disc's own audio tracks first: the best source there is.
  if (tree.disc) {
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
    MusicSource kind;
    if (!music_source_of(f.path, &kind)) continue;
    const std::string folder = parent_of(f.path);
    if (!music_folder(last_part(folder))) continue;
    const std::string above = lower(parent_of(folder));
    const std::string top = lp.empty() ? "" : lp.substr(0, lp.size() - 1);
    if (above != top && last_part(above) != base) continue;  // another game's or a mod's music
    found.push_back({f.path, kind, f.read});
  }
  for (const auto& [name, bytes] : packs) {
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
  return true;
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
