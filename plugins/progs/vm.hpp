// THE QC VIRTUAL MACHINE: runs a checked progs (file.hpp) over its own global and entity memory,
// deterministic to the bit. Every value is a 32-bit word (a vector three); floats are IEEE single
// precision and every operation is done in single precision, in the order the opcode states
// (opcodes.hpp, vm.cpp), with the build's flags (no contraction, no fast math).
//
// MEMORY. The globals are the file's block, copied at creation (`reset_globals` copies it again).
// The entities are one array of `max_entities` x `entityfields` words, all zero when cleared;
// entity 0 is the world. VALUES: an entity is its number times its size in BYTES (the world 0);
// a field is its offset in words; a pointer (made only by ADDRESS) is a byte offset from the start
// of the entity array; a function is an index into the functions (0 null); a string is 0 (empty),
// a positive offset into the file's table, -(k + 1) for the engine's string k (held until
// `clear_engine_strings`), -(kTempBase + i) for slot i of the ring of 8 temporaries the engine
// hands a hook (valid until 8 more are made: the ring's depth is part of the contract),
// -(kBuiltinTempBase + i) for slot i of the ring of 64 a builtin's text result lives in, or
// -(kZoneBase + i) for zone slot i (`zone_string`, kept until `unzone_string` or the map's end).
//
// CALLING (`call_function`, the engine's entry; CALL0..CALL8 inside): push the caller on the call
// stack (32 functions deep at most: "stack overflow"), save the callee's `locals` words from its
// first parameter's global on the local stack (2048 words in all: "locals stack overflow"), copy
// each parameter from its slot (global 4 + 3i) packed into the callee's parameter globals, and run
// from its first statement; RETURN or DONE copies three words from `a` to the return slot (1..3),
// restores the saved words and pops. A builtin (a negative first statement) runs from the game's
// TABLE (`set_builtin`) with the argument count of its CALL, writes its result to the return slot,
// and may call back into QC (a nested engine call on the same stacks).
//
// GUARDS: each engine call may run 99999 statements; the 100000th is the fault "runaway loop".
// Every entity value, field, pointer, function index and string value is checked where it is
// used, and a bad one is a fault, never a silent read. A FAULT stops the engine call:
// `call_function` returns false, the stacks are unwound to where the engine's call began, and
// `vm.fault` holds the message with the QC stack trace (function, file, statement, and line when
// the progs has a line table), innermost first; `fault_text` says it in the diagnostics' shape. The
// game decides what a fault ends; a faulted VM refuses calls until `clear_fault`.
//
// THE PROFILE counts every statement against the function running it, every entry into a function
// and every call of a builtin, until `clear_profile`. Nothing in a call allocates: every array is
// sized at `create_vm`.
//
// ge:summary The QC virtual machine: globals, entities, strings, calls, guards, faults, profile
// ge:keywords qc progs vm interpreter execute call_function builtin globals entities edicts
// ge:keywords field pointer string temp-string engine-string runaway stack-overflow fault
// ge:keywords stack-trace profile trace deterministic float32 entity-value set_builtin
// ge:keywords create_vm reset_globals clear_entities vm_error fault_text temp_string
// ge:keywords builtin_temp_string zone_string unzone_string strzone strunzone ring-of-64
// ge:skill progs
// ge:test tests/progs_vm_test.cpp tests/progs_ops_test.cpp
#pragma once

#include <bit>
#include <cstdint>
#include <string>
#include <vector>

#include "core/prng.hpp"
#include "progs/file.hpp"
#include "progs/layout.hpp"
#include "progs/text.hpp"

