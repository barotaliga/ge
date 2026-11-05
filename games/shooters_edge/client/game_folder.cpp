// The game folder a player loads (game_folder.hpp).
//
// ge:summary The game folder a player loads: the install checked, its game folder, kept, programs
// ge:keywords game-folder check_game_folder game_of remembered_folder remember_folder load_programs
// ge:skill shooter classicenhance
// ge:test tests/shooters_edge_game_folder_test.cpp
#include "shooters_edge/client/game_folder.hpp"

#include <ctime>
#include <filesystem>
#include <fstream>
#include <system_error>
#include <utility>

#include "core/output.hpp"
#include "shooters_edge/server/mod_match.hpp"

namespace shooters_edge::client {

namespace fs = std::filesystem;
namespace out = ge::core::output;

namespace {

constexpr const char* kKeptFile = "game_folder.txt";

std::string lower(std::string s) {
  for (char& c : s) c = c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c;
  return s;
}

// The entry of `dir` named `name` in any case, or "".
fs::path entry_named(const fs::path& dir, std::string_view name) {
  std::error_code ec;
  for (const fs::directory_entry& e : fs::directory_iterator(dir, ec)) {
    if (lower(e.path().filename().string()) == lower(std::string(name))) return e.path();
  }
  return {};
}

// Whether `base` (an install's base folder) holds the first numbered pack or its content zip.
bool holds_first_pack(const fs::path& base, const ge::classicenhance::Profile& content) {
  std::error_code ec;
  for (const std::string& name :
       {ge::classicenhance::pack_name(content, 0), ge::classicenhance::zip_name(content, 0)}) {
    const fs::path p = entry_named(base, name);
    if (!p.empty() && fs::is_regular_file(p, ec)) return true;
  }
  return false;
}

}  // namespace

bool check_game_folder(const std::string& folder, std::string* basedir, std::string* error,
                       const ge::classicenhance::Profile& content) {
  std::error_code ec;
  fs::path root = fs::absolute(fs::path(folder), ec).lexically_normal();
  if (!root.has_filename()) root = root.parent_path();  // a trailing separator
  const std::string first =
      std::string(content.base_dir) + "/" + ge::classicenhance::pack_name(content, 0);
  if (!fs::is_directory(root, ec)) {
    *error = folder +
             " is not a folder; next: pick the folder your game is installed in (the "
             "one holding " +
             first + ")";
    return false;
  }
  // The base folder itself picked: the install is its parent.
  if (lower(root.filename().string()) == lower(std::string(content.base_dir)) &&
      holds_first_pack(root, content))
    root = root.parent_path();
  const fs::path base = entry_named(root, content.base_dir);
  if (base.empty() || !fs::is_directory(base, ec)) {
    *error = root.string() + " holds no " + std::string(content.base_dir) +
             " folder, so it is "
             "not " +
             std::string(content.what) +
             "'s install (only that game is played); next: "
             "pick the folder your game is installed in, the one holding " +
             first;
    return false;
  }
  if (!holds_first_pack(base, content)) {
    *error = base.string() + " holds no " + ge::classicenhance::pack_name(content, 0) + " (nor " +
             ge::classicenhance::zip_name(content, 0) +
             "); next: pick an install with its "
             "packs in place, or copy them from your game's media into " +
             base.string();
    return false;
  }
  *basedir = root.string();
  return true;
}

std::string game_of(const std::string& basedir, const ge::classicenhance::Profile& content) {
  std::error_code ec;
  if (!content.multiplayer_dir.empty()) {
    const fs::path mp = fs::path(basedir.empty() ? "." : basedir) / content.multiplayer_dir;
    if (fs::is_directory(mp, ec)) return std::string(content.multiplayer_dir);
  }
  return std::string(content.base_dir);
}

void choose_folder(Args& args, const std::string& prefs) {
  std::string basedir, why;
  if (!args.basedir_set && !check_game_folder(args.basedir, &basedir, &why)) {
    const std::string kept = remembered_folder(prefs);
    if (!kept.empty() && check_game_folder(kept, &basedir, &why)) {
      out::status("shooters_edge: the game loaded last, " + basedir + " (-basedir names another)");
      args.basedir = basedir;
    }
  }
  if (!args.game_set) args.game = game_of(args.basedir);
}

std::string remembered_folder(const std::string& prefs) {
  if (prefs.empty()) return {};
  std::ifstream f(fs::path(prefs) / kKeptFile);
  std::string line;
  if (!f || !std::getline(f, line)) return {};
  while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
  return line;
}

bool remember_folder(const std::string& prefs, const std::string& basedir) {
  if (prefs.empty()) return false;
  const fs::path file = fs::path(prefs) / kKeptFile;
  const fs::path tmp = fs::path(prefs) / (std::string(kKeptFile) + ".tmp");
  {
    std::ofstream f(tmp, std::ios::trunc);
    if (!f) return false;
    f << basedir << "\n";
    if (!f) return false;
  }
  std::error_code ec;
  fs::rename(tmp, file, ec);
  return !ec;
}

bool load_programs(const GameAdapter& adapter, const SearchPath& files,
                   const std::string& progs_path, GameContent& out, std::string& error,
                   const profiles::GameProfile& game) {
  out.campaign = nullptr;
  out.progs = {};
  out.pack_progs = {};
  out.item_names.clear();
  out.arsenal.clear();
  bool loaded = false;
  if (!progs_path.empty()) {
    std::string source;
    loaded = shooters_edge::server::load_mod(progs_path, out.progs, source, error);
  } else {
    FoundProgram found;
    std::string why;
    if (adapter.find_program(adapter, files, false, found, why)) {
      out::status("shooters_edge: the game's multiplayer program " + std::string(found.file.name) +
                  " runs (-progs names another)");
      out.progs = std::move(found.progs);
      loaded = true;
    } else {
      if (!why.empty()) out::status("shooters_edge: the baseline mod runs (" + why + ")");
      loaded = baseline_progs(out.progs, error);
      if (!archive_paths(files).empty()) {
        out.item_names = profiles::item_name_lines(game.tables.names);
        out.arsenal = profiles::arsenal_lines(game.arsenal);
      }
    }
  }
  if (!loaded) return false;
  // The campaign: the program given when it is a single-player one, else the packs' own.
  if (single_player(out.progs)) {
    out.campaign = &out.progs;
  } else {
    FoundProgram found;
    std::string why;
    if (adapter.find_program(adapter, files, true, found, why)) {
      out.pack_progs = std::move(found.progs);
      out.campaign = &out.pack_progs;
    }
  }
  const ge::classicenhance::Profile& content = profiles::content_of(game);
  std::vector<uint8_t> store;
  out.registered = !find_file(files, content.full_game, store).empty();
  out.start_map = std::string(content.start_map);
  out.first_map =
      find_file(files, ge::classicenhance::asset_path(content.maps, out.start_map), store).empty()
          ? std::string(game.fallback_map)
          : out.start_map;
  return true;
}

void campaign_commands(ge::console::Console& con, Listen& listen, const SearchPath& files,
                       const GameContent& content, std::string& next_map, std::string& load_ask) {
  using CArgs = ge::console::Args;
  using Con = ge::console::Console;
  if (content.campaign == nullptr) {
    for (const char* name : {"newgame", "save", "load"}) con.remove_command(name);
    return;
  }
  con.add_command("newgame", "newgame: a new single-player game from the start map",
                  [&listen, &next_map, &content](Con&, const CArgs&) {
                    listen.opt.console.emplace_back("deathmatch 0");
                    listen.opt.console.emplace_back("coop 0");
                    next_map = content.start_map;
                  });
  con.add_command("save",
                  "save <name>: the single-player game saved as saves/<name>.save (<name>.sav: "
                  "the classic text save in the game folder)",
                  [&listen, &files](Con& c, const CArgs& a) {
                    if (a.size() < 2) {
                      c.print("save: save <name>");
                      return;
                    }
                    std::string why, name = a[1];
                    const bool classic = classic_save_name(name);
                    std::vector<uint8_t> bytes;
                    std::string text;
                    const bool made = classic
                                          ? listen_save_text(listen, text, why)
                                          : listen_save(listen, bytes, why,
                                                        static_cast<int64_t>(std::time(nullptr)));
                    if (classic) bytes.assign(text.begin(), text.end());
                    if (!made || !write_save(files, name, bytes, why, classic))
                      c.print(why);
                    else
                      c.print("saved " + a[1]);
                  });
  con.add_command("load", "load <name>: the game saved as saves/<name>.save, else <name>.sav",
                  [&load_ask](Con& c, const CArgs& a) {
                    if (a.size() < 2)
                      c.print("load: load <name>");
                    else
                      load_ask = a[1];
                  });
}

bool start_map(Listen& listen, const GameContent& content, std::unique_ptr<LoadedMap> map,
               std::string& error) {
  if (content.campaign != nullptr && wants_campaign(listen.opt))
    return listen.campaign_host
               ? listen_change(listen, std::move(map), error)
               : listen_campaign(listen, *content.campaign, std::move(map), listen.opt, error);
  if (listen.campaign_host || listen.progs != &content.progs)
    return listen_start(listen, content.progs, std::move(map), listen.opt, error);
  return listen_change(listen, std::move(map), error);
}

bool load_asked_save(Listen& listen, const GameContent& content, const SearchPath& files,
                     const std::string& name, std::string& error) {
  std::vector<uint8_t> bytes;
  ListenOptions lo = listen.opt;
  lo.console.emplace_back("deathmatch 0");
  std::string base = name;
  bool classic = classic_save_name(base);
  if (!classic && !read_save(files, base, bytes, error))
    classic = read_save(files, base, bytes, error, true);
  else if (classic && !read_save(files, base, bytes, error, true))
    bytes.clear();
  if (bytes.empty() || content.campaign == nullptr) return false;
  if (classic)
    return listen_load_text(
        listen, *content.campaign, lo,
        std::string_view(reinterpret_cast<const char*>(bytes.data()), bytes.size()), name, error);
  return listen_load(listen, *content.campaign, lo, bytes, name, error);
}

}  // namespace shooters_edge::client
