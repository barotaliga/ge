// THE HIGH-LAG TELEPORT of the classic protocol family: the moves a player made before he saw the
// server turn him (a teleport's far end, a respawn), made right for where he faces now, on the
// server that runs them and on the client that predicts them, so neither walks him the old way for
// a round trip and the two agree.
//
// The server turns a player with a `setangle` in the next unreliable datagram; under the
// extension (QuakeWorld's MVD bit 2, `cl_pext_lagteleport`) it carries a KIND: 1 a teleport, 2 a
// respawn. The commands the client sent before that packet reached it still hold the old view.
//
// - A TELEPORT keeps each such command's angles and turns its forward and side move together, a
//   vector about the vertical axis, by the turn (`turn_move`): under the old view the turned move
//   pushes the player the way the untouched move would under the new one. The up move and the
//   pitch are left; the move's numbers are whole, their fractions dropped toward zero.
// - A RESPAWN takes the new yaw into each such command and leaves its move.
//
// THE SERVER (`LagTurn`, `lag_turn_sent`, `lag_turn_moves`): when it writes the setangle it keeps
// the outgoing sequence of the packet that carries it, its own incoming sequence then, and the
// turn: the new yaw less the yaw of the last command it had from him. Until a packet comes back
// acknowledging that packet or a later one, every move packet's commands are turned before they
// run: the newest always, the one before it when its sequence (the packet's less 1) is past the
// incoming sequence kept, the oldest when the packet's less 2 is (a command already run before
// the turn is never touched; the older two run only after a loss). The packet that acknowledges
// it ends the turn, untouched. The server's copy of the last command is turned too.
//
// THE CLIENT (`lag_turn_client`): on a teleport's setangle it turns the commands sent and not yet
// acknowledged by the new yaw less the yaw of the command the packet acknowledges, and keeps the
// turning the player did since: his view's yaw is his view's as it stands plus the turn (the
// pitch and roll the server's). A respawn takes the server's angles into those commands and the
// view.
//
// ge:summary The high-lag teleport: moves made before a server's turn arrived, turned to match it
// ge:keywords lag-teleport high-lag-teleport cl_pext_lagteleport setangle teleport turn_move
// ge:keywords lag_turn_moves lag_turn_client lag_turn_sent in-flight-commands fixangle kind respawn
// ge:skill classic net shooter
// ge:test tests/classic_lag_turn_test.cpp
#pragma once

#include <cstdint>

#include "shooter/pmove.hpp"

namespace ge::classic::protocol {

// A setangle's kind byte under the extension.
enum class TurnKind : uint8_t { unknown = 0, teleport = 1, respawn = 2 };

// `c`'s forward and side move turned by `yaw_deg` (the new view's yaw less the command's), whole.
void turn_move(shooter::MoveCmd& c, float yaw_deg);

// One command turned for a turn of `kind`: a teleport's move turned by `yaw_deg`; a respawn's yaw
// set to `new_yaw`.
void turn_command(shooter::MoveCmd& c, TurnKind kind, float yaw_deg, float new_yaw);

// The server's side of one client's turn in flight.
struct LagTurn {
  bool on = false;
  TurnKind kind = TurnKind::teleport;
  uint32_t out_seq = 0;  // the outgoing sequence of the packet that carries the setangle
  uint32_t in_seq = 0;   // the server's incoming sequence from him when it was written
  float yaw = 0;         // the turn, degrees: the new yaw less his last command's
  float new_yaw = 0;     // the yaw he was turned to
};

// The setangle written to packet `out_seq` (the client's last incoming sequence `in_seq`), turning
// him from `last_yaw` (his last command's) to `new_yaw`; `last` (the server's copy of his last
// command, replayed after a loss) turned at once.
void lag_turn_sent(LagTurn& t, TurnKind kind, uint32_t out_seq, uint32_t in_seq, float last_yaw,
                   float new_yaw, shooter::MoveCmd* last);

// A move packet of sequence `seq` acknowledging `ack`: its three commands (oldest first, as the
// packet carries them) turned while `t` is in flight; the turn ended once `ack` reaches it. The
// commands turned are counted in the return.
int lag_turn_moves(LagTurn& t, uint32_t seq, uint32_t ack, shooter::MoveCmd cmds[3]);

// The client's side: a setangle of `kind` to `angles` read in a packet acknowledging `acked`, the
// commands `sent` (by sequence & 63, `sent_seq` the sequence each holds) from `acked + 1` up to
// `next` (the next outgoing sequence) turned; `view`, the player's view as it stands, becomes the
// view to show after it.
void lag_turn_client(TurnKind kind, const shooter::Vec3f& angles, uint32_t acked, uint32_t next,
                     shooter::MoveCmd sent[64], const uint32_t sent_seq[64], shooter::Vec3f& view);

}  // namespace ge::classic::protocol
