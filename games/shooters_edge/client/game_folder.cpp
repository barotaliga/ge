// The game folder a player loads (game_folder.hpp).
//
// ge:summary The game folder a player loads: the install checked, its game folder, programs
// ge:keywords game-folder check_game_folder game_of choose_folder fs_basegame load_programs
// ge:skill shooter classicenhance
// ge:test tests/shooters_edge_game_folder_test.cpp
#include "shooters_edge/client/game_folder.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <system_error>
#include <utility>

#include "classic/content/fs_words.hpp"
#include "classic/content/load_order.hpp"
#include "core/output.hpp"
#include "shooters_edge/client/installs.hpp"
#include "shooters_edge/client/lobby_room.hpp"
#include "shooters_edge/server/mod_match.hpp"
#include "shooters_edge/server/q2_campaign.hpp"
#include "shooters_edge/server/q2_native.hpp"
#include "shooters_edge/server/q2_pack.hpp"

namespace sb = ge::shooterbots;

namespace shooters_edge::client {

namespace fs = std::filesystem;
namespace out = ge::core::output;

namespace {

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
             " is not a folder. Pick the folder your game is installed in (the "
             "one holding " +
             first + ").";
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
             "'s install (only that game is played). "
             "Pick the folder your game is installed in, the one holding " +
             first + ".";
    return false;
  }
  if (!holds_first_pack(base, content)) {
    *error = base.string() + " holds no " + ge::classicenhance::pack_name(content, 0) + " (nor " +
             ge::classicenhance::zip_name(content, 0) +
             "). Pick an install with its "
             "packs in place, or copy them from your game's media into " +
             base.string() + ".";
    return false;
  }
  *basedir = root.string();
  return true;
}

const ge::classicenhance::Profile* install_of(const std::string& folder, std::string* basedir,
                                              std::string* error) {
  std::string first_why;
  for (const ge::classicenhance::Profile& p : ge::classicenhance::kProfiles) {
    std::string why;
    if (check_game_folder(folder, basedir, &why, p)) return &p;
    if (first_why.empty()) first_why = std::move(why);
  }
  *error = first_why + " (the games known: " + ge::classicenhance::profile_ids() + ")";
  return nullptr;
}

std::string game_of(const std::string& basedir, const ge::classicenhance::Profile& content) {
  std::error_code ec;
  if (!content.multiplayer_dir.empty()) {
    const fs::path mp = fs::path(basedir.empty() ? "." : basedir) / content.multiplayer_dir;
    if (fs::is_directory(mp, ec)) return std::string(content.multiplayer_dir);
  }
  return std::string(content.base_dir);
}

std::string own_game_folder(const std::string& data_root) {
  return data_root.empty() ? "." : (fs::path(data_root) / "se").string();
}

void choose_folder(Args& args, const BaseGame& base, const std::string& own) {
  const ge::classicenhance::Profile* want =
      base.game.empty() ? nullptr : ge::classicenhance::find_profile(base.game);
  if (!base.game.empty() && want == nullptr)
    out::warn("shooters_edge: " + std::string(kBaseGame) + " " + base.game +
              " is no game this build knows (it knows " + ge::classicenhance::profile_ids() +
              "); next: set " + std::string(kBaseGame) + " to one of them, or \"\"");
  if (want == nullptr) {
    // Shooter's Edge's own game: its own folder in the data folder.
    args.basedir = own;
    if (!args.game_set) args.game = game_of(args.basedir);
    return;
  }
  args.profile = std::string(want->id);
  const std::string kept = base.dir(want->id);
  std::string basedir, why;
  if (!kept.empty() && check_game_folder(kept, &basedir, &why, *want)) {
    out::status("shooters_edge: " + std::string(want->what) + " in " + basedir + " (" +
                basedir_setting(want->id) + ")");
    args.basedir = basedir;
    if (!args.game_set) args.game = game_of(basedir, *want);
    return;
  }
  if (!kept.empty())
    out::warn("shooters_edge: " + basedir_setting(want->id) + " " + kept +
              " does not open: " + why + "; next: pick its folder in the chooser");
  else
    out::status("shooters_edge: no install of " + std::string(want->what) + " is set (" +
                basedir_setting(want->id) + "); asking for its folder");
  args.ask_install = true;
  args.basedir = own;
  if (!args.game_set) args.game = game_of(args.basedir, *want);
}

