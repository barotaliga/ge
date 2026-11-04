// THE WINDOW JOINED TO A SERVER: the `shooters_edge` window playing on a server somewhere else over
// the classic protocol (`connect <host>[:port]` in the console), as a client of it rather than the
// player of its own listen server.
//
// HOW IT IS MADE: the protocol is the classic client's (classic_client.hpp: the challenge, the
// connect, the signon, the moves, the downloads), driven by the window's hands in place of a
// script; and what the server sends is read as a client recording of it (client/demo.hpp), so the
// window draws a game it plays exactly as it draws one it watches: the map, the entities, the
// other players, the status, the sounds and the printed lines all come off the `DemoState` the
// recording's reader keeps. The recording is the client's own (classic_record.hpp's
// `ClientRecorder` on its `record`), fed to the reader as it grows and dropped as it is read.
//
// THE CLOCK: the client sends one move a tick, 77 a second (13 ms, the server's frame), so the
// window's wall time is turned into the ticks that are due (at most `kMostTicks` a call, the rest
// dropped, so a stall never floods the server). The network's time is that tick clock: nothing
// here reads a wall clock, so a session over the simulated network replays.
//
// THE VIEW is the client's own (`Remote::view`, degrees as the protocol's: pitch down positive,
// yaw counter-clockwise from +x), turned by the window's mouse every frame and sent with every
// move; the drawn view takes it every frame, so the mouse is never a tick behind. When the server
// turns the player (a spawn, a teleport: `setangle`) the view takes the server's angles.
//
// NOT YET (M18h): the player's own moves are not predicted, so what he sees of himself is a round
// trip late (plugins/shooter/predict.hpp is the next step); the other players are drawn where the
// last frame put them.
//
// ge:summary The window joined to a server: the classic client driven by the hands, drawn as a demo
// ge:keywords remote-client connect disconnect join-server classic-client live-demo network-play
// ge:keywords remote_connect remote_step remote_disconnect setangle tick-clock reconnect
// ge:keywords remote_hands add_remote_commands netstatus
// ge:skill net shooter
// ge:test tests/shooters_edge_remote_test.cpp
#pragma once

#include <memory>
#include <string>
#include <vector>

#include "console/console.hpp"
#include "input/state.hpp"
#include "net/transport.hpp"
#include "shooters_edge/client/controls.hpp"
#include "shooters_edge/client/demo.hpp"
#include "shooters_edge/net/classic_client.hpp"
#include "shooters_edge/net/classic_record.hpp"

namespace shooters_edge::net {

inline constexpr int kTickUs = 13000;  // a move's length: the server's frame
inline constexpr int kMostTicks = 8;   // ticks a call may run; a longer stall drops the rest

enum class RemoteStep : uint8_t {
  waiting,    // nothing new to draw yet (connecting, or no packet since the last call)
  frame,      // draw `demo.state`
  new_level,  // load `demo.state.map`, then draw
  dropped,    // the server refused or dropped the client: `Remote::why`
};

struct Remote {
  bool active = false;
  std::string server_name;  // as asked ("host:port")
  gn::Address server;
  gn::Transport own;                   // remote_connect's UDP socket
  gn::Transport* transport = nullptr;  // what it talks over (`own`, or remote_start's)
  ClassicClientEnd end;
  ClientRecorder recording;
  client::Demo demo;      // the recording read back: what the window draws
  sh::Vec3f view;         // the player's angles, degrees (sent with every move)
  sh::MoveCmd hands;      // the window's command, sent at the next tick (an impulse held to it)
  int64_t now_us = 0;     // the tick clock
  double owed_s = 0;      // wall time not yet a tick
  int levels = 0;         // level starts drawn
  int64_t setangles = 0;  // the server's turns taken
  ClientStage said = ClientStage::challenge;  // the stage last reported
  std::string why;                            // why it dropped
};

// Joins `address` ("host", "host:port"; the classic server port when none) over a UDP socket of
// its own. False with the reason when the address or the socket does not open.
bool remote_connect(Remote& r, const std::string& address, const ClientConfig& cfg,
                    std::string& error);
// Joins over a transport already open (the simulated network's, in tests and labs).
void remote_start(Remote& r, gn::Transport& t, const gn::Address& server, const ClientConfig& cfg);
// Leaves: the server told (a disconnect, three times), the socket closed, the state cleared.
void remote_disconnect(Remote& r);
// One window frame of `wall_s` s: the ticks due run (each sending `r.hands` with `r.view`), every
// packet read, the drawn state brought up to the newest. `said` takes the stages reached; what
// every frame read carried (prints, sounds started and stopped, voice, a centre message) is kept
// together in `demo.state` for the window to hear and show, as a played demo's one frame's is. An
// impulse in `r.hands` is cleared once sent.
RemoteStep remote_step(Remote& r, double wall_s, std::vector<std::string>& said);
// The stage, as the console says it ("connecting to host:27500", "playing on host:27500").
std::string remote_status(const Remote& r);

// The window's hands this frame: the mouse and the turn keys turn `r.view`, the moves make the
// command the next tick sends (an impulse not yet sent is kept until one is).
void remote_hands(Remote& r, const client::Moves& m, const ge::input::InputState& in,
                  const client::ControlSettings& c, double dt);

// `connect <host>[:port]`, `disconnect`, `reconnect` (the last server again) and `netstatus`; the
// player's `name`, `topcolor` and `bottomcolor` cvars are read when a connect is made.
void add_remote_commands(ge::console::Console& con, Remote& r);

}  // namespace shooters_edge::net
