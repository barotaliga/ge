// The game folder a player loads (game_folder.hpp).
//
// ge:summary The game folder a player loads: the install checked, its game folder, kept, programs
// ge:keywords game-folder check_game_folder game_of remembered_game remember_game load_programs
// ge:skill shooter classicenhance
// ge:test tests/shooters_edge_game_folder_test.cpp
#include "shooters_edge/client/game_folder.hpp"

#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <system_error>
#include <utility>

#include "core/output.hpp"
#include "shooters_edge/client/lobby_room.hpp"
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

namespace {

void play_found(Args& args, const FoundInstall& f, const std::string& how) {
  out::status("shooters_edge: " + std::string(f.content->what) + " (" + f.release + ") in " +
              f.basedir + ", " + how + " (-basedir names another)");
  args.basedir = f.basedir;
  args.profile = std::string(f.content->id);
  if (!args.game_set) args.game = game_of(f.basedir, *f.content);
}

}  // namespace

void choose_folder(Args& args, const std::string& prefs, const std::vector<std::string>& places,
                   std::vector<FoundInstall>* found) {
  if (!args.basedir_set) {
    const KeptGame kept = remembered_game(prefs);
    // Started inside an install, or placed above some: played at once, ahead of the one kept.
    std::vector<std::string> where{args.basedir};
    where.insert(where.end(), places.begin(), places.end());
    std::vector<FoundInstall> below;
    for (const std::string& place : where) {
      if (place.empty()) continue;
      for (FoundInstall& f : installs_below(place)) {
        std::error_code ec;
        const bool seen = std::any_of(below.begin(), below.end(), [&](const FoundInstall& b) {
          return fs::equivalent(b.basedir, f.basedir, ec);
        });
        if (!seen) below.push_back(std::move(f));
      }
    }
    if (found != nullptr) *found = below;
    for (const std::string& place : where) {
      FoundInstall inside;
      if (!place.empty() && install_around(place, inside)) {
        std::error_code ec;
        if (found != nullptr && std::none_of(found->begin(), found->end(), [&](const auto& b) {
              return fs::equivalent(b.basedir, inside.basedir, ec);
            }))
          found->insert(found->begin(), inside);
        play_found(args, inside, "the install the game was started in");
        return;
      }
    }
    if (const int pick = pick_install(below, kept.basedir); pick >= 0) {
      play_found(args, below[static_cast<size_t>(pick)],
                 below.size() == 1 ? "the install beside the game"
                                   : "one of " + std::to_string(below.size()) +
                                         " installs beside the game (games lists them)");
      return;
    }
    const ge::classicenhance::Profile* content =
        kept.basedir.empty() ? nullptr : ge::classicenhance::find_profile(kept.profile);
    std::string basedir, why;
    if (!kept.basedir.empty() && content == nullptr) {
      out::warn("shooters_edge: the game loaded last is " + kept.profile +
                ", which this build does not know (it knows " + ge::classicenhance::profile_ids() +
                "); next: OPTIONS > CLASSIC ENHANCED > LOAD GAME");
    } else if (content != nullptr && check_game_folder(kept.basedir, &basedir, &why, *content)) {
      out::status("shooters_edge: the game loaded last, " + std::string(content->what) + " in " +
                  basedir + " (-basedir names another)");
      args.basedir = basedir;
      args.profile = std::string(content->id);
      if (!args.game_set) args.game = game_of(basedir, *content);
      return;
    } else if (content != nullptr) {
      out::warn("shooters_edge: the game loaded last no longer opens: " + why +
                "; so the folder the game runs in; next: OPTIONS > CLASSIC ENHANCED > LOAD GAME");
    }
  }
  // The install named or the folder the game runs in: whichever game its folders say it is.
  std::string basedir, why;
  if (const ge::classicenhance::Profile* content = install_of(args.basedir, &basedir, &why)) {
    args.profile = std::string(content->id);
    if (!args.game_set) args.game = game_of(args.basedir, *content);
    return;
  }
  if (!args.game_set) args.game = game_of(args.basedir);
}

KeptGame remembered_game(const std::string& prefs) {
  KeptGame kept;
  if (prefs.empty()) return kept;
  std::ifstream f(fs::path(prefs) / kKeptFile);
  std::string line;
  bool keyed = false;
  while (f && std::getline(f, line)) {
    while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
    if (line.rfind("profile ", 0) == 0) {
      kept.profile = line.substr(8);
      keyed = true;
    } else if (line.rfind("basedir ", 0) == 0) {
      kept.basedir = line.substr(8);
      keyed = true;
    } else if (!keyed && kept.basedir.empty() && !line.empty()) {
      kept.basedir = line;  // the older form: the folder alone
    }
  }
  if (!kept.basedir.empty() && kept.profile.empty())
    kept.profile = std::string(ge::classicenhance::classic_profile().id);
  return kept;
}

