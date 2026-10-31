// THE CLASSIC SERVER'S WORLD: everything one match keeps (the entity pool, the area tree, the brush
// models, the program and its globals, the server's clock, the clients' connection state) and the
// services every part of the loop shares: linking an entity after it moves, touching triggers and
// impacts, spawning and freeing entities, calling the program, sounds.
//
// LINKING (`link_edict`): an entity's `absmin`/`absmax` are its origin plus its box, grown 15 units
// sideways for an item (kFlagItem) and 1 unit on every axis otherwise (movement stops a hair short
// of touching, so boxes must overlap a little to find each other); a `not` solid is unlinked and
// stays out; with `touch_triggers` the trigger touches run after (`touch_links`).
//
// TOUCHING: TRIGGERS (`touch_links`): every trigger whose box meets the entity's, in the area
// tree's listing order, other than the entity, with a touch function and still a trigger when its
// turn comes, runs its touch with self the trigger and other the entity. IMPACTS (`impact`): the
// mover's touch (self the mover, other the hit), then the hit's (self the hit, other the mover),
// each only when it has one and is not a `not` solid. Both set `time` to the server's time and
// restore `self` and `other` after.
//
// THE CLOCK: `time` is the server's time in seconds (a double), advanced only by the server's
// frame (frame.hpp) on the run clock: never the wall clock. A run is a pure function of its map,
// its settings and the commands it received in order.
//
// The pool is reserved at creation (`ServerSettings::max_edicts`, at most kMaxEdicts); nothing in
// a frame allocates. A freed slot is reused once it has been free half a second (or at once in the
// map's first two seconds), so a reference to a just-removed entity does not land on a new one.
//
// ge:summary The classic server's world and shared services: pool, links, touches, calls, sounds
// ge:keywords server sv edicts spawn free remove link_edict unlink touch_links impact trigger
// ge:keywords set_origin set_size setmodel sound globals call program clients server-time
// ge:keywords server_hash state-hash digest
// ge:skill shooter
// ge:test tests/shooter_server_test.cpp
#pragma once

#include <cstdint>
#include <vector>

#include "shooter/area.hpp"
#include "shooter/brush_hull.hpp"
#include "shooter/pmove.hpp"
#include "shooter/program.hpp"

