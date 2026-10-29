// THE MOD ABI, the engine's half (docs/mods.md). A mod is a shared library built from C++ against
// this header and its game's (Shooter's Edge's: games/shooters_edge/sim/mod_api.hpp); the game
// loads it (engine/mod/loader.hpp) and the two talk through TABLES OF C FUNCTION POINTERS, one
// each way, with plain C types only across them: fixed-width integers, float, double, pointers to
// those, and NUL-terminated UTF-8 text the caller owns for the call. No C++ type, no exception and
// no allocation crosses the line, so a mod built by any compiler of the platform's C ABI loads.
// This file is C (C99) as well as C++: it compiles as either.
//
//   ge_mod_entry(host_abi)   the one exported symbol: the mod's table (`ge_mod_v1`), or null
//   ge_mod_v1                mod -> game: its name, the game it is for, its state's size, init,
//                            and the game's own hook table
//   ge_mod_host_v1           game -> mod: the engine's libm, the game's random stream, the log,
//                            and the game's own services table
//
// HOW THE TABLES GROW without breaking an old mod: every table starts with `size` (the sizeof the
// writer compiled) and the reader touches an entry only when the table is long enough to hold it
// (`GE_MOD_HAS`); entries are only ever appended; a null entry means "not offered". The version is
// `GE_MOD_ABI` = major << 16 | minor: a new entry is a minor bump, a changed meaning or layout is a
// major one, and a major the loader does not speak is refused, naming both versions.
//
// DETERMINISM: a mod keeps its whole state in the block the game owns (`state_bytes`, handed to
// every call), never in a global, so the game copies it into its snapshots and folds it into its
// digests; it draws only from the game's stream, reads no clock, and calls the transcendentals
// through the table (the engine's libm, the same bits on every platform).
//
// ge:summary The mod ABI's engine half: the entry symbol, the mod and host tables, versions
// ge:keywords mod mods modding abi c-abi plugin shared-library ge_mod_entry ge_mod_v1
// ge:keywords ge_mod_host_v1 abi-version table-size ge_mod_has state-block player-plugin
// ge:skill mod
// ge:test tests/mod_loader_test.cpp
#pragma once

#include <stdint.h>

#define GE_MOD_ABI_MAJOR 1
#define GE_MOD_ABI_MINOR 0
#define GE_MOD_ABI ((uint32_t)((GE_MOD_ABI_MAJOR << 16) | GE_MOD_ABI_MINOR))
#define GE_MOD_ABI_MAJOR_OF(v) ((uint32_t)(v) >> 16)
#define GE_MOD_ABI_MINOR_OF(v) ((uint32_t)(v) & 0xffffu)

// The largest state block a game keeps for one mod, bytes.
#define GE_MOD_STATE_MAX (1u << 20)

// The exported entry's name, as the loader looks it up.
#define GE_MOD_ENTRY_NAME "ge_mod_entry"

// True when `table` (a pointer to a table whose first field is `size`) is long enough to hold
// `member`: the test every reader makes before it touches an entry added after the first
// version.
#define GE_MOD_HAS(type, table, member) \
  ((table) != 0 && (table)->size >= (uint32_t)(offsetof(type, member) + sizeof((table)->member)))

#ifdef __cplusplus
#include <cstddef>
#define GE_MOD_EXTERN_C extern "C"
#else
#include <stddef.h>
#define GE_MOD_EXTERN_C
#endif

#if defined(_WIN32)
#define GE_MOD_EXPORT GE_MOD_EXTERN_C __declspec(dllexport)
#else
#define GE_MOD_EXPORT GE_MOD_EXTERN_C __attribute__((visibility("default")))
#endif

// Log levels for `ge_mod_host_v1::log`.
#define GE_MOD_LOG_INFO 0
#define GE_MOD_LOG_WARN 1
#define GE_MOD_LOG_ERROR 2

// GAME -> MOD: what every game hands every mod. `ctx` is the game's, passed back unchanged.
typedef struct ge_mod_host_v1 {
  uint32_t size;  // sizeof as the game compiled it
  uint32_t abi;   // the GE_MOD_ABI the game speaks
  void* ctx;
  // The engine's libm: the same bits on every platform (a mod never calls <cmath>'s). sqrt,
  // fabs, floor, ceil, trunc and fmod are exact everywhere, so a mod may call its own.
  double (*sin)(double x);
  double (*cos)(double x);
  double (*tan)(double x);
  double (*asin)(double x);
  double (*acos)(double x);
  double (*atan)(double x);
  double (*atan2)(double y, double x);
  double (*exp)(double x);
  double (*expm1)(double x);
  double (*log)(double x);
  double (*log2)(double x);
  double (*pow)(double x, double y);
  double (*cbrt)(double x);
  double (*tanh)(double x);
  // The game's seeded stream (its state is in the game's snapshot): the next 32 bits, and a
  // double in [0, 1) made from them.
  uint32_t (*random_u32)(void* ctx);
  double (*random)(void* ctx);
  // A line for the game's log (GE_MOD_LOG_*); the game copies the text.
  void (*log_line)(void* ctx, int32_t level, const char* text);
  // The game's own services table (its size first; the game's header names its type).
  const void* game;
} ge_mod_host_v1;

// MOD -> GAME: the table `ge_mod_entry` returns. Static data in the library: the game reads it
// and never writes it.
typedef struct ge_mod_v1 {
  uint32_t size;         // sizeof as the mod compiled it
  uint32_t abi;          // the GE_MOD_ABI the mod was built against
  const char* name;      // the package's name (the manifest's), kebab case
  const char* version;   // the package's version
  const char* game;      // the game id it is for ("shooters_edge")
  uint32_t game_api;     // the version of that game's hook table it was built against
  uint32_t state_bytes;  // the state block the game keeps for it, at most GE_MOD_STATE_MAX
  // A new match: the block is zeroed, then this fills it (may be null).
  void (*init)(void* state, const ge_mod_host_v1* host);
  // The game's hook table (its size first; the game's header names its type).
  const void* hooks;
} ge_mod_v1;

// The one symbol a mod exports. `host_abi` is the loader's GE_MOD_ABI, so a mod may hand an older
// host an older table; null refuses to load.
typedef const ge_mod_v1* (*ge_mod_entry_fn)(uint32_t host_abi);
