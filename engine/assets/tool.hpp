// THE `assets` EXECUTABLE: the built assets' half that has to run inside the game's own code,
// because it reads the game's data sets (assets/data.hpp). It links every plugin and game and
// every `assets/<name>/data.cpp`, as the `labs` executable links every lab. `ge asset` drives it;
// the `assets_check` test runs its check over the tree.
//
//   assets list [--root=D] [--json]      every asset under D: its manifest, its model, its state
//   assets data <name>                   the data set's JSON, as the builder is fed it
//   assets check [--root=D] [<name>...]  every model (or these) against its stamp and budget, and
//                                        loaded: `ok <name> ...` or `FAIL <name>: <problem>` lines
//   assets stamp <name> --blender=<v> [--root=D]   write <name>.stamp for the sources as they are
//
// Exit 0 clean, 1 when a check fails, 2 on a usage error. `--root` defaults to the current
// directory.
//
// ge:summary The assets executable: list the built assets, print a data set, check and stamp
// ge:keywords assets tool executable list data check stamp run_assets assets-check
// ge:skill assets
// ge:test tests/assets_manifest_test.cpp
#pragma once

namespace ge::assets {

int run_assets(int argc, const char* const* argv);

}  // namespace ge::assets
