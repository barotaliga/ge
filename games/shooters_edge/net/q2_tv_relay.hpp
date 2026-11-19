// OUR TV FOR QUAKE II, END TO END (docs/tv.md § Quake II's TV): a watcher of ours joins a
// server of Quake II (ours or an unmodified one) as a spectator; its recording is broadcast as a
// replay file version 2 (client/q2_tv.hpp) into a TV node (engine/tv/node.hpp), which holds
// it back by the delay; the node's stream is read back into the recording's blocks and served by
// Quake II face (q2_tv_face.hpp) to unmodified clients of Quake II on the relay's port.
//
//   watcher (34) -> broadcast (replay v2) -> TV node (delay) -> face (34) -> viewers
//
// The watcher asks for a whole frame every `whole_every` seconds, so a viewer who joins late
// starts within that much of the delayed match. It is a camera: a spectator flying free that
// follows one player at a time from behind and above, 20 s each.
// The relay redials a server that dropped it after 5 s.
//
// ge:summary Our TV for Quake II: a watcher's recording through a TV node to stock viewers
// ge:keywords tv q2 watcher spectator broadcast tv-node delay relay stock-viewer protocol-34
// ge:keywords q2_tv_relay_open q2_tv_relay_update q2_tv_relay_close
// ge:skill net shooter
// ge:test tests/shooters_edge_q2_tv_face_test.cpp
#pragma once

#include <memory>
#include <string>
#include <vector>

#include "net/transport.hpp"
#include "shooters_edge/client/demo_v2.hpp"
#include "shooters_edge/client/q2_tv.hpp"
#include "shooters_edge/net/q2_client.hpp"
#include "shooters_edge/net/q2_tv_face.hpp"
#include "tv/node.hpp"

namespace shooters_edge::net {

struct Q2TvRelayConfig {
  Q2TvConfig face;              // the viewers' side: the name, the password, the limits
  std::string watcher = "tv";   // the watcher's name on the server
  std::string server_password;  // the server's, when it asks one
  double delay_s = 7;           // the match held back this long
  double whole_every = 2;       // s between whole frames asked of the server
};

struct Q2TvRelay {
  Q2TvRelayConfig cfg;
  gn::Address server;
  gn::Transport* watcher_port = nullptr;  // the caller's, to the server
  gn::Transport* face_port = nullptr;     // the caller's, the viewers'
  Q2ClientEnd watcher;
  std::unique_ptr<client::Q2Broadcast> broadcast;
  ge::tv::Node node;
  ge::tv::Watch watch;
  client::DemoUnpacker unpacker;
  std::vector<uint8_t> blocks;  // the delayed recording's bytes, unpacked, not yet served
  size_t taken = 0;             // of the watcher's recording, the bytes broadcast
  size_t served = 0;            // of `blocks`, the bytes handed to the face
  Q2TvFace face;
  double retry_at = 0;
  int64_t dials = 0;
  int target = 0;          // the camera's player (its entity number; 0: none yet)
  int target_since = 0;    // the watcher's tick it was taken
  ge::shooter::Vec3f aim;  // the camera's move angles
  std::string error;       // the last thing that went wrong (the relay keeps going)
  std::vector<std::string> log;
};

// The relay started: the watcher dials `server` on `watcher_port`, the face serves on
// `face_port`. `now_us` is the caller's clock.
void q2_tv_relay_open(Q2TvRelay& r, gn::Transport& watcher_port, gn::Transport& face_port,
                      const gn::Address& server, const Q2TvRelayConfig& cfg, int64_t now_us);
// Every millisecond or so: the watcher, the broadcast, the node, the face.
void q2_tv_relay_update(Q2TvRelay& r, int64_t now_us);
void q2_tv_relay_close(Q2TvRelay& r);

}  // namespace shooters_edge::net
