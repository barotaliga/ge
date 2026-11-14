// SHOOTER'S EDGE'S RULES API: a whole game's rules shipped as a mod package (docs/mods.md § Rules
// as a package). Where a mod (mod_api.hpp) stacks hooks on rules the game already runs, a RULES
// PACKAGE is the rules: the program the classic server runs in place of a compiled one, as a
// native program (plugins/progs/native.hpp) whose declaration and functions come from the
// package. C (C99) as well as C++.
//
// THE PACKAGE returns a `ge_mod_v1` whose `game` is SE_RULES_GAME, whose `game_api` is
// SE_RULES_API and whose `hooks` points at an `se_rules_v1`: the program's FORM (multiplayer or
// single-player, plugins/progs/layout.hpp), its own globals and fields by name and type, its
// strings, and its functions by name with their parameters' sizes. The server lays that out as a
// compiled program is laid out, so the game, the bots and the saves find a global, a field or a
// spawn function by name exactly as they would in bytecode. Its `state_bytes` is 0: a program's
// whole state is its globals and fields, which live in the game's VM (so a snapshot, a save and
// the digest hold them).
//
// A FUNCTION runs with one argument, the CALL: the services and the game's context, plus where
// the declaration landed (each own global's and field's offset, each string's value, each
// function's number) and the form's offsets in plugins/progs/layout.hpp's order. Every word of
// the game's state is read and written through the services, never through a pointer, and every
// access is checked: a field of no entity, or an offset out of range, faults the call (the game's
// log names it) and every later access in it answers 0, as the bytecode's fault does. The
// SERVER'S CALLS are the classic builtins by their numbers (`call_builtin`), with their arguments
// in the parameter globals, so a rule reaches exactly the code a compiled program reaches.
//
// The tree's own rules (games/shooters_edge/rules: the arena and the campaign) are written
// against this table: built into the game, and built as the `arena` and `campaign` packages that
// play the same bits (tests/shooters_edge_rules_package_test.cpp).
//
// ge:summary Shooter's Edge's rules API: a whole program of rules as a mod package
// ge:keywords rules-package rules-api se_rules_v1 se_rules_call_v1 se_rules_services_v1
// ge:keywords native-program package arena-package campaign-package call_builtin
// ge:skill mod progs shooter
// ge:test tests/shooters_edge_rules_package_test.cpp
#pragma once

#include "mod/abi.hpp"

#define SE_RULES_GAME "shooters_edge_rules"
#define SE_RULES_API 1  // the version of the tables below

// The program's form (plugins/progs/layout.hpp).
#define SE_RULES_FORM_MULTIPLAYER 0
#define SE_RULES_FORM_SINGLE_PLAYER 1

// A global's or a field's type, as the progs format numbers them.
#define SE_RULES_TYPE_STRING 1
#define SE_RULES_TYPE_FLOAT 2
#define SE_RULES_TYPE_VECTOR 3
#define SE_RULES_TYPE_ENTITY 4
#define SE_RULES_TYPE_FUNCTION 6

typedef struct se_rules_call_v1 se_rules_call_v1;

typedef struct se_rules_def_v1 {
  const char* name;
  uint32_t type;  // SE_RULES_TYPE_*
} se_rules_def_v1;

typedef struct se_rules_function_v1 {
  const char* name;
  void (*run)(const se_rules_call_v1* call);
  uint32_t parm_count;   // at most 8
  const uint8_t* parms;  // each parameter's words (1, or 3 for a vector)
} se_rules_function_v1;

// MOD -> GAME: the program (the `ge_mod_v1`'s `hooks`).
typedef struct se_rules_v1 {
  uint32_t size;     // sizeof as the package compiled it
  uint32_t api;      // SE_RULES_API
  uint32_t form;     // SE_RULES_FORM_*
  const char* file;  // what a fault's stack names as the functions' file
  const se_rules_def_v1* globals;
  uint32_t global_count;
  const se_rules_def_v1* fields;
  uint32_t field_count;
  const char* const* strings;
  uint32_t string_count;
  const se_rules_function_v1* functions;
  uint32_t function_count;
} se_rules_v1;

// GAME -> MOD: the services. Words are 32 bits: a float's bits, an entity, a string or a function.
typedef struct se_rules_services_v1 {
  uint32_t size;  // sizeof as the game compiled it
  uint32_t api;   // SE_RULES_API
  // Whether the call has faulted (every access after a fault answers 0 and writes nothing).
  int32_t (*failed)(void* ctx);
  // A global word (an offset out of range faults).
  uint32_t (*global)(void* ctx, int32_t ofs);
  void (*set_global)(void* ctx, int32_t ofs, uint32_t word);
  // `words` (1 or 3) words of an entity's field from `field` on (no entity in use faults; the
  // world's fields are read only once the map has loaded).
  void (*field)(void* ctx, int32_t ent, int32_t field, int32_t words, uint32_t* out);
  void (*set_field)(void* ctx, int32_t ent, int32_t field, int32_t words, const uint32_t* in);
  // A string value's text (null for none); valid until the next call into the game.
  const char* (*string)(void* ctx, int32_t s);
  // The server's builtin `number` with `argc` arguments already in the parameter globals; its
  // result in the return global. Nonzero when it ran.
  int32_t (*call_builtin)(void* ctx, int32_t number, int32_t argc);
  // The program's function `number` (a field's think, a spawn function), run by the game's call.
  int32_t (*call_function)(void* ctx, int32_t number);
} se_rules_services_v1;

// What a function runs with.
struct se_rules_call_v1 {
  const se_rules_services_v1* services;
  void* ctx;
  // Where the declaration landed, by its index in each list.
  const int32_t* globals;    // offsets
  const int32_t* fields;     // offsets
  const int32_t* strings;    // string values
  const int32_t* functions;  // function numbers
  // The form's engine-side offsets in plugins/progs/layout.hpp's order (-1 where the form has
  // none): GE_PROGS_LAYOUT_GLOBALS's, then GE_PROGS_LAYOUT_FIELDS's.
  const int32_t* form_globals;
  uint32_t form_global_count;
  const int32_t* form_fields;
  uint32_t form_field_count;
};
