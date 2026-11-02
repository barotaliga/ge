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
//   game's maps carry them (plugins/brush/pictures.hpp), and a PNG or TGA in the game's files
//   stands in for one by its name, alpha and all: `textures/<map>/<name>` first, then
//   `textures/<name>`, `.png` before `.tga`, a `*` in the name written `#`. A later game's
//   pictures live in other files of its own and its adapter finds them by the texture names.
// - **list_demos**, **find_demo** and **open_demo**: the recordings in the game's files, a demo
//   found by a name (with the game's demo extensions tried in order when it has none) or a path,
//   and its bytes opened for playback (client/demo.hpp, the formats told by their first bytes).
//
// - **open_model**: a model the server names ("progs/armor.mdl") read into a shared FORM the
//   window draws (plugins/morph's `Model`, a sprite, a brush item), with a picture with alpha for
//   each skin the files hold one for (the first game's `progs/player.mdl_0.png`, or `.tga`).
// - **open_sound**: a sound the server or a demo names read to samples (`sound/*.wav`).
// - **pictures**: the HUD's and the menus' pictures (client/classic_pictures.hpp), each from a PNG
//   or TGA of its name where the files hold one (`gfx/sbar.png`; alpha kept, its size a whole
//   multiple), else classic-enhance's upscaled copy when asked, else the pack's lump.
// - **find_config**: a config file `exec` names, from the game's files after the player's folders
//   (`read_configs_from`); the startup files run before the game's files open.
// - **open_locations**: a map's place names into plugins/shooter/locs.hpp's `Locs` (the first
//   game's `locs/<map>.loc`); with none, a game names places off the map's items.
//
// Not yet here: the held weapons' models from the game's files. These are load-time calls, never
// a frame's: plain function pointers, no state (docs/shooters-edge.md § The game adapter). The
// console's `mods`, `maps` and `demos` print their answers (`add_adapter_commands`).
//
// ge:summary A classic game's adapter: mods, programs by form and side, maps, textures, demos
// ge:keywords game-adapter adapter classic-game list_mods find_program list_maps map_info
// ge:keywords map_textures list_demos find_demo open_demo mod-folder program-form program-side
// ge:keywords per-game sequel add_adapter_commands console-mods console-maps console-demos
// ge:keywords replacement-texture png tga alpha open_model open_sound model-form skin-stand-in
// ge:keywords replacement-skin pictures hud-pictures menu-pictures replacement-picture
// ge:keywords find_config config-file cfg exec open_locations loc-file place-names
// ge:keywords read_configs_from
// ge:skill shooter
// ge:test tests/shooters_edge_game_adapter_test.cpp
#pragma once

#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "assets/image.hpp"
#include "audio/pcm.hpp"
#include "brush/map.hpp"
#include "brush/pictures.hpp"
#include "console/console.hpp"
#include "morph/model.hpp"
#include "progs/file.hpp"
#include "shooter/locs.hpp"
#include "shooters_edge/client/classic_pictures.hpp"
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

// The shared form a model file is read into.
enum class ModelForm : uint8_t { none, morph, sprite, brush };

// A model the game's files hold, read.
struct OpenedModel {
  ModelForm form = ModelForm::none;
  ge::morph::Model morph;    // form morph
  ge::morph::Sprite sprite;  // form sprite
  ge::brush::Map brush;      // form brush
  // A full-colour picture with alpha standing in for each of `morph`'s skins (width 0: none).
  std::vector<ge::assets::Image> skins;
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
  // The textures of the map `map` (its name, "dm6"): their pixels (views into `map_file` or
  // `store`, which must outlive them), and a full-colour picture with alpha where the game's files
  // hold one in a texture's place.
  std::vector<ge::brush::MapPicture> (*map_textures)(const GameAdapter& self,
                                                     const SearchPath& files, std::string_view map,
                                                     std::span<const uint8_t> map_file,
                                                     std::vector<uint8_t>& store) = nullptr;
  // The model `name` read into its form. False with `error` "" when the files do not hold it,
  // else with the reason.
  bool (*open_model)(const GameAdapter& self, const SearchPath& files, const std::string& name,
                     OpenedModel& out, std::string& error) = nullptr;
  // The sound `name` (as the server or a demo names it) read; false as open_model is.
  bool (*open_sound)(const GameAdapter& self, const SearchPath& files, const std::string& name,
                     ge::audio::Pcm& out, std::string& error) = nullptr;
  // The HUD's and the menus' pictures the files hold, each from a picture file of its name when
  // there is one, else classic-enhance's upscaled copy when `enhance` (the packs' cache under
  // `cache_root`, empty: its own folder), else the game's own.
  ClassicPictures (*pictures)(const GameAdapter& self, const SearchPath& files, bool enhance,
                              const std::filesystem::path& cache_root) = nullptr;
  // The config file `name` ("mymod.cfg") the files hold: its text; false when they hold none.
  bool (*find_config)(const GameAdapter& self, const SearchPath& files, std::string_view name,
                      std::string& text) = nullptr;
  // The place names of the map `map` ("dm6") the files hold, added to `out` (`out.from`: the file
  // they were read from); false when the files hold none, or none that reads as a point.
  bool (*open_locations)(const GameAdapter& self, const SearchPath& files, std::string_view map,
                         ge::shooter::Locs& out) = nullptr;
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

// The console's `exec` finding a config file in the game's files too, through `adapter`'s
// find_config, once its search directories hold none of the name (`adapter` and `files` must
// outlive the console's reads).
void read_configs_from(ge::console::Console& con, const GameAdapter& adapter,
                       const SearchPath& files);

}  // namespace shooters_edge::client
