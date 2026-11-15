// THE CALLS EVERY CLASSIC GAME'S ADAPTER SHARES (game_adapter.hpp): what is the same for any
// classic game because it reads the game's rows (profiles/game_profile.hpp and the content row)
// or a family of files the engine's readers know whatever game holds them, so a game's adapter is
// its own table of these and the few calls that are truly its own (its pictures, its menu, its
// demos, its held weapons). A file is read by what it is, never by which game it came from: a
// map of the 38 family copied into the first game's folder draws with its wall textures, a model
// is read by its kind, so the games' files mix where a remix asks (docs/goals/M18u.md).
//
// - **list_mods**, **find_program**, **list_maps**, **map_info**, **modes**: by the game's rows.
// - **map_textures**, **open_model**, **open_sound**, **find_config**, **open_locations**,
//   **stand_in**: the shooter plugin's open calls (plugins/classic/content/open.hpp), which any
//   shooter game shares, passed the game's content row where they need it.
// - **list_demos** and **find_demo** by the row's demo extensions.
//
// ge:summary The calls every classic game's adapter shares: mods, programs, maps, textures, models
// ge:keywords adapter-parts shared-adapter game-adapter list_mods find_program list_maps map_info
// ge:keywords map_textures wal wall-texture open_model open_sound find_config open_locations
// ge:keywords list_demos find_demo stand_in replacement-texture remix mixed-files modes
// ge:skill shooter
// ge:test tests/shooters_edge_game_adapter_test.cpp
#pragma once

#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "classic/content/open.hpp"
#include "shooters_edge/client/game_adapter.hpp"

namespace shooters_edge::client::parts {

using ge::classic::content::folded;
using ge::classic::content::stand_in;

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

}  // namespace shooters_edge::client::parts
