// The network's console words (net_commands.hpp).
//
// ge:summary dns and net_stats on the console
// ge:keywords dns net_stats add_net_commands
// ge:skill net console
// ge:test tests/net_rates_test.cpp
#include "net/net_commands.hpp"

#include <string>
#include <utility>

#include "net/address.hpp"
#include "net/udp_socket.hpp"

namespace ge::net {

void add_net_commands(console::Console& con, std::function<const Transport*()> socket) {
  using Con = console::Console;
  using Args = console::Args;
  con.add_command("dns", "dns <name>: the address a host name stands for",
                  [](Con& c, const Args& a) {
                    if (a.size() < 2) {
                      c.print("Usage: dns <name>");
                      return;
                    }
                    Address to;
                    std::string why;
                    if (parse_address(a[1] + ":0", to) || resolve_ipv4(a[1], 0, to, &why)) {
                      std::string text = to_string(to);
                      if (const size_t colon = text.rfind(':'); colon != std::string::npos)
                        text.resize(colon);
                      c.print(a[1] + " resolved to " + text);
                    } else {
                      c.print("Couldn't resolve " + a[1] + ": " + why);
                    }
                  });
  con.add_command("net_stats", "net_stats: what the game's connection has sent and received",
                  [socket = std::move(socket)](Con& c, const Args&) {
                    const Transport* t = socket ? socket() : nullptr;
                    if (t == nullptr || t->kind == TransportKind::none) {
                      c.print("Not connected to anything.");
                      return;
                    }
                    c.print("Sent:     " + std::to_string(t->sent_packets) + " packets, " +
                            std::to_string(t->sent_bytes) + " bytes");
                    c.print("Received: " + std::to_string(t->received_packets) + " packets, " +
                            std::to_string(t->received_bytes) + " bytes");
                    c.print("Refused sends: " + std::to_string(t->send_failures));
                  });
}

}  // namespace ge::net
