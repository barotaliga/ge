// `ge mod`: A C++ MOD FROM NOTHING TO A PACKAGE (docs/mods.md, consumer skill `mod`):
//
//   ge mod new <dir> --game=<id>   a mod repo from templates/mod-<id>/: its CMakeLists.txt (which
//                                  includes the engine tree's cmake/Mods.cmake), a working mod in
//                                  src/mod.cpp, a .gitignore and a README
//   ge mod build [<dir>]           CMake configure and build (Release) into <dir>/build; the
//                                  package (the library and its mod.manifest) lands in
//                                  <dir>/build/mods/<name>/ and is checked as `ge mod check` does;
//                                  compiler and CMake errors come back as findings
//   ge mod check <package>         the engine's loader on a package directory or manifest: the
//                                  manifest, the ABI, the game and its API, the hash, the library,
//                                  the entry table; each refusal a finding with its next step
//   ge mod pack <package>...       one package from several platforms' builds of the same mod:
//                                  each checked, their manifests merged, their libraries copied
//                                  into --out (default dist/<name>-<version>/)
//   ge mod key                     a new signing key: the secret (kept out of every repository,
//                                  handed to sign by the environment) and its public key
//   ge mod sign <package>          mod.sig beside the manifest: the signature over it by the key in
//                                  --key-env (default GE_MOD_KEY); check then names the signer
//                                  against --trusted=<file> (mod/signature.hpp)
//   ge mod check <package> --run   then the package played in the game's mod scenario by the
//                                  game's own CLI (`<cli> mod run <package> --json`: Shooter's
//                                  Edge's `secli`), twice: the replay must hold; the answer adds
//                                  what the mod changed and what its hooks cost. The CLI is
//                                  --runner=<file or folder>, else the one built beside this ge,
//                                  else the one on the PATH
//
// THE GAMES THAT OFFER MODS (`kModGames`): the game id, the version of its mod API, and where its
// header lives in the engine tree, and the game's CLI that plays its mod scenario. One row a game;
// Shooter's Edge's is the first.
//
// ge:summary ge mod new, build, check and pack: a C++ mod scaffolded, built, checked and packed
// ge:keywords cli ge mod mods modding new build check pack package manifest template mod-games
// ge:keywords run_mod_new run_mod_build run_mod_check run_mod_pack kmodgames
// ge:keywords run_mod_key run_mod_sign signing signature trusted-keys key-env
// ge:keywords check-run mod-run runner replay hook-cost
// ge:skill agent-interface mod
// ge:test tests/cli_mod_test.cpp
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "cli/outcome.hpp"

namespace ge::cli {

struct ModGame {
  std::string_view id;       // the game id a manifest names
  uint32_t api;              // the version of its mod API (its header's SE_MOD_API, ...)
  std::string_view include;  // the include root its header is under, in the engine tree
  std::string_view header;   // the header a mod includes, from that root
  std::string_view cli;      // the game's CLI, whose `mod run <package>` plays its mod scenario
};
inline constexpr ModGame kModGames[] = {
    {"shooters_edge", 1, "games", "shooters_edge/sim/mod_api.hpp", "secli"},
};
const ModGame* find_mod_game(std::string_view id);

struct ModOptions {
  fs::path dir;        // the mod repo, or the package
  std::string game;    // --game
  std::string engine;  // --engine: the engine tree (default: the tree this ge was built from)
  std::string name;    // --name (new): default the directory's name
  std::vector<fs::path> packages;  // pack
  std::string out;                 // pack: --out
  std::string key_env;             // sign: --key-env (default GE_MOD_KEY)
  std::string trusted;             // check: --trusted, a trusted-keys file
  bool trusted_only = false;       // check: --trusted-only
  bool run = false;                // check: --run, the package played in the game's scenario
  std::string runner;              // check: --runner, the game's CLI (a file or its folder)
};

Outcome run_mod_new(const ModOptions& o);
Outcome run_mod_build(const ModOptions& o);
Outcome run_mod_check(const ModOptions& o);
Outcome run_mod_pack(const ModOptions& o);
Outcome run_mod_key();
Outcome run_mod_sign(const ModOptions& o);

// A loader message "<what>; next: <next>" as a finding at `where`.
Finding mod_finding(Severity s, const std::string& message, const std::string& where);

}  // namespace ge::cli
