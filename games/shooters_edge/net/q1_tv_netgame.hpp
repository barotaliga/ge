// A RECORDING AS THE TV FOR SINGLE-PLAYER CLIENTS (docs/tv.md § Single-player clients at the TV):
// any recording (a single-player one as it is; a client or multiview recording converted
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
// WHO A VIEWER FOLLOWS (a multiview recording): each viewer picks the player it watches. Fire
// follows the next player, jump the one before, a weapon key (impulse 1 to 8) the player of that
// number, and `cmd follow <name or number>` any of them. A player's view is a FEED: the recording
// converted to his view, made the first time someone asks for it (a pause of about a second on a
// long recording) and kept. Every feed plays on the first feed's clock and levels, so a viewer
// switching is sent "reconnect" and the new feed's signon and joins it where the match is now.
//
// ge:summary A recording played as a netgame on the single-player protocol to its oldest clients
// ge:keywords tv netgame single-player protocol-15 old-clients connect accept reliable fragment
// ge:keywords signon prespawn spawn begin reconnect udp_tv_netgame_open udp_tv_netgame_frame
// ge:skill net shooter
// ge:test tests/shooters_edge_q1_tv_netgame_test.cpp
#pragma once

#include <array>
#include <cstdint>
#include <deque>
#include <future>
#include <memory>
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

// One view of the recording as single-player blocks: the whole recording (a single-player one, or
// a multiview one converted to one player's view), its levels, and its place on the play clock.
struct NetgameFeed {
  int slot = -1;  // the multiview slot it rides (-1: the recording's own view)
  std::vector<uint8_t> bytes;
  std::vector<NetgameBlock> blocks;
  std::vector<NetgameLevel> levels;
  int64_t start_us = -1;  // when the current level started playing
  size_t next = 0;        // the next block due
  int level = 0;
  uint64_t epoch = 1;  // raised at each level start and each time round
};

// A player a viewer may follow: the multiview slot and the name.
struct NetgamePlayer {
  int slot = 0;
  std::string name;
};

struct NetgameViewer {
  NetgameStage stage = NetgameStage::free;
  size_t feed = 0;      // the feed it watches (NetgameTv::feeds)
  int want_slot = -1;   // the player it asked to follow, whose view is still being converted
  uint8_t buttons = 0;  // the last move's buttons, so a press is seen once
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
  // A view asked for the first time is converted on a thread of its own while the play goes on,
  // and the viewers who asked for it are moved when it is ready (a TV); false converts it at once,
  // holding the frame (a test or a lab, which wants the same steps every run).
  bool convert_in_background = false;
};

// A view being converted on its own thread: the player's slot, and the feed or the reason.
struct NetgameConverted {
  NetgameFeed feed;
  bool ok = false;
  std::string error;
};
struct NetgamePending {
  int slot = 0;
  std::future<NetgameConverted> done;
};

struct NetgameTv {
  NetgameTvConfig cfg;
  gn::Transport* t = nullptr;
  std::vector<NetgameFeed> feeds;  // [0]: the first view, whose clock and levels every feed keeps
  // A multiview recording, kept to convert more views from (shared with a conversion's thread).
  std::shared_ptr<const std::vector<uint8_t>> source;
  std::vector<NetgamePending> pending;  // views being converted
  std::vector<NetgamePlayer> players;   // who a viewer may follow (a multiview recording's players)
  bool loop = true, ended = false;
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
// Viewer `viewer` (an index in `viewers`) sent to follow player `player` (an index in
// `players`): the view converted when it is the first to ask, then "reconnect" and its signon.
// False with `error` when there is no such viewer or player or the view does not convert.
bool udp_tv_netgame_follow(NetgameTv& tv, size_t viewer, size_t player, int64_t now_us,
                           std::string& error);
// Who the viewers follow, for a status line: " (following Reppie, Jukka)", or "" when the
// recording has one view or no one watches.
std::string udp_tv_netgame_following(const NetgameTv& tv);
// Every viewer told the TV is going (a reliable `disconnect` each, sent once).
void udp_tv_netgame_stop(NetgameTv& tv, int64_t now_us);

}  // namespace shooters_edge::net
