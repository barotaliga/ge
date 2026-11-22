// THE NETWORK'S CONSOLE WORDS, for any game on engine/net:
// - `dns <name>`: a host name looked up, "<name> resolved to <address>", or "Couldn't resolve
//   <name>: <why>" (an address given as numbers comes back as it is).
// - `net_stats`: what the game's socket has sent and received since it opened, in packets and
//   bytes, and the sends the system refused; "Not connected to anything." with none open.
//
// ge:summary The network's console words: dns and net_stats
// ge:keywords dns net_stats resolve host-name packets bytes console net-commands
// ge:keywords add_net_commands
// ge:skill net console
// ge:test tests/net_rates_test.cpp
#pragma once

#include <functional>

#include "console/console.hpp"
#include "net/transport.hpp"

namespace ge::net {

// The words on `con`; `socket` answers the game's open transport now (null: none).
void add_net_commands(console::Console& con, std::function<const Transport*()> socket);

}  // namespace ge::net
