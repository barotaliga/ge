// Games taken into the data folder (take_in.hpp).
//
// ge:summary The data folder's place, and a pointed-at game copied into it
// ge:keywords import take-in data-folder copy
// ge:skill shooter
// ge:test tests/shooters_edge_take_in_test.cpp
#include "shooters_edge/client/take_in.hpp"

#include <filesystem>
#include <fstream>
#include <utility>

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

std::string lower_word(std::string s) {
  for (char& c : s)
    if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
  return s;
}

}  // namespace

std::string data_root(const std::string& placed, const std::string& prefs,
                      const std::string& home) {
  if (!home.empty() && writable(fs::path(home))) return fs::path(home).string();
  if (!prefs.empty() && writable(fs::path(prefs) / "data"))
    return (fs::path(prefs) / "data").string();
  if (!placed.empty() && writable(fs::path(placed) / "data"))
    return (fs::path(placed) / "data").string();
  return "";
}

std::string take_home_dir(std::vector<std::string>* plus) {
  std::string home;
  for (size_t i = 0; i + 2 < plus->size();) {
    if (lower_word((*plus)[i]) == "+set" && lower_word((*plus)[i + 1]) == "homedir") {
      home = expand_home((*plus)[i + 2]);
      plus->erase(plus->begin() + static_cast<std::ptrdiff_t>(i),
                  plus->begin() + static_cast<std::ptrdiff_t>(i + 3));
    } else {
      ++i;
    }
  }
  return home;
}

void add_home_dir(ge::console::Console& con, const std::string& root) {
  con.cvars().add({.name = "homedir",
                   .fallback = root,
                   .flags = ge::console::kReadOnly,
                   .help = "The folder the game writes in: the games taken in, your settings, "
                           "screenshots, recordings and downloads. Choose it with +set homedir on "
                           "the command line.",
                   .kind = ge::console::CvarKind::text});
}

bool inside(const std::string& path, const std::string& root) {
  std::error_code ec;
  const std::string p = fs::weakly_canonical(path, ec).generic_string();
  const std::string r = fs::weakly_canonical(root, ec).generic_string();
  return !r.empty() && (p == r || p.starts_with(r + "/"));
}

namespace {

std::string pack_words(const std::vector<const ge::classicenhance::MissionPack*>& packs) {
  std::string out;
  for (size_t i = 0; i < packs.size(); i++) {
    if (i > 0) out += i + 1 == packs.size() ? " and " : ", ";
    out += std::string(packs[i]->name);
  }
  return out;
}

bool has_game(const content::ImportReport& r) {
  std::error_code ec;
  return r.base ||
         std::filesystem::is_regular_file(
             std::filesystem::path(r.game_dir) / ge::classicenhance::first_pack_path(*r.game), ec);
}

}  // namespace

std::string take_in_words(const content::ImportReport& r, const std::string& from) {
  const std::string game(r.game->what);
  const std::string packs = pack_words(r.packs);
  // What was taken: the game, the game with its packs, or the packs alone.
  const std::string what = !r.base ? packs : r.packs.empty() ? game : game + " with " + packs;
  // How a pack is played: over the game, from its own folder.
  std::string play;
  if (!r.packs.empty()) {
    play = " Start ";
    for (size_t i = 0; i < r.packs.size(); i++)
      play += (i > 0 ? (i + 1 == r.packs.size() ? " or " : ", ") : "") + std::string("-game ") +
              std::string(r.packs[i]->folder);
    play += " to play " + std::string(r.packs.size() == 1 ? "it" : "a mission pack") + ".";
  }
  const bool have_game = has_game(r);
  const std::string needs =
      have_game ? ""
                : " " + std::string(r.packs.size() == 1 ? "It plays" : "They play") + " over " +
                      game + ", which is not here yet: add " + game + " too.";
  if (r.unchanged)
    return what + " was taken in from " + from +
           " before and nothing there has changed: it plays from " + r.game_dir + "." + play +
           needs;
  int kept = 0;
  for (const content::StoredTrack& t : r.tracks) kept += t.kept ? 1 : 0;
  const auto count = [](size_t n, const char* one, const char* many) {
    return std::to_string(n) + " " + (n == 1 ? one : many);
  };
  return "Took " + what + " in from " + from + ": " + count(r.copied.size(), "file", "files") +
         " copied, " + count(r.same.size(), "was", "were") + " already there, " +
         count(r.tracks.size(), "music track", "music tracks") + " (" + std::to_string(kept) +
         " new). It plays from " + r.game_dir + "." + play + needs;
}

std::string offer_words(const content::Offer& o) {
  return "Found " + std::string(o.pack->name) + ", a mission pack for " +
         std::string(o.game->what) + ", in " + o.source + ". Type import \"" + o.source +
         "\" to take it in.";
}

void offer_packs(ge::console::Console& con, const std::string& data_root) {
  if (data_root.empty()) return;
  for (const content::Offer& o : content::find_offers(content::store_folders(), data_root))
    con.print(offer_words(o));
}

