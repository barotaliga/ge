// A PLAYER ON THE SERVER: how a client's packet becomes moves. A player is moved as soon as its
// packet is read, by its own commands' lengths; everything else moves in the physics frame.
//
// A PACKET (`run_packet`) carries the three newest commands (oldest first), so one lost packet
// loses no input. With `lost` the packets the client sent that never arrived (off the sequence
// numbers): the per-packet "already touched" set is cleared (an entity touches a player at most
// once a packet); when fewer than 20 were lost, the client's last command (its buttons cleared) is
// repeated for each lost packet beyond two, then the oldest of the three runs when two or more
// were lost and the middle one when one or more were; then the newest; then the post-think; and
// the newest is kept as the client's last command with its buttons cleared (lag never fires twice).
//
// A COMMAND (`run_player_command`): first THE SPEED CHECK (`speed_check`, not for bots): with
// `elapsed` the whole milliseconds of server time since the last check (rounded to nearest), a
// command longer than `elapsed` is paid from a banked surplus, or cut to `elapsed + bank` (the
// bank emptied) when that is not enough; a shorter one banks the difference; the bank stays within
// 0..500 ms. Then a command over 50 ms is split into halves (msec / 2, the second without its
// impulse, each split again while too long), which skip the check. Each piece:
//  1. buttons: bit 1 -> button0 (attack), bit 2 -> button2 (jump), bit 4 -> button1 (use); a
//     non-zero impulse is kept in `impulse` (a zero one does not clear it);
//  2. the pitch clamped to sv_minpitch..sv_maxpitch; unless `fixangle`, the view takes the
//     command's angles; a living player's model faces it: pitch -view pitch / 3, the view's yaw
//     (unless `fixangle`), roll 0;
//  3. dt = msec x 0.001, at most 0.1;
//  4. not a spectator: the pre-think (frametime dt, time the server's), with THE LANDING GUARD (a
//     player on the ground and falling whose pre-think set the vertical speed to exactly 0 without
//     touching the horizontal gets the vertical speed back), then the player's own think;
//  5. the move (pmove.hpp) from the entity's fields (the origin offset by its mins against the
//     move's box, the water jump time in `teleport_time`, gravity and max speed from the fields or
//     the defaults) over the move's solids (`build_player_world`);
//  6. the result back into the fields, the solid stood on as `groundentity`;
//  7. unless a `not` solid: relinked with trigger touches, then each solid the move touched that
//     has not touched this player in this packet and has a touch function runs it (self the
//     solid, other the player).
// THE POST-THINK after a packet: not a spectator, the post-think (time the server's) with the
// same landing guard, then the new missile; a spectator runs the spectator think instead.
//
// THE MOVE'S SOLIDS (`build_player_world`): the world, then every solid entity within 256 units of
// the player on every axis (by absolute boxes), but the player and what it owns, at most
// kMaxSolids in all, walking the area tree's solid lists: a node's list, then child 0 when the
// 512-unit box reaches above the split, then child 1 when it reaches below.
//
// ge:summary A player on the server: the packet, the speed check, the command, the move's solids
// ge:keywords client player packet usercmd command dropped lost speed-check speedcheck bank
// ge:keywords split pre-think post-think landing-guard touched fixangle pitch-clamp physents
// ge:keywords run_packet run_player_command speed_check build_player_world connect_client
// ge:skill shooter
// ge:test tests/shooter_client_test.cpp
#pragma once

#include <cstdint>

#include "shooter/server.hpp"

namespace ge::shooter {

constexpr int kSpeedBankMaxMs = 500;
constexpr int kMaxRepeatLost = 20;  // more lost packets than this replay nothing lost
constexpr float kSolidsReach = 256;  // the move's solids lie within this of the player, units

// A client's movement packet: its sequence number and its three newest commands, oldest first.
struct Packet {
  int client = 0;  // the slot
  uint32_t sequence = 0;
  MoveCmd cmds[3];
};

// A client takes a player slot (entity slot + 1): the program's new parms, connect and
// put-in-server hooks (a spectator's connect hook instead).
void connect_client(Server& sv, int slot, bool spectator, bool bot);
void disconnect_client(Server& sv, int slot);

// The command length the speed check lets through, and the bank it leaves.
int speed_check(ClientSlot& c, double now, int msec);

// Runs one packet as the server reads it; returns how many packets were lost before it (-1 when
// the packet is older than one already read and is ignored).
int run_packet(Server& sv, const Packet& p);

// Runs one command for a player (the speed check unless `checked` is false, then the split).
void run_player_command(Server& sv, int slot, const MoveCmd& cmd, bool checked = true);

// The move's list of solids around `origin` for player entity `player`; `ents[i]` is the entity
// of solid i.
void build_player_world(const Server& sv, int player, Vec3f origin, MoveWorld& world, int* ents);

// The player move's type for an entity's movetype and health.
MoveType move_type_of(const Edict& ed, bool old_spectator);

}  // namespace ge::shooter
