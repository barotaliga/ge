// A LIVE BROADCAST PLAYED AS A GROWING DEMO (docs/hub.md § Watching in a browser; docs/tv.md § The
// watcher): a TV node's stream (engine/tv/node.hpp's `Watch`) turned into the recording the demo
// player reads, the web viewer's and the window's alike.
//
// THE STREAM is a replay file version 2 growing a block a second; each call unpacks the whole
// blocks it has not unpacked (demo_v2.hpp's `unpack_more`) onto the recording. The recording opens
// once `kLiveLead` blocks are in hand, so the player starts a little behind what has come and does
// not stall at once; after that each block grows the open demo. The player that runs out before
// the next block holds its last frame (`tv_live_hold`) until the broadcast ends.
//
// THE WINDOW'S END (`TvWatch`): a UDP socket and a TV client (engine/tv/net_link.hpp) joined to
// the node, `se -tv <host:port>` (the hub's `<game>://tv/<node>/<stream>` link through the CLI).
//
//   TvWatch w{node};   every frame: tv_watch_feed(w, t, now_us, why) (or tv_watch_start, then
//   tv_watch_frame), then the paced player's step, held: tv_live_hold(w.live, w.client.watch, ...)
//
// ge:summary A live TV broadcast played as a growing demo: the web viewer's and the window's
// ge:keywords tv live broadcast watch growing-demo unpack_more lead hold tv_watch tv-client
// ge:keywords se-tv open-in-the-game
// ge:skill net replay
// ge:test tests/shooters_edge_tv_live_test.cpp
#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "net/address.hpp"
#include "net/transport.hpp"
#include "shooters_edge/client/demo_v2.hpp"
#include "shooters_edge/client/timedemo.hpp"
#include "tv/net_link.hpp"
#include "tv/node.hpp"

namespace shooters_edge::client {

// Blocks in hand before a live broadcast starts playing (each one second of the match).
inline constexpr int64_t kLiveLead = 2;

struct TvLive {
  DemoUnpacker unpacker;
  std::vector<uint8_t> opening;  // the recording before it opens (`kLiveLead` blocks)
  bool open = false;             // the recording is the demo's and grows there
};

enum class TvLiveTake : uint8_t {
  waiting,  // not open yet: the header or the lead to come
  opened,   // opened this call: `t.demo` (and `t.start`) at its start, `t.active` set
  playing,  // open, grown by what came
  failed,   // `line` says why in a player's words, `error` the detail
};
// What `watch` holds that `live` has not taken, unpacked onto the demo (`t.demo` once open).
TvLiveTake tv_live_take(TvLive& live, const ge::tv::Watch& watch, Timedemo& t, std::string& line,
                        std::string& error);
// The paced player's step while live: an end before the broadcast ended holds the last frame (the
// next block has not come), so it is a frame; after the end it is the end.
TimedemoStep tv_live_hold(const TvLive& live, const ge::tv::Watch& watch, TimedemoStep step,
                          Timedemo& t);

// The window's end of a broadcast: `node` names it; the link opens on the first feed.
struct TvWatch {
  explicit TvWatch(std::string at = {}) : node(std::move(at)) {}
  std::string node;  // host:port ("": none)
  ge::net::Transport socket;
  ge::tv::TvClient client;
  TvLive live;
  bool on = false;
};
// Joins the TV node at `node` ("host:port"; 27900 when no port). False with the reason when the
// address does not read or the socket does not open. `now_us` is the clock the frames go on with.
bool tv_watch_start(TvWatch& w, const std::string& node, int64_t now_us, std::string& error);
// One frame of the link, then what came onto the demo. `opened` hands the caller `t` to start
// playing (paced, heard, active).
TvLiveTake tv_watch_frame(TvWatch& w, Timedemo& t, int64_t now_us, std::string& line,
                          std::string& error);
// The window's whole frame of it: the link opened on the first call when `w.node` names one, the
// frame taken, the demo made paced, heard and smooth, named the broadcast. False with the player's
// words in `why` when the broadcast cannot be watched (the window ends the demo with them); true
// with nothing to do when no broadcast is asked for or after it stopped.
bool tv_watch_feed(TvWatch& w, Timedemo& t, int64_t now_us, std::string& why);
// The link let go; a later feed does nothing.
void tv_watch_stop(TvWatch& w);

}  // namespace shooters_edge::client
