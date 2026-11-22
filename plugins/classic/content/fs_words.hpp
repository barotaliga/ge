// THE FILE WORDS a classic game's console answers over its search path (content/files.hpp), the
// same for every classic game that has them, client and server alike:
//
// - `dir <directory> [extensions]`: the files directly in a folder of the game, every archive and
//   folder merged, sorted ignoring case with each name once, then "<n> files listed"; the
//   extensions a suffix matched ignoring case, several split by `;`.
// - `fdir <filter>`: every file whose whole path matches the filter (`*` any run, `/` included,
//   `?` one character, `\` the next one as itself; ignoring case; several split by `;`), listed
//   the same way. Folders are walked 8 deep.
// - `whereis <path> [all]`: which archive or folder holds a file and its size, the first one (or
//   every one with a second word), then "<n> instances of <path>" or "<path> was not found". A
//   path of 64 characters or more is not looked for in archives (the archives' own limit).
// - `link` and `softlink` (`<name> <target>`: a link made or moved; none: the list), `unlink` and
//   `softunlink` (`<name>`, `-a` every link, `-h` the help): the read-only rewrites of
//   files.hpp's FileLinks.
// - `fs_restart`'s banner around the `path` listing (the reopening is the game's: it knows the
//   folders), the links kept.
//
// A game path is taken as the words take it (`normal_game_path`): `\` read as `/`, repeated,
// leading and trailing slashes dropped, `.` and `..` resolved, never above the game's root.
//
// ge:summary The classic file words: dir, fdir, whereis, links and fs_restart's banner
// ge:keywords dir fdir whereis link softlink unlink softunlink fs_restart file-words
// ge:keywords normal_game_path filter_match wildcard search-path console-words fs_command
// ge:skill classic vfs
// ge:test tests/classic_content_fs_words_test.cpp
#pragma once

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "classic/content/files.hpp"

namespace ge::classic::content {

// `path` as a game path (above); "" when nothing is left.
std::string normal_game_path(std::string_view path);

// The path the first of `links` whose name starts `path` (ignoring case) rewrites it to: that
// prefix replaced by its target. "" when none does.
std::string linked(const std::vector<std::pair<std::string, std::string>>& links,
                   std::string_view path);

// Whether `name` matches `filter` (`*`, `?` and `\` as above, ignoring case; one filter, no `;`).
bool filter_match(std::string_view filter, std::string_view name);

// The file words above but `fs_restart`: true when `w[0]` is one (`w` the line's words), its
// answer in `out` (lines ending in a newline). The links are changed through `path.links`.
bool fs_command(const SearchPath& path, const std::vector<std::string>& w, std::string& out);

// `fs_restart`'s answer once the game has opened `path` again: "----- FS_Restart -----", the
// `path` listing with its totals, then a line of dashes.
std::string fs_restart_text(const SearchPath& path);

// THE GAME FOLDER'S NAME CHECKED (Quake II's `game`): the base folder's own name (`base`, any
// case) becomes "" silently; a name with anything but ASCII letters, digits, `_` and `-` becomes ""
// with the line "'game' should contain characters [A-Za-z0-9_-] only." in `said`. True when the
// name was kept as given.
bool game_folder_check(std::string& game, std::string_view base, std::string& said);

}  // namespace ge::classic::content
