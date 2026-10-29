// SHOOTER'S EDGE'S MOD API: the game's half of the mod ABI (engine/mod/abi.hpp, docs/mods.md). A
// mod for Shooter's Edge includes both headers, returns a `ge_mod_v1` whose `game` is
// SE_MOD_GAME and whose `hooks` points at an `se_mod_hooks_v1`, and reaches the game through the
// `se_mod_services_v1` its host table's `game` points at. C (C99) as well as C++.
//
// THE HOOKS are the game-program seam's (plugins/shooter/program.hpp): the same events, at the
// same points of the loop. The game's own program runs each event FIRST and the mod's hook after,
// so a mod sees what the game just did (the missile in `newmis` after a player's post-think) and
// may change it. A mod's own functions (an entity's think, touch or blocked set to one) are
// numbered by the mod from 1: `function(k)` gives the number to store in a field, and the loop
// calls `call(k)` with `self` and `other` set when it comes due. `call_function` runs any function
// by its number (the game's or the mod's), so a mod that takes over an entity's touch can still run
// the game's after its own.
//
// THE SERVICES never hand out a pointer into the game: an entity is a NUMBER (0 the world,
// 1..max_players the players), a field is an SE_FIELD_* id, and every argument is checked (a bad
// entity or field is refused with a warning in the game's log and answers 0), so the same mod
// source works where the mod cannot address the game's memory (docs/mods.md § The store form).
// Positions are the map format's axes (x and y across, z up) in units; angles in degrees.
//
// ge:summary Shooter's Edge's mod API: the hooks (the program seam's), the services, the fields
// ge:keywords mod mods modding shooters-edge se_mod_hooks_v1 se_mod_services_v1 se-field hooks
// ge:keywords services start_frame player_pre_think player_post_think think touch call_function
// ge:keywords newmis game-api
// ge:skill mod shooter
// ge:test tests/shooters_edge_mod_test.cpp
#pragma once

#include "mod/abi.hpp"

#define SE_MOD_GAME "shooters_edge"
#define SE_MOD_API 1  // the version of the hook and services tables below

// Entity fields by id. Vectors: three floats. Functions are int32 function numbers.
#define SE_FIELD_ORIGIN 1         // vec (set: relinks, as set_origin)
#define SE_FIELD_VELOCITY 2       // vec
#define SE_FIELD_ANGLES 3         // vec
#define SE_FIELD_AVELOCITY 4      // vec
#define SE_FIELD_MINS 5           // vec (read only: set_size)
#define SE_FIELD_MAXS 6           // vec (read only: set_size)
#define SE_FIELD_V_ANGLE 7        // vec
#define SE_FIELD_MOVETYPE 8       // int (SE_MOVETYPE_*)
#define SE_FIELD_SOLID 9          // int
#define SE_FIELD_FLAGS 10         // int
#define SE_FIELD_OWNER 11         // int (an entity)
#define SE_FIELD_GROUNDENTITY 12  // int (an entity)
#define SE_FIELD_THINK 13         // int (a function)
#define SE_FIELD_TOUCH 14         // int (a function)
#define SE_FIELD_BLOCKED 15       // int (a function)
#define SE_FIELD_NEXTTHINK 16     // float, the server's time in s
#define SE_FIELD_HEALTH 17        // float
#define SE_FIELD_GRAVITY 18       // float, a factor (0 means 1)
#define SE_FIELD_MAXSPEED 19      // float (0 means the server's)
#define SE_FIELD_BUTTON0 20       // float: attack held
#define SE_FIELD_IMPULSE 21       // float
#define SE_FIELD_LTIME 22         // float, a pusher's own clock

// Movetypes, as the loop numbers them (plugins/shooter/entity.hpp).
#define SE_MOVETYPE_NONE 0
#define SE_MOVETYPE_WALK 3
#define SE_MOVETYPE_STEP 4
#define SE_MOVETYPE_FLY 5
#define SE_MOVETYPE_TOSS 6
#define SE_MOVETYPE_PUSH 7
#define SE_MOVETYPE_NOCLIP 8
#define SE_MOVETYPE_FLYMISSILE 9
#define SE_MOVETYPE_BOUNCE 10

// GAME -> MOD: the services (the host table's `game`). Every function takes the host's `ctx`.
typedef struct se_mod_services_v1 {
  uint32_t size;  // sizeof as the game compiled it
  uint32_t api;   // SE_MOD_API
  // The loop's globals: the server's time and the step's length (s), `self` and `other` of the
  // event running, and `newmis` (a missile the game's program just spawned; 0 none).
  float (*time)(void* ctx);
  float (*frametime)(void* ctx);
  int32_t (*self)(void* ctx);
  int32_t (*other)(void* ctx);
  int32_t (*newmis)(void* ctx);
  int32_t (*max_entities)(void* ctx);
  int32_t (*max_players)(void* ctx);
  // 1 when entity `e` is in use.
  int32_t (*in_use)(void* ctx, int32_t e);
  // Fields by id (SE_FIELD_*): a getter of the wrong kind or a bad entity answers 0.
  float (*get_float)(void* ctx, int32_t e, int32_t field);
  void (*set_float)(void* ctx, int32_t e, int32_t field, float v);
  int32_t (*get_int)(void* ctx, int32_t e, int32_t field);
  void (*set_int)(void* ctx, int32_t e, int32_t field, int32_t v);
  void (*get_vec)(void* ctx, int32_t e, int32_t field, float out[3]);
  void (*set_vec)(void* ctx, int32_t e, int32_t field, const float v[3]);
  // The loop's entity calls: a new entity's number (-1 when the pool is full), remove, the box.
  int32_t (*spawn)(void* ctx);
  void (*remove)(void* ctx, int32_t e);
  void (*set_size)(void* ctx, int32_t e, const float mins[3], const float maxs[3]);
  // The number of the mod's own function k (1..), to store in a think, touch or blocked field.
  int32_t (*function)(void* ctx, uint32_t k);
  // Runs function `fn` (the game's or the mod's) with `self` and `other` set, then puts them back.
  void (*call_function)(void* ctx, int32_t fn, int32_t self, int32_t other);
  // A sound from entity `e` on `channel`; the game copies the sample's name.
  void (*sound)(void* ctx, int32_t e, int32_t channel, const char* sample, float volume,
                float attenuation);
} se_mod_services_v1;

// MOD -> GAME: the hooks (the mod table's `hooks`). Any may be null. `state` is the mod's block.
typedef struct se_mod_hooks_v1 {
  uint32_t size;  // sizeof as the mod compiled it
  uint32_t api;   // SE_MOD_API the mod was built against
  // After the map's own entities are spawned, before the first frame.
  void (*match_start)(void* state, const ge_mod_host_v1* host);
  // Each physics frame, before any entity runs (after the game's StartFrame).
  void (*start_frame)(void* state, const ge_mod_host_v1* host);
  // Before each of a player's commands is moved; after all of a packet's commands are moved.
  void (*player_pre_think)(void* state, const ge_mod_host_v1* host, int32_t player);
  void (*player_post_think)(void* state, const ge_mod_host_v1* host, int32_t player);
  void (*client_connect)(void* state, const ge_mod_host_v1* host, int32_t player);
  void (*put_client_in_server)(void* state, const ge_mod_host_v1* host, int32_t player);
  void (*client_disconnect)(void* state, const ge_mod_host_v1* host, int32_t player);
  // The mod's own function k, come due as an entity's think, touch or blocked.
  void (*call)(void* state, const ge_mod_host_v1* host, uint32_t k, int32_t self, int32_t other);
} se_mod_hooks_v1;
