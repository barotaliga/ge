// THE OS'S UDP SOCKETS (BSD sockets on macOS, Linux, iOS and Android; Winsock on Windows), over
// the socket shim in os_socket.hpp (the only header that includes the OS's). Non-blocking: a
// receive with nothing waiting returns at once, so a node polls it once a frame. IPv4 and IPv6 (a
// socket is bound to one family). Also the OS's entropy, which a real game seeds its node's salts
// and secret from (the simulated network seeds them from a lab's seed instead).
//
// ge:summary The OS's non-blocking UDP sockets (POSIX and Winsock), IPv4 and IPv6; OS entropy
// ge:keywords net udp socket sockets bsd-sockets winsock bind sendto recvfrom non-blocking
// ge:keywords ipv4 ipv6 loopback entropy random-device os showip host-name host_addresses
// ge:skill net
// ge:test tests/net_udp_socket_test.cpp
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "net/address.hpp"

namespace ge::net {

struct UdpSocket {
  intptr_t handle = -1;  // the OS's descriptor; -1 when closed
  Address local;         // the address it is bound to (the port the OS picked for port 0)
};

// Bind a non-blocking socket at `bind` (port 0: any free port; a family's zero address: every
// interface). On failure `error` says what the OS refused and the socket stays closed.
bool udp_open(UdpSocket& s, const Address& bind, std::string* error);
void udp_close(UdpSocket& s);
// One datagram; false when the OS refused it (the reason in `error`, when given).
bool udp_send(UdpSocket& s, const Address& to, const uint8_t* data, size_t size,
              std::string* error = nullptr);
// The next datagram waiting: its size (0 is a valid empty datagram), or -1 when none is.
int udp_receive(UdpSocket& s, uint8_t* buffer, size_t capacity, Address& from);

// 64 bits from the OS's random source, for a real node's salts and secret.
uint64_t os_entropy();

// A host's IPv4 address by name, as the OS's resolver finds it: `text` is "host[:port]" or an
// address "1.2.3.4[:port]" (`port` when it names none). It blocks while the resolver asks, so a
// game calls it when the player acts (a refresh, a connect), never in a frame's hot path. False
// and `error` when the name does not resolve.
bool resolve_ipv4(std::string_view text, uint16_t port, Address& out, std::string* error);

// One of this machine's addresses (`showip`), with the name a reverse lookup finds ("" when none).
struct HostAddress {
  Address address;
  std::string reverse;
};
// This machine's name, the resolver's canonical name for it ("" when it gives none) and its
// addresses (IPv4, and IPv6 too when `ipv6`). It blocks while the resolver asks: an operator's
// word, never a frame's work. False and `error` ("Couldn't get system host name", "Couldn't
// resolve <name>: <why>") when the name cannot be had or looked up.
bool host_addresses(std::string& name, std::string& canonical, std::vector<HostAddress>& out,
                    bool ipv6, std::string* error);
// `showip`'s lines: "Hostname: <canonical>", one "IP      : <address> (<reverse>)" or
// "IP6     : ..." per address, then one "<what> socket bound to <address>" per socket
// (`sockets`' first: "Server UDP", "Server TCP", "Server UDP6", ...).
std::string showip_text(const std::string& canonical, const std::vector<HostAddress>& addresses,
                        const std::vector<std::pair<std::string, Address>>& sockets);

}  // namespace ge::net
