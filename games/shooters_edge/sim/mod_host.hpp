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
// ge:summary Shooter's Edge hosting a C++ mod: the seam after the game's program, the services
// ge:keywords mod mods modding mod-host mod_program create_mod_host check_hooks mod_match_start
// ge:keywords mod_hash mod-run state-block snapshot services refusal kmodfuncbase composite
// ge:skill mod shooter
// ge:test tests/shooters_edge_mod_test.cpp
#pragma once

#include <cstdint>
#include <deque>
#include <string>
#include <vector>

#include "core/prng.hpp"
#include "shooter/server.hpp"
#include "shooters_edge/sim/mod_api.hpp"

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
  ModRun run;
  std::vector<std::string> log;  // the mod's lines and the refusals, in order
  int64_t log_dropped = 0;
  std::deque<std::string> sounds;  // the sound names a mod started, interned
};

// The game's checks of a table beyond the engine's (engine/mod/loader.hpp): the hooks' size and
// api. Each error says what to do next.
std::vector<std::string> check_hooks(const ge_mod_v1* table);

// Prepares `h` (in place) for a match over `base`: the tables filled, the stream seeded, the state
// block zeroed and the mod's init run. The errors of `check_hooks`, if any (then nothing is set).
std::vector<std::string> create_mod_host(ModHost& h, const ge_mod_v1* table,
                                         const sh::Program& base, uint32_t seed);

// The seam the server is created with.
sh::Program mod_program(ModHost& h);

// After the map's own entities are spawned: the mod's match_start.
void mod_match_start(ModHost& h, sh::Server& sv);

// The server's hash folded with the mod's run (its block, its stream, its counts).
uint64_t mod_hash(const ModRun& run, uint64_t server_hash);

}  // namespace shooters_edge::sim
