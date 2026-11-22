// NET_RESTART: a dedicated server's socket closed and opened again on `net_ip` and `net_port`, a
// changed port taking effect without a restart. When the new place cannot be had and the last
// place that opened differs, the server goes back to it ("Reverting to the last valid port
// <n>..."); when neither opens, "Couldn't open server UDP port." and the server runs on with no
// socket until the next `net_restart`. Nothing is printed when it opens.
//
// ge:summary net_restart: the server's socket opened again, the last good port when the new fails
// ge:keywords net_restart reopen socket net_port net_ip last-valid-port
// ge:skill net
// ge:test tests/server_net_restart_test.cpp
#pragma once

#include <string>

#include "net/transport.hpp"

namespace ge::server {

// `sock` closed and opened at `ip`:`port` ("" every interface), else at `good_ip`:`good_port`
// with `ip` and `port` set back to them; the lines said in `said`. True when `sock` is open.
bool net_restart(net::Transport& sock, std::string& ip, int& port, const std::string& good_ip,
                 int good_port, std::string& said);

}  // namespace ge::server
