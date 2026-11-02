// `ge demo`: recordings packed and unpacked without the game (engine/vfs).
//
//   ge demo pack <file> [--level=19] [--out=<file>]   the recording packed as one zstd frame, the
//                                                     way a game packs what it records, written to
//                                                     `<file>z` (match.mvd to match.mvdz) and
//                                                     unpacked again to check it is the same bytes:
//                                                     the sizes, the ratio and the times
//   ge demo unpack <file> [--out=<file>]              the one recording inside a packed or archived
//                                                     file (vfs/unpack.hpp: zstd, gzip, xz, tar,
//                                                     zip, 7z, stacked) written plain
//
// THE NAMES: a packed recording is its name with a `z` after it (`.mvd` to `.mvdz`, `.dem` to
// `.demz`), so `unpack` takes the `z` off again; a file packed by its own tools loses that tool's
// extension (`.gz`, `.zst`, `.xz`, `.tgz` to `.tar`); an archive's recording keeps its own name
// (the file name alone: a path inside the archive never leaves the folder). Neither command writes
// over the file it reads.
//
// ge:summary ge demo: a recording packed to zstd as a game packs it, or unpacked from any archive
// ge:keywords cli ge demo pack unpack zstd mvdz recording archive gz tar zip 7z xz ratio
// ge:keywords run_demo packed_name unpacked_name
// ge:skill agent-interface
// ge:test tests/cli_demo_test.cpp
#pragma once

#include <string>
#include <string_view>

#include "cli/commands.hpp"
#include "cli/outcome.hpp"

namespace ge::cli {

// `match.mvd` to `match.mvdz`.
std::string packed_name(std::string_view file);
// The plain recording's name for `file`, whose recording is called `inner` ("" when the layers
// keep no name): `inner`'s file name, else `file` with its packing's extension or the `z` off.
std::string unpacked_name(std::string_view file, std::string_view inner);

// A parsed `ge demo <pack|unpack>` line (`sub`), run.
Outcome run_demo(const Args& a, std::string_view sub);

}  // namespace ge::cli
