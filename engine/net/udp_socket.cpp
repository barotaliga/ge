// The UDP sockets over each OS's API. Winsock is started once, on the first open.
//
// ge:summary UDP sockets over BSD sockets and Winsock; the OS's entropy
// ge:keywords net udp socket winsock wsastartup sockaddr fcntl o-nonblock ioctlsocket
// ge:skill net
// ge:test tests/net_udp_socket_test.cpp
#include "net/udp_socket.hpp"

#include <cstring>
#include <random>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cerrno>
#endif

namespace ge::net {

namespace {

#if defined(_WIN32)
using SockLen = int;
bool startup() {
  static const bool ok = [] {
    WSADATA data;
    return WSAStartup(MAKEWORD(2, 2), &data) == 0;
  }();
  return ok;
}
int last_error() { return WSAGetLastError(); }
// An ICMP "port unreachable" from an earlier send surfaces on the next receive; it is skipped.
bool unreachable(int e) { return e == WSAECONNRESET; }
void close_handle(intptr_t h) { closesocket(static_cast<SOCKET>(h)); }
#else
using SockLen = socklen_t;
bool startup() { return true; }
int last_error() { return errno; }
bool unreachable(int e) { return e == ECONNREFUSED; }
void close_handle(intptr_t h) { ::close(static_cast<int>(h)); }
#endif

std::string os_error(const char* what, int code) {
  return std::string(what) + " failed (OS error " + std::to_string(code) + ")";
}

// The address as the OS's struct; false for a family the OS has no socket for.
bool to_sockaddr(const Address& a, sockaddr_storage& out, SockLen& len) {
  std::memset(&out, 0, sizeof out);
  if (a.family == Family::ipv4) {
    auto* in = reinterpret_cast<sockaddr_in*>(&out);
    in->sin_family = AF_INET;
    in->sin_port = htons(a.port);
    std::memcpy(&in->sin_addr, a.bytes, 4);
    len = sizeof(sockaddr_in);
    return true;
  }
  if (a.family == Family::ipv6) {
    auto* in = reinterpret_cast<sockaddr_in6*>(&out);
    in->sin6_family = AF_INET6;
    in->sin6_port = htons(a.port);
    std::memcpy(&in->sin6_addr, a.bytes, 16);
    len = sizeof(sockaddr_in6);
    return true;
  }
  return false;
}

Address from_sockaddr(const sockaddr_storage& s) {
  Address a;
  if (s.ss_family == AF_INET) {
    const auto* in = reinterpret_cast<const sockaddr_in*>(&s);
    a.family = Family::ipv4;
    std::memcpy(a.bytes, &in->sin_addr, 4);
    a.port = ntohs(in->sin_port);
  } else if (s.ss_family == AF_INET6) {
    const auto* in = reinterpret_cast<const sockaddr_in6*>(&s);
    a.family = Family::ipv6;
    std::memcpy(a.bytes, &in->sin6_addr, 16);
    a.port = ntohs(in->sin6_port);
  }
  return a;
}

}  // namespace

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
  const auto h = ::socket(sa.ss_family, SOCK_DGRAM, IPPROTO_UDP);
#if defined(_WIN32)
  if (h == INVALID_SOCKET) {
#else
  if (h < 0) {
#endif
    if (error) *error = os_error("socket()", last_error());
    return false;
  }
  const auto handle = static_cast<intptr_t>(h);
  if (sa.ss_family == AF_INET6) {
    int only = 1;  // one family a socket: an IPv6 socket never answers IPv4 mapped addresses
    setsockopt(h, IPPROTO_IPV6, IPV6_V6ONLY, reinterpret_cast<const char*>(&only), sizeof only);
  }
#if defined(_WIN32)
  u_long nb = 1;
  const bool nonblocking = ioctlsocket(h, FIONBIO, &nb) == 0;
#else
  const bool nonblocking = fcntl(h, F_SETFL, fcntl(h, F_GETFL, 0) | O_NONBLOCK) == 0;
#endif
  if (!nonblocking || ::bind(h, reinterpret_cast<const sockaddr*>(&sa), len) != 0) {
    if (error)
      *error = os_error(nonblocking ? "bind() to " : "set non-blocking", last_error()) +
               (nonblocking ? " at " + to_string(bind) : "");
    close_handle(handle);
    return false;
  }
  sockaddr_storage bound;
  SockLen blen = sizeof bound;
  getsockname(h, reinterpret_cast<sockaddr*>(&bound), &blen);
  s.handle = handle;
  s.local = from_sockaddr(bound);
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
