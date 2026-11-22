// Games taken into the data folder (take_in.hpp).
//
// ge:summary The data folder's place, and a pointed-at game copied into it
// ge:keywords import take-in data-folder copy
// ge:skill shooter
// ge:test tests/shooters_edge_take_in_test.cpp
#include "shooters_edge/client/take_in.hpp"

#include <filesystem>
#include <fstream>

#include "classic/content/import.hpp"
#include "classic/content/soundtrack.hpp"
#include "core/output.hpp"
#include "shooters_edge/client/args.hpp"
#include "shooters_edge/client/demo.hpp"
#include "shooters_edge/client/settings.hpp"

namespace shooters_edge::client {

namespace fs = std::filesystem;
namespace content = ge::classic::content;

namespace {

bool writable(const fs::path& dir) {
  std::error_code ec;
  fs::create_directories(dir, ec);
  if (!fs::is_directory(dir, ec)) return false;
  const fs::path probe = dir / ".write-check";
  { std::ofstream(probe) << "ok"; }
  const bool ok = fs::exists(probe, ec);
  fs::remove(probe, ec);
  return ok;
}

}  // namespace

std::string data_root(const std::string& placed, const std::string& prefs) {
  if (!prefs.empty() && writable(fs::path(prefs) / "data"))
    return (fs::path(prefs) / "data").string();
  if (!placed.empty() && writable(fs::path(placed) / "data"))
    return (fs::path(placed) / "data").string();
  return "";
}

bool adopt_placed_settings(const std::string& placed, const std::string& root) {
  if (placed.empty() || root.empty()) return false;
  const fs::path old_root = fs::path(placed) / "data";
  std::error_code ec;
  if (fs::equivalent(old_root, root, ec)) return false;
  const fs::path from = series_settings_file(old_root.string());
  const fs::path to = series_settings_file(root);
  if (!fs::is_regular_file(from, ec) || fs::exists(to, ec)) return false;
  fs::create_directories(to.parent_path(), ec);
  return fs::copy_file(from, to, ec) && !ec;
}

bool inside(const std::string& path, const std::string& root) {
  std::error_code ec;
  const std::string p = fs::weakly_canonical(path, ec).generic_string();
  const std::string r = fs::weakly_canonical(root, ec).generic_string();
  return !r.empty() && (p == r || p.starts_with(r + "/"));
}

bool take_in(const std::string& from, const std::string& root, std::string* basedir,
             std::string* summary, std::string* error,
             const std::function<void(const std::string&)>& progress) {
  if (!root.empty() && inside(from, root)) {
    *basedir = from;
    summary->clear();
    return true;
  }
  if (root.empty()) {
    *error =
        "There is no folder this game can write its data in. Move the game to a folder you "
        "can write to.";
    return false;
  }
  content::ImportReport r;
  content::ImportOptions opts;
  opts.progress = progress;
  std::string why;
  if (!content::import_game(from, root, &r, &why, opts)) {
    *error = "Could not take the game in from " + from + ": " + why;
    return false;
  }
  *basedir = r.game_dir;
  if (r.unchanged) {
    *summary = std::string(r.game->what) + " was taken in from " + from +
               " before and nothing there has changed: it plays from " + r.game_dir + ".";
    return true;
  }
  int kept = 0;
  for (const content::StoredTrack& t : r.tracks) kept += t.kept ? 1 : 0;
  const auto count = [](size_t n, const char* one, const char* many) {
    return std::to_string(n) + " " + (n == 1 ? one : many);
  };
  *summary = "Took " + std::string(r.game->what) + " in from " + from + ": " +
             count(r.copied.size(), "file", "files") + " copied, " +
             count(r.same.size(), "was", "were") + " already there, " +
             count(r.tracks.size(), "music track", "music tracks") + " (" + std::to_string(kept) +
             " new). It plays from " + r.game_dir + ".";
  return true;
}

bool take_in_basedir(std::string* basedir, const std::string& root) {
  namespace out = ge::core::output;
  if (basedir->empty()) return true;
  std::string taken, said, why;
  if (!take_in(expand_home(*basedir), root, &taken, &said, &why,
               [](const std::string& line) { out::info("shooters_edge: " + line); })) {
    out::error("shooters_edge: -basedir " + *basedir + " was not copied into the data folder " +
               root + ": " + why + "; next: give your game's folder, its disc or its disc image");
    return false;
  }
  *basedir = taken;
  if (!said.empty()) out::info("shooters_edge: " + said);
  return true;
}

void add_take_in_commands(ge::console::Console& con, std::string* ask, const std::string* basedir,
                          ge::audio::MusicConsole* music, const std::string* game,
                          const bool* remap) {
  if (music != nullptr)  // its cvar and `cd` are the window's own, bound before the game's
    music->track_of = [basedir, game, remap](int track) {
      if (game != nullptr && remap != nullptr && *remap)
        track = content::q2_remap_track(*game, track);
      return content::track_path(*basedir, track);
    };
  using Con = ge::console::Console;
  using CArgs = ge::console::Args;
  con.add_command("basedir", "basedir <folder>: load your game's install (the folder holding id1)",
                  [ask, basedir](Con& c, const CArgs& a) {
                    if (a.size() < 2)
                      c.print("basedir: " + *basedir + " (basedir <folder> loads another)");
                    else
                      *ask = expand_home(a.size() == 2 ? a[1] : a.rest(0));
                  });
  con.add_command("import",
                  "import <folder or disc image>: copy your game into this game's data folder and "
                  "load it",
                  [ask](Con& c, const CArgs& a) {
                    if (a.size() < 2)
                      c.print("import: give your game's folder, its disc or its disc image");
                    else
                      *ask = expand_home(a.size() == 2 ? a[1] : a.rest(0));
                  });
}

bool take_in_ask(std::string* ask, const std::string& root, std::string* error,
                 const std::function<void(const std::string&)>& print) {
  std::string taken, said;
  if (!take_in(*ask, root, &taken, &said, error, print)) {
    print(*error);
    return false;
  }
  if (!said.empty()) print(said);
  *ask = taken;
  return true;
}

void demo_music(ge::audio::MusicConsole& music, const DemoState& s) {
  if (s.cdtrack >= 0) ge::audio::play_level_music(music, s.cdtrack);
}

void level_music(ge::audio::MusicConsole& music, std::string_view entities) {
  ge::audio::play_level_music(music, content::level_track(entities));
}

}  // namespace shooters_edge::client
