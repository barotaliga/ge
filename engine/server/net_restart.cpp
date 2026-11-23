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

bool open_ipv6(net::Transport& sock, int ipv6, const std::string& ip6, int port,
               std::string& said) {
  if (ipv6 < 2 || sock.kind != net::TransportKind::udp) return false;
  const std::string where =
      "[" + (ip6.empty() ? std::string("::") : ip6) + "]:" + std::to_string(port);
  net::Address bind;
  std::string why;
  if (!net::parse_address(where, bind) || bind.family != net::Family::ipv6) {
    said += where +
            ": bad interface address: net_ip6 is not an IPv6 address; listening on IPv4 "
            "alone\n";
    return false;
  }
  if (!net::open_udp6_beside(sock, bind, &why)) {
    said += where + ": can't bind socket: " + why + "; listening on IPv4 alone\n";
    return false;
  }
  return true;
}

}  // namespace ge::server