bool take_in(const std::string& from, const std::string& root, std::string* basedir,
             std::string* summary, std::string* error,
             const std::function<void(const std::string&)>& progress,
             const std::function<void(const std::string&, double)>& share) {
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
  opts.share = share;
  std::string why;
  if (!content::import_game(from, root, &r, &why, opts)) {
    *error = "Could not take the game in from " + from + ": " + why;
    return false;
  }
  *summary = take_in_words(r, from);
  if (!has_game(r)) {  // the packs are kept; the game playing stays until its game is added
    *error = *summary;
    summary->clear();
    return false;
  }
  *basedir = r.game_dir;
  return true;
}

std::string take_in_basedir(BaseGame* base, const std::string& root, bool later) {
  namespace out = ge::core::output;
  const auto at = base->dirs.find(base->game);
  if (at == base->dirs.end()) return "";
  std::string* basedir = &at->second;
  *basedir = base->dir(base->game);  // a relative folder from the program's (installs.hpp)
  if (basedir->empty() || root.empty() || inside(*basedir, root)) return "";
  content::ImportReport r;
  content::ImportOptions opts;
  opts.taken_only = later;
  opts.progress = [](const std::string& line) { out::info("shooters_edge: " + line); };
  std::string why;
  if (!content::import_game(expand_home(*basedir), root, &r, &why, opts) || !has_game(r)) {
    if (later) return *basedir;
    out::error("shooters_edge: the install " + *basedir + " was not copied into the data folder " +
               root + ": " + why + "; next: give your game's folder, its disc or its disc image");
    return "";
  }
  *basedir = r.game_dir;
  return "";
}

void add_take_in_commands(ge::console::Console& con, std::string* ask, const std::string* basedir,
                          ge::audio::MusicConsole* music, const std::string* game,
                          const bool* remap) {
  if (music != nullptr)  // its cvar and `cd` are the window's own, bound before the game's
    music->track_of = [basedir, game, remap](int track) {
      // A mission pack's own tracks (taken off its disc) first, by the numbers its levels give.
      if (game != nullptr && !game->empty() && game->find('/') == std::string::npos) {
        const std::string own = content::track_path(*basedir + "/" + *game, track);
        if (!own.empty()) return own;
      }
      if (game != nullptr && remap != nullptr && *remap)
        track = content::q2_remap_track(*game, track);
      return content::track_path(*basedir, track);
    };
  if (music != nullptr)  // a track named: `ogg play <name>`, a server's music by text
    music->file_of = [basedir](const std::string& name) {
      return content::music_file(*basedir, name);
    };
  using Con = ge::console::Console;
  using CArgs = ge::console::Args;
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
                 const std::function<void(const std::string&)>& print,
                 const std::function<void(const std::string&, double)>& share) {
  std::string taken, said;
  if (!take_in(*ask, root, &taken, &said, error, print, share)) {
    print(*error);
    return false;
  }
  if (!said.empty()) print(said);
  *ask = taken;
  return true;
}

TakeIn::~TakeIn() {
  if (thread_.joinable()) thread_.join();
}

bool TakeIn::start(std::string* ask, const std::string& root, bool from_menu) {
  if (thread_.joinable() || ask->empty()) return false;
  const std::string from = std::exchange(*ask, {});
  done_ = ok_ = false;
  share_ = -1;
  what_.clear();
  from_menu_ = from_menu;
  thread_ = std::thread([this, from, root] {
    std::string folder = from, error;
    const bool ok = take_in_ask(
        &folder, root, &error,
        [this](const std::string& line) {
          const std::lock_guard<std::mutex> lock(mu_);
          lines_.push_back(line);
        },
        [this](const std::string& what, double share) {
          const std::lock_guard<std::mutex> lock(mu_);
          what_ = what;
          share_ = share;
        });
    const std::lock_guard<std::mutex> lock(mu_);
    folder_ = ok ? folder : std::string();
    error_ = ok ? std::string() : error;
    ok_ = ok;
    done_ = true;
  });
  return true;
}

bool TakeIn::finished(ge::console::Console& con, std::string* folder, std::string* error) {
  if (!thread_.joinable()) return false;
  std::vector<std::string> lines;
  bool done = false;
  {
    const std::lock_guard<std::mutex> lock(mu_);
    lines.swap(lines_);
    done = done_;
  }
  for (const std::string& line : lines) con.print(line);
  if (!done) return false;
  thread_.join();
  *folder = folder_;
  *error = error_;
  return true;
}

void TakeIn::show(ge::console::ConsoleView& view) {
  const bool copying = thread_.joinable();
  if (copying) {
    const std::lock_guard<std::mutex> lock(mu_);
    if (!what_.empty())
      view.set_foot(what_, share_);  // once the copy has begun: a source taken in before has none
  }
  if (copying != held_) {
    held_ = copying;
    view.set_full(copying);
    if (!copying) view.set_foot("");
  }
}

void demo_music(ge::audio::MusicConsole& music, const DemoState& s) {
  if (!s.cdname.empty())
    ge::audio::play_level_named(music, s.cdname);
  else if (s.cdtrack >= 0)
    ge::audio::play_level_music(music, s.cdtrack);
}

void level_music(ge::audio::MusicConsole& music, std::string_view entities) {
  ge::audio::play_level_music(music, content::level_track(entities));
}

}  // namespace shooters_edge::client
