// THE GAME FOLDER THE PLAYER POINTS AT: the classic game's install, chosen with `-basedir`, with
// OPTIONS ▸ CLASSIC ENHANCED ▸ LOAD GAME (the desktop's folder chooser) or the console's `basedir`,
// and what the window reads out of it once it is open.
//
// - **Which folder** (`check_game_folder`): the install's own folder, the one holding the content
//   profile's base folder with its first numbered pack (`id1/pak0.pak`, or the content zip standing
//   in for it, names in any case). The base folder itself picked is taken as its parent. Any other
//   folder is refused with what it lacks: only a game the content profiles know is played.
// - **Which game folder over it** (`game_of`): the profile's multiplayer folder (`multiplayer_dir`:
// its player
//   skins and its multiplayer program) when the install has one, else the base folder, so the
//   install plays as the multiplayer client sees it.
// - **The folder kept** (`remembered_folder`, `remember_folder`): the last folder loaded, one line
//   in `<prefs>/game_folder.txt`, opened at the next start when `-basedir` names none and the
//   folder the game runs in holds no game (`choose_folder`).
// - **What it holds** (`load_programs`): the multiplayer program (`-progs`, else the game's own
//   program files, a mod's folder first, else the baseline mod), the single-player program the
//   campaign runs, whether the packs are the full game, and the first map (the profile's start
//   map when the packs hold it, else the game's fallback); and the campaign's commands it lights
//   (`campaign_commands`: `newgame`, `coopgame`, `save`, `load`).
//
// ge:summary The game folder a player loads: the install checked, its game folder, programs
// ge:keywords game-folder load-game basedir install check_game_folder game_of multiplayer-dir
// ge:keywords remembered_folder remember_folder prefs load_programs first-map campaign-program
// ge:keywords choose_folder campaign_commands newgame coopgame save load
// ge:skill shooter classicenhance
// ge:test tests/shooters_edge_game_folder_test.cpp
#pragma once

#include <string>
#include <vector>

#include "classicenhance/profile.hpp"
#include "console/console.hpp"
#include "progs/file.hpp"
#include "shooters_edge/client/args.hpp"
#include "shooters_edge/client/files.hpp"
#include "shooters_edge/client/game_adapter.hpp"
#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/profiles/game_profile.hpp"

namespace shooters_edge::client {

// The install at `folder` (or its base folder picked): true with the install's folder in
// `*basedir`; false with what is missing and the next step in `*error`.
bool check_game_folder(
    const std::string& folder, std::string* basedir, std::string* error,
    const ge::classicenhance::Profile& content = ge::classicenhance::classic_profile());

// The game folder played over the install at `basedir`: the multiplayer folder when it has one,
// else the base folder.
std::string game_of(const std::string& basedir, const ge::classicenhance::Profile& content =
                                                    ge::classicenhance::classic_profile());

// The install the window opens, into `args`: `-basedir`, else the folder loaded last (kept under
// `prefs`) when the folder the game runs in is no install; then the game folder over it, `-game`,
// else `game_of`.
void choose_folder(Args& args, const std::string& prefs);

// The folder loaded last, kept under `prefs` (the platform's per-user folder); "" when none was
// kept or `prefs` is empty.
std::string remembered_folder(const std::string& prefs);
// Keeps `basedir` for the next start; false (nothing kept) when `prefs` is empty or unwritable.
bool remember_folder(const std::string& prefs, const std::string& basedir);

// What the window plays out of an open game folder. `campaign` points into this struct (at
// `progs` or `pack_progs`) or is null, so it is filled in place and never moved.
struct GameContent {
  ge::progs::Progs progs;       // the multiplayer program
  ge::progs::Progs pack_progs;  // the packs' single-player program, when `progs` is not one
  const ge::progs::Progs* campaign = nullptr;  // what NEW GAME runs; null: no campaign
  bool registered = false;                     // the packs are the full game
  std::string start_map;                       // the profile's start map
  std::string first_map;  // the start map when the packs hold it, else the game's fallback
  // The game's item names as server lines (profiles/items.hpp's `item_name_lines`) when the
  // baseline mod plays the game's packs, so its pickups speak the game's words; empty when the
  // game's own program runs (it has its own) or no packs are open (the mod's own words).
  std::vector<std::string> item_names;
};

// `out` filled from `files` (`progs_path`: `-progs`, "" for the game's own); false with the
// reason when the multiplayer program does not load. The status lines say which program runs.
bool load_programs(const GameAdapter& adapter, const SearchPath& files,
                   const std::string& progs_path, GameContent& out, std::string& error,
                   const profiles::GameProfile& game = profiles::classic_game());

// The campaign's console commands while `content` has a campaign, taken away while it has none
// (NEW GAME is lit by `newgame`): `newgame` (a new single-player game from the start map), `save
// <name>` (the game into `saves/<name>.save`, or the classic text save `<name>.sav`) and `load
// <name>`. `newgame` sets `next_map` and `load` sets `load_ask`, which the window reads where it
// changes the map; `listen`, `files`, `content` and both strings must outlive the console.
void campaign_commands(ge::console::Console& con, Listen& listen, const SearchPath& files,
                       const GameContent& content, std::string& next_map, std::string& load_ask);

// `map` started on `listen` as the window changes a map: on the campaign's host when the folder has
// a campaign and the options ask for one (`deathmatch 0`), else as a match of `content`'s program
// (a new server when the last was a campaign's or another program's). False with the reason.
bool start_map(Listen& listen, const GameContent& content, std::unique_ptr<LoadedMap> map,
               std::string& error);

// The save `name` that `load` asked for, playing on `listen` as a single-player game: ours by that
// name, else the classic text save (`<name>.sav` names it alone). False with the reason.
bool load_asked_save(Listen& listen, const GameContent& content, const SearchPath& files,
                     const std::string& name, std::string& error);

}  // namespace shooters_edge::client
