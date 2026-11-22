// THE SEARCH PATH'S CONSOLE WORDS, for any game that mounts its files on one (search.hpp):
// - `path`: every source mounted, the one that wins first, each with its count of files.
// - `dir <folder> [ext]`: the files in a folder (its subfolders' too), names after the folder,
//   only those ending `ext` when given (with or without its dot); then how many were listed.
// - `fdir <pattern>`: every file whose whole name fits a pattern of `*` (any run) and `?` (any one
//   character), letters in either case; then how many.
// - `whereis <file> [all]`: which source the game reads a file from; with `all`, every source that
//   holds it, the winner first.
// The lines read like a game's ("3 files listed.", "maps/dm2.bsp is in pak1.pak"): a player types
// them.
//
// ge:summary The search path's console words: path, dir, fdir, whereis
// ge:keywords path dir fdir whereis search-path list-files wildcard console vfs
// ge:keywords add_search_commands glob_match
// ge:skill vfs console
// ge:test tests/vfs_search_commands_test.cpp
#pragma once

#include <string_view>

#include "console/console.hpp"
#include "vfs/search.hpp"

namespace ge::vfs {

// Whether `name` fits `pattern` (`*` any run, `?` any one character, ASCII case ignored).
bool glob_match(std::string_view pattern, std::string_view name);

// The four words on `con`, over `sp` (which outlives the console).
void add_search_commands(console::Console& con, const SearchPath& sp);

}  // namespace ge::vfs
