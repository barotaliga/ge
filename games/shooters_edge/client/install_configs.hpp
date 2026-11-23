// THE CONFIGS OF A CLASSIC INSTALL, RUN IN THE INSTALL'S ORDER: a player's install (a classic game
// as its bundle lays it out, with its client's folder beside the game folders) carries the configs
// that client runs at its start; the window runs the same files in the same order, so the
// player's binds, names and settings come with them, and then its own saved settings.
//
// - **Where `exec` looks** (`config_dirs`): the window's own settings folder (what it saves),
//   then the game folder played (the multiplayer folder or a mod), the bundled client's folder,
//   and the base folder, each on disk; the game's archives after them (client/game_adapter.hpp's
//   `read_configs_from`). So `exec configs/config.cfg` in the game folder's `autoexec.cfg` finds
//   the client folder's `configs/config.cfg`, as the bundled client finds it.
// - **The order** (`start_configs`, each game's steps in client/load_order.hpp): `default.cfg` (a
//   copy on disk; the one inside the base folder's first pack is the game's own and is not run);
//   then the install's main config and the files its client runs after it (below); then the
//   series' one config and the window's own saved `config.cfg` (client/settings.hpp), so a change
//   made in this game sticks; then the command line's `+set` pairs, then its other `+` commands.
//   - Quake: `<client folder>/configs/config.cfg`, with `autoexec.cfg` run just before
//     it and again after it, as the bundled client does;
//   - Quake II: `config.cfg` (`q2config.cfg` on Windows, each tried after the other), the mod
//     folder's before the base folder's; after it the base folder's `autoexec.cfg`, then the
//     mod's, then `postexec.cfg`; and `postinit.cfg` last, after the command line.
// - **Settings this game does not have** (the bundled client's own HUD, its renderer's), met in
//   the install's configs, are passed over quietly and counted: one line says how many.
//
// Without a main config, the order is the engine's (console/cfg.hpp's `start`: default, the
// window's own config.cfg, autoexec, the command line) over the same folders. `-config <file>` (a
// session's own config) keeps the engine's order.
//
// ge:summary A classic install's configs run in its own order: default, main config, autoexec
// ge:keywords install-configs config-order startup-order autoexec default-cfg config-cfg
// ge:keywords client-folder configs-folder start_configs config_dirs unknown-settings q2config
// ge:keywords postexec postinit
// ge:keywords plus-set command-line nquake
// ge:skill shooter console
// ge:test tests/shooters_edge_install_configs_test.cpp
#pragma once

#include <filesystem>
#include <functional>
#include <string>
#include <vector>

#include "classicenhance/profile.hpp"
#include "console/console.hpp"

namespace shooters_edge::client {

// The folders `exec` reads on disk for the install at `basedir` playing `game` (above).
std::vector<std::filesystem::path> config_dirs(const std::string& basedir, const std::string& game,
                                               const ge::classicenhance::Profile& content);

// What `start_configs` ran, for the status line and the tests.
struct ConfigsRun {
  std::vector<std::string> files;  // each file run, in order, as found
  int passed_over = 0;             // lines naming a setting or command this game does not have
};

// The series' one config (client/settings.hpp), run just before the game's own saved file, by
// `outside` (the loaded game's layer away while it runs: client/cvar_layers.hpp's
// `without_game_cvars`; empty: run as it is).
struct SeriesConfig {
  std::filesystem::path file;
  std::function<void(const std::function<void()>&)> outside;
};

// The game's own saved settings (`<game folder>/settings/config.cfg`) run, as another game is
// loaded; false when there are none. Where the loaded game has `fs_autoexec` (Quake II) and it is
// on, each game folder's autoexec.cfg (the base's, then the mod's) and postexec.cfg run after.
bool run_game_settings(ge::console::Console& con, const std::string& basedir,
                       const std::string& game, const ge::classicenhance::Profile& content);

// The startup over `con` (its search path set to `config_dirs` first by the caller), with the
// command line's words `plus` (from the first `+` word on).
ConfigsRun start_configs(ge::console::Console& con, const std::string& basedir,
                         const std::string& game, const ge::classicenhance::Profile& content,
                         const std::vector<std::string>& plus, const SeriesConfig& series = {});

// The recordings a mod folder's own start file (`quake.rc`, in the mod's packs or loose) plays at
// the title (its `startdemos` words, the last such line's), so a mission pack's title loop plays
// the pack's own demos; "" when `game` is the base folder or the file names none.
std::string mod_startdemos(const std::string& basedir, const std::string& game,
                           const ge::classicenhance::Profile& content);

}  // namespace shooters_edge::client
