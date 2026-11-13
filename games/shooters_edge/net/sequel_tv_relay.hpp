// OUR TV FOR THE SEQUEL, END TO END (docs/tv.md § The sequel's TV): a watcher of ours joins a
// server of the sequel (ours or an unmodified one) as a spectator; its recording is broadcast as a
// replay file version 2 (client/sequel_tv.hpp) into a TV node (engine/tv/node.hpp), which holds
// it back by the delay; the node's stream is read back into the recording's blocks and served by
// the sequel face (sequel_tv_face.hpp) to unmodified clients of the sequel on the relay's port.
//
//   watcher (34) -> broadcast (replay v2) -> TV node (delay) -> face (34) -> viewers
//
// The watcher asks for a whole frame every `whole_every` seconds, so a viewer who joins late
// starts within that much of the delayed match. It is a camera: a spectator flying free that
// follows one player at a time from behind and above, 20 s each.
// The relay redials a server that dropped it after 5 s.
//
// ge:summary Our TV for the sequel: a watcher's recording through a TV node to stock viewers
// ge:keywords tv sequel watcher spectator broadcast tv-node delay relay stock-viewer protocol-34
// ge:keywords sequel_tv_relay_open sequel_tv_relay_update sequel_tv_relay_close
// ge:skill net shooter
// ge:test tests/shooters_edge_sequel_tv_face_test.cpp
#pragma once

#include <memory>
#include <string>
#include <vector>

#include "net/transport.hpp"
#include "shooters_edge/client/demo_v2.hpp"
#include "shooters_edge/client/sequel_tv.hpp"
#include "shooters_edge/net/sequel_client.hpp"
#include "shooters_edge/net/sequel_tv_face.hpp"
#include "tv/node.hpp"

namespace shooters_edge::net {

struct SequelTvRelayConfig {
  SequelTvConfig face;          // the viewers' side: the name, the password, the limits
  std::string watcher = "tv";   // the watcher's name on the server
  std::string server_password;  // the server's, when it asks one
  double delay_s = 7;           // the match held back this long
  double whole_every = 2;       // s between whole frames asked of the server
};

struct SequelTvRelay {
  SequelTvRelayConfig cfg;
  gn::Address server;
  gn::Transport* watcher_port = nullptr;  // the caller's, to the server
  gn::Transport* face_port = nullptr;     // the caller's, the viewers'
  SequelClientEnd watcher;
  std::unique_ptr<client::SequelBroadcast> broadcast;
  ge::tv::Node node;
  ge::tv::Watch watch;
  client::DemoUnpacker unpacker;
  std::vector<uint8_t> blocks;  // the delayed recording's bytes, unpacked, not yet served
  size_t taken = 0;             // of the watcher's recording, the bytes broadcast
  size_t served = 0;            // of `blocks`, the bytes handed to the face
  SequelTvFace face;
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
void sequel_tv_relay_open(SequelTvRelay& r, gn::Transport& watcher_port, gn::Transport& face_port,
                          const gn::Address& server, const SequelTvRelayConfig& cfg,
                          int64_t now_us);
// Every millisecond or so: the watcher, the broadcast, the node, the face.
void sequel_tv_relay_update(SequelTvRelay& r, int64_t now_us);
void sequel_tv_relay_close(SequelTvRelay& r);

}  // namespace shooters_edge::net
