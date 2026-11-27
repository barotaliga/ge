// THE GAME FOLDER THE WINDOW OPENS: the install of the game `fs_basegame` names, the folder its
// `fs_basedir_<id>` holds (client/installs.hpp), set by SELECT GAME, OPTIONS ▸ CLASSIC ENHANCED ▸
// LOAD GAME (the desktop's folder chooser) or the console, and what the window reads out of it
// once it is open.
//
// - **Which folder** (`check_game_folder`): the install's own folder, the one holding the content
//   profile's base folder with its first numbered pack (`id1/pak0.pak`, or the content zip standing
//   in for it, names in any case). The base folder itself picked is taken as its parent. Any other
//   folder is refused with what it lacks: only a game the content profiles know is played.
// - **Which game** (`install_of`): the first content profile whose base folder and first pack the
//   folder holds, so an install of Quake II plays as Quake II with no switch to set.
// - **Which game folder over it** (`game_of`): the profile's multiplayer folder
//   (`multiplayer_dir`: its player skins and its multiplayer program) when the install has one,
//   else the base folder, so the install plays as the multiplayer client sees it.
// - **Which install opens** (`choose_folder`): the game `fs_basegame` names, from its
//   `fs_basedir_<id>`, or the window asks for its folder when that is empty or does not open; with
//   `fs_basegame` "", Shooter's Edge's own game, from its own folder in the data folder
//   (`own_game_folder`, `<data folder>/se`). The installs around the game
//   (client/install_search.hpp) are only listed, for `games`.
// - **Where the player's files go**: under the game folder opened, so each game keeps its own:
//   `<data folder>/<q1|q2|se>/<game folder>/` holds its `saves/`, `screenshots/`, recordings,
//   `settings/config.cfg` (its own meanings) and downloads. A classic game is played from its copy
//   in the data folder (client/take_in.hpp), never from the install it was taken from.
// - **What it holds** (`load_programs`): the multiplayer program (`-progs`, else the game's own
//   program files, a mod's folder first, else the baseline mod), the single-player program the
//   campaign runs, whether the packs are the full game, and the first map (the profile's start
//   map when the packs hold it, else the game's fallback); and the campaign's commands it lights
//   (`campaign_commands`: `newgame`, `coopgame`, `save`, `load`).
//
// ge:summary The game folder a player loads: the install checked, its game folder, programs
// ge:keywords game-folder load-game basedir install check_game_folder game_of multiplayer-dir
// ge:keywords install_of which-game content-profile q2
// ge:keywords fs_basegame fs_basedir game-played load_programs first-map
// ge:keywords campaign-program path-command search-path
// ge:keywords own_game_folder user-files per-game-folder
// ge:keywords choose_folder campaign_commands newgame coopgame save load new_game_level intro
// ge:skill shooter classicenhance
// ge:test tests/shooters_edge_game_folder_test.cpp
#pragma once

#include <span>
#include <string>
#include <vector>

#include "classicenhance/profile.hpp"
#include "console/console.hpp"
#include "progs/file.hpp"
#include "shooters_edge/client/args.hpp"
#include "shooters_edge/client/files.hpp"
#include "shooters_edge/client/game_adapter.hpp"
#include "shooters_edge/client/installs.hpp"
#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/profiles/game_profile.hpp"

