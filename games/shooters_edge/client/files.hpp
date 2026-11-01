// THE GAME'S FILES: where the client finds a map, a palette or a picture by its path in the game
// ("maps/dm6.bsp", "gfx/palette.lmp"). The engine's search path (engine/vfs/search.hpp) over the
// classic game's folders under the base folder (`-basedir <dir>`): the content profile's base
// folder first (`id1` for the first game, classicenhance/profile.hpp), then the game folder
// `-game` names over it when that is another (a mod), so a mod's file of the same path wins and
// everything else comes from the base. Each folder mounts its numbered packs in number order,
// then its zips (`*.pk3`) and unpacked zips (`*.pk3dir`), then its loose files, a later one
// winning; names are matched ignoring case. A loose file written into the game folder after it
// was opened (a demo just recorded) is found too: the game folder on disk is looked at first.
// Archives are read whole at mount and checked; a file is a view into its archive where it is
// stored unpacked.
//
// ge:summary The client's search path: the base folder and a mod folder over it, packs, pk3s
// ge:keywords search-path basedir game-folder mod gamedir pak pk3 pack archive find_file files
// ge:keywords open_game_dir loose-files palette maps map_names content_packs archive_paths
// ge:skill vfs shooter
// ge:test tests/shooters_edge_files_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "vfs/search.hpp"

namespace shooters_edge::client {

struct SearchPath {
  std::string folder;       // the game folder on disk, the mod's when one is named ("" none)
  ge::vfs::SearchPath vfs;  // every source mounted, the last winning
};

// The base folder `<basedir>/<the profile's base folder>` and, when `game` names another, the mod
// folder `<basedir>/<game>` over it. False with the reason and the next step when the game folder
// is missing or an archive does not read.
bool open_game_dir(const std::string& basedir, const std::string& game, SearchPath& out,
                   std::string& error);

// Adds an archive held in memory (a pack or a zip: a test's, a lab's or a stand-in), searched
// before the ones added earlier.
bool add_archive(SearchPath& path, const std::string& name, std::vector<uint8_t> bytes,
                 std::string& error);

// The file at `path` ('/' separated, as the game names it): a loose file's bytes copied into
// `store` and viewed, or a view into its pack. Empty when no place has it.
std::span<const uint8_t> find_file(const SearchPath& path, std::string_view name,
                                   std::vector<uint8_t>& store);

// The maps the search path holds (`maps/<name>.bsp`, loose or in an archive), by name, `start`
// first and the rest in order.
std::vector<std::string> map_names(const SearchPath& path);

// Every archive mounted (packs and zips), by its path, in mount order.
std::vector<std::string> archive_paths(const SearchPath& path);
// The numbered packs of the first folder mounted (the base game's: pak0, pak1, ...), in order:
// what classic-enhance keys its cache by (classicenhance/cached.hpp's `find_cache`).
std::vector<std::span<const uint8_t>> content_packs(const SearchPath& path);

// THE SAVES: ours `<game folder>/saves/<name>.save`; with `classic`, the classic text save
// `<game folder>/<name>.sav`, where the classic game keeps its own (the name letters, digits, `_`
// and `-` only). Written whole to a temporary file and renamed over the old one, so a save is
// never half written. False with the reason and the next step.
bool write_save(const SearchPath& path, const std::string& name, std::span<const uint8_t> bytes,
                std::string& error, bool classic = false);
bool read_save(const SearchPath& path, const std::string& name, std::vector<uint8_t>& out,
               std::string& error, bool classic = false);
// Whether `name` asks for the classic text save (it ends in `.sav`, then taken off).
bool classic_save_name(std::string& name);
// The line a menu shows for the save `name`: our save's title when `saves/<name>.save` reads,
// else the classic text save's comment (`<name>.sav`'s second line, `_` read as a space); ""
// when there is neither (an empty slot).
std::string save_label(const SearchPath& path, const std::string& name);

}  // namespace shooters_edge::client
