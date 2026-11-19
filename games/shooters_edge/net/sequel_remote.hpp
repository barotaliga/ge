// THE WINDOW JOINED TO A SERVER OF THE SEQUEL: the `shooters_edge` window, with the sequel loaded,
// playing on a server of the sequel's protocol (`connect <host>[:port]`, port 27910 when none):
// a stock server of the sequel running its own game or a mod, or ours (`sesv -game 2`).
//
// HOW IT IS MADE: the protocol is the scripted sequel client's (sequel_client.hpp) driven by the
// window's hands, reading straight into the window's drawn recording (`Remote::demo`), so the
// window draws a game it plays as it draws a sequel recording it watches. Everything a frame of
// the window does not ask for is the classic join's (classic_remote.hpp): the tick clock, the
// console's `connect`, `disconnect`, `reconnect` and `netstatus`, the stages said on the console.
//
// THE HANDS: a move a tick with the window's view angles. The sequel's game draws the view as those
// angles plus the turn the server holds for the player (its delta angles: a spawn's facing, a
// teleport's), so the drawn view is the hands' plus that turn, never a tick behind. A weapon key
// (`impulse 1` .. `impulse 11`: the weapon groups and the quick weapon ask the same way) goes as
// the sequel's `use <weapon>` command, which a stock server takes and an impulse it does not.
//
// BETWEEN FRAMES: the server sends 10 frames a second; the other entities, the other players and
// the player himself are drawn between the last two frames read, by the window's clock against
// when each frame came (one frame behind the newest), so nothing steps at 10 Hz.
// THE EASY JUMP: with `cl_speedjump 1` the user info says so and the hands strafe for the player
// as on the first game's servers (shooter/speedjump_assist.hpp), off the newest prediction, by the
// sequel's air law at the air acceleration the server states; at the share our server tells
// (`*ge_ext` bit 16, svc 126; its `pm_speedjump`, 0 the aid off), else `kStockShare`.
//
// ge:summary The window joined to a server of the sequel: its client driven by the hands, drawn
// ge:summary as a sequel recording, its frames drawn in between
// ge:keywords remote-client connect join-server sequel protocol-34 sequel-join delta-angles
// ge:keywords use-weapon impulse lerp sequel_remote_start sequel_remote_step
// ge:keywords cl_speedjump easy-jump speedjump-share
// ge:skill net shooter
// ge:test tests/shooters_edge_sequel_net_test.cpp
#pragma once

#include <string>
#include <vector>

#include "shooters_edge/net/classic_remote.hpp"

namespace shooters_edge::net {

// The sequel's client started over `r.transport` to `r.server` with the player's settings.
void sequel_remote_start(Remote& r, const ClientConfig& cfg);
// One window frame of `wall_s` s (classic_remote.hpp's `remote_step` for a server of the sequel).
RemoteStep sequel_remote_step(Remote& r, double wall_s, std::vector<std::string>& said);

}  // namespace shooters_edge::net
