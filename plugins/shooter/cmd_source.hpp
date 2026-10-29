// A COMMAND SOURCE: what fills a player slot that no client's packets fill (a bot). The server's
// host asks the source for one command each time the slot's command clock comes due, exactly as a
// person's client builds one (usercmd.hpp), and sends it to the loop in a packet like any other:
// the three newest commands, a sequence number, the slot's speed check skipped (a bot's clock is
// the server's own). The source READS the server and returns a command; it never writes the
// server, so it can do nothing a player cannot, and every change to the move or the game program
// is a change it plays under at once.
//
// The host sets the command's `msec` (the clock's length, passed in as `msec`) and puts its
// angles and axes on the protocol's grid (`quantize_angle`, `quantize_axis`), so a bot's command
// is a command a client could have sent. The impulse rides in the command.
//
// A plain context and one function pointer, in the style of the program seam (program.hpp), so a
// bot links against this plugin alone and not against the game's server.
//
// ge:summary The command-source seam: what fills a bot's slot, one command when its clock is due
// ge:keywords cmd-source command-source bot slot usercmd seam next-command read-only server
// ge:skill shooter bots
// ge:test tests/shooters_edge_host_test.cpp
#pragma once

#include "shooter/pmove.hpp"

namespace ge::shooter {

struct Server;

struct CmdSource {
  void* ctx = nullptr;
  // The slot's next command, `msec` long, given the server as it stands after the last frame.
  MoveCmd (*next)(void* ctx, const Server& sv, int slot, int msec) = nullptr;
};

}  // namespace ge::shooter
