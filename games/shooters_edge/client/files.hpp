// THE GAME'S FILES: where the client finds a map, a palette or a picture by its path in the game
// ("maps/dm6.bsp", "gfx/palette.lmp"). A SEARCH PATH over one game folder under the base folder
// (`-basedir <dir>`, the game folder `id1` by default): the folder's loose files first, then its
// numbered pack archives newest first (`pak0.pak`, `pak1.pak`, ... read while they run on; the
// first missing number ends the list), so a later pack or a loose file overrides an earlier one.
// The archives are read through engine/vfs's pack reader (every directory checked before a file
// is handed out) and kept in memory; a file is a view into its archive.
//
// ge:summary The client's search path: a game folder's loose files and its numbered packs
// ge:keywords search-path basedir game-folder pak pack archive find_file open_game_dir files
// ge:keywords loose-files palette maps map_names
// ge:skill vfs shooter
// ge:test tests/shooters_edge_listen_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "vfs/pak.hpp"

namespace shooters_edge::client {

struct SearchPath {
  struct Archive {
    std::string path;  // on disk
    std::vector<uint8_t> bytes;
    ge::vfs::Pak dir;
  };
  std::string folder;             // the game folder on disk ("" none: archives added by hand)
  std::vector<Archive> archives;  // in the order read; searched newest first
};

// The game folder `<basedir>/<game>` and its packs. False with the reason and the next step when
// the folder is missing or a pack does not read.
bool open_game_dir(const std::string& basedir, const std::string& game, SearchPath& out,
                   std::string& error);

// Adds a pack held in memory (a test's or a lab's), searched before the ones added earlier.
bool add_archive(SearchPath& path, std::string name, std::vector<uint8_t> bytes,
                 std::string& error);

// The file at `path` ('/' separated, as the game names it): a loose file's bytes copied into
// `store` and viewed, or a view into its pack. Empty when no place has it.
std::span<const uint8_t> find_file(const SearchPath& path, std::string_view name,
                                   std::vector<uint8_t>& store);

// The maps the search path holds (`maps/<name>.bsp`, loose or in a pack), by name, `start` first
// and the rest in order.
std::vector<std::string> map_names(const SearchPath& path);

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
