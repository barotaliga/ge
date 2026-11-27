// THE GAME PLAYED AND EACH CLASSIC GAME'S INSTALL, as settings the window, its CLI and a server
// read alike, and which edition an install is.
//
// - **The settings** (`bind_base_game`): `fs_basegame`, the game played (`""` Shooter's Edge's
//   own, else a content profile's id: `q1`, `q2`), and `fs_basedir_<id>` for each game the content
//   profiles know (classicenhance/profile.hpp), the folder its install is in (`""` none). In the
//   shell's layer, archived: the series config (client/settings.hpp) keeps them. Opening an
//   install sets both; setting them in the console opens the game they name; a dedicated server
//   takes them as its rules (client/remix_dedicated.hpp). A relative folder starts from the
//   program's own folder (the one holding the app on a Mac), never the folder it was started in,
//   so `fs_basedir_q1 quake` is the `quake` folder beside the game wherever it is started from.
// - **The player's own files** (`own_files`): saves, screenshots and recordings are kept under the
//   game folder played, in the data folder; `fs_artifacts_basedir 1` keeps them under the same game
//   folder in the game's install instead (`fs_basedir_<id>`, relative from the program's folder),
//   so a copy of the game beside the program carries them. Read when a game opens.
// - **Read before the game opens** (`read_base_game`): the series config's lines and the command
//   line's `+set` words, so the window and its CLI know which install to open before any config
//   runs.
// - **Which edition** (`edition_of`): the base game's places asked by their packs' directories
//   alone (vfs `game_has_file`, nothing mounted), the profile's full-game file found in them or
//   not (`full_game`: a picture only the full game's packs hold), so an install reads as the
//   shareware game or the registered one the moment it is picked.
//
// ge:summary The game played and each classic game's install as settings, and their editions
// ge:keywords installs select-game fs_basegame fs_basedir base-game bind_base_game read_base_game
// ge:keywords game-played edition shareware fs_artifacts_basedir own_files player-files
// ge:keywords registered full-game game_rows mission-pack packs_of pack-row
// ge:skill shooter classicenhance
// ge:test tests/shooters_edge_game_folder_test.cpp
#pragma once

#include <filesystem>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "classicenhance/profile.hpp"
#include "console/console.hpp"
#include "console/cvar.hpp"
#include "shooters_edge/menu/games_page.hpp"

