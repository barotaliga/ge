// OUR TV NODE'S SEQUEL FACE (docs/tv.md § The sequel's TV): a match of the sequel, as the blocks of
// a client recording (each one server packet's messages), served to unmodified clients of the
// sequel over its own protocol 34, so a player watches a match through our TV with the client
// they already have. The blocks come from wherever the caller takes them: a watcher of ours joined
// to the server as a spectator (net/sequel_client.hpp), through a TV node's delay (engine/tv) and
// back out of the replay file (client/sequel_tv.hpp).
//
// THE FACE KEEPS what a viewer needs to start: the server data, every configstring as it stands
// now, the baselines, and the blocks since the newest whole frame (a frame that deltas from none;
// the watcher asks for one every few seconds, `SequelClientConfig::whole_every`). Every stuffed
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
// ge:summary Our TV node's sequel face: a match's blocks served to unmodified sequel clients
// ge:keywords tv-node sequel-face spectate viewer protocol-34 signon whole-frame relay reliable
// ge:keywords sequel_tv_start sequel_tv_block sequel_tv_frame sequel_tv_stop
// ge:skill net shooter
// ge:test tests/shooters_edge_sequel_tv_face_test.cpp
#pragma once

#include <array>
#include <cstdint>
#include <deque>
#include <map>
#include <span>
#include <string>
#include <vector>

#include "net/transport.hpp"
#include "shooters_edge/client/demo.hpp"
#include "shooters_edge/net/classic_channel.hpp"

namespace shooters_edge::net {

namespace gn = ge::net;

struct SequelTvConfig {
  std::string name = "Shooter's Edge TV";
  int max_viewers = 32;
  std::string password;      // "": none asked
  double timeout = 30;       // s of silence before a viewer is dropped
  size_t most_behind = 600;  // blocks waiting for a viewer before it is dropped
};

enum class SequelViewerStage : uint8_t { free, signon, active };

struct SequelViewer {
  SequelViewerStage stage = SequelViewerStage::free;
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

struct SequelTvFace {
  SequelTvConfig cfg;
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
  std::array<SequelViewer, 64> viewers;
  std::map<uint64_t, int32_t> challenges;  // by host
  uint32_t next_challenge = 0x5eb1;
  // Counts.
  int64_t blocks = 0, whole_frames = 0, levels = 0, refused = 0, dropped = 0;
  int64_t downloads_refused = 0;  // files viewers asked for: the TV serves none
  int64_t bytes_out = 0;
  std::string error;             // a block that did not read (the face keeps serving the rest)
  std::vector<std::string> log;  // what it would print on its console (the first 256)
};

void sequel_tv_start(SequelTvFace& f, gn::Transport& t, const SequelTvConfig& cfg);
// One block of the source (its messages, the length taken off), in order.
void sequel_tv_block(SequelTvFace& f, std::span<const uint8_t> block);
// Reads every viewer's datagrams and sends what is due.
void sequel_tv_frame(SequelTvFace& f, int64_t now_us);
// Every viewer sent the disconnect.
void sequel_tv_stop(SequelTvFace& f);
int sequel_tv_viewers(const SequelTvFace& f);

}  // namespace shooters_edge::net