namespace {

// The compiled module `qwprogs.qvm` a mod folder of `files` holds, into `out`; false when none.
bool qvm_of(const SearchPath& files, std::vector<uint8_t>& out) {
  std::vector<uint8_t> store;
  const std::span<const uint8_t> found = find_file(files, "qwprogs.qvm", store);
  if (found.empty()) return false;
  out.assign(found.begin(), found.end());
  return true;
}

}  // namespace

bool load_programs(const GameAdapter& adapter, const SearchPath& files,
                   const std::string& progs_path, GameContent& out, std::string& error,
                   const profiles::GameProfile& game) {
  out.campaign = nullptr;
  out.progs = {};
  out.pack_progs = {};
  out.module.clear();
  out.native_library.clear();
  out.item_names.clear();
  out.arsenal.clear();
  bool loaded = false;
  if (!progs_path.empty()) {
    std::string source;
    loaded = shooters_edge::server::load_mod(progs_path, out.progs, source, error);
  } else if (std::filesystem::path lib;
             !files.folder.empty() &&
             shooters_edge::server::find_native_library(
                 std::filesystem::path(files.folder).parent_path(),
                 std::filesystem::path(files.folder).filename().string(), "", lib) &&
             lib.parent_path() == std::filesystem::path(files.folder)) {
    // A mod folder's game library of Quake II's interface: it plays the match; the baseline
    // stands in for the rest.
    out::status("shooters_edge: the mod's game library " + lib.filename().string() + " runs");
    out.native_library = lib.string();
    loaded = baseline_progs(out.progs, error);
  } else if (adapter.game != nullptr && adapter.game->id == std::string_view("q1") &&
             qvm_of(files, out.module)) {
    // A mod folder's compiled module: it plays the match; the baseline stands in for the rest
    // (a campaign is still the packs' own program).
    out::status("shooters_edge: the mod's compiled module qwprogs.qvm runs");
    loaded = baseline_progs(out.progs, error);
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
        for (std::string& line : profiles::word_lines(game.words))
          out.item_names.push_back(std::move(line));
        if (!game.arena_rules.empty())
          out.item_names.push_back("localinfo rules " + std::string(game.arena_rules));
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
  out.rules_campaign = out.campaign == nullptr && game.rules_campaign;
  const ge::classicenhance::Profile& content = profiles::content_of(game);
  std::vector<uint8_t> store;
  out.registered = !find_file(files, content.full_game, store).empty();
  out.start_map = std::string(content.start_map);
  // A Quake II mission pack starts on its own first map (its own `newgame`).
  if (game.id == "q2")
    if (const std::string_view first = shooters_edge::server::q2_pack_start_map(
            shooters_edge::server::q2_pack_of_folder(files.folder));
        !first.empty())
      out.start_map = std::string(first);
  out.episodes.start_map = out.start_map;
  out.episodes.first_maps.assign(game.episode_maps.begin(), game.episode_maps.end());
  out.first_map =
      find_file(files, ge::classicenhance::asset_path(content.maps, out.start_map), store).empty()
          ? std::string(game.fallback_map)
          : out.start_map;
  // NEW GAME as the packs' default.cfg names it (a pack's own first map and its movies first),
  // when the packs hold that map.
  std::string map, shows;
  const std::span<const uint8_t> cfg = find_file(files, "default.cfg", store);
  if (new_game_level(std::string(cfg.begin(), cfg.end()), map, shows) &&
      !find_file(files, ge::classicenhance::asset_path(content.maps, map), store).empty()) {
    out.first_map = map;
    out.new_game_shows = shows;
  }
  return true;
}

bool new_game_level(const std::string& cfg, std::string& map, std::string& shows) {
  // The words of `text` from `at` on, split at blanks; a `"` starts or ends a quoted word.
  const auto word = [](const std::string& text, size_t& at) {
    at = std::min(text.find_first_not_of(" \t\r", at), text.size());
    const bool quoted = at < text.size() && text[at] == '"';
    const size_t start = at + (quoted ? 1 : 0);
    const size_t stop =
        std::min(text.find_first_of(quoted ? "\"" : " \t\r;\"", start), text.size());
    at = stop + (quoted && stop < text.size() ? 1 : 0);
    return text.substr(start, stop - start);
  };
  size_t from = 0;
  while (from < cfg.size()) {
    const size_t end = std::min(cfg.find('\n', from), cfg.size());
    std::string line = cfg.substr(from, end - from);
    from = end + 1;
    for (char& c : line) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    size_t at = 0;
    if (word(line, at) != "alias" || word(line, at) != "newgame") continue;
    const std::string body = word(line, at);  // its commands, `;` between them
    for (size_t part = 0; part < body.size();) {
      const size_t semi = std::min(body.find(';', part), body.size());
      const std::string command = body.substr(part, semi - part);
      part = semi + 1;
      size_t w = 0;
      if (word(command, w) != "map") continue;
      const sv::LevelString ls = sv::level_string(word(command, w));
      if (ls.map.empty()) return false;
      map = ls.map;
      shows = ls.shows;
      return true;
    }
    return false;
  }
  return false;
}

namespace {

// NEW GAME and `coopgame` for a game whose own rules keep the campaign (GameContent's
// `rules_campaign`): the multiplayer program's game on the first map with `deathmatch` 0, `coop`
// off or on; a new game each time (`ListenOptions::new_game`). `coopgame` seats its companions
// once the game has started (client/q2_party.hpp), `addbot` and `removebot` after.
void rules_campaign_commands(ge::console::Console& con, Listen& listen, const GameContent& content,
                             std::string& next_map) {
  using CArgs = ge::console::Args;
  using Con = ge::console::Console;
  con.add_command("newgame", "newgame: a new single-player game from the first map",
                  [&listen, &next_map, &content](Con&, const CArgs&) {
                    listen.opt.console.emplace_back("deathmatch 0");
                    listen.opt.console.emplace_back("coop 0");
                    listen.opt.companions = 0;
                    listen.opt.new_game = true;
                    next_map = content.first_map;
                    listen.cinematic = content.new_game_shows;  // the pack's movies first
                  });
  con.add_command("coopgame",
                  "coopgame [bots] [novice|average|expert] [map]: a new coop game (everyone on "
                  "one team; each player comes back as he entered the level) with that many "
                  "companion bots beside you, 1 by default, from the first map or the map named; "
                  "addbot and removebot change the party during the game",
                  [&listen, &next_map, &content](Con&, const CArgs& a) {
                    std::string map = content.first_map;
                    int bots = 1;
                    sb::AimSkill skill = sb::AimSkill::average;
                    for (size_t i = 1; i < a.size(); i++) {
                      const std::string& w = a[i];
                      if (!w.empty() && w[0] >= '0' && w[0] <= '9')
                        bots = std::atoi(w.c_str());
                      else if (!skill_named(w, skill))
                        map = w;
                    }
                    listen.opt.console.emplace_back("deathmatch 0");
                    listen.opt.console.emplace_back("coop 1");
                    listen.opt.companions = bots;
                    listen.opt.companion_skill = skill;
                    listen.opt.new_game = true;
                    next_map = map;
                  });
}

}  // namespace

namespace {

// Where a save named `name` goes, as the player reads it: the classic file in the game folder,
// else ours under saves/.
std::string save_shown(const std::string& name, bool classic) {
  return classic ? name + ".sav" : "saves/" + name + ".save";
}

// `save <name>` and `load <name>` (and Quake II's words for them, `savegame` and `loadgame`).
// `quake`: Quake's own words round them ("Saving game to ...", "done.", "Loading game from
// ..."), as the original prints them; else ours.
void save_commands(ge::console::Console& con, Listen& listen, const SearchPath& files,
                   std::string& load_ask, bool quake) {
  using CArgs = ge::console::Args;
  using Con = ge::console::Console;
  for (const char* save : {"save", "savegame"})
    con.add_command(save,
                    "save <name>: the single-player game saved as saves/<name>.save (<name>.sav: "
                    "the classic text save in the game folder)",
                    [&listen, &files, quake](Con& c, const CArgs& a) {
                      if (a.size() < 2) {
                        c.print("save: save <name>");
                        return;
                      }
                      std::string why, name = a[1];
                      const bool classic = classic_save_name(name);
                      if (quake) c.print("Saving game to " + save_shown(name, classic) + "...");
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
                        c.print(quake ? "done." : "saved " + a[1]);
                    });
  for (const char* load : {"load", "loadgame"})
    con.add_command(load, "load <name>: the game saved as saves/<name>.save, else <name>.sav",
                    [&load_ask, quake](Con& c, const CArgs& a) {
                      if (a.size() < 2) {
                        c.print("load: load <name>");
                        return;
                      }
                      load_ask = a[1];
                      std::string name = a[1];
                      const bool classic = classic_save_name(name);
                      if (quake) c.print("Loading game from " + save_shown(name, classic) + "...");
                    });
}

}  // namespace

void campaign_commands(ge::console::Console& con, Listen& listen, const SearchPath& files,
                       const GameContent& content, std::string& next_map, std::string& load_ask) {
  using CArgs = ge::console::Args;
  using Con = ge::console::Console;
  if (content.campaign == nullptr) {
    for (const char* name : {"newgame", "coopgame", "save", "load", "savegame", "loadgame"})
      con.remove_command(name);
    if (content.rules_campaign) {  // Quake II's game keeps its own saves (server/q2_save.hpp)
      rules_campaign_commands(con, listen, content, next_map);
      save_commands(con, listen, files, load_ask, false);
    }
    return;
  }
  con.add_command("newgame", "newgame: a new single-player game from the start map",
                  [&listen, &next_map, &content](Con& c, const CArgs&) {
                    listen.opt.console.emplace_back("deathmatch 0");
                    listen.opt.console.emplace_back("coop 0");
                    // What the player has finished opens and shuts the start map's episodes.
                    for (const char* k : {"sv_episodes_in_order", "sv_episodes_done"})
                      if (const ge::console::Cvar* v = c.cvars().find(k))
                        listen.opt.console.emplace_back(std::string(k) + " " + v->value);
                    listen.opt.companions = 0;
                    listen.opt.new_game = true;
                    next_map = content.start_map;
                  });
  con.add_command(
      "coopgame",
      "coopgame [bots] [novice|average|expert] [map]: a new coop game (teammates spared unless "
      "sv_friendlyfire 1) with that many companion bots beside you, 1 by default, from the start "
      "map or the map named; addbot and removebot change the party during the game",
      [&listen, &next_map, &content](Con& c, const CArgs& a) {
        int bots = 1;
        sb::AimSkill skill = sb::AimSkill::average;
        std::string map = content.start_map;
        for (size_t i = 1; i < a.size(); i++) {
          const std::string& w = a[i];
          if (!w.empty() && w[0] >= '0' && w[0] <= '9') {
            bots = std::atoi(w.c_str());
          } else if (!skill_named(w, skill)) {
            map = w;
          }
        }
        if (bots < 0 || bots > kPartyMost) {
          c.print("coopgame: 0 to " + std::to_string(kPartyMost) + " bots");
          return;
        }
        listen.opt.console.emplace_back("deathmatch 0");
        listen.opt.console.emplace_back("coop 1");
        for (const char* k : {"sv_episodes_in_order", "sv_episodes_done"})
          if (const ge::console::Cvar* v = c.cvars().find(k))
            listen.opt.console.emplace_back(std::string(k) + " " + v->value);
        listen.opt.companions = bots;
        listen.opt.companion_skill = skill;
        listen.opt.new_game = true;  // even with a campaign running: coop on, the bots seated
        next_map = map;
      });
  save_commands(con, listen, files, load_ask, true);
}

bool start_map(Listen& listen, const GameContent& content, std::unique_ptr<LoadedMap> map,
               std::string& error) {
  if (content.campaign != nullptr && wants_campaign(listen.opt)) {
    if (listen.campaign_host && !listen.opt.new_game)
      return listen_change(listen, std::move(map), error);
    listen.opt.new_game = false;
    return listen_campaign(listen, *content.campaign, std::move(map), listen.opt, error);
  }
  // A new game on rules that keep their own campaign starts their server afresh (its settings
  // are the new game's); else a level change keeps it (net/qw_game.hpp).
  const bool fresh = content.rules_campaign && listen.opt.new_game;
  listen.opt.new_game = false;
  if (fresh) {  // the new game's companions seated once it runs (`coopgame <bots>`)
    // A campaign has no lobby (its companions are the game's own, client/q2_party.hpp): the
    // server starts without one, and sv_lobby stays as the player set it for the next match.
    const bool lobby = listen.opt.lobby;
    listen.opt.lobby = false;
    const bool started = listen_start(listen, content.progs, std::move(map), listen.opt, error);
    listen.opt.lobby = lobby;
    if (!started) return false;
    const std::string add = "addbot " + std::string(sb::aim_skill_name(listen.opt.companion_skill));
    for (int k = 0; k < listen.opt.companions; k++) listen_lobby(listen, add);
    return true;
  }
  if (listen.campaign_host || listen.progs != &content.progs)
    return listen_start(listen, content.progs, std::move(map), listen.opt, error);
  return listen_change(listen, std::move(map), error);
}

std::vector<std::string> save_slot_labels(const SearchPath& files,
                                          std::span<const std::string> names) {
  std::vector<std::string> out;
  out.reserve(names.size());
  for (const std::string& name : names) out.push_back(save_label(files, name));
  return out;
}

bool keep_autosave(Listen& listen, const SearchPath& files) {
  const std::vector<uint8_t> autosave = listen_take_autosave(listen);
  std::string why;
  if (!autosave.empty() && !write_save(files, "save0", autosave, why, false))
    listen.prints.push_back("The autosave was not written: " + why + "\n");
  return std::exchange(listen.load_menu_asked, false);
}

std::string load_save_asked(Listen& listen, const GameContent& content, const SearchPath& files,
                            std::string& ask) {
  const std::string name = std::exchange(ask, std::string());
  std::string error;
  return load_asked_save(listen, content, files, name, error) ? "loaded " + name : "load: " + error;
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
  if (!bytes.empty() && !classic && content.campaign == nullptr && content.rules_campaign)
    return listen_load_q2(listen, lo, bytes, name, error);  // Quake II's own save
  if (bytes.empty() || content.campaign == nullptr) return false;
  if (classic)
    return listen_load_text(
        listen, *content.campaign, lo,
        std::string_view(reinterpret_cast<const char*>(bytes.data()), bytes.size()), name, error);
  return listen_load(listen, *content.campaign, lo, bytes, name, error);
}

void add_path_command(ge::console::Console& con, const SearchPath& files) {
  con.add_command("path", "path: the folders and archives the game's files come from",
                  [&files](ge::console::Console& c, const ge::console::Args&) {
                    std::string text = ge::classic::content::search_path_text(files);
                    while (!text.empty()) {
                      const size_t nl = text.find('\n');
                      c.print(text.substr(0, nl));
                      text.erase(0, nl == std::string::npos ? text.size() : nl + 1);
                    }
                  });
  // The read-only links over the search path (classic/content/fs_words.hpp), and fs_restart's
  // listing of it; dir, fdir and whereis are the engine's (vfs/search_commands.hpp).
  const auto say = [](ge::console::Console& c, std::string text) {
    while (!text.empty()) {
      const size_t nl = text.find('\n');
      c.print(text.substr(0, nl));
      text.erase(0, nl == std::string::npos ? text.size() : nl + 1);
    }
  };
  const std::pair<const char*, const char*> words[] = {
      {"link", "link [<name> <target>]: a link over the game's files made, or listed"},
      {"softlink", "softlink [<name> <target>]: a link over the game's files made, or listed"},
      {"unlink", "unlink <name> | -a: a link over the game's files taken away"},
      {"softunlink", "softunlink <name> | -a: a link over the game's files taken away"}};
  for (const auto& [word, help] : words) {
    con.add_command(word, help, [&files, say](ge::console::Console& c, const ge::console::Args& a) {
      std::vector<std::string> w;
      for (size_t i = 0; i < a.size(); ++i) w.push_back(a[i]);
      std::string out;
      if (ge::classic::content::fs_command(files, w, out)) say(c, out);
    });
  }
  con.add_command("fs_restart", "fs_restart: the folders and archives the game reads, listed again",
                  [&files, say](ge::console::Console& c, const ge::console::Args&) {
                    say(c, ge::classic::content::fs_restart_text(files));
                  });
}

}  // namespace shooters_edge::client
