// A LIVE BROADCAST OF A SERVER ON THE ENGINE'S TV WIRE (docs/tv.md § The watcher; docs/hub.md §
// Live broadcasts): `setv --watch=<server>`. The watcher (qw_watcher.hpp) joins the server as
// a spectator and its view, a replay file growing a block a second, feeds a TV node
// (engine/tv/net_link.hpp's `TvHost`) that game windows join over UDP (`se -tv <node>`) and
// browsers over a WebSocket (`--web=<tcp port>`, the hub's Watch page). Listed on the masters as a
// TV node under the game's own protocol, its record names the server it watches (`source`), so the
// game's hub shows the broadcast's Watch on that server's card.
//
// THE DELAY holds every block back by `delay_s` of the match's own time, so no viewer can call
// positions to a player. A WATCHER DROPPED (the server restarted, refused it, or stopped) ends the
// broadcast; the node waits `retry_s` and starts a new one, its viewers let go to join again.
//
//   watch_tv_open(w, bind, server, cfg, error)   every frame: watch_tv_update(w, now_us)
//   (or `up` and `down` opened by the caller, then watch_tv_start(w, server, cfg, now_us))
//   watch_tv_close(w, now_us)                    says bye to the masters
//
// ge:summary setv --watch: a server's spectator view broadcast live on our TV wire and WebSocket
// ge:keywords watch-tv setv live broadcast tv-node watcher spectator hub watch-button source
// ge:keywords watch_tv_open watch_tv_update websocket master
// ge:skill net replay
// ge:test tests/shooters_edge_qw_watch_tv_test.cpp
#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "net/address.hpp"
#include "net/transport.hpp"
#include "shooters_edge/net/qw_watcher.hpp"
#include "tv/net_link.hpp"
#include "tv/websocket.hpp"

namespace shooters_edge::net {

struct WatchTvConfig {
  std::string name = "Shooter's Edge TV";  // the node's name on the hub and in its viewers' chat
  std::string watcher = "tv";              // the spectator's name on the server
  std::string server_password;             // the server's spectator password, when it has one
  double delay_s = 0;                      // the match held back, s of its own time
  int max_viewers = 32;                    // game windows over UDP (browsers are counted apart)
  uint16_t web_port = 0;                   // the WebSocket viewers' TCP port (0: none)
  std::vector<gn::Address> masters;        // listed there as a TV node (none: unlisted)
  double retry_s = 5;                      // a dropped watcher joins again after this long
  uint64_t seed = 1;
};

struct WatchTv {
  WatchTvConfig config;
  gn::Address server;
  gn::Transport up;                    // the watcher's socket to the server
  gn::Transport down;                  // the TV node's: game windows, status asks, the masters
  std::unique_ptr<QwWatcher> watcher;  // never moved once started (its client points in)
  ge::tv::TvHost host;
  ge::tv::TvWeb web;
  bool has_web = false;
  int broadcasts = 0;            // started, counting the first
  int64_t retry_at_us = -1;      // when the next broadcast starts (-1: one is on)
  std::vector<std::string> log;  // what happened, in the operator's words, oldest first
};

// The node bound at `bind` (and the WebSocket's port on the same host), its watcher joining
// `server`. False with the reason when a socket does not open.
bool watch_tv_open(WatchTv& w, const gn::Address& bind, const gn::Address& server,
                   const WatchTvConfig& config, std::string& error);
// The same on `w.up` and `w.down` the caller opened (the simulated network's in a test), and
// `w.web` when `w.has_web`.
void watch_tv_start(WatchTv& w, const gn::Address& server, const WatchTvConfig& config,
                    int64_t now_us);
// One frame: the watcher, its new bytes onto the node, the node's viewers, the masters.
void watch_tv_update(WatchTv& w, int64_t now_us);
// The node's line for the operator's status: the broadcast, its viewers, the bytes sent.
std::string watch_tv_status(const WatchTv& w);
// The masters told it is gone, the sockets closed.
void watch_tv_close(WatchTv& w, int64_t now_us);

}  // namespace shooters_edge::net
