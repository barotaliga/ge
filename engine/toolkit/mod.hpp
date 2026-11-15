// THE `mod` GROUP: a C++ mod package (docs/mods.md) checked, packed and signed, for a mod's author
// with either front door:
//
//   mod check <package>        the engine's loader on a package directory or manifest: the
//                              manifest, the ABI, the game and its API, the hash, the library, the
//                              entry table, the signature; each refusal a finding with its next
//                              step
//   mod pack <package>...      one package from several platforms' builds of one mod: each checked,
//                              their manifests merged, their libraries copied into --out
//   mod key                    a new signing key: the secret (kept out of every repository) and
//                              its public key
//   mod sign <package>         mod.sig beside the manifest, by the key in --key-env (GE_MOD_KEY)
//
// `ge` adds what needs a game repo (`ge mod new`, `ge mod build`, `ge mod check --run`:
// engine/cli/mod.hpp); a game's CLI plays a package with its own `mod run`.
//
// THE GAMES THAT OFFER MODS (`kModGames`): the game id, the version of its mod API, where its
// header lives in the engine tree, and the game's CLI that plays its mod scenario. One row a game.
//
// ge:summary The mod command group: a C++ mod package checked, packed, signed; the mod games
// ge:keywords toolkit mod mods check pack key sign package manifest loader signature
// ge:keywords trusted-keys key-env kmodgames find_mod_game mod_finding mod_rows modder
// ge:skill agent-interface mod
// ge:test tests/toolkit_mod_test.cpp
#pragma once

#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "toolkit/call.hpp"

namespace ge::toolkit {

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
// Every mod game's id, comma-separated (for a refusal's next step).
std::string mod_games_list();

// What the mod commands read, as either front door parsed it.
struct ModArgs {
  std::filesystem::path dir;                    // the package (check, sign)
  std::vector<std::filesystem::path> packages;  // pack
  std::string game;                             // check: --game
  std::string out;                              // pack: --out
  std::string key_env;                          // sign: --key-env (default GE_MOD_KEY)
  std::string trusted;                          // check: --trusted, a trusted-keys file
  bool trusted_only = false;                    // check: --trusted-only
  bool wasm = false;                            // check: --wasm, the wasm32 module only
};

// The bodies; `program` spells the next steps ("ge", "secli").
Outcome mod_check(const std::string& program, const ModArgs& a);
Outcome mod_pack(const std::string& program, const ModArgs& a);
Outcome mod_key(const std::string& program);
Outcome mod_sign(const std::string& program, const ModArgs& a);

// A loader message "<what>; next: <next>" as a finding at `where`.
Finding mod_finding(Severity s, const std::string& message, const std::string& where);

std::span<const Row> mod_rows();

}  // namespace ge::toolkit