namespace ge::shooter {

constexpr int kMaxEdicts = 2048;  // the protocol's limit
constexpr int kMaxClients = 32;
constexpr int kMaxSounds = 256;  // sounds a frame keeps for the host (more are counted, dropped)

// Which player move the server runs (sv_playermove; framemove.hpp says when each moves).
enum class PlayerMove : uint8_t {
  arena,     // the predicted move (pmove.hpp): each command moves the player as it arrives
  original,  // the original move (framemove.hpp): the server's own step
};

// The server's settings: the move's (sent to clients) and the loop's own.
struct ServerSettings {
  MoveSettings move;
  double mintic = 0.013;                // sv_mintic: the physics step, s
  double maxtic = 0.1;                  // sv_maxtic: the longest physics step, s
  float maxvelocity = 2000;             // sv_maxvelocity, units/s
  float minpitch = -70, maxpitch = 80;  // sv_minpitch, sv_maxpitch: the view's pitch, degrees
  bool speedcheck = true;               // sv_speedcheck
  int maxclients = kMaxClients;
  int max_edicts = 512;
  double start_time = 1.0;  // the server's time when the map starts, s
  // Where a new entity is first looked for (0: right after the client slots, maxclients + 1; a
  // QC program's server uses 33, after all 32 slots, as its entity numbers expect).
  int first_dynamic = 0;
  // A full pool: false, spawning fails (-1); true, the last entity is unlinked, cleared and
  // reused with a warning counted (`LoopCounts::spawns_full`), as a QC program's server does.
  bool full_reuses_last = false;
  // The single-player clock (frame.hpp): a frame's packets and physics see the time the frame
  // started at, the time advancing after them, and an entity thinks at most once a frame.
  bool single_player_clock = false;
  // The player move, and its program: a single-player program jumps for itself in its pre-think
  // (the original move then never jumps), a multiplayer one expects the move to (sent to clients).
  PlayerMove playermove = PlayerMove::arena;
  bool program_jumps = false;
  // The original move's own settings (the rest are `move`'s).
  float edgefriction = 2;  // edgefriction: the friction's factor where the ground ahead falls away
  bool nostep = false;     // sv_nostep: no stepping up
  float rollangle = 2;     // cl_rollangle: the body's roll at speed, degrees
  float rollspeed = 200;   // cl_rollspeed: the sideways speed that rolls it fully, units/s
};

// A sound the loop or the program started this frame, for the host to play.
struct SoundEvent {
  int ent = 0;
  int channel = 0;
  const char* sample = "";  // the content's sound name
  float volume = 1, attenuation = 1;
};

// What a client's connection keeps for the loop (the player is entity slot + 1).
struct ClientSlot {
  bool active = false;
  bool spectator = false;
  bool bot = false;             // bots skip the speed check
  bool old_spectator = false;   // a client that knows only the old spectator move
  uint32_t last_sequence = 0;   // the newest packet received
  bool any_packet = false;      // a packet has been received
  MoveCmd last_cmd;             // the newest command, buttons cleared
  bool jump_held = false;       // the move's jump-held flag between commands
  Vec3f ground_normal;          // the plane the move last stood on (its jump and landing fix)
  int speed_bank_ms = 0;        // the speed check's banked surplus, 0..500
  double speed_check_time = 0;  // the server time of the last speed check
  int last_msec = 0;            // ms since the server last ran this player's command (for others)
  double last_cmd_time = 0;     // the server time its last command ran
  MoveCmd latest;               // the original move on the single-player clock: the command the
  bool any_latest = false;      // next frame moves by (it persists until a newer one arrives)
};

// What the loop did, counted (the lab and the tests read them; they feed no rule).
struct LoopCounts {
  int64_t thinks = 0, trigger_touches = 0, impacts = 0, player_touches = 0;
  int64_t pushes = 0, pushes_blocked = 0, new_missiles = 0, commands = 0, commands_lost = 0;
  int64_t speed_cuts = 0, physics_frames = 0, sounds_dropped = 0, spawns_full = 0;
};

struct Server {
  ServerSettings set;
  std::vector<Edict> edicts;  // reserved to set.max_edicts; [0] the world
  int num_edicts = 1;         // slots in use (free ones included)
  AreaTree area;
  std::vector<BrushModel> models;  // by modelindex; [0] none, [1] the world
  Program program;
  Globals g;
  double time = 0;          // the server's time, s
  int64_t frame = 0;        // server frames run
  double last_physics = 0;  // the server time physics last ran
  bool physics_started = false;
  double base_time = 0;    // the time frame `base_frame` ended at (start_time, or after settling)
  int64_t base_frame = 0;  // the frame `base_time` is counted from
  std::vector<ClientSlot> clients;  // set.maxclients
  SoundEvent sounds[kMaxSounds];
  int sound_count = 0;
  // The original walk for every trace (hull.hpp): set on the single-player clock's server, and
  // for the length of an original move's own traces on any server (framemove.hpp).
  bool original_walk = false;
  std::vector<uint32_t> touched;  // per entity: the packet stamp it last touched a player in
  uint32_t touch_stamp = 0;
  LoopCounts counts;
};

// A server on these brush models (`models[0]` the world), with the program, at `set.start_time`.
// Entity 0 is the world (solid bsp, modelindex 1); slots 1..maxclients are the players', free.
void create_server(Server& sv, const ServerSettings& set, const std::vector<BrushModel>& models,
                   const Program& program);

// A new entity (fields at their defaults), looked for from `first_dynamic`; -1 when the pool is
// full (or its last entity again, with `full_reuses_last`).
int spawn_edict(Server& sv);
// Removes an entity: unlinked, its fields reset, its slot free.
void free_edict(Server& sv, int e);

void link_edict(Server& sv, int e, bool touch_triggers);
void unlink_edict(Server& sv, int e);
void touch_links(Server& sv, int e);
void impact(Server& sv, int e1, int e2);

// The program's builtins the loop shares: moves an entity and relinks it (no touches).
void set_origin(Server& sv, int e, Vec3f origin);
void set_size(Server& sv, int e, Vec3f mins, Vec3f maxs);
// Gives an entity a brush model: its index and its bounds as its box.
void set_brush_model(Server& sv, int e, int modelindex);

// Runs program function `fn` (nothing for kNoFunc) with the globals as they are.
void call_program(Server& sv, Func fn);
// Starts a sound (kept for the host; past kMaxSounds a frame it is counted and dropped).
void start_sound(Server& sv, int e, int channel, const char* sample, float volume,
                 float attenuation);

// The world's hull 0 at a point.
int world_contents(const Server& sv, Vec3f p);

// A digest of the whole match state (FNV-1a 64 over the server's time and every entity's and
// client's loop fields, bit for bit): two runs of the same commands agree on it every frame.
uint64_t server_hash(const Server& sv);

}  // namespace ge::shooter
