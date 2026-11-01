// A CLASSIC GAME'S ADAPTER: the one set of calls the shell asks a classic game's files through,
// each implemented once per game, so the window, the console, the menus and the labs never know
// how a game lays out its mods, names its programs, stores its maps' textures or records its
// demos. The first game's are `classic_adapter()` (client/classic_adapter.cpp); a later game is a
// second table of the same calls in a file of its own, and nothing of the first game's is in it.
// The data that is only names (folders, packs, file names, extensions) stays in the profile rows
// (profiles/game_profile.hpp, classicenhance/profile.hpp) the calls read; the readers a family of
// files shares stay below (engine/vfs: folders, packs and zips; plugins/brush: every map family).
//
// THE CALLS, and the first game's answer to each:
//
// - **list_mods**: the mod folders beside the base folder in an install: each folder over the
//   base that holds packs, zips or one of the game's program files (loose or in its packs), with
//   the programs it holds. A later game may also read a folder's own description file.
// - **find_program**: a mode's program through the search path: the game row's program files in
//   order, each of a FORM (QC bytecode, a native game library, a sandboxed bytecode, the engine's
//   C++ mod) and a SIDE (server, client, menus). Only QC runs here today: a file of a form with no
//   runner is named in `why` and passed over, as is a QC file of the other mode's layout.
// - **list_maps** and **map_info**: the maps the files hold (the start map first), and one map's
//   name, title (the world's message), family and texture count.
// - **map_textures**: a map's textures' pixels, one entry a texture in the map's order. The first
//   game's maps carry them (plugins/brush/pictures.hpp); a later game's live in other files of its
//   own and its adapter finds them by the map's texture names.
// - **list_demos**, **find_demo** and **open_demo**: the recordings in the game's files, a demo
//   found by a name (with the game's demo extensions tried in order when it has none) or a path,
//   and its bytes opened for playback (client/demo.hpp, the formats told by their first bytes).
//
// Not yet here (each moves behind a call of its own, in its own change): the models (the world's,
// the player's and its skins, the held weapons, the items) and the sounds; the HUD's and the
// menus' pictures; the config files and the location files.
//
// These are load-time calls, never a frame's: a table of plain function pointers, no state. The
// console's `mods`, `maps` and `demos` print their answers (`add_adapter_commands`).
//
// ge:summary A classic game's adapter: mods, programs by form and side, maps, textures, demos
// ge:keywords game-adapter adapter classic-game list_mods find_program list_maps map_info
// ge:keywords map_textures list_demos find_demo open_demo mod-folder program-form program-side
// ge:keywords per-game sequel add_adapter_commands console-mods console-maps console-demos
// ge:skill shooter
// ge:test tests/shooters_edge_game_adapter_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "brush/map.hpp"
#include "brush/pictures.hpp"
#include "console/console.hpp"
#include "progs/file.hpp"
#include "shooters_edge/client/demo.hpp"
#include "shooters_edge/client/files.hpp"
#include "shooters_edge/profiles/game_profile.hpp"

namespace shooters_edge::client {

// A mod folder in an install.
struct ModFolder {
  std::string folder;                 // its name under the base folder's parent
  uint32_t archives = 0;              // its packs and zips
  std::vector<std::string> programs;  // the game's program files it holds, as the row names them
};

// A program found for a mode.
struct FoundProgram {
  profiles::ProgramFile file;  // which of the row's files, its form and side
  ge::progs::Progs progs;      // loaded, for the QC form
};

// What a map is, read off its file.
struct MapInfo {
  std::string name;   // as the game names it ("dm6")
  std::string title;  // the world's message, "" when it has none
  ge::brush::Family family = ge::brush::Family::v29;
  uint32_t textures = 0;
};

struct GameAdapter {
  const profiles::GameProfile* game = nullptr;  // the rows the calls read
  // The mod folders beside the base folder under `basedir`, by name.
  std::vector<ModFolder> (*list_mods)(const GameAdapter& self,
                                      const std::string& basedir) = nullptr;
  // The first program of the mode (`single`: the single-player one) the files hold that this
  // build runs. False when none is: `why` then names each file passed over and why ("" when none
  // is there).
  bool (*find_program)(const GameAdapter& self, const SearchPath& files, bool single,
                       FoundProgram& out, std::string& why) = nullptr;
  // The maps the files hold, by name, the start map first.
  std::vector<std::string> (*list_maps)(const GameAdapter& self, const SearchPath& files) = nullptr;
  // The map `name` read; false with the reason.
  bool (*map_info)(const GameAdapter& self, const SearchPath& files, std::string_view name,
                   MapInfo& out, std::string& error) = nullptr;
  // The map's textures' pixels (views into `map_file` or `store`, which must outlive them).
  std::vector<ge::brush::MapPicture> (*map_textures)(const GameAdapter& self,
                                                     const SearchPath& files,
                                                     std::span<const uint8_t> map_file,
                                                     std::vector<uint8_t>& store) = nullptr;
  // The recordings the files hold, by path.
  std::vector<std::string> (*list_demos)(const GameAdapter& self,
                                         const SearchPath& files) = nullptr;
  // The demo `name` (a name the files hold, with or without its extension, or a file's path):
  // its bytes and where it was found; false with the reason and the next step.
  bool (*find_demo)(const GameAdapter& self, const SearchPath& files, const std::string& name,
                    std::vector<uint8_t>& out, std::string& found, std::string& error) = nullptr;
  // A recording's bytes opened for playback; false with where it broke.
  bool (*open_demo)(const GameAdapter& self, std::vector<uint8_t> bytes, Demo& out,
                    std::string& error) = nullptr;
};

// The first game's adapter.
const GameAdapter& classic_adapter();
// The adapter of a game row (the first game's for a row with none of its own).
const GameAdapter& adapter_of(const profiles::GameProfile& game);

// Whether this build has a runner for programs of `form`.
bool runs_form(profiles::ProgramForm form);

// The console's `mods`, `maps` and `demos`, each printing the adapter's answer (`basedir` the
// install's folder the mods are beside; `adapter` and `files` must outlive the console).
void add_adapter_commands(ge::console::Console& con, const GameAdapter& adapter,
                          const SearchPath& files, const std::string& basedir);

}  // namespace shooters_edge::client
