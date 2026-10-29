// THE SERVER'S FRAME ON THE RUN CLOCK. The classic server runs frames as often as it can off the
// wall clock; this one runs one frame per step of the run clock (engine/loop/run_clock.hpp), a
// step of sv_mintic (0.013 s by default), so a server's run is a pure function of its map, its
// settings and the commands it received in order, and replays to the bit from its input log.
//
// One frame (`server_frame`), in order:
//  1. the server's time advances by one step (`start_time + frame x sv_mintic`, multiplied out so
//     no sum drifts);
//  2. the packets that arrived since the last frame run, IN THE ORDER GIVEN (a live server hands
//     them in arrival order, a replay in its log's order): each player is moved at once by its
//     packet's commands (client.hpp);
//  3. physics (physics.hpp) with frametime the step (the map's first physics frame: sv_maxtic,
//     as the classic first frame is capped there).
// Sending is the protocol's (M18g): what a client is sent is read off the server after the frame.
// The frame's sounds (`Server::sounds`) are the ones it started; the next frame clears them.
//
// ge:summary The server's frame on the run clock: time, the packets in order, physics
// ge:keywords server-frame frame run-clock mintic maxtic packets order deterministic replay
// ge:keywords server_frame step-length
// ge:skill shooter
// ge:test tests/shooter_client_test.cpp
#pragma once

#include "shooter/client.hpp"

namespace ge::shooter {

// One server frame: the step, the packets in order, physics.
void server_frame(Server& sv, const Packet* packets, int count);

}  // namespace ge::shooter
