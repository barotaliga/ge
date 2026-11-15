// LOADING A MOD (docs/mods.md § The loader): a package's manifest read and checked, its library
// for this platform found, hashed against the manifest, opened, and its entry table resolved and
// checked against the engine's ABI and the game that loads it. Every refusal names the version or
// the value that failed and what to do next; a game shows the text as it is.
//
// In order (the first failure stops the load, with every manifest error at once):
//   1. the manifest (`mod.manifest` in the directory given, or the file given) parses and checks,
//      and its signature (`mod.sig`, signature.hpp), when there is one, holds over it; a game that
//      loads only trusted packages refuses one that is unsigned or signed by a key it does not
//      list;
//   2. its `abi` major is this engine's and its minor no newer than this engine's;
//   3. its `game` is the loading game's and its `game_api` range holds the game's version;
//   4. it lists a file for the form loaded (below), the file is there, and its SHA-256 is the
//      listed one;
//   5. the library opens and exports `ge_mod_entry`, which returns a table (for the wasm32 module:
//      it reads, validates and instantiates, and its `ge_mod_entry` returns a table in its memory);
//   6. the table is long enough, speaks this ABI's major, and names the manifest's name and game,
//      a game_api inside the manifest's range, a state block within GE_MOD_STATE_MAX, and hooks.
// With no game (`GameOffer::game` empty, what `ge mod check` does), step 3's game half is skipped.
//
// THE FORM (`GameOffer::form`): a desktop build loads the native library for its platform and
// falls back to the package's `wasm32` module when there is none; a store build (iOS, Android:
// `default_form`) loads only the module, since those stores do not let an app load native code
// it downloaded. A module loaded leaves `table` null and its checked bytes in `wasm`: a game
// instantiates its own copy for each match (engine/mod/wasm/mod.hpp), so two hosts never share
// a module's memory. A store build that links a mod into the app ahead of time (linked.hpp)
// takes the module's row and, when the module's SHA-256 is the one linked, calls the linked entry
// instead: `table` is set, `linked` true and `wasm` empty, so the game hosts it as native code.
//
// ge:summary Loads a mod package: manifest, ABI, game, hash, library, entry table, each checked
// ge:keywords mod loader load_mod game-offer loaded-mod refuse abi-version game-api hash
// ge:keywords entry-table ge_mod_entry checks diagnostics
// ge:skill mod
// ge:test tests/mod_loader_test.cpp
#pragma once

#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include "mod/abi.hpp"
#include "mod/library.hpp"
#include "mod/manifest.hpp"
#include "mod/signature.hpp"

namespace ge::mod {

// What the loading game offers: its id and the version of its hook table.
// Which of a package's forms a game loads.
enum class Form : uint8_t {
  native_or_wasm,  // the native library for this platform, else the wasm32 module
  wasm_only,       // the wasm32 module only (a store build; `ge mod check --wasm`)
  native_only,     // the native library only
};

// The form this build loads by default: wasm_only where a store forbids native code it did not
// ship (iOS, Android), native_or_wasm elsewhere.
constexpr Form default_form() {
#if defined(__ANDROID__) || (defined(__APPLE__) && TARGET_OS_IPHONE)
  return Form::wasm_only;
#else
  return Form::native_or_wasm;
#endif
}

struct GameOffer {
  std::string_view game;  // "" checks a package without a game
  uint32_t api = 0;
  const std::vector<TrustedKey>* trusted = nullptr;  // the keys the game trusts (none: no list)
  bool trusted_only = false;                         // refuse a package no listed key signed
  Form form = default_form();
  bool linked = true;  // a module's linked build when the app has one (false: always interpret)
};

// A mod table's fields as read from a library or a module, and the hook table's own size.
struct TableFields {
  uint32_t abi = 0;
  std::string name, game;
  uint32_t game_api = 0, state_bytes = 0;
  bool has_hooks = false;
  uint32_t hooks_bytes = 0;
};

struct LoadedMod {
  std::filesystem::path dir;  // the package's directory
  Manifest manifest;
  SignatureCheck signature;  // who signed it (signature_words says it to a player)
  std::string platform;      // the file loaded was listed for this platform
  Library library;           // open while the table is in use
  const ge_mod_v1* table = nullptr;
  // The wasm32 module's bytes, checked (its hash, its validation, its table), when that form was
  // loaded (`platform` is wasm32 and `table` null).
  std::vector<uint8_t> wasm;
  bool is_wasm() const { return !wasm.empty(); }
  bool linked = false;  // the module's row, run by the app's linked build of it (linked.hpp)
  TableFields fields;   // the table as loaded, in either form
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

// The table's fields checked against the manifest (step 6 after its length and ABI): the errors,
// each with what to do next.
std::vector<std::string> check_fields(const TableFields& t, const Manifest& m);

}  // namespace ge::mod
