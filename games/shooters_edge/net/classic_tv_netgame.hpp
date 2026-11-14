// A RECORDING AS THE TV FOR SINGLE-PLAYER CLIENTS (docs/tv.md § Single-player clients at the TV):
// any classic recording (a single-player one as it is; a client or multiview recording converted
// to one, client/demo_convert.hpp) played as a netgame server on the single-player protocol
// (version 15), so the oldest single-player client and current ones that keep its protocol
// connect with `connect "host:26000"` and watch it as they would a netgame.
//
// THE TRANSPORT (porting/shooters-edge/netgame-transport.md): every datagram but a control one is
// an 8-byte big-endian header (flags and the whole length, then a sequence) and its payload. A
// control packet (the CTL bit, no sequence) asks to connect (the game's name, net version 3); the
// accept names this same port, so a viewer behind a NAT keeps the one hole it opened and viewers
// are told apart by their address. A reliable message goes in fragments of at most 1024 bytes, one
// in flight, each resent after a second until acknowledged; an unreliable message is one datagram
// of at most 1024 bytes.
//
// THE SIGNON: the recording's own. Each level's blocks are cut at its `signonnum` 1, 2 and 3: the
// first part (the server info, which the client loads the level from) goes as ONE reliable
// message, since anything else arriving while the client loads ends its connection; the second
// when it asks "prespawn"; the third at "spawn"; "begin" seats it in the play.
//
// THE PLAY: one clock for every viewer, the recording's own times (a silence over a second cut to
// one). Each due block is cut at its messages: the frame's own (the time, the client data, the
// entity updates, sounds, particles and effects) go unreliable, cut into datagrams of at most 1024
// bytes, with the recorded view's angles as a `setangle`, so the viewer sees what the recorder saw;
// the rest (prints, names, frags, light styles, stuffed text) join the reliable queue. A viewer
// that joins late misses nothing an update needs: every frame carries every entity in view as a
// delta from its baseline, which the signon holds.
//
// LEVELS AND THE LOOP: a new level in the recording, and the end (it starts again), send every
// viewer "reconnect" and then the new level's first part, as a level change on a netgame does.
//
// ge:summary A recording played as a netgame on the single-player protocol to its oldest clients
// ge:keywords tv netgame single-player protocol-15 old-clients connect accept reliable fragment
// ge:keywords signon prespawn spawn begin reconnect udp_tv_netgame_open udp_tv_netgame_frame
// ge:skill net shooter
// ge:test tests/shooters_edge_classic_tv_netgame_test.cpp
#pragma once

#include <array>
#include <cstdint>
#include <deque>
#include <string>
#include <vector>

#include "net/transport.hpp"

namespace shooters_edge::net {

namespace gn = ge::net;

// One block of the single-player recording: its messages' offsets and its due time.
struct NetgameBlock {
  uint32_t at = 0, size = 0;      // the message bytes in the recording
  std::array<float, 3> angles{};  // the recorded view
  std::vector<uint32_t> marks;    // each message's offset in the block
  int64_t due_us = 0;             // on the play clock, from the level's start
  int level = 0;
};
// A level: its first block, the blocks that end its signon parts (`signonnum` 1, 2, 3), and the
// first block played after them.
struct NetgameLevel {
  size_t first = 0, play = 0, end = 0;  // blocks [first, end)
  std::array<size_t, 3> signon{};       // the block holding `signonnum` n + 1
  std::array<uint32_t, 3> cut{};        // the offset in that block just past it
};

enum class NetgameStage : uint8_t { free, loading, prespawned, spawned, playing };

struct NetgameViewer {
  NetgameStage stage = NetgameStage::free;
  gn::Address addr;
  int64_t heard_us = 0, sent_us = 0, accepted_us = 0;
  uint64_t epoch = 0;  // the play epoch (level or round) its signon is for
  // The reliable side out: messages waiting, the one in flight and its next fragment.
  std::deque<std::vector<uint8_t>> queue;
  std::vector<uint8_t> flight;
  size_t flight_at = 0, frag = 0;  // the fragment in flight: its offset and its length
  bool waiting_ack = false;
  int64_t frag_sent_us = 0;
  uint32_t send_seq = 0, unrel_seq = 0;
  // The reliable side in.
  uint32_t recv_seq = 0, unrel_in = 0;
  std::vector<uint8_t> partial;
  bool sealed = false;  // the queue's last message must stay alone (a level's first part)
  std::string name;
};

struct NetgameTvConfig {
  std::string name = "Shooter's Edge TV";
  int max_viewers = 16;
  double drop_s = 30;  // a viewer silent this long is let go
};

struct NetgameTv {
  NetgameTvConfig cfg;
  gn::Transport* t = nullptr;
  std::vector<uint8_t> bytes;  // the single-player recording
  std::vector<NetgameBlock> blocks;
  std::vector<NetgameLevel> levels;
  bool loop = true, ended = false;
  // The play clock.
  int64_t start_us = -1;  // when the current level started playing
  size_t next = 0;        // the next block due
  int level = 0;
  uint64_t epoch = 1;  // raised at each level start and each time round
  std::vector<NetgameViewer> viewers;
  uint64_t frames = 0, rounds = 0;
};

// `bytes` (any recording open_demo takes) opened as single-player blocks: converted when it is a
// client or multiview recording, cut into its levels. False with `error` when it does not read or
// has no whole signon.
bool udp_tv_netgame_open(NetgameTv& tv, std::vector<uint8_t> bytes, bool loop, std::string& error);
// `tv` serving on `t` (bound by the caller).
void udp_tv_netgame_start(NetgameTv& tv, gn::Transport& t, const NetgameTvConfig& cfg);
// Every datagram waiting read, the blocks due by `now_us` sent, resends and keepalives.
void udp_tv_netgame_frame(NetgameTv& tv, int64_t now_us);
// Viewers connected (any stage).
int udp_tv_netgame_viewers(const NetgameTv& tv);
// Every viewer told the TV is going (a reliable `disconnect` each, sent once).
void udp_tv_netgame_stop(NetgameTv& tv, int64_t now_us);

}  // namespace shooters_edge::net
