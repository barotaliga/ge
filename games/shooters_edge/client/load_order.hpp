// THE ORDER EACH CLASSIC GAME LOADS IN: how its own client ranks the files of an install and runs
// its configs at the start, one table a game, so the window loads each game as that game loads.
//
// - **Files** (`layout`, read by client/files.hpp's `open_game_dir`): in every game a folder's
//   packs named `pak` and a digit come first by number (`pak0`, `pak1` ... `pak10`, a `.pkz` or
//   `.pk3` too), then every other pack and zip (`.pak`, `.pkz`, `.pk3`) by name; one whose name
//   starts with `_` is turned off and never mounted. The first game's folders mount their loose
//   files over those (a loose file wins), with the bundled client's folder between the base folder
//   and the game folder. The sequel's mount their loose files first and their archives over them (a
//   pack in a folder wins over a loose file there), with the rerelease's archive at the install's
//   root (`Q2Game.kpf` in its client's spelling, `root_archive`) under the base folder.
// - **Startup** (`start`, run by client/install_configs.hpp's `start_configs`): each step in
//   order. `main` is the install's own main config (`main_where`, `main_files`), or the window's
//   own saved config when the install has none; `own` runs the window's config after the
//   install's. The sequel's `default.cfg` comes from its packs when no copy is on disk
//   (`defaults_in_packs`): the one its pak0 holds, binds and all.
//
// ge:summary Each classic game's load order: its files' ranks and its startup configs, in order
// ge:keywords load-order startup-order search-order game-layout archives-over-loose pkz
// ge:keywords pak-names underscore-off pack-order
// ge:keywords load_order_of start-step defaults-in-packs root-archive q2game-kpf
// ge:skill shooter console vfs
// ge:test tests/shooters_edge_install_configs_test.cpp
#pragma once

#include <array>
#include <cstdint>
#include <string_view>
#include <vector>

#include "classicenhance/profile.hpp"
#include "vfs/search.hpp"

namespace shooters_edge::client {

// One step of a game's startup, in the order the game runs them.
enum class StartStep : uint8_t {
  defaults,              // default.cfg
  autoexec_before_main,  // autoexec.cfg, only when the install has a main config
  main,                  // the install's main config, else the window's own saved config
  autoexec,              // autoexec.cfg, the first found
  folder_autoexecs,      // the base folder's autoexec.cfg, then the mod folder's
  postexec,              // postexec.cfg
  own,                   // the window's own saved config, when the install had a main config
  command_sets,          // the command line's +set pairs
  command_rest,          // the command line's other + commands
  postinit,              // postinit.cfg
};

// Where an install keeps its main config.
enum class MainWhere : uint8_t {
  client_configs,  // `<the bundled client's folder>/configs/<file>`
  game_folders,    // the mod folder, then the base folder
};

struct LoadOrder {
  ge::vfs::GameLayout layout;
  bool client_folder = false;      // the bundled client's folder between base and game
  std::string_view root_archive;   // an archive at the install's root under the base folder
  bool defaults_in_packs = false;  // default.cfg from the packs when no copy is on disk
  MainWhere main_where = MainWhere::client_configs;
  std::array<std::string_view, 2> main_files{};  // the main config's names, the first tried first
  std::vector<StartStep> start;
};

// The load order of the game whose content profile is `content` (the first game's for any other).
const LoadOrder& load_order_of(const ge::classicenhance::Profile& content);

}  // namespace shooters_edge::client
