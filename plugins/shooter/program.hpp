// THE GAME PROGRAM: the rules of a match (weapons, items, doors, scoring, what a touch does) are
// not the loop's. The loop calls the program at fixed points with its globals set, and the program
// reads and writes the entities' fields (entity.hpp) and the globals. One seam serves a native
// C++ program now and a bytecode one later: every hook, think, touch and blocked function is a
// FUNCTION NUMBER, and the program runs function `fn` when the loop calls `call(ctx, server, fn)`.
// A native program keeps a table of C++ functions by number; a bytecode program runs that
// function's code (mirroring the loop's entity fields into its own field memory around the call).
// A plain struct of a context and one function pointer, so the seam is a C ABI as well.
//
// When the loop calls, and with which globals (`self`, `other` entity numbers; `time` the
// program's float seconds; `frametime` the step's length):
//   start_frame          each physics frame, before any entity runs     self world, other world
//   player_pre_think     before each of a player's commands is moved    self the player
//   player_post_think    after all of a packet's commands are moved     self the player
//   an entity's think    when its nextthink comes due (physics.hpp)     self it, other world
//   an entity's touch    on contact (server.hpp: triggers, impacts)     self the toucher's pair
//   a pusher's blocked   when something stops it (push.hpp)            self the pusher, other it
//   client_connect, put_client_in_server, client_disconnect, client_kill, spectator_connect,
//   spectator_think, spectator_disconnect, set_new_parms, set_change_parms: at those events
// The loop reads back `force_retouch` (frames left to re-run every entity's trigger touches) and
// `newmis` (an entity the program just spawned, run 0.05 s ahead: physics.hpp).
//
// ge:summary The game-program seam: the hooks by function number, the globals the loop sets
// ge:keywords program game-program progs hooks globals self other time frametime newmis
// ge:keywords force_retouch start_frame player_pre_think player_post_think think touch blocked
// ge:keywords client_connect put_client_in_server spectator native bytecode function-number
// ge:skill shooter
// ge:test tests/shooter_server_test.cpp
#pragma once

#include "shooter/entity.hpp"

namespace ge::shooter {

struct Server;

// The program's globals the loop sets and reads.
struct Globals {
  float time = 0;       // the server's time as the program's float, s
  float frametime = 0;  // the current step's length, s
  int self = 0, other = 0;
  float force_retouch = 0;  // frames left to relink every entity with its trigger touches
  int newmis = 0;           // an entity just spawned to be run 0.05 s ahead; 0 none
};

struct Program {
  void* ctx = nullptr;
  // Runs program function `fn` (never kNoFunc) with the server's globals as set.
  void (*call)(void* ctx, Server& sv, Func fn) = nullptr;
  // The hooks' function numbers; kNoFunc where the program has none.
  Func start_frame = kNoFunc;
  Func player_pre_think = kNoFunc;
  Func player_post_think = kNoFunc;
  Func client_connect = kNoFunc;
  Func put_client_in_server = kNoFunc;
  Func client_disconnect = kNoFunc;
  Func client_kill = kNoFunc;
  Func spectator_connect = kNoFunc;
  Func spectator_think = kNoFunc;
  Func spectator_disconnect = kNoFunc;
  Func set_new_parms = kNoFunc;
  Func set_change_parms = kNoFunc;
};

}  // namespace ge::shooter