namespace shooters_edge::client {

// The install at `folder` (or its base folder picked): true with the install's folder in
// `*basedir`; false with what is missing and the next step in `*error`.
bool check_game_folder(
    const std::string& folder, std::string* basedir, std::string* error,
    const ge::classicenhance::Profile& content = ge::classicenhance::q1_profile());

// Which classic game the install at `folder` (or its base folder picked) is: the first content
// profile whose folders it holds, with the install's folder in `*basedir`; null when it is none
// of them, with why in `*error` (what the first profile's install would hold, and the others
// named).
const ge::classicenhance::Profile* install_of(const std::string& folder, std::string* basedir,
                                              std::string* error);

// The game folder played over the install at `basedir`: the multiplayer folder when it has one,
// else the base folder.
std::string game_of(const std::string& basedir,
                    const ge::classicenhance::Profile& content = ge::classicenhance::q1_profile());

// The install the window opens, into `args`: the game `base.game` names, from its
// `fs_basedir_<id>` when that still checks, else `args.ask_install` (the chooser asks for it);
// `base.game` "" (or a game no profile knows) is Shooter's Edge's own, in `own`
// (`own_game_folder`), which is also the folder while the chooser asks. Then the game folder over
// it, `-game`, else `game_of`.
void choose_folder(Args& args, const BaseGame& base, const std::string& own);

// Shooter's Edge's own game's folder, `<data folder>/se`: its saves, screenshots, recordings and
// settings kept apart from each classic game's (`<data folder>/q1`, `<data folder>/q2`, where
// the game taken in is). With no data folder, "." (nothing is written).
std::string own_game_folder(const std::string& data_root);

// What the window plays out of an open game folder. `campaign` points into this struct (at
// `progs` or `pack_progs`) or is null, so it is filled in place and never moved.
struct GameContent {
  ge::progs::Progs progs;  // the multiplayer program
  // A mod folder's compiled module (its `qwprogs.qvm`), which plays a match in place of `progs`
  // (the mod's rules, physics and weapons: classic remix's other parts take only what it leaves);
  // empty: `progs` plays.
  std::vector<uint8_t> module;
  // A mod folder's game library of Quake II's interface (server/q2_native.hpp: the
  // community's mods built for Quake II), which plays a match in place of `progs` on any map,
  // classic remix's too; empty: none.
  std::string native_library;
  ge::progs::Progs pack_progs;  // the packs' single-player program, when `progs` is not one
  const ge::progs::Progs* campaign = nullptr;  // what NEW GAME runs; null: no campaign
  // No campaign program, but the game's own rules keep one (profiles::GameProfile's
  // `rules_campaign`): NEW GAME and `coopgame` start the multiplayer program's game with
  // `deathmatch` 0 on the start map.
  bool rules_campaign = false;
  bool registered = false;    // the packs are the full game
  std::string start_map;      // the profile's start map
  sv::EpisodeOrder episodes;  // the start map and each episode's first map (the profile's)
  // The first map NEW GAME starts: the map the packs' default.cfg names for `newgame` (a pack's
  // own first map) when they hold it, else the start map when they hold it, else the game's
  // fallback; `new_game_shows` the movies that cfg plays before it (`+` joined), else none.
  std::string first_map;
  std::string new_game_shows;
  // The game's item names and its server's words as server lines (profiles/items.hpp's
  // `item_name_lines`, profiles/words.hpp's `word_lines`) when the baseline mod plays the game's
  // packs, so its pickups, obituaries, clock and lobby speak the game's words; empty when the
  // game's own program runs (it has its own) or no packs are open (the mod's own words).
  std::vector<std::string> item_names;
  // The game's weapons and armour as server lines (profiles/arsenal.hpp's `arsenal_lines`) when
  // our programs play the game's packs, so they fire and absorb by its rows; empty otherwise
  // (our programs' own rows).
  std::vector<std::string> arsenal;
};

// The level a game's default.cfg gives its `newgame` alias (`alias newgame "map *ntro.cin+base1"`,
// in any case): true with its map and the movies before it (`+` joined, `*` taken off); false
// when the cfg has no such alias or it starts no map.
bool new_game_level(const std::string& cfg, std::string& map, std::string& shows);

// `out` filled from `files` (`progs_path`: `-progs`, "" for the game's own); false with the
// reason when the multiplayer program does not load. The status lines say which program runs.
bool load_programs(const GameAdapter& adapter, const SearchPath& files,
                   const std::string& progs_path, GameContent& out, std::string& error,
                   const profiles::GameProfile& game = profiles::q1_game());

// The campaign's console commands while `content` has a campaign, taken away while it has none
// (NEW GAME is lit by `newgame`): `newgame` (a new single-player game from the start map), `save
// <name>` (the game into `saves/<name>.save`, or the classic text save `<name>.sav`) and `load
// <name>`. `newgame` sets `next_map` and `load` sets `load_ask`, which the window reads where it
// changes the map; `listen`, `files`, `content` and both strings must outlive the console.
void campaign_commands(ge::console::Console& con, Listen& listen, const SearchPath& files,
                       const GameContent& content, std::string& next_map, std::string& load_ask);

// `map` started on `listen` as the window changes a map: on the campaign's host when the folder has
// a campaign and the options ask for one (`deathmatch 0`), else as a match of `content`'s program
// (a new server when the last was a campaign's or another program's). A running campaign changes
// map, unless `newgame`, `coopgame` or `sv_coop 1` asked for a new game
// (`ListenOptions::new_game`): then a new campaign starts on the options, coop and companions with
// it. False with the reason.
bool start_map(Listen& listen, const GameContent& content, std::unique_ptr<LoadedMap> map,
               std::string& error);

// Each save's line in the menu's order (`s0`, `s1` ... or `save0` ...: files.hpp's `save_label`).
std::vector<std::string> save_slot_labels(const SearchPath& files,
                                          std::span<const std::string> names);

// Quake II's autosave (server/q2_save.hpp) kept as `saves/save0.save` when its rules made one
// as a level was entered; true when the rules asked for the load menu (single player's death).
bool keep_autosave(Listen& listen, const SearchPath& files);

// The save `name` that `load` asked for, playing on `listen` as a single-player game: ours by that
// name, else the classic text save (`<name>.sav` names it alone). False with the reason.
bool load_asked_save(Listen& listen, const GameContent& content, const SearchPath& files,
                     const std::string& name, std::string& error);
// The same for the save `ask` names, `ask` cleared: the console's words, "loaded <name>" or
// "load: " and why not.
std::string load_save_asked(Listen& listen, const GameContent& content, const SearchPath& files,
                            std::string& ask);

// The console's `path`: where the game's files come from, the first searched at the top
// (plugins/classic/content/files.hpp's search_path_text over `files`, read when typed).
void add_path_command(ge::console::Console& con, const SearchPath& files);

}  // namespace shooters_edge::client
