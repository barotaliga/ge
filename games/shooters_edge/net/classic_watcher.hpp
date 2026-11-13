// THE WATCHER (docs/tv.md § The watcher, M57h): a spectator that joins a classic server with no
// broadcaster (its stream port off, or a server that has none) and turns what it sees into a
// broadcast on the replay file for a TV node (engine/tv/node.hpp).
//
// It is our scripted classic client (classic_client.hpp) joined as a spectator, recording as a
// player's client recording does (classic_record.hpp's `ClientRecorder`, plain), and its recording
// handed to a `RecordingBroadcast` (client/recording_tv.hpp) as it grows: a block a second of the
// network's clock. What a viewer gets is what one spectator was sent: the watcher follows the play
// as a spectator does and does not see the whole map, as a multiview recording would.
//
//   watcher_start(w, transport, server, cfg, meta)   every frame: watcher_tick(w, now_us), then
//   tv_feed(node, recording_broadcast_fresh(w.cast), now_ms)       at the end: watcher_stop(w)
//
// The watcher ends when the client is dropped, or by `watcher_stop`; its broadcast then ends.
//
// ge:summary The watcher: a spectator on a server with no broadcaster, its view broadcast to TV
// ge:keywords watcher spectator tv broadcast no-broadcaster stream-port-off recording replay-v2
// ge:keywords watcher_start watcher_tick watcher_stop
// ge:skill net replay
// ge:test tests/shooters_edge_classic_watcher_test.cpp
#pragma once

#include <cstddef>
#include <cstdint>

#include "net/address.hpp"
#include "net/transport.hpp"
#include "replay/stream.hpp"
#include "shooters_edge/client/recording_tv.hpp"
#include "shooters_edge/net/classic_client.hpp"
#include "shooters_edge/net/classic_record.hpp"

namespace shooters_edge::net {

// Not moved once started: the client keeps a pointer to the recorder.
struct ClassicWatcher {
  ClassicClientEnd client;
  ClientRecorder recording;
  client::RecordingBroadcast cast;
  size_t fed = 0;  // of the recording's bytes, those handed to the broadcast
  bool ended = false;
};

// Joins `server` as a spectator (`cfg.spectator` is set) and begins the broadcast.
void watcher_start(ClassicWatcher& w, gn::Transport& t, const gn::Address& server, ClientConfig cfg,
                   const ge::replay::Meta& meta = {}, int64_t now_us = 0);
// One client frame, then the recording's new bytes onto the broadcast; a dropped client ends it.
void watcher_tick(ClassicWatcher& w, int64_t now_us);
// The recording and the broadcast ended (the trailer written); the client is left as it is.
void watcher_stop(ClassicWatcher& w);

}  // namespace shooters_edge::net
