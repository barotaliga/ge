// The dedicated server's `net_restart` (engine/server/net_restart.hpp) on loopback sockets: the
// socket opened again on its own port with nothing said; a taken port falls back to the last good
// one, saying so and setting the port back; when neither opens, the refusal.
//
// ge:summary Tests net_restart: the socket reopened, the last good port, the refusal
// ge:keywords test net_restart reopen socket net_port
// ge:skill net testing
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