namespace ge::progs {

constexpr int kMaxCallDepth = 32;                     // functions active at once
constexpr int kLocalStackWords = 2048;                // locals saved across all active calls
constexpr int kRunawayLimit = 100000;                 // the statement of an engine call that faults
constexpr int kTempStrings = 8;                       // the ring of temporaries
constexpr int kTempStringBytes = 2048;                // each, NUL included
constexpr int32_t kTempBase = 0x100000;               // a temporary's value is -(kTempBase + slot)
constexpr int kMaxEntitiesDefault = 2048;             // the protocol's limit
constexpr int kBuiltinTempStrings = 64;               // the ring a builtin's text result lives in
constexpr int32_t kBuiltinTempBase = kTempBase + 16;  // a builtin temporary: -(base + slot)
constexpr int kZoneStrings = 1024;                    // strings a program keeps (strzone)
constexpr int32_t kZoneBase = 0x200000;               // a zone string: -(kZoneBase + slot)

struct Vm;
// A builtin: reads its arguments from the parameter slots (`vm.argc` of them), writes its result
// to the return slot, and calls `vm_error` to fault.
using Builtin = void (*)(Vm& vm);

struct BuiltinSlot {
  Builtin fn = nullptr;
  const char* name = "";
};

// Where STATE and the world guard look: the program's form's offsets (`create_vm` sets them from
// `Progs::layout`; the multiplayer form's for a standalone program).
struct EngineSlots {
  int self = kGSelf;  // global: the entity STATE writes
  int time = kGTime;  // global: the time STATE adds 0.1 s to
  int nextthink = kFNextthink, frame = kFFrame, think = kFThink;  // fields
};

struct VmLimits {
  int max_entities = kMaxEntitiesDefault;
  int engine_strings = 4096;          // how many engine strings at once
  int engine_string_bytes = 1 << 18;  // their text, NULs included
  int max_builtins = 512;             // the table's size
};

struct Frame {
  int32_t function;   // the function that was running (0: the engine)
  int32_t statement;  // its statement (the CALL), or -1 for the engine
};

// What stopped an engine call. `frames[0]` is the faulting statement; then each caller outwards.
struct Fault {
  bool set = false;
  char message[256] = {};
  int depth = 0;
  Frame frames[kMaxCallDepth + 1] = {};
};

struct FunctionProfile {
  int64_t statements = 0;  // statements run while it was the running function
  int64_t calls = 0;       // entries
};

// Hooks a tool installs to watch a run (`ge qc trace`); every pointer may be null. `enter` sees
// the parameters in place, `leave` the return slot written; `builtin` runs after the builtin.
struct TraceHooks {
  void* ctx = nullptr;
  void (*enter)(void* ctx, const Vm& vm, int32_t fn, int depth) = nullptr;
  void (*leave)(void* ctx, const Vm& vm, int32_t fn, int depth) = nullptr;
  void (*builtin)(void* ctx, const Vm& vm, int32_t number, int argc, int depth) = nullptr;
  void (*statement)(void* ctx, const Vm& vm, int32_t fn, int32_t statement) = nullptr;
};

struct Vm {
  const Progs* progs = nullptr;
  std::vector<uint32_t> globals;
  std::vector<uint32_t> entities;  // max_entities x entityfields words
  int32_t entityfields = 0;
  int32_t entity_bytes = 0;  // an entity value's step: entityfields x 4
  int32_t max_entities = 0;
  int32_t num_entities = 1;   // entities in use (the game sets it); values past it are refused
  bool world_locked = false;  // the map is running: ADDRESS of a world field faults
  EngineSlots slots;
  std::vector<BuiltinSlot> builtins;  // by number; [0] never called
  int argc = 0;                       // the running builtin's argument count
  void* user = nullptr;               // the game's, for its builtins
  const TraceHooks* trace = nullptr;  // when set, every hook runs (slower)

  // The stacks (sized at creation).
  int depth = 0;
  Frame stack[kMaxCallDepth];
  std::vector<uint32_t> local_stack;
  int local_used = 0;
  int32_t function = 0;   // the running function (0: none)
  int32_t statement = 0;  // the running statement, kept current across a builtin

  // Strings the engine made.
  std::vector<char> engine_text;
  std::vector<int32_t> engine_offsets;
  int engine_count = 0;
  int engine_used = 0;
  char temps[kTempStrings][kTempStringBytes] = {};
  int temp_next = 0;
  std::vector<char> builtin_temps;  // kBuiltinTempStrings x kTempStringBytes
  int builtin_temp_next = 0;
  std::vector<std::string> zone;  // kZoneStrings slots; a slot keeps its capacity when freed
  std::vector<uint8_t> zone_used;

  // What the standard builtins keep (stdlib.hpp): the seeded generator random draws from (the
  // game's, set before a call; null faults a draw) and the program's own word list (tokenize).
  ge::core::Rng* rng = nullptr;
  Words words;

  std::vector<FunctionProfile> profile;  // by function
  std::vector<int64_t> builtin_calls;    // by builtin number
  int64_t statements_run = 0;            // since creation

