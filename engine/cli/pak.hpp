// `ge pak`: content archives (a pack, `.pak`, or a zip, `.pk3`) read, written and checked without
// a game, through the engine's own readers (engine/vfs/pak.hpp, engine/vfs/zip.hpp):
//
//   ge pak ls <archive> [--sha256]               every file: its path, bytes and CRC-32 (and, for
//                                                a zip, how it is stored); --sha256 adds each
//                                                file's SHA-256
//   ge pak ls <gamedir> [--mods=a,b] [--hidden]  the directory mounted as the game mounts it
//                                                (engine/vfs/search.hpp, mods over it): every
//                                                file, the source that wins it and the ones it
//                                                hides; --hidden only the overridden ones
//   ge pak extract <archive> [<file>...] --out=<dir>   every file, or the ones named, written
//                                                under <dir>; a name that would leave <dir> (an
//                                                absolute path, a `..`) is refused and named
//   ge pak build <dir> --out=<archive>           every file under <dir>, by path, packed; the
//                                                kind is the extension (.pak or .pk3)
//   ge pak convert <in> <out>                    one kind rewritten as the other (or the same),
//                                                every file and its order kept
//   ge pak sum <archive> [<file>...] [--out=<sums>] [--check=<sums>]   each file's SHA-256 in
//                                                `sha256sum`'s format; --check holds the archive
//                                                to a sums file, a changed or missing file an
//                                                error finding
//
// A zip is written stored (no compression): the engine has no deflate writer, and every zip
// reader takes stored entries. A pack's paths are at most 55 bytes; a longer one is refused by
// name. Names are matched as each archive's own system did: a pack's exactly, a zip's with ASCII
// case folded and '\' as '/'.
//
// ge:summary ge pak: list, extract, build, convert and checksum pak and pk3 content archives
// ge:keywords cli ge pak pk3 pack zip archive list extract build convert unpack repack
// ge:keywords checksum sha256 crc32 verify sums run_pak gamedir search-path winner hides
// ge:skill agent-interface vfs
// ge:test tests/cli_pak_test.cpp
#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "cli/commands.hpp"
#include "cli/outcome.hpp"

namespace ge::cli {

// One archive's files, unpacked, in its directory's order.
struct PakFiles {
  bool zip = false;
  std::vector<std::pair<std::string, std::vector<uint8_t>>> files;
};

// A `sha256sum` line read back: "<64 hex>  <path>" (or " *<path>"); false for any other line.
bool parse_sum_line(std::string_view line, std::string* hex, std::string* path);
// Whether an archive's name may be written under a directory: relative, no `..`, no drive.
bool safe_entry_path(std::string_view name);

// `ge pak ls` over a game directory (pak_game.cpp).
Outcome pak_ls_game(const Args& a);
// Rows as an aligned text table, the first row the header.
std::string pak_table(const std::vector<std::vector<std::string>>& rows);

// `ge pak <ls|extract|build|convert|sum>` (`sub`), run.
Outcome run_pak(const Args& a, const std::string& sub);

}  // namespace ge::cli