bool remember_game(const std::string& prefs, const std::string& basedir,
                   const ge::classicenhance::Profile& content) {
  if (prefs.empty()) return false;
  const fs::path file = fs::path(prefs) / kKeptFile;
  const fs::path tmp = fs::path(prefs) / (std::string(kKeptFile) + ".tmp");
  {
    std::ofstream f(tmp, std::ios::trunc);
    if (!f) return false;
    f << "profile " << content.id << "\nbasedir " << basedir << "\n";
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
        for (std::string& line : profiles::word_lines(game.words))
          out.item_names.push_back(std::move(line));
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
  out.episodes.start_map = out.start_map;
  out.episodes.first_maps.assign(game.episode_maps.begin(), game.episode_maps.end());
  out.first_map =
      find_file(files, ge::classicenhance::asset_path(content.maps, out.start_map), store).empty()
          ? std::string(game.fallback_map)
          : out.start_map;
  return true;
}

namespace {

// NEW GAME and `coopgame` for a game whose own rules keep the campaign (GameContent's
// `rules_campaign`): the multiplayer program's game on the first map with `deathmatch` 0, `coop`
// off or on; a new game each time (`ListenOptions::new_game`). Companions cannot play its maps
// yet, so `coopgame` seats none.
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
                  });
  con.add_command("coopgame",
                  "coopgame [map]: a new coop game from the first map or the map named (everyone "
                  "on one team; each player comes back as he entered the level)",
                  [&listen, &next_map, &content](Con& c, const CArgs& a) {
                    std::string map = content.first_map;
                    for (size_t i = 1; i < a.size(); i++) {
                      const std::string& w = a[i];
                      sh::AimSkill skill;
                      if (!w.empty() && w[0] >= '0' && w[0] <= '9') {
                        if (std::atoi(w.c_str()) > 0)
                          c.print("Companions can't join this game yet, so you start alone.");
                      } else if (!skill_named(w, skill)) {
                        map = w;
                      }
                    }
                    listen.opt.console.emplace_back("deathmatch 0");
                    listen.opt.console.emplace_back("coop 1");
                    listen.opt.companions = 0;
                    listen.opt.new_game = true;
                    next_map = map;
                  });
}

}  // namespace

void campaign_commands(ge::console::Console& con, Listen& listen, const SearchPath& files,
                       const GameContent& content, std::string& next_map, std::string& load_ask) {
  using CArgs = ge::console::Args;
  using Con = ge::console::Console;
  if (content.campaign == nullptr) {
    for (const char* name : {"newgame", "coopgame", "save", "load"}) con.remove_command(name);
    if (content.rules_campaign) rules_campaign_commands(con, listen, content, next_map);
    return;
  }
  con.add_command("newgame", "newgame: a new single-player game from the start map",
                  [&listen, &next_map, &content](Con& c, const CArgs&) {
                    listen.opt.console.emplace_back("deathmatch 0");
                    listen.opt.console.emplace_back("coop 0");
                    // What the player has finished opens and shuts the start map's episodes.
                    if (const ge::console::Cvar* done = c.cvars().find("sv_episodes_done"))
                      listen.opt.console.emplace_back("sv_episodes_done " + done->value);
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
        sh::AimSkill skill = sh::AimSkill::average;
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
        if (const ge::console::Cvar* done = c.cvars().find("sv_episodes_done"))
          listen.opt.console.emplace_back("sv_episodes_done " + done->value);
        listen.opt.companions = bots;
        listen.opt.companion_skill = skill;
        listen.opt.new_game = true;  // even with a campaign running: coop on, the bots seated
        next_map = map;
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
  if (content.campaign != nullptr && wants_campaign(listen.opt)) {
    if (listen.campaign_host && !listen.opt.new_game)
      return listen_change(listen, std::move(map), error);
    listen.opt.new_game = false;
    return listen_campaign(listen, *content.campaign, std::move(map), listen.opt, error);
  }
  // A new game on rules that keep their own campaign starts their server afresh (its settings
  // are the new game's); else a level change keeps it (net/classic_game.hpp).
  const bool fresh = content.rules_campaign && listen.opt.new_game;
  listen.opt.new_game = false;
  if (fresh || listen.campaign_host || listen.progs != &content.progs)
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
