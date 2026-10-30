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
// ge:keywords loose-files palette maps
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

}  // namespace shooters_edge::client
