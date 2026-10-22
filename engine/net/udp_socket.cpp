// The UDP sockets over each OS's API (os_socket.hpp).
//
// ge:summary UDP sockets over BSD sockets and Winsock; the OS's entropy
// ge:keywords net udp socket winsock sendto recvfrom
// ge:skill net
// ge:test tests/net_udp_socket_test.cpp
#include "net/udp_socket.hpp"

#include <random>

#include "net/os_socket.hpp"

namespace ge::net {

using namespace os;

bool udp_open(UdpSocket& s, const Address& bind, std::string* error) {
  udp_close(s);
  sockaddr_storage sa;
  SockLen len = 0;
  if (!startup()) {
    if (error) *error = "WSAStartup failed: Winsock 2.2 is not available";
    return false;
  }
  if (!to_sockaddr(bind, sa, len)) {
    if (error) *error = "udp_open: " + to_string(bind) + " is not an IPv4 or IPv6 address";
    return false;
  }
  const Handle h = ::socket(sa.ss_family, SOCK_DGRAM, IPPROTO_UDP);
  if (!valid(h)) {
    if (error) *error = os_error("socket()", last_error());
    return false;
  }
  const auto handle = static_cast<intptr_t>(h);
  v6_only(h, sa);
  const bool nonblocking = set_nonblocking(h);
  if (!nonblocking || ::bind(h, reinterpret_cast<const sockaddr*>(&sa), len) != 0) {
    if (error)
      *error = os_error(nonblocking ? "bind() to " : "set non-blocking", last_error()) +
               (nonblocking ? " at " + to_string(bind) : "");
    close_handle(handle);
    return false;
  }
  s.handle = handle;
  s.local = local_of(h);
  return true;
}

void udp_close(UdpSocket& s) {
  if (s.handle != -1) close_handle(s.handle);
  s.handle = -1;
  s.local = Address{};
}

bool udp_send(UdpSocket& s, const Address& to, const uint8_t* data, size_t size,
              std::string* error) {
  sockaddr_storage sa;
  SockLen len = 0;
  if (s.handle == -1 || !to_sockaddr(to, sa, len)) {
    if (error)
      *error = "udp_send to " + to_string(to) + ": the socket is closed or the family wrong";
    return false;
  }
#if defined(_WIN32)
  const int sent = ::sendto(static_cast<SOCKET>(s.handle), reinterpret_cast<const char*>(data),
                            static_cast<int>(size), 0, reinterpret_cast<const sockaddr*>(&sa), len);
#else
  const auto sent = ::sendto(static_cast<int>(s.handle), data, size, 0,
                             reinterpret_cast<const sockaddr*>(&sa), len);
#endif
  if (sent < 0 || static_cast<size_t>(sent) != size) {
    if (error) *error = os_error("sendto()", last_error());
    return false;
  }
  return true;
}

int udp_receive(UdpSocket& s, uint8_t* buffer, size_t capacity, Address& from) {
  if (s.handle == -1) return -1;
  while (true) {
    sockaddr_storage sa;
    SockLen len = sizeof sa;
#if defined(_WIN32)
    const int got =
        ::recvfrom(static_cast<SOCKET>(s.handle), reinterpret_cast<char*>(buffer),
                   static_cast<int>(capacity), 0, reinterpret_cast<sockaddr*>(&sa), &len);
#else
    const auto got = ::recvfrom(static_cast<int>(s.handle), buffer, capacity, 0,
                                reinterpret_cast<sockaddr*>(&sa), &len);
#endif
    if (got >= 0) {
      from = from_sockaddr(sa);
      return static_cast<int>(got);
    }
    if (unreachable(last_error())) continue;
    return -1;  // nothing waiting (or an error a retry next frame meets again)
  }
}

uint64_t os_entropy() {
  std::random_device rd;
  return static_cast<uint64_t>(rd()) << 32 | rd();
}

}  // namespace ge::net
