// A SCRIPTED SEQUEL CLIENT (protocol 34, sequel_net.hpp): the client half the sequel's net server's
// tests and the `se-sequel-net` lab drive, and the one a watcher or a bridge joins a live server
// with. It asks for a challenge, connects, follows the signon as the server stuffs it
// (`cmd configstrings ...`, `cmd baselines ...`, `precache <n>` answered with `begin <n>`), then
// sends a move every tick: the newest frame it read whole (so the server deltas from it) and its
// three newest commands from a script or a brain.
//
// WHAT IT READS: every in-band payload is one block of the sequel's own recording format, appended
// to `demo.bytes` with its length and read by the recording's reader (client/demo.hpp,
// `next_sequel`), so the client's state is `demo.state` and its recording is `demo.bytes` (end it
// with `sequel_client_stop`; a length of -1 closes it). A level change (`changing`, then
// `reconnect`) sends `new` again on the same connection; the server's reconnect message starts a
// fresh connect.
//
// Nothing reads a wall clock: `sequel_client_tick` takes the network's time, so a session replays.
//
// ge:summary A scripted sequel-protocol client: challenge, connect, signon, frames and moves
// ge:keywords sequel-client protocol-34 scripted-client signon getchallenge connect configstrings
// ge:keywords baselines precache begin move ack-frame spectator recording dm2 reconnect
// ge:keywords sequel_client_start sequel_client_tick sequel_client_stop sequel_client_command
// ge:skill net shooter
// ge:test tests/shooters_edge_sequel_net_test.cpp
#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "net/transport.hpp"
#include "shooters_edge/client/demo.hpp"
#include "shooters_edge/net/classic_channel.hpp"
#include "shooters_edge/net/classic_client.hpp"
#include "shooters_edge/net/sequel_net.hpp"

namespace shooters_edge::net {

struct SequelClientConfig {
  std::string name = "player";
  std::string skin = "male/grunt";
  int rate = 15000;
  bool spectator = false;
  std::string password;
  uint16_t qport = 1;
  int seed = 1;
  // The command at a tick (msec set by the client); null: stands still.
  std::function<sh::MoveCmd(int tick)> brain;
};

struct SequelClientEnd {  // NOLINT(clang-analyzer-optin.performance.Padding): one a client
  SequelClientConfig cfg;
  gn::Transport* transport = nullptr;
  gn::Address server;
  ClientStage stage = ClientStage::challenge;
  std::string refused;  // the server's reason when refused or dropped
  int32_t challenge = 0;
  Channel ch;
  double now = 0, last_ask = -1, last_tick = -1;
  int tick = 0;

  client::Demo demo;  // what the server sent, as the sequel's recording, read as it comes
  bool recording = true;
  std::vector<std::string> stuffed;  // every line stuffed, in order (the first 256)
  std::vector<std::string> prints;   // every print's text, in order (the first 256)
  int signon_steps = 0;              // signon commands answered
  int levels = 0;                    // server data read
  double connected_at = -1, active_at = -1;  // the network's times, s

  SequelCmd cmds[3];  // the three newest, oldest first
  int64_t frames = 0, moves = 0, datagrams_in = 0, bytes_in = 0, bytes_out = 0;
  std::string error;  // a payload it could not read
};

void sequel_client_start(SequelClientEnd& c, gn::Transport& t, const gn::Address& server,
                         const SequelClientConfig& cfg, double now);
// One client frame at the network's time: every datagram read, then one packet sent.
void sequel_client_tick(SequelClientEnd& c, int64_t now_us);
// Queues a string command on the reliable channel (`say hello`).
void sequel_client_command(SequelClientEnd& c, const std::string& line);
// Says goodbye (`disconnect`, three times) and closes the recording.
void sequel_client_stop(SequelClientEnd& c);

}  // namespace shooters_edge::net
