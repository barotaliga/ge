// THE OS'S UDP SOCKETS (BSD sockets on macOS, Linux, iOS and Android; Winsock on Windows), the
// only file that includes them. Non-blocking: a receive with nothing waiting returns at once, so
// a node polls it once a frame. IPv4 and IPv6 (a socket is bound to one family). Also the OS's
// entropy, which a real game seeds its node's salts and secret from (the simulated network
// seeds them from a lab's seed instead).
//
// ge:summary The OS's non-blocking UDP sockets (POSIX and Winsock), IPv4 and IPv6; OS entropy
// ge:keywords net udp socket sockets bsd-sockets winsock bind sendto recvfrom non-blocking
// ge:keywords ipv4 ipv6 loopback entropy random-device os
// ge:skill net
// ge:test tests/net_udp_socket_test.cpp
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

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

}  // namespace ge::net
