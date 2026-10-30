// A LOADED PROGS DRIVES THE LOOP: the adapter between the shooter loop's program seam
// (plugins/shooter/program.hpp) and the QC VM (plugins/progs). The loop calls a function number;
// the adapter mirrors the loop's entity fields and globals into the VM's memory, runs that QC
// function, and mirrors them back, so a mod's StartFrame, PlayerPreThink, PlayerPostThink and its
// entities' think, touch and blocked functions run the match. The server around it (the builtins,
// the hooks with arguments, the map's entities: games/shooters_edge/server) calls through
// `enter_qc` / `run_qc` / `leave_qc` and keeps the two sides in step around a builtin with
// `flush_to_world` and `refresh_from_world`.
//
// - THE FORM: every offset below is the program's form's (`layout`, progs/layout.hpp), so a
//   multiplayer program (crc 54730) and a single-player one (5927) run on the same adapter.
// - HOOKS come from the hook globals the program stored (StartFrame ... SetChangeParms, read once
//   when the adapter is made) and the optional ones by name (SpectatorConnect,
//   SpectatorThink, SpectatorDisconnect). A function number of the loop IS a QC function index.
// - THE MIRROR: before each call every entity in use has its loop fields (origin, velocity,
//   angles, the box, movetype, solid, flags, the functions, nextthink, health, the buttons, the
//   extension fields gravity, maxspeed and movement, ...) written into its field words, and
//   `self`, `other`, `time`, `frametime`, `newmis`, `force_retouch` into the globals; after it, the
//   same are read back (movement is the loop's alone). Ints the loop keeps are QC floats; entities
//   are entity values (number x entityfields x 4); functions are raw words. The program's own
//   fields (and the extension fields the loop does not keep) live only in the VM and survive.
// - A NESTED CALL (a touch run by a builtin that moves an entity) mirrors the same way and puts
//   back the server it found, so the outer call goes on with both sides in step.
// - THE SEAM'S BUILTINS (`qc_spawn` #14, `qc_remove` #15, `qc_setorigin` #2) act on the loop at
//   once: a spawned entity is the loop's (`spawn_edict`, under the progs' allocation rule when the
//   server is set up for it: from entity 33, 2048 at most), cleared in the VM; a removed one is
//   freed in the loop and gets the progs' free rule in the VM (its model, takedamage, modelindex,
//   colormap, skin, frame, health, classname, origin, angles and solid cleared, nextthink -1; its
//   other fields kept).
// - A FAULT ends the QC call (the VM unwinds), is counted with its text kept (`faults`,
//   `last_fault`), and the match goes on: what was written before it stays.
//
// ge:summary The adapter that lets a loaded progs drive the shooter loop's program seam
// ge:keywords qc progs adapter program-seam mirror fields globals hooks think touch blocked
// ge:keywords start_frame player_pre_think player_post_think spawn remove setorigin free-rule
// ge:keywords qc_program create_qc_program enter_qc run_qc leave_qc flush_to_world
// ge:keywords refresh_from_world optional-fields extension-fields movement gravity maxspeed
// ge:skill progs shooter
// ge:test tests/shooters_edge_qc_program_test.cpp
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "core/prng.hpp"
#include "progs/vm.hpp"
#include "shooter/program.hpp"
#include "shooter/server.hpp"

namespace shooters_edge::sim {

namespace sh = ge::shooter;

// The optional fields a program may declare, found by name after loading; -1 when absent.
struct OptionalFields {
  int items2 = -1, maxspeed = -1, gravity = -1, movement = -1, vw_index = -1;
  int hideentity = -1, trackent = -1, visclients = -1, hideplayers = -1, teleported = -1;
};

constexpr int kMaxQcNesting = 40;
constexpr int kQcBuiltins = 1024;  // the builtin table: the extension numbers reach past 512  // QC
                                   // calls in progress at once (the VM's 32 deep, and more)

struct QcProgram {
  ge::progs::Vm vm;
  // The program's form: every engine global and field the adapter and the server's builtins
  // touch is read off it (the multiplayer form for a program that names none).
  const ge::progs::Layout* layout = &ge::progs::kMultiplayerLayout;
  sh::Server* sv = nullptr;               // the server of the call running now (null between calls)
  sh::Server* outer[kMaxQcNesting] = {};  // the server each call in progress replaced
  int nesting = 0;
  std::vector<uint8_t> live;  // by entity number: in use in the VM's mirror
  OptionalFields fields;
  sh::Program hooks;     // the hook numbers, filled at creation
  ge::core::Rng rng{1};  // what random, randomvec and movetogoal draw from (seeded by the game)
  int64_t calls = 0;
  int64_t faults = 0;
  std::string last_fault;  // the newest fault's text with its QC stack
  // Called before and after every QC call the seam makes (the server logs the hook with the
  // first and saves the spawn parms after SetNewParms with the second); null for none.
  void (*before_call)(QcProgram& q, sh::Func fn) = nullptr;
  void (*after_call)(QcProgram& q, sh::Func fn) = nullptr;
  void* owner = nullptr;  // the game's, for `after_call` and its builtins
};

// The adapter over a loaded `progs` (which must outlive it) for a pool of `max_edicts`: the VM
// made, the hooks and optional fields read. False with `error` when the pool is past the VM's
// entity limit.
bool create_qc_program(QcProgram& q, const ge::progs::Progs& progs, int max_edicts,
                       std::string& error);
// The seam the server is created with (or assigned to `Server::program`).
sh::Program qc_program(QcProgram& q);

// A call from outside the seam (a hook with arguments): `enter_qc` mirrors the server in (the
// globals from `sv.g`), the caller sets parameters, `run_qc` runs (a fault counted and cleared;
// false then), the caller reads the return slot, `leave_qc` mirrors back and restores the server
// the adapter had before.
void enter_qc(QcProgram& q, sh::Server& sv);
bool run_qc(QcProgram& q, int32_t fn);
void leave_qc(QcProgram& q);

// Inside a builtin: every entity in use and the globals the loop reads, VM to loop (before the
// builtin reads or moves the world), and every entity's loop fields back into the VM (after it
// moved something; the globals are left as the running QC has them).
void flush_to_world(QcProgram& q);
void refresh_from_world(QcProgram& q);

// Inside a call (between enter_qc and leave_qc, or in a builtin): a new entity in the loop and
// cleared in the VM (-1 when the pool is full); an entity freed in the loop with the progs' free
// rule in the VM.
int qc_spawn_entity(QcProgram& q);
void qc_free_entity(QcProgram& q, int e);

// The seam's builtins, for the game's catalogue at the numbers it gives them.
void qc_spawn(ge::progs::Vm& vm);
void qc_remove(ge::progs::Vm& vm);
void qc_setorigin(ge::progs::Vm& vm);

// The adapter a builtin runs under.
inline QcProgram& qc_of(ge::progs::Vm& vm) { return *static_cast<QcProgram*>(vm.user); }
// The parameter's entity number, or -1 after faulting the call with "<builtin>: ...".
int entity_parm(ge::progs::Vm& vm, int i, const char* builtin);

}  // namespace shooters_edge::sim
