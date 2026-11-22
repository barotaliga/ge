// `serverstatus [address]`: a server asked for its info and its players from a socket of its own,
// joined or not, and the answer printed as a block: "Status response from <ip:port>", a blank line,
// the info a key a line (the key padded to 20, a space, the value), a blank line, the header
// "Num Score Ping Name", then each player by score, highest first: the number from 1 in 3, the
// score in 5, the ping in 4, the name. With no address it asks the server joined (or being
// joined); with none, "Usage: serverstatus [address]". An address it cannot read: "Bad address:
// <text>". The port is the loaded game's (27910 for Quake II, 27500 else) when none is given. An
// answer later than 6 s is not waited for, and nothing is said of one that never comes.
//
// Both games' answers are read: Quake II's (a print: the info line, then `score ping "name"` a
// player) and QuakeWorld's (`n`: the info line, then `userid frags minutes ping "name" ...`).
//
// ge:summary serverstatus: a server's info and players asked on a socket of its own and printed
// ge:keywords serverstatus status-response server-status players score ping status-query
// ge:keywords qw_serverstatus format_status
// ge:skill net console
// ge:test tests/shooters_edge_qw_serverstatus_test.cpp
#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "console/console.hpp"
#include "net/address.hpp"
#include "net/transport.hpp"

namespace shooters_edge::net {

struct Remote;

inline constexpr double kStatusWaitS = 6;  // an answer later than this is not waited for

// The status asked and not yet answered.
struct StatusAsk {
  ge::net::Transport own;
  ge::net::Address to;
  double until_s = -1;  // when it stops waiting (-1: nothing asked)
  bool opened = false;
};

// The block printed for an answer's text (the header and its `print\n` or `n` taken off) from
// `where`; false when the text is not a status answer (no info line).
bool format_status(std::string_view where, std::string_view text, std::vector<std::string>& out);

// The command on `con`, asking through `r` (its server joined, its game's port); and the step that
// reads the answer, every frame at `now_s`, its lines into `said`.
void add_serverstatus_command(ge::console::Console& con, Remote& r);
void serverstatus_step(Remote& r, double now_s, std::vector<std::string>& said);

}  // namespace shooters_edge::net
