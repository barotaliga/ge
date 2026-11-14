// A SCRIPTED SINGLE-PLAYER CLIENT THAT WATCHES A NETGAME: the client side of the single-player
// transport (version 15) and its signon, enough to sit in front of `setv`'s netgame face and watch.
// It is written from the transport's facts (porting/shooters-edge/netgame-transport.md), never
// from the face it is pointed at, so a test or a lab that drives the face with it checks the face
// against those facts: it connects with the control packet, takes the accept's port, acknowledges
// every reliable fragment and joins them into messages, answers each `signonnum` with the stage's
// commands (resent after a second while unacknowledged), and once seated reads every frame with the
// game's own reader of the protocol and sends a move a tick. A "reconnect" starts the signon again.
//
// ge:summary A scripted single-player client: connects to a netgame, signs on and watches it
// ge:keywords netgame single-player protocol-15 watcher scripted-client signon reliable resend
// ge:skill net shooter
// ge:test tests/shooters_edge_classic_tv_netgame_test.cpp
#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "net/transport.hpp"
#include "shooters_edge/client/demo.hpp"

namespace shooters_edge::net {

struct NetgameWatcher {
  ge::net::Transport* t = nullptr;
  ge::net::Address server;
  bool accepted = false;
  int signon = 0;  // the last signon stage answered (3: watching)
  // Counts of what came in: reliable messages, frame datagrams, entities present over every frame,
  // the recorded view's `setangle`s, the loops back to the start.
  int reliable_messages = 0, frames = 0, entity_updates = 0, setangles = 0, reconnects = 0;
  int resends = 0;                // reliable commands sent again after a second
  int seatings = 0;               // times signon 3 was answered: once, then once after each loop
  int64_t bytes_in = 0;           // every datagram's bytes from the server, headers in
  int64_t first_connect_us = -1;  // when the first connect went out
  int64_t seated_us = -1;         // when signon 3 was first answered
  int64_t last_seated_us = -1;    // when it was last answered (after a loop or a switch)
  float last_time = -1;           // the server's time on the last frame
  bool time_fell = false;         // a frame's time below the one before it, outside a loop
  // What the next moves carry (a press is a change from 0), and the lines the server printed.
  uint8_t buttons = 0, impulse = 0;
  std::vector<std::string> prints;
  std::array<int, 16>
      frags{};  // each player's frags, as the server last said (reset at a reconnect)
  shooters_edge::client::Demo view;  // what the client knows, as the game's reader reads it
  std::string read_error;            // the first message the reader refused, with why

  // Internal: the transport's sequences, the message being joined, the reliable command in flight.
  uint32_t recv_seq = 0, send_seq = 0, unrel_out = 0;
  std::vector<uint8_t> partial, in_flight;
  bool pending = false;
  int64_t flight_us = 0;
  std::vector<std::string> outbox;
};

// Points the watcher at the face: `t` is its own socket, `server` the face's address.
void netgame_watch(NetgameWatcher& w, ge::net::Transport& t, const ge::net::Address& server);
// A console command sent to the server (`cmd <text>` on a client), reliable.
void netgame_watch_command(NetgameWatcher& w, const std::string& text);
// One tick at `now`: a connect every half second until accepted, then every datagram in, the
// commands out, and a move once seated.
void netgame_watch_tick(NetgameWatcher& w, int64_t now);

}  // namespace shooters_edge::net