namespace shooters_edge::client {

// The setting that names the game played.
inline constexpr std::string_view kBaseGame = "fs_basegame";
// The setting that keeps the player's own files (saves, screenshots, recordings) in each game's
// install, `fs_basedir_<id>`, instead of the data folder.
inline constexpr std::string_view kArtifactsBasedir = "fs_artifacts_basedir";
// The setting that names the install of the game `id` (a content profile's id): `fs_basedir_<id>`.
std::string basedir_setting(std::string_view id);

// The game played and each game's install, as their settings hold them.
struct BaseGame {
  std::string game;  // `fs_basegame`: "" Shooter's Edge's own, else a content profile's id
  // `fs_basedir_<id>` by profile id, every profile's present ("" no install).
  std::map<std::string, std::string, std::less<>> dirs;
  // The folder a relative `fs_basedir_<id>` starts from: the program's own (`placed_folder`: the
  // one holding the app on a Mac); "" the folder the game runs in.
  std::string from;
  // `fs_artifacts_basedir`: the player's own files in the install (`own_files`), not the data
  // folder.
  bool artifacts = false;
  BaseGame();
  // The install of game `id` ("" none, or an id no profile has), `~` and a relative path resolved
  // (`resolve_basedir`).
  std::string dir(std::string_view id) const;
  // The folder the player's own files of game `id` go in, `folder` the game folder played over
  // `basedir`: `<install>/<folder>` with `artifacts` and an install set, else `<basedir>/<folder>`
  // (the copy in the data folder, or Shooter's Edge's own folder there).
  std::string own_files(std::string_view id, const std::string& basedir,
                        const std::string& folder) const;
};

// A `fs_basedir_<id>` value as a folder: `~` the home folder, a relative path from `from` (the
// program's folder), an absolute one as it is; "" stays "".
std::string resolve_basedir(const std::string& value, const std::string& from);

// Binds `fs_basegame` and every `fs_basedir_<id>` to `*out` (which outlives `cvars`), archived,
// keeping the values `*out` holds; those values, for `set_base_game` once the configs have run (a
// config may name the folder a game was copied from, or none).
BaseGame bind_base_game(ge::console::Cvars& cvars, BaseGame* out);
// Sets the settings in `cvars` to `base`'s values: the game, and every install it names (an empty
// one leaves the setting as it is); what `ask_base_game` compares, for its `*seen`.
std::string set_base_game(ge::console::Cvars& cvars, const BaseGame& base);
// The settings as `config` (a config's text: `name "value"` lines, `set`/`seta` before them or
// not) and then the command line's `plus` words (`+set <name> <value>`) leave them.
BaseGame read_base_game(std::string_view config, const std::vector<std::string>& plus);
// The same with the configs read from `files` in turn (a missing one is none).
BaseGame read_base_game(const std::vector<std::filesystem::path>& files,
                        const std::vector<std::string>& plus);
// The window opened game `id`'s install at `basedir`: both settings set to it in `cvars` (bound to
// `base`), and `*seen` to what `base_game_asked` compares.
void keep_base_game(ge::console::Cvars& cvars, const BaseGame& base, std::string_view id,
                    const std::string& basedir, std::string* seen);
// What the settings ask the window to open now that they differ from `*seen` (the game and folder
// last opened or asked; `*seen` then becomes them): the install `fs_basedir_<game>` names, or ""
// with `*why` (in the player's words) when nothing can open: Shooter's Edge's own game (the next
// start opens it), or a game with no install set. "" with `*why` empty when nothing changed.
std::string base_game_asked(const BaseGame& base, std::string* seen, std::string* why);
// The window's frame: `*folder` set to what `base_game_asked` asks for, or its reason printed.
void ask_base_game(const BaseGame& base, std::string* seen, std::string* folder,
                   ge::console::Console& con);

enum class Edition : uint8_t { unknown, shareware, full };

// Which edition the install at `basedir` of `content` is: unknown when its base folder is missing.
Edition edition_of(const std::string& basedir, const ge::classicenhance::Profile& content);

// The edition in the game's words for the player ("Shareware", "Registered"; Quake II's "Demo",
// "Full game").
std::string_view edition_words(Edition e, const ge::classicenhance::Profile& content);

// SELECT GAME's rows: each classic game added on this computer (its `fs_basedir_<id>` set in
// `base`, the folder still holding it), in the profiles' order; the game `playing` (a profile id,
// "" none) is one of them, from `playing_install`; `remixed` the one classic remix plays it with
// ("" none). Each with its edition. A game never added is not listed: ADD GAME adds it. Under a
// game's row, a row for each of its mission packs that install holds (`<install>/<pack folder>/`
// with its first pack), the one playing when `playing_folder` (the game folder playing over the
// install) is the pack's.
std::vector<shooters_edge::menu::GameRow> game_rows(const BaseGame& base, std::string_view playing,
                                                    const std::string& playing_install,
                                                    std::string_view remixed = {},
                                                    std::string_view playing_folder = {});

// The game folder to play over the install at `basedir`: the mission pack `*pack` names when it is
// one of `content`'s, else the game's own (`game_of`); `*pack` is cleared either way.
std::string folder_over(const std::string& basedir, const ge::classicenhance::Profile& content,
                        std::string* pack);

// The mission packs of `content` the install at `basedir` holds, in the profile's order.
std::vector<const ge::classicenhance::MissionPack*> packs_of(
    const std::string& basedir, const ge::classicenhance::Profile& content);

}  // namespace shooters_edge::client
