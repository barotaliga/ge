// A MOD AS A WEBASSEMBLY MODULE (docs/mods.md § The store form): the portable form of a package,
// the same source as the native library compiled for wasm32, run by the engine's interpreter.
//
// THE ENTRY: the module exports `ge_mod_entry(host_abi) -> ge_mod_v1*` as the library does; the
// pointer is an address in the module's memory, where the table sits in wasm32's layout (every
// pointer four bytes: a string's address, or a function pointer's table slot). `load_wasm_mod`
// instantiates the module, calls the entry and reads the table, its strings and the game's hook
// table (its size and api, then one slot a hook) into a `WasmMod`.
//
// THE HOST'S SIDE IN THE MODULE'S MEMORY: the engine grows the module's memory to lay out what the
// mod reads through pointers: the host table (`ge_mod_host_v1` in wasm32's layout, its functions
// bound by bind.hpp to the engine's libm and the game's stream and log), the game's services
// table (laid by the game, `reserve` and `bind_native`), and the state block. A hook is then
// called with the state block's and the host table's addresses (`call_hook`), exactly as the
// native form passes their pointers.
//
// A SNAPSHOT of the mod is its instance's memory and globals (`snapshot` and `restore`): the state
// block, the mod's own statics and anything it laid out, all of it.
//
// ge:summary A mod's WebAssembly module: the entry, the tables in wasm32's layout, hooks called
// ge:keywords wasm wasm-mod load_wasm_mod ge_mod_entry host-table state-block reserve call_hook
// ge:keywords wasm32-layout snapshot restore store-form portable-mod
// ge:skill mod
// ge:test tests/mod_wasm_mod_test.cpp
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "mod/abi.hpp"
#include "mod/wasm/bind.hpp"
#include "mod/wasm/interp.hpp"
#include "mod/wasm/module.hpp"

namespace ge::mod::wasm {

// wasm32's layout of the ABI's tables (mod/abi.hpp): offsets in bytes.
inline constexpr uint32_t kModTableBytes = 36;   // ge_mod_v1
inline constexpr uint32_t kHostTableBytes = 84;  // ge_mod_host_v1
inline constexpr uint32_t kHostGameAt = 80;      // ge_mod_host_v1::game

// The fuel a hook runs on: far more than a hook needs, so only a runaway stops.
inline constexpr uint64_t kHookFuel = 50'000'000;
// The memory a mod may have, in pages (16 MiB).
inline constexpr uint32_t kModMemoryPages = 256;

struct WasmMod {
  Module module;
  Instance in;
  Bindings bindings;
  // The mod's table.
  uint32_t abi = 0;
  std::string name, version, game;
  uint32_t game_api = 0;
  uint32_t state_bytes = 0;
  uint32_t init = 0;  // a slot, or 0
  // The game's hook table: its size and api as the mod built it, then its slots.
  uint32_t hooks_size = 0, hooks_api = 0;
  std::vector<uint32_t> hooks;
  // Laid out by the engine in the module's memory.
  uint32_t host_at = 0, state_at = 0;
};

// The module in `bytes` instantiated, its entry called with `host_abi` and its tables read into
// `out`. False with `error` (what failed; next: what to do).
bool load_wasm_mod(const uint8_t* bytes, size_t size, uint32_t host_abi, WasmMod& out,
                   std::string& error);

// `bytes` of the module's memory set aside for the host (zeroed, 16-byte aligned, past everything
// the mod has); its address, or 0 when the memory cannot grow that far.
uint32_t reserve(WasmMod& m, uint32_t bytes);

// The host table laid out from the native one (its libm, stream and log bound into the module's
// table; `ctx` handed to them), with `game_at` the address of the game's services table; then the
// state block. False with `error`.
bool lay_host(WasmMod& m, const ge_mod_host_v1& native, uint32_t game_at, std::string& error);

// A new match: the state block zeroed, then the mod's init (if any) run.
Trap start_match(WasmMod& m);

// Hook `k` of the game's table (0 the first after size and api) called with the state block's and
// the host table's addresses, then `args`. Trap::none and nothing done when the mod left it null.
template <typename R, typename... A>
Trap call_hook(WasmMod& m, uint32_t k, R* result, A... args) {
  if (k >= m.hooks.size() || m.hooks[k] == 0) return Trap::none;
  return call_slot<R>(m.in, m.hooks[k], kHookFuel, result, m.state_at, m.host_at, args...);
}

// The mod's whole state (its memory and globals), and back.
struct WasmSnapshot {
  std::vector<uint8_t> memory;
  std::vector<uint64_t> globals;
};
void snapshot(const WasmMod& m, WasmSnapshot& out);
void restore(WasmMod& m, const WasmSnapshot& from);

}  // namespace ge::mod::wasm
