// The definitions behind net_restart.hpp.
//
// ge:summary net_restart's definitions: close, open, fall back to the last good port
// ge:keywords net_restart reopen socket
// ge:skill net
// ge:test tests/server_net_restart_test.cpp
#include "server/net_restart.hpp"

namespace ge::server {

namespace {

bool open_at(net::Transport& sock, const std::string& ip, int port) {
  net::Address bind;
  std::string why;
  net::close_transport(sock);
  return net::parse_address((ip.empty() ? "0.0.0.0" : ip) + ":" + std::to_string(port), bind) &&
         net::open_udp_transport(sock, bind, &why);
}

}  // namespace

bool net_restart(net::Transport& sock, std::string& ip, int& port, const std::string& good_ip,
                 int good_port, std::string& said) {
  if (open_at(sock, ip, port)) return true;
  if (good_port != port || good_ip != ip) {
    said += "Reverting to the last valid port " + std::to_string(good_port) + "...\n";
    ip = good_ip;
    port = good_port;
    if (open_at(sock, ip, port)) return true;
  }
  said += "Couldn't open server UDP port.\n";
  return false;
}

}  // namespace ge::server
