// A LOADED PROGS DRIVES THE LOOP: the adapter between the shooter loop's program seam
// (plugins/shooter/program.hpp) and the QC VM (plugins/progs). The loop calls a function number;
// the adapter mirrors the loop's entity fields and globals into the VM's memory, runs that QC
// function, and mirrors them back, so a mod's StartFrame, PlayerPreThink, PlayerPostThink and its
// entities' think, touch and blocked functions run the match.
//
// - HOOKS come from the hook globals the program stored (StartFrame 81 ... SetChangeParms 89, read
//   once when the adapter is made) and the optional ones by name (SpectatorConnect,
//   SpectatorThink, SpectatorDisconnect). A function number of the loop IS a QC function index.
// - THE MIRROR: before each call every entity in use has its loop fields (origin, velocity,
//   angles, the box, movetype, solid, flags, the functions, nextthink, health, the buttons, ...)
//   written into its field words, and `self`, `other`, `time`, `frametime`, `newmis`,
//   `force_retouch` into the globals; after it, the same are read back. Ints the loop keeps are
//   QC floats; entities are entity values (number x entityfields x 4); functions are raw words.
//   The program's own fields live only in the VM and survive between calls.
// - THE SEAM'S BUILTINS (`spawn`, `remove`, `setorigin`) act on the loop at once: a spawned entity
//   is the loop's (`spawn_edict`), cleared in the VM; a removed one is freed in the loop and gets
//   the progs' free rule in the VM (its model, takedamage, modelindex, colormap, skin, frame,
//   health, classname, origin, angles and solid cleared, nextthink -1; its other fields kept).
//   Their numbers are the game's choice (the numbered catalogue is M18p's).
// - A FAULT ends the QC call (the VM unwinds), is counted with its text kept (`faults`,
//   `last_fault`), and the match goes on: what was written before it stays.
//
// ge:summary The adapter that lets a loaded progs drive the shooter loop's program seam
// ge:keywords qc progs adapter program-seam mirror fields globals hooks think touch blocked
// ge:keywords start_frame player_pre_think player_post_think spawn remove setorigin free-rule
// ge:keywords qc_program create_qc_program install_seam_builtins
// ge:skill progs shooter
// ge:test tests/shooters_edge_qc_program_test.cpp
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "progs/vm.hpp"
#include "shooter/program.hpp"
#include "shooter/server.hpp"

namespace shooters_edge::sim {

namespace sh = ge::shooter;

struct QcProgram {
  ge::progs::Vm vm;
  sh::Server* sv = nullptr;   // the server of the call running now (null between calls)
  std::vector<uint8_t> live;  // by entity number: in use in the VM's mirror
  int field_gravity = -1, field_maxspeed = -1;  // optional fields found by name; -1 absent
  sh::Program hooks;                            // the hook numbers, filled at creation
  int64_t calls = 0;
  int64_t faults = 0;
  std::string last_fault;  // the newest fault's text with its QC stack
};

// The adapter over a loaded `progs` (which must outlive it) for a pool of `max_edicts`: the VM
// made, the hooks read. False with `error` when the pool is past the VM's entity limit.
bool create_qc_program(QcProgram& q, const ge::progs::Progs& progs, int max_edicts,
                       std::string& error);
// The seam the server is created with (or assigned to `Server::program`).
sh::Program qc_program(QcProgram& q);
// The builtins the seam needs, at the numbers the game gives (0 leaves one out).
void install_seam_builtins(QcProgram& q, int spawn, int remove, int setorigin);

}  // namespace shooters_edge::sim
