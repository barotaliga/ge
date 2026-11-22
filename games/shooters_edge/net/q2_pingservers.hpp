// `pingservers`: Quake II's quick look round for servers from the console. It asks `info 34` of
// every server on the LAN (the broadcast to port 27910) and of each address in the address book
// (`adr0`, `adr1`, ... in order until one is not registered, the empty ones skipped; a port may be
// given, 27910 when not), on a socket of its own, and prints each server's one-line answer as it
// comes. A broadcast takes every answer for 3 seconds, an address its own server's first answer
// for 6. An address it cannot read says "Bad address: <text>" and the rest are asked. The answers
// are the console's only: JOIN A GAME asks the servers itself.
//
// `passive`: a connection the other way round, for a player a server cannot reach first. It
// leaves any game, listens on a socket of its own ("Listening for passive connections at
// <address>.") for a server's `passive_connect`, and joins the server that sent it ("Received
// passive connect from <address>." and `connect`); typed again, or once a game is joined, it stops
// ("No longer listening for passive connections." when typed).
//
// ge:summary pingservers and passive: the LAN and the address book asked, a server's call awaited
// ge:keywords pingservers ping-servers address-book adr0 info lan broadcast q2 passive
// ge:keywords passive_connect reverse-connect
// ge:skill net console
// ge:test tests/shooters_edge_q2_pingservers_test.cpp
#pragma once

#include <string>
#include <vector>

#include "console/console.hpp"
#include "net/transport.hpp"

namespace shooters_edge::net {

inline constexpr double kPingBroadcastS = 3, kPingDirectS = 6;

struct PingSweep {
  struct Ask {
    ge::net::Address to;
    bool broadcast = false;
    double until_s = 0;  // answers after this are not taken
  };
  ge::net::Transport own;
  ge::net::Transport* link = nullptr;  // the socket to ask over instead (a test's)
  std::vector<Ask> asks;
  bool opened = false;
};

struct PassiveListen {
  ge::net::Transport own;
  ge::net::Transport* link = nullptr;  // the socket to listen on instead (a test's)
  bool on = false;
};

// `pingservers` on `con`, asking through `sweep`; `now_s` is the clock the step reads.
void add_pingservers_command(ge::console::Console& con, PingSweep& sweep, double (*now_s)());
// The answers come since the last frame, each a line into `said`; the socket closed once every
// ask is done.
// `passive` on `con`, listening through `p`.
void add_passive_command(ge::console::Console& con, PassiveListen& p);
// A `passive_connect` come since the last frame: said, and its address returned ("" none); the
// listening stopped. `joined`: a game is joined, so it stops quietly.
std::string passive_step(PassiveListen& p, bool joined, int64_t now_us,
                         std::vector<std::string>& said);
void pingservers_step(PingSweep& sweep, double now_s, int64_t now_us,
                      std::vector<std::string>& said);

}  // namespace shooters_edge::net
