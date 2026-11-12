// THE CALLS EVERY CLASSIC GAME'S ADAPTER SHARES (game_adapter.hpp): what is the same for any
// classic game because it reads the game's rows (profiles/game_profile.hpp and the content row)
// or a family of files the engine's readers know whatever game holds them, so a game's adapter is
// its own table of these and the few calls that are truly its own (its pictures, its menu, its
// demos, its held weapons). A file is read by what it is, never by which game it came from: a
// map of the 38 family copied into the first game's folder draws with its wall textures, a model
// is read by its kind, so the games' files mix where a remix asks (docs/goals/M18u.md).
//
// - **list_mods**, **find_program**, **list_maps**, **map_info**, **modes**: by the game's rows.
// - **map_textures**: a map carrying its pictures (29, BSP2) gives them; any other family's come
//   from their wall texture files (`textures/<name>.wal`, brush/wal.hpp); then a PNG or TGA of a
//   texture's name stands in (`textures/<map>/<name>`, `textures/<name>`).
// - **open_model** by the file's kind (`.mdl`, `.spr`, `.bsp`, `.iqm` or a skeletal model's first
//   bytes), **open_sound** under the content row's sound folder, **find_config**,
//   **open_locations** (`locs/<map>.loc`), **list_demos** and **find_demo** by the row's demo
//   extensions.
// - **stand_in**: the first PNG or TGA of a list of stems the files hold, read.
//
// ge:summary The calls every classic game's adapter shares: mods, programs, maps, textures, models
// ge:keywords adapter-parts shared-adapter game-adapter list_mods find_program list_maps map_info
// ge:keywords map_textures wal wall-texture open_model open_sound find_config open_locations
// ge:keywords list_demos find_demo stand_in replacement-texture remix mixed-files modes
// ge:skill shooter
// ge:test tests/shooters_edge_game_adapter_test.cpp
#pragma once

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "shooters_edge/client/game_adapter.hpp"

namespace shooters_edge::client::parts {

std::string folded(std::string_view s);

std::vector<ModFolder> list_mods(const GameAdapter& self, const std::string& basedir);
bool find_program(const GameAdapter& self, const SearchPath& files, bool single, FoundProgram& out,
                  std::string& why);
std::vector<std::string> list_maps(const GameAdapter& self, const SearchPath& files);
bool map_info(const GameAdapter& self, const SearchPath& files, std::string_view name, MapInfo& out,
              std::string& error);
GameModes modes(const GameAdapter& self);
std::vector<ge::brush::MapPicture> map_textures(const GameAdapter& self, const SearchPath& files,
                                                std::string_view map,
                                                std::span<const uint8_t> map_file,
                                                std::vector<uint8_t>& store);
bool open_model(const GameAdapter& self, const SearchPath& files, const std::string& name,
                OpenedModel& out, std::string& error);
bool open_sound(const GameAdapter& self, const SearchPath& files, const std::string& name,
                ge::audio::Pcm& out, std::string& error);
bool find_config(const GameAdapter& self, const SearchPath& files, std::string_view name,
                 std::string& text);
bool open_locations(const GameAdapter& self, const SearchPath& files, std::string_view map,
                    ge::shooter::Locs& out);
std::vector<std::string> list_demos(const GameAdapter& self, const SearchPath& files);
bool find_demo(const GameAdapter& self, const SearchPath& files, const std::string& name,
               std::vector<uint8_t>& out, std::string& found, std::string& error);

// The first picture file of `stems` the files hold, each tried as .png then .tga, read; one that
// will not read is named (`own` says what is drawn instead) and the next is tried.
std::optional<ge::assets::Image> stand_in(const SearchPath& files,
                                          std::span<const std::string> stems,
                                          const std::string& own);

}  // namespace shooters_edge::client::parts
