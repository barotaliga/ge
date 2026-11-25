// The dedicated server's `net_restart` (engine/server/net_restart.hpp) on loopback sockets: the
// socket opened again on its own port with nothing said; a taken port falls back to the last good
// one, saying so and setting the port back; when neither opens, the refusal.
//
// ge:summary Tests net_restart: the socket reopened, the last good port, the refusal
// ge:keywords test net_restart reopen socket net_port
// ge:skill net testing
#include <cstdio>
#include <string>

#include "check.hpp"
#include "net/address.hpp"
#include "net/transport.hpp"
#include "server/net_restart.hpp"

namespace gn = ge::net;
namespace gs = ge::server;

TEST(net_restart_opens_again_and_falls_back_to_the_last_good_port) {
  gn::Transport sock, taken;
  std::string err;
  if (!gn::open_udp_transport(sock, gn::loopback_v4(0), &err) ||
      !gn::open_udp_transport(taken, gn::loopback_v4(0), &err)) {
    ge::test::skip("no loopback UDP socket here: " + err);
    return;
  }
  const int mine = sock.local.port, theirs = taken.local.port;
  std::string ip = "127.0.0.1", said;
  int port = mine;
  CHECK(gs::net_restart(sock, ip, port, ip, mine, said));
  CHECK(said.empty() && sock.local.port == mine);

  // A port another socket holds: back to the last good one.
  port = theirs;
  CHECK(gs::net_restart(sock, ip, port, "127.0.0.1", mine, said));
  CHECK_MSG(said == "Reverting to the last valid port " + std::to_string(mine) + "...\n", said);
  CHECK(port == mine && sock.local.port == mine);

  // Neither can be had: the refusal, and no socket.
  said.clear();
  gn::Transport holder;
  gn::close_transport(sock);
  CHECK(gn::open_udp_transport(holder, gn::loopback_v4(static_cast<uint16_t>(mine)), &err));
  port = theirs;
  CHECK(!gs::net_restart(sock, ip, port, "127.0.0.1", mine, said));
  CHECK_MSG(said == "Reverting to the last valid port " + std::to_string(mine) +
                        "...\nCouldn't open server UDP port.\n",
            said);
  gn::close_transport(holder);
  gn::close_transport(taken);
  gn::close_transport(sock);
}

// `net_enable_ipv6` 2: an IPv6 socket on the same port beside the IPv4 one; a datagram from an
// IPv6 peer is read through the one transport and the answer leaves by the IPv6 socket. A bad
// `net_ip6` is said and the server runs on IPv4 alone.
TEST(an_ipv6_socket_listens_beside_the_ipv4_one) {
  gn::Transport sock, peer;
  std::string err;
  if (!gn::open_udp_transport(sock, gn::loopback_v4(0), &err)) {
    ge::test::skip("no loopback UDP socket here: " + err);
    return;
  }
  const int port = sock.local.port;
  std::string said;
  CHECK(!gs::open_ipv6(sock, 1, "::1", port, said) && said.empty());  // 1: IPv4 alone
  CHECK(!gs::open_ipv6(sock, 2, "not-an-address", port, said));
  CHECK_MSG(said.find("bad interface address") != std::string::npos, said);
  said.clear();
  gn::Address v6;
  if (!gn::parse_address("[::1]:0", v6) || !gn::open_udp_transport(peer, v6, &err) ||
      !gs::open_ipv6(sock, 2, "::1", port, said)) {
    // Not a skip: the IPv4 test above still ran, and a skip fails a test CTest does not expect
    // to skip. The machine simply has no IPv6.
    std::fprintf(stderr, "note: no IPv6 loopback here, nothing checked: %s%s\n", err.c_str(),
                 said.c_str());
    gn::close_transport(sock);
    return;
  }
  CHECK(sock.udp6.local.family == gn::Family::ipv6 && sock.udp6.local.port == port);
  gn::Address to;
  CHECK(gn::parse_address("[::1]:" + std::to_string(port), to));
  const uint8_t hello[3] = {'h', 'i', '6'};
  CHECK(gn::transport_send(peer, to, hello, sizeof hello, 0));
  uint8_t buf[16];
  gn::Address from;
  int n = -1;
  for (int i = 0; i < 200 && n < 0; i++) n = gn::transport_receive(sock, buf, sizeof buf, from, 0);
  CHECK(n == 3 && from.family == gn::Family::ipv6);
  if (n == 3) {
    CHECK(gn::transport_send(sock, from, buf, 3, 0));  // back by the IPv6 socket
    n = -1;
    for (int i = 0; i < 200 && n < 0; i++)
      n = gn::transport_receive(peer, buf, sizeof buf, from, 0);
    CHECK(n == 3 && from.port == port);
  }
  gn::close_transport(peer);
  gn::close_transport(sock);
  CHECK(sock.udp6.handle == -1);
}
