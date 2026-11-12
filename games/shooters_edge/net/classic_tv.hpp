// OUR TV NODE'S CLASSIC FACE (docs/tv.md § The classic face): a relay that pulls one match from a
// classic server's stream port (classic_qtv.hpp is ours; any server that speaks the stream protocol
// will do) and serves it, delayed, to classic viewers and to other relays over the same protocol.
//
// UP: the node dials the server, asks for the match (`QTV`, `VERSION: 1`, `RECEIVE`, its name in
// `USERINFO`, every method it knows in `AUTH` when it has a password), answers a challenge with
// the whole request again and the response, and reads the records after `BEGIN`. A connection
// that closes, or never begins, is dialled again after 2 s, doubling to an hour, the wait reset
// once a level has come through; a `PERROR` (a wrong password, a version) is not retried.
//
// THE DELAY: each record is released `delay_s` after it arrived, in order, so a viewer watches
// the match that long after the players (what a tournament's broadcast asks for: no viewer can
// call positions to a player).
//
// DOWN: each viewer asks as a relay asks a server, with the node's own password; once let in it
// is sent `BEGIN` (unless `RAW`), then the catch-up the node writes from the match as it stands
// (classic_mv_state.hpp: the level's signon, what changed since, every player and entity), then
// every record as it is released. A viewer who comes before the first level waits for it.
//
// CHAT AND THE VIEWER LIST: a viewer's `say` and `say_team` stay on the node: every viewer is
// printed `#0:qtv_<say|say_team>:#<id>:<name>: <text>`. `say_game <text>` (or a say whose text
// starts with `say_game `) goes up to the server as `say_game "#<id>:<name>: <text>"`, which the
// server prints to its players and writes into the match. Each viewer let in goes up as `qul 1
// <id> "<name>"` and one who leaves as `qul 3 <id>`; the whole list goes up again whenever the
// server lets the node in. A viewer's name is its `USERINFO` name with its spaces trimmed: an
// empty one, `console`, or one holding `#` or `:` is `unnamed`, and a name another viewer has
// gets 8 letters in brackets after it, cut to 25.
//
// LIMITS: `max_viewers` at once (more are told so and closed), a handshake in `pending_s`, a
// viewer with more than its catch-up and `max_queue` waiting, or that takes nothing for
// `viewer_timeout_s`, is dropped.
//
// Every clock is the caller's `now` (seconds), so a node over loopback in a lab runs the same
// every time.
//
// ge:summary Our TV node's classic face: a match pulled from a stream port, delayed, served
// ge:keywords tv-node relay qtv stream-port upstream downstream viewers delay catch-up back-off
// ge:keywords say_game qul viewer-names tv_open tv_update tv_close classic-face
// ge:skill net shooter
// ge:test tests/shooters_edge_classic_tv_test.cpp
#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <string>
#include <vector>

#include "core/prng.hpp"
#include "net/address.hpp"
#include "net/tcp_socket.hpp"
#include "shooters_edge/net/classic_mv_state.hpp"
#include "shooters_edge/net/classic_qtv.hpp"

namespace shooters_edge::net {

struct TvConfig {
  std::string name = "tv";       // what the node calls itself to the server
  std::string server_password;   // the server's stream password (empty: none)
  std::string password;          // the node's own, for its viewers (empty: none)
  double delay_s = 7;            // a record's wait between arriving and going out
  int max_viewers = 1000;        // at once
  double pending_s = 5;          // a viewer's handshake
  double viewer_timeout_s = 30;  // no bytes taken for this long drops a viewer
  size_t max_queue = 192 << 10;  // bytes past its catch-up waiting for a viewer, at most
  double first_retry_s = 2;      // the first wait before dialling the server again
  double last_retry_s = 3600;    // the longest
};

enum class TvUpState : uint8_t { idle, asking, challenged, streaming, refused };

struct TvViewer {
  int id = 0;
  gn::TcpStream tcp;
  QtvState state = QtvState::pending;
  double since = 0, last_taken = 0;
  std::string in;
  std::vector<uint8_t> up;
  std::vector<uint8_t> out;
  size_t allowed = 0;  // its catch-up's bytes plus the queue's limit
  std::string name;
  std::string challenge;
  QtvAuth method = QtvAuth::none;
  bool waiting = false;  // let in before any level: fed from the next one
  int64_t sent = 0, catch_up = 0;
  bool gone = false;
  double gone_at = -1;
};

struct TvCounts {
  int64_t dials = 0, levels = 0, records = 0, released = 0, viewers = 0, refused = 0, dropped = 0;
  int64_t chat = 0, game_chat = 0, qul = 0;
};

struct TvNode {
  TvConfig cfg;
  // Up.
  gn::Address server;
  gn::TcpStream up;
  TvUpState state = TvUpState::idle;
  std::string up_text;        // the answer's text so far
  std::vector<uint8_t> held;  // records arrived, not yet whole
  struct Pending {
    double release_at = 0;
    std::vector<uint8_t> record;
  };
  std::deque<Pending> pending;  // whole records waiting out the delay
  double retry_at = 0, retry_s = 2;
  std::string last_error;  // why the server refused or the connection fell
  MvState match;           // the match as released
  // Down.
  gn::TcpListener listener;
  std::vector<std::unique_ptr<TvViewer>> viewers;
  int next_id = 1;
  ge::core::Rng rng{11};
  TvCounts counts;
  std::vector<std::string> log;  // dials, levels, viewers let in, refused and dropped
};

// Listens for viewers at `bind` (port 0: any free one) and dials `server` at the first update.
bool tv_open(TvNode& n, const gn::Address& bind, const gn::Address& server, const TvConfig& cfg,
             std::string& error);
// At `now`: the server dialled or read, records released and fanned out, viewers let in, their
// chat and names sent where they go, stalled viewers dropped.
void tv_update(TvNode& n, double now);
void tv_close(TvNode& n);
int tv_watching(const TvNode& n);

// A viewer's name as the node keeps it (`taken` other viewers' names): see the header.
std::string tv_viewer_name(const std::string& wanted, const std::vector<std::string>& taken,
                           ge::core::Rng& rng);

}  // namespace shooters_edge::net
