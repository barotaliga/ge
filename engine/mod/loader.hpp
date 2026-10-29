// LOADING A MOD (docs/mods.md § The loader): a package's manifest read and checked, its library
// for this platform found, hashed against the manifest, opened, and its entry table resolved and
// checked against the engine's ABI and the game that loads it. Every refusal names the version or
// the value that failed and what to do next; a game shows the text as it is.
//
// In order (the first failure stops the load, with every manifest error at once):
//   1. the manifest (`mod.manifest` in the directory given, or the file given) parses and checks;
//   2. its `abi` major is this engine's and its minor no newer than this engine's;
//   3. its `game` is the loading game's and its `game_api` range holds the game's version;
//   4. it lists a file for this platform, the file is there, and its SHA-256 is the listed one;
//   5. the library opens and exports `ge_mod_entry`, which returns a table;
//   6. the table is long enough, speaks this ABI's major, and names the manifest's name and game,
//      a game_api inside the manifest's range, a state block within GE_MOD_STATE_MAX, and hooks.
// With no game (`GameOffer::game` empty, what `ge mod check` does), step 3's game half is skipped.
//
// ge:summary Loads a mod package: manifest, ABI, game, hash, library, entry table, each checked
// ge:keywords mod loader load_mod game-offer loaded-mod refuse abi-version game-api hash
// ge:keywords entry-table ge_mod_entry checks diagnostics
// ge:skill mod
// ge:test tests/mod_loader_test.cpp
#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include "mod/abi.hpp"
#include "mod/library.hpp"
#include "mod/manifest.hpp"

namespace ge::mod {

// What the loading game offers: its id and the version of its hook table.
struct GameOffer {
  std::string_view game;  // "" checks a package without a game
  uint32_t api = 0;
};

struct LoadedMod {
  std::filesystem::path dir;  // the package's directory
  Manifest manifest;
  std::string platform;  // the file loaded was listed for this platform
  Library library;       // open while the table is in use
  const ge_mod_v1* table = nullptr;
};

struct LoadResult {
  std::vector<std::string> errors;  // the refusal (empty when loaded)
  std::vector<std::string> warnings;
  bool ok() const { return errors.empty(); }
};

// `path` is a package directory or its manifest file.
LoadResult load_mod(const std::filesystem::path& path, const GameOffer& offer, LoadedMod& out);

// Step 6 alone, on a table already in hand (the tests' and the loader's): the errors, if any.
std::vector<std::string> check_table(const ge_mod_v1* t, const Manifest& m);

}  // namespace ge::mod
