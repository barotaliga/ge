// SHOOTER'S EDGE HOSTS A C++ MOD (docs/mods.md, the API in mod_api.hpp): a loaded mod's table
// (engine/mod/loader.hpp) put on the loop's program seam (plugins/shooter/program.hpp) behind the
// game's own program, native or bytecode. `mod_program` is a `Program` that runs, for each hook the
// loop calls, the game's own function first and then the mod's hook; a function number at or past
// `kModFuncBase` is the mod's own (`k = fn - kModFuncBase`); any other goes to the game's program.
//
// THE SERVICES (se_mod_services_v1) act on the server of the call running and check every argument:
// an entity out of range or free, a field of another kind, a mod function 0, a remove of the world
// or a player is REFUSED: the call does nothing (a getter answers 0), the refusal is counted and
// logged, and the match goes on. A sound's name is copied into the host's own table.
//
// DETERMINISM: everything a mod can change between frames is in `ModRun` (its state block, its
// stream, its counts), the only part of the host a snapshot copies (`ModRun` is a value); the run's
// digest is the server's hash folded with it (`mod_hash`). The stream is seeded by the match.
//
// A ModHost holds pointers to itself (the host table's `ctx`): create it in place and never move
// it.
//
// A WEBASSEMBLY MOD (the store form, engine/mod/wasm/mod.hpp): `create_wasm_mod_host` lays the
// game's services and the host table out in the module's memory (each service bound by its C++
// type, so the same functions serve both forms) and fills `wasm_table` with native hooks that call
// the module's, so the seam, the services and the digest are the native form's own. `ModRun::state`
// stays the block a snapshot copies: it is copied into the module's memory before every call and
// back after it, so a seek takes a wasm mod back too, and its digest is the native build's when
// its block has the same layout. A hook that traps (its fuel
// spent, a bad access) stops the mod for the match: the trap is refused and logged once, and its
// hooks do nothing more.
//
// ge:summary Shooter's Edge hosting a C++ mod: the seam after the game's program, the services
// ge:keywords mod mods modding mod-host mod_program create_mod_host check_hooks mod_match_start
// ge:keywords mod_hash mod-run state-block snapshot services refusal kmodfuncbase composite
// ge:skill mod shooter
// ge:test tests/shooters_edge_mod_test.cpp
#pragma once

#include <cstdint>
#include <deque>
#include <memory>
#include <string>
#include <vector>

#include "core/prng.hpp"
#include "mod/wasm/mod.hpp"
#include "shooter/server.hpp"
#include "shooters_edge/sim/mod_api.hpp"

namespace ge::mod {
struct LoadedMod;  // mod/loader.hpp
}

namespace shooters_edge::sim {

namespace sh = ge::shooter;

constexpr sh::Func kModFuncBase = 1 << 24;  // the mod's function k is kModFuncBase + k
constexpr uint32_t kModFuncMax = 1u << 20;  // k runs 1..kModFuncMax
constexpr sh::Func kModHookBase = 1 << 30;  // the seam's own hook numbers (mod_program)
constexpr int kModLogMax = 512;             // log lines kept; more are counted, dropped

// What a snapshot of the mod copies: everything the mod can change.
struct ModRun {
  std::vector<uint8_t> state;  // the mod's block, `state_bytes` long
  ge::core::Rng rng{1};        // the stream the mod draws from
  int64_t hook_calls = 0;      // hooks and functions of the mod run
  int64_t refusals = 0;        // service calls refused
};

struct ModHost {
  const ge_mod_v1* table = nullptr;
  const se_mod_hooks_v1* hooks = nullptr;
  ge_mod_host_v1 host{};
  se_mod_services_v1 services{};
  sh::Program base;          // the game's own program, run before each of the mod's hooks
  sh::Server* sv = nullptr;  // the server of the call running (null between calls)
  void* rules = nullptr;     // the rules whose own services the host offers (the sequel's), or null
  ModRun run;
  std::vector<std::string> log;  // the mod's lines and the refusals, in order
  int64_t log_dropped = 0;
  std::deque<std::string> sounds;  // the sound names a mod started, interned
  // A WebAssembly mod's instance, which must outlive the host (null for a native mod); the native
  // tables that call into it; whether a trap stopped it.
  ge::mod::wasm::WasmMod* wasm = nullptr;
  ge_mod_v1 wasm_table{};
  se_mod_hooks_v1 wasm_hooks{};
  bool wasm_stopped = false;
};

// The game's checks of a table beyond the engine's (engine/mod/loader.hpp): the hooks' size and
// api. Each error says what to do next.
std::vector<std::string> check_hooks(const ge_mod_v1* table);

// Prepares `h` (in place) for a match over `base`: the tables filled, the stream seeded, the state
// block zeroed and the mod's init run. The errors of `check_hooks`, if any (then nothing is set).
std::vector<std::string> create_mod_host(ModHost& h, const ge_mod_v1* table,
                                         const sh::Program& base, uint32_t seed);

// The same for a WebAssembly mod `m` (loaded by ge::mod::wasm::load_wasm_mod, not yet hosted): its
// game and api checked, the services and the host table laid out in its memory, then as above.
std::vector<std::string> create_wasm_mod_host(ModHost& h, ge::mod::wasm::WasmMod& m,
                                              const sh::Program& base, uint32_t seed);

// The same over rules' own services (below).
std::vector<std::string> create_wasm_mod_host_over(ModHost& h, ge::mod::wasm::WasmMod& m,
                                                   const se_mod_services_v1& services, void* rules,
                                                   uint32_t seed);

// A loaded package hosted in whichever form it was loaded (engine/mod/loader.hpp): a native one's
// table, or its module instantiated afresh into `instance` (which must outlive the host). Over
// `services` and `rules` when `services` is given, else over `base`.
std::vector<std::string> host_package(ModHost& h, const ge::mod::LoadedMod& package,
                                      std::unique_ptr<ge::mod::wasm::WasmMod>& instance,
                                      const se_mod_services_v1* services, void* rules,
                                      const sh::Program& base, uint32_t seed);

// The same for rules that keep their own entities off the loop's seam (the sequel's,
// server/sequel_mods.hpp): `services` (each taking the host's `ctx`, the ModHost) over `rules`.
std::vector<std::string> create_mod_host_over(ModHost& h, const ge_mod_v1* table,
                                              const se_mod_services_v1& services, void* rules,
                                              uint32_t seed);
// A service call refused (counted, logged, the match goes on); the mod's block (null: none).
void mod_refuse(ModHost& h, const char* service, const std::string& why);
void* mod_state(ModHost& h);

// The seam the server is created with.
sh::Program mod_program(ModHost& h);

// After the map's own entities are spawned: the mod's match_start.
void mod_match_start(ModHost& h, sh::Server& sv);

// The server's hash folded with the mod's run (its block, its stream, its counts).
uint64_t mod_hash(const ModRun& run, uint64_t server_hash);

}  // namespace shooters_edge::sim
