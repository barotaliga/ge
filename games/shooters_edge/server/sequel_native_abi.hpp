// THE SEQUEL'S GAME LIBRARY, ITS BINARY INTERFACE (M18v10): the records and the two tables of
// calls through which a server hosts an existing, unmodified game library of the sequel (a mod's
// rules, built as a native library), laid out byte for byte as such a library was compiled
// against. The facts, with their sizes and offsets measured on 64-bit x86-64, are in
// porting/shooters-edge/sequel-game-library.md; the names here are this tree's own.
//
// - The library exports one entry point, `kNativeEntry`, that takes the server's table of calls
//   (`NativeImports`, 44 entries in a fixed order) and returns its own (`NativeExports`), whose
//   first field is the interface's version, `kNativeVersion`.
// - The game owns the entity array; the server reads and writes only each entity's head
//   (`NativeEntityHead`), at `edicts + n × edict_size`, never by its own idea of the size.
// - A record larger than 16 bytes returned by value (the trace's) goes through the C ABI's hidden
//   pointer, so every call here has C linkage and these exact records.
//
// The static checks below hold the 64-bit layout; a 32-bit build has its own (pointers 4 bytes)
// and is not loaded by this server.
//
// ge:summary The sequel's game library interface: the records and the two tables of calls
// ge:keywords game-library native-game-library gamelib abi entry-point imports exports edict
// ge:keywords entity-head player-state move-block trace-result cvar-record m18v10
// ge:skill shooter mod
// ge:test tests/shooters_edge_sequel_native_test.cpp
#pragma once

#include <cstddef>
#include <cstdint>