  Fault fault;
};

// A VM over `progs` (which must outlive it): globals copied, entities zero, the stacks, the string
// stores, the builtin table and the profile sized from `limits`.
void create_vm(Vm& vm, const Progs& progs, const VmLimits& limits = {});
// The globals back to the file's values.
void reset_globals(Vm& vm);
// Every entity's fields to zero.
void clear_entities(Vm& vm);
void clear_entity(Vm& vm, int number);

// Builtin `number` (>= 1) is `fn`; `name` is what a trace and a profile call it.
void set_builtin(Vm& vm, int number, Builtin fn, const char* name);

// The engine's call of function `fn`. True when it returned; false on a fault (`vm.fault`).
bool call_function(Vm& vm, int32_t fn);
// Faults the running call with `message` (a builtin's error): the call returns false.
void vm_error(Vm& vm, const char* message);
void clear_fault(Vm& vm);
// The fault in the diagnostics' shape: the message, then each frame (function, file, statement,
// line when known), innermost first, then what to do.
std::string fault_text(const Vm& vm);
// One frame as "name (file) statement N" with ", line L" when the line table has it.
std::string frame_text(const Vm& vm, const Frame& f);

void clear_profile(Vm& vm);

// Words.
inline float as_float(uint32_t w) { return std::bit_cast<float>(w); }
inline uint32_t as_word(float f) { return std::bit_cast<uint32_t>(f); }
inline float global_float(const Vm& vm, int ofs) {
  return as_float(vm.globals[static_cast<size_t>(ofs)]);
}
inline void set_global_float(Vm& vm, int ofs, float v) {
  vm.globals[static_cast<size_t>(ofs)] = as_word(v);
}
inline int32_t global_int(const Vm& vm, int ofs) {
  return static_cast<int32_t>(vm.globals[static_cast<size_t>(ofs)]);
}
inline void set_global_int(Vm& vm, int ofs, int32_t v) {
  vm.globals[static_cast<size_t>(ofs)] = static_cast<uint32_t>(v);
}
// Parameter `i`'s first word, and the return slot.
inline int parm(int i) { return kGlobalParm0 + 3 * i; }
// The form of the program the VM runs (the multiplayer form for a standalone program): where a
// builtin finds the engine's globals and fields.
inline const Layout& vm_layout(const Vm& vm) {
  return vm.progs != nullptr && vm.progs->layout != nullptr ? *vm.progs->layout
                                                            : kMultiplayerLayout;
}

// Entities: a value from a number and back (-1 when the value names no entity in use).
inline int32_t entity_value(const Vm& vm, int number) { return number * vm.entity_bytes; }
int entity_number(const Vm& vm, int32_t value);
// Entity `number`'s field word `field` (unchecked: the caller passes numbers it owns).
inline uint32_t* entity_field(Vm& vm, int number, int field) {
  return vm.entities.data() + static_cast<size_t>(number) * static_cast<size_t>(vm.entityfields) +
         static_cast<size_t>(field);
}
inline const uint32_t* entity_field(const Vm& vm, int number, int field) {
  return vm.entities.data() + static_cast<size_t>(number) * static_cast<size_t>(vm.entityfields) +
         static_cast<size_t>(field);
}

// Strings: a value's text, or nullptr when the value names no string.
const char* vm_string(const Vm& vm, int32_t value);
// A string the engine holds until `clear_engine_strings`; 0 (and a fault) when the store is full.
int32_t engine_string(Vm& vm, const char* text);
void clear_engine_strings(Vm& vm);
// A temporary in the ring (cut to 2047 bytes): valid until 8 more are made.
int32_t temp_string(Vm& vm, const char* text);
// A builtin's text result in the ring of 64 (cut to 2047 bytes): valid until 64 more are made.
int32_t builtin_temp_string(Vm& vm, const char* text);
// A copy kept in a zone slot until `unzone_string` or `clear_zone_strings`; 0 and a fault ("out
// of string memory") when all 1024 are taken. A slot's buffer is kept for reuse, so a program
// that zones and unzones in a loop allocates only while its strings grow.
int32_t zone_string(Vm& vm, const char* text);
// Frees a zone string: true when `value` is one (freeing a freed slot again is allowed), false
// when it names no zone slot (the caller's error).
bool unzone_string(Vm& vm, int32_t value);
void clear_zone_strings(Vm& vm);

}  // namespace ge::progs
