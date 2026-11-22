// The network's console words (net_commands.hpp).
//
// ge:summary dns, net_stats and a client's showip on the console
// ge:keywords dns net_stats add_net_commands showip add_client_showip
// ge:skill net console
// ge:test tests/net_rates_test.cpp
#include "net/net_commands.hpp"

#include <string>
#include <utility>
#include <vector>

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

void add_client_showip(console::Console& con, std::function<const Transport*()> socket) {
  con.add_command("showip", "showip: this machine's addresses and the socket the game plays on",
                  [socket = std::move(socket)](console::Console& c, const console::Args&) {
                    std::string name, canonical, why;
                    std::vector<HostAddress> addresses;
                    if (!host_addresses(name, canonical, addresses, true, &why)) {
                      c.print(why);
                      return;
                    }
                    std::vector<std::pair<std::string, Address>> sockets;
                    const Transport* t = socket ? socket() : nullptr;
                    if (t != nullptr && t->kind != TransportKind::none)
                      sockets.emplace_back(
                          t->local.family == Family::ipv6 ? "Client UDP6" : "Client UDP", t->local);
                    std::string text = showip_text(canonical, addresses, sockets);
                    if (!text.empty() && text.back() == '\n') text.pop_back();
                    c.print(text);
                  });
}

}  // namespace ge::net