namespace shooters_edge::server::native {

inline constexpr const char* kNativeEntry = "GetGameAPI";
inline constexpr int kNativeVersion = 3;
inline constexpr int kNativeStats = 32;
inline constexpr int kNativeTouch = 32;
inline constexpr int kNativeEntityClusters = 16;

// An entity's network state (84 bytes).
struct NativeState {
  int32_t number;
  float origin[3];
  float angles[3];
  float old_origin[3];
  int32_t model[4];
  int32_t frame;
  int32_t skin;
  uint32_t effects;
  int32_t renderfx;
  int32_t solid;  // the packed box, written by link
  int32_t sound;
  int32_t event;  // cleared after each frame is sent
};

// The packed move state (28 bytes): eighths of a unit, 65536ths of a turn.
struct NativeMoveState {
  int32_t type;  // 0 normal, 1 spectator, 2 dead, 3 gib, 4 freeze
  int16_t origin[3];
  int16_t velocity[3];
  uint8_t flags;
  uint8_t time;  // units of 8 ms
  int16_t gravity;
  int16_t delta_angles[3];
};

// The player's state (184 bytes): what each client's frame carries.
struct NativePlayer {
  NativeMoveState move;
  float view_angles[3];
  float view_offset[3];
  float kick_angles[3];
  float gun_angles[3];
  float gun_offset[3];
  int32_t gun_index;
  int32_t gun_frame;
  float blend[4];
  float fov;
  int32_t rdflags;
  int16_t stats[kNativeStats];
};

// The head of a client record (188 bytes); the game's own fields follow.
struct NativeClientHead {
  NativePlayer ps;
  int32_t ping;  // ms, written by the server before each frame
};

// The head of an entity record (280 bytes); the game's own fields follow.
struct NativeEntityHead {
  NativeState s;
  NativeClientHead* client;  // null for anything but a player
  int32_t inuse;
  int32_t linkcount;
  void* area[2];  // the server's list links; null when unlinked
  int32_t num_clusters;
  int32_t clusters[kNativeEntityClusters];
  int32_t headnode;
  int32_t area_num, area_num2;
  int32_t svflags;
  float mins[3], maxs[3];
  float absmin[3], absmax[3];
  float size[3];
  int32_t solid;  // kSolid*
  int32_t clipmask;
  NativeEntityHead* owner;
};

inline constexpr int32_t kSolidNot = 0, kSolidTrigger = 1, kSolidBox = 2, kSolidBsp = 3;
inline constexpr int32_t kSvfNoClient = 1, kSvfDeadMonster = 2, kSvfMonster = 4;

// One movement command (16 bytes).
struct NativeCmd {
  uint8_t msec;
  uint8_t buttons;
  int16_t angles[3];
  int16_t forward, side, up;
  uint8_t impulse;
  uint8_t light;
};

struct NativePlane {
  float normal[3];
  float dist;
  uint8_t type;
  uint8_t signbits;
  uint8_t pad[2];
};

struct NativeSurface {
  char name[16];
  int32_t flags;
  int32_t value;
};

// The trace's answer (72 bytes): `surface` and `ent` are never null.
struct NativeTrace {
  int32_t allsolid;
  int32_t startsolid;
  float fraction;
  float endpos[3];
  NativePlane plane;
  NativeSurface* surface;
  int32_t contents;
  NativeEntityHead* ent;
};

using NativeTraceFn = NativeTrace (*)(const float* start, const float* mins, const float* maxs,
                                      const float* end);
using NativeContentsFn = int32_t (*)(const float* point);

// The block the game hands the move (384 bytes).
struct NativeMoveBlock {
  NativeMoveState s;
  NativeCmd cmd;
  int32_t snapinitial;
  int32_t numtouch;
  NativeEntityHead* touchents[kNativeTouch];
  float view_angles[3];
  float view_height;
  float mins[3], maxs[3];
  NativeEntityHead* groundentity;
  int32_t watertype;
  int32_t waterlevel;
  NativeTraceFn trace;
  NativeContentsFn pointcontents;
};

// A setting as the game reads it through the pointer it keeps (56 bytes). It never moves.
struct NativeCvar {
  char* name;
  char* string;
  char* latched_string;
  int32_t flags;
  int32_t modified;
  float value;
  NativeCvar* next;
  char* default_string;
};

inline constexpr int32_t kCvarArchive = 1, kCvarUserInfo = 2, kCvarServerInfo = 4, kCvarNoSet = 8,
                         kCvarLatch = 16;

// The server's calls, in the interface's order.
struct NativeImports {
  void (*bprintf)(int level, const char* fmt, ...);
  void (*dprintf)(const char* fmt, ...);
  void (*cprintf)(NativeEntityHead* ent, int level, const char* fmt, ...);
  void (*centerprintf)(NativeEntityHead* ent, const char* fmt, ...);
  void (*sound)(NativeEntityHead* ent, int channel, int index, float volume, float attenuation,
                float offset);
  void (*positioned_sound)(const float* origin, NativeEntityHead* ent, int channel, int index,
                           float volume, float attenuation, float offset);
  void (*configstring)(int index, const char* text);
  void (*error)(const char* fmt, ...);
  int (*modelindex)(const char* name);
  int (*soundindex)(const char* name);
  int (*imageindex)(const char* name);
  void (*setmodel)(NativeEntityHead* ent, const char* name);
  NativeTrace (*trace)(const float* start, const float* mins, const float* maxs, const float* end,
                       NativeEntityHead* pass, int mask);
  int (*pointcontents)(const float* point);
  int (*in_pvs)(const float* a, const float* b);
  int (*in_phs)(const float* a, const float* b);
  void (*set_area_portal)(int portal, int open);
  int (*areas_connected)(int a, int b);
  void (*linkentity)(NativeEntityHead* ent);
  void (*unlinkentity)(NativeEntityHead* ent);
  int (*box_edicts)(const float* mins, const float* maxs, NativeEntityHead** list, int most,
                    int kind);
  void (*pmove)(NativeMoveBlock* block);
  void (*multicast)(const float* origin, int to);
  void (*unicast)(NativeEntityHead* ent, int reliable);
  void (*write_char)(int c);
  void (*write_byte)(int c);
  void (*write_short)(int c);
  void (*write_long)(int c);
  void (*write_float)(float f);
  void (*write_string)(const char* s);
  void (*write_position)(const float* pos);
  void (*write_dir)(const float* dir);
  void (*write_angle)(float f);
  void* (*tag_malloc)(int size, int tag);
  void (*tag_free)(void* block);
  void (*free_tags)(int tag);
  NativeCvar* (*cvar)(const char* name, const char* value, int flags);
  NativeCvar* (*cvar_set)(const char* name, const char* value);
  NativeCvar* (*cvar_forceset)(const char* name, const char* value);
  int (*argc)();
  const char* (*argv)(int n);
  const char* (*args)();
  void (*add_command_string)(const char* text);
  void (*debug_graph)(float value, int color);
};

// The game's calls and its entity array, in the interface's order.
struct NativeExports {
  int32_t apiversion;
  void (*init)();
  void (*shutdown)();
  void (*spawn_entities)(const char* map, char* entities, const char* spawn_point);
  void (*write_game)(const char* file, int autosave);
  void (*read_game)(const char* file);
  void (*write_level)(const char* file);
  void (*read_level)(const char* file);
  int (*client_connect)(NativeEntityHead* ent, char* userinfo);
  void (*client_begin)(NativeEntityHead* ent);
  void (*client_userinfo_changed)(NativeEntityHead* ent, char* userinfo);
  void (*client_disconnect)(NativeEntityHead* ent);
  void (*client_command)(NativeEntityHead* ent);
  void (*client_think)(NativeEntityHead* ent, NativeCmd* cmd);
  void (*run_frame)();
  void (*server_command)();
  uint8_t* edicts;  // the entity array, as bytes: entity n at edicts + n × edict_size
  int32_t edict_size;
  int32_t num_edicts;
  int32_t max_edicts;
};

using NativeEntryFn = NativeExports* (*)(NativeImports* imports);

static_assert(sizeof(NativeState) == 84 && sizeof(NativeMoveState) == 28);
static_assert(sizeof(NativePlayer) == 184 && sizeof(NativeCmd) == 16);
static_assert(sizeof(NativePlane) == 20 && sizeof(NativeSurface) == 24);
static_assert(sizeof(void*) != 8 ||
                  (sizeof(NativeEntityHead) == 280 && offsetof(NativeEntityHead, svflags) == 200 &&
                   offsetof(NativeEntityHead, owner) == 272 && sizeof(NativeTrace) == 72 &&
                   offsetof(NativeTrace, ent) == 64 && sizeof(NativeMoveBlock) == 384 &&
                   offsetof(NativeMoveBlock, groundentity) == 352 && sizeof(NativeCvar) == 56 &&
                   sizeof(NativeImports) == 352 && sizeof(NativeExports) == 152 &&
                   offsetof(NativeExports, edicts) == 128 && sizeof(NativeClientHead) == 188),
              "the game library's 64-bit layout");

}  // namespace shooters_edge::server::native
