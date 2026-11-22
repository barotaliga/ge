// OUR TV NODE'S QUAKE II FACE (docs/tv.md § Quake II's TV): a match of Quake II, as the blocks of
// a client recording (each one server packet's messages), served to unmodified clients of the
// Quake II over its own protocol 34, so a player watches a match through our TV with the client
// they already have. The blocks come from wherever the caller takes them: a watcher of ours joined
// to the server as a spectator (net/q2_client.hpp), through a TV node's delay (engine/tv) and
// back out of the replay file (client/q2_tv.hpp).
//
// THE FACE KEEPS what a viewer needs to start: the server data, every configstring as it stands
// now, the baselines, and the blocks since the newest whole frame (a frame that deltas from none;
// the watcher asks for one every few seconds, `Q2ClientConfig::whole_every`). Every stuffed
// line is taken out of the blocks: they were the watcher's server's to its own client.
//
// A VIEWER asks for a challenge and connects as to a server (`connect 34 <qport> <challenge>
// "<userinfo>"`, the password in `password` when one is set; a refusal is a print a player reads).
// On `new` it is sent the signon (the server data, the configstrings, the baselines, then
// `precache`), a unit of about 1 KB at a time, each but the last ending with a prompt the client
// echoes (`cmd configstrings <count> <n>`) to ask for the next; on `begin` it is sent the blocks
// from the newest whole frame and then every block as it comes. Everything a viewer is sent goes on
// the reliable channel, in order: the frames delta against the ones before them, so a viewer must
// hold every one, and a lost packet is resent rather than leaving the viewer on a frame it cannot
// read. The server data says the stream is a recording played (attract loop 1), so the viewer's
// client neither predicts nor moves. What a viewer sends besides is ignored; a viewer silent for
// `timeout` seconds, or that falls behind by more than `most_behind` blocks, is dropped.
//
// A NEW LEVEL (the server data again, after a `reconnect`): the face starts over and every viewer
// is sent `reconnect`, so its client asks for the new signon.
//
// OUT OF BAND it answers `getchallenge`, `connect`, `info 34`, `status` (the node's name, the
// source's map, its viewers) and `ping`.
//
// ge:summary Our TV node's Quake II face: a match's blocks served to unmodified Quake II clients
// ge:keywords tv-node q2-face spectate viewer protocol-34 signon whole-frame relay reliable
// ge:keywords q2_tv_start q2_tv_block q2_tv_frame q2_tv_stop
// ge:skill net shooter
// ge:test tests/shooters_edge_q2_tv_face_test.cpp
#pragma once

#include <array>
#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <span>
#include <string>
#include <vector>

#include "net/transport.hpp"
#include "shooters_edge/client/demo.hpp"
#include "shooters_edge/net/protocol.hpp"

namespace shooters_edge::net {

namespace gn = ge::net;

struct Q2TvConfig {
  std::string name = "Shooter's Edge TV";
  int max_viewers = 32;
  std::string password;      // "": none asked
  double timeout = 30;       // s of silence before a viewer is dropped
  size_t most_behind = 600;  // blocks waiting for a viewer before it is dropped
  // The server data's attract loop: 1 a recording played (the client neither predicts nor takes
  // keys), 0 a game (a relay's viewers type their words; their views are frozen instead).
  uint8_t attract_loop = 1;
};

enum class Q2ViewerStage : uint8_t { free, signon, active };

struct Q2Viewer {
  Q2ViewerStage stage = Q2ViewerStage::free;
  gn::Address address;
  uint16_t qport = 0;
  std::string name;
  Channel ch;
  double heard = 0, sent = 0;
  std::deque<std::vector<uint8_t>> queue;  // reliable messages waiting, in order
  int64_t blocks = 0;                      // blocks sent
  bool asked = false;                      // it said `new`
  bool signed_on = false;                  // it was sent this level's signon
  int go = 0;  // in its signon: units it may be sent now (one an echoed prompt)
};

struct Q2TvFace {
  Q2TvConfig cfg;
  gn::Transport* transport = nullptr;
  double now = 0;
  client::Demo reader;  // walks the source's blocks
  std::vector<client::DemoSpan> spans;
  // What a viewer needs to start.
  std::vector<uint8_t> server_data;  // the message, its id first; empty: no level yet
  int32_t count = 0;                 // the server data's count
  std::string map;
  std::map<int, std::vector<uint8_t>> strings;    // each configstring's message, by index
  std::vector<std::vector<uint8_t>> baselines;    // each baseline's message, in order
  std::vector<std::vector<uint8_t>> since_whole;  // the blocks from the newest whole frame
  std::array<Q2Viewer, 64> viewers;
  std::map<uint64_t, int32_t> challenges;  // by host
  uint32_t next_challenge = 0x5eb1;
  // Counts.
  int64_t blocks = 0, whole_frames = 0, levels = 0, refused = 0, dropped = 0;
  int64_t downloads_refused = 0;  // files viewers asked for: the TV serves none
  int64_t bytes_out = 0;
  std::string error;             // a block that did not read (the face keeps serving the rest)
  std::vector<std::string> log;  // what it would print on its console (the first 256)
  // A RELAY'S HOOKS (q2_mvd_relay.hpp): a viewer's command the face does not know itself (null:
  // ignored), and a viewer begun (it now has the frames).
  std::function<void(Q2Viewer&, const std::string& line)> on_command;
  std::function<void(Q2Viewer&)> on_begin;
};

void q2_tv_start(Q2TvFace& f, gn::Transport& t, const Q2TvConfig& cfg);
// One block of the source (its messages, the length taken off), in order.
void q2_tv_block(Q2TvFace& f, std::span<const uint8_t> block);
// Reads every viewer's datagrams and sends what is due.
void q2_tv_frame(Q2TvFace& f, int64_t now_us);
// Every viewer sent the disconnect.
void q2_tv_stop(Q2TvFace& f);
int q2_tv_viewers(const Q2TvFace& f);
// A message queued to one viewer alone, after what it was sent already (a print, a layout).
void q2_tv_send(Q2Viewer& v, std::span<const uint8_t> msg);
// Viewer `v` of `from` moved to `to` (a relay's channel, or another player's view), as a server
// changing levels moves a client: its connection kept, told `changing` and `reconnect`, so its
// client asks `to` for the signon. False when `to` is full (the viewer stays).
bool q2_tv_move(Q2TvFace& from, Q2TvFace& to, Q2Viewer& v);
// The viewer at `host` with `qport`; null when none is.
Q2Viewer* q2_tv_find(Q2TvFace& f, const gn::Address& host, uint16_t qport);

}  // namespace shooters_edge::net
