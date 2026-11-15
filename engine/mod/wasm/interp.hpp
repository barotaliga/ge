// THE ENGINE'S WEBASSEMBLY INTERPRETER (docs/mods.md § The store form, M52c): a checked module
// (module.hpp) instantiated over the host's functions and run, with no JIT, so it needs no
// executable memory and runs where native code may not load.
//
// AN INSTANCE is the module's state, all of it plain data: its linear memory (a byte vector,
// bounded by the module's declared maximum and the host's cap), its globals, its table of function
// references and the value stack. Copying an `Instance` copies the mod: that is how a snapshot of
// the match keeps it (docs/mods.md § Determinism).
//
// IMPORTS are the host's functions, matched by module and name and checked against the type the
// module declares. A host function reads its arguments as raw bits, may read or write the
// instance's memory, and returns false (with `Instance::trap_message` set) to stop the call.
// The host may also put its functions in the table (`add_host_function`), so the module reaches
// them through a function pointer, the way a mod calls the ABI's tables (mod/abi.hpp).
//
// DETERMINISM: every instruction's result is defined to the bit. Integer arithmetic wraps; float
// arithmetic is IEEE 754 in the engine's float mode (no FMA, no flush to zero); and the one
// freedom the format leaves, the bits of a NaN an operation makes, is taken away: every float
// operation that can make a NaN returns the canonical one (0x7fc00000, 0x7ff8000000000000).
// abs, neg, copysign, the reinterprets, loads and stores keep the bits they are given.
//
// THE LIMITS: each call runs on a fuel budget (instructions), so an endless loop stops with
// `Trap::fuel`, never a hung game; the value stack is bounded (`kStackSlots`) and calls nest at
// most `kMaxDepth` deep; memory grows only to the module's maximum and the host's cap. A trap
// leaves the instance as it was at the trap (the host decides whether to keep the mod).
//
// ge:summary The WebAssembly interpreter: instances, imports, calls, fuel, traps, NaNs made one
// ge:keywords wasm interpreter instance instantiate call trap fuel host-function import table
// ge:keywords linear-memory canonical-nan deterministic no-jit store-form call_indirect
// ge:skill mod
// ge:test tests/mod_wasm_test.cpp
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "mod/wasm/module.hpp"

namespace ge::mod::wasm {

struct Instance;

// A host function: `args` the call's arguments as bits (an i32 in the low 32), `results` where
// it writes its own. False stops the call with `in.trap_message` saying why.
using HostFn = bool (*)(Instance& in, void* user, const uint64_t* args, uint64_t* results);

struct HostImport {
  std::string module, name;
  FuncType type;  // what the host function takes and returns
  HostFn fn = nullptr;
  void* user = nullptr;
};

enum class Trap : uint8_t {
  none,
  unreachable,     // the unreachable instruction
  memory,          // a load or store past the memory's end
  divide_by_zero,  // an integer division or remainder by zero
  overflow,        // an integer division's overflow, or a float too big for its integer
  conversion,      // a NaN converted to an integer
  indirect,        // a call through the table to no function, or one of another type
  stack,           // the value stack or the call depth exhausted
  fuel,            // the call's instruction budget spent
  host,            // a host function stopped the call
};

const char* trap_name(Trap t);

constexpr uint32_t kNullFunc = 0xffffffffu;
constexpr size_t kStackSlots = 1u << 16;
constexpr int kMaxDepth = 256;

struct HostSlot {
  FuncType type;
  HostFn fn = nullptr;
  void* user = nullptr;
};

struct Instance {
  const Module* module = nullptr;
  std::vector<uint8_t> memory;
  uint32_t memory_max = 0;  // pages: the module's maximum, capped by the host
  std::vector<uint64_t> globals;
  std::vector<uint32_t> table;  // a function index (the host's past the module's), or kNullFunc
  std::vector<HostSlot> host;   // the imports, in order, then the functions the host added
  std::vector<uint64_t> stack;
  int depth = 0;
  uint64_t fuel = 0;  // left in the call that runs
  std::string trap_message;

  // The function index a host function in `host[k]` answers to.
  uint32_t host_index(size_t k) const;
};

// `m` instantiated over `imports` into `out`: the imports linked by module and name (each
// module import must be found, with the same type), the memory made (at most `memory_cap` pages),
// the globals, table, elements and data laid out, then the start function run on `fuel`. False
// with `error` naming what is missing or what trapped.
bool instantiate(const Module& m, const std::vector<HostImport>& imports, uint32_t memory_cap,
                 uint64_t fuel, Instance& out, std::string& error);

// A host function added to the instance's table (the table grows by one): the index a mod reads
// as a function pointer; kNullFunc when the table is at its maximum or there is none.
uint32_t add_host_function(Instance& in, const FuncType& type, HostFn fn, void* user);

// Function `func` called with `args` (as many as its type takes), its results written to
// `results`, on `fuel` instructions. Trap::none when it returned; else why it stopped, with
// `in.trap_message` naming the function and the instruction.
Trap call(Instance& in, uint32_t func, const uint64_t* args, uint64_t* results, uint64_t fuel);

// Reading and writing the instance's memory from the host: false when [at, at + n) is past it.
bool read_memory(const Instance& in, uint32_t at, void* out, uint32_t n);
bool write_memory(Instance& in, uint32_t at, const void* src, uint32_t n);
// A NUL-terminated string at `at`, at most `max` bytes; false when it runs past the memory.
bool read_string(const Instance& in, uint32_t at, uint32_t max, std::string& out);

}  // namespace ge::mod::wasm
