// THE OS'S SOCKET API, shared by the UDP and the TCP sockets and included by nothing else: BSD
// sockets on macOS, Linux, iOS and Android, Winsock on Windows (started once, on the first open).
// The platform's headers stay behind this file, so no engine header pulls them in; it is not
// installed with the engine's headers.
//
// ge:summary The OS socket shim the UDP and TCP sockets share: headers, errors, sockaddr, flags
// ge:keywords net socket os winsock wsastartup sockaddr non-blocking fcntl ioctlsocket shim
// ge:skill net
// ge:test tests/net_udp_socket_test.cpp tests/net_tcp_socket_test.cpp
#pragma once

#include <cstring>
#include <string>

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
#include <netinet/tcp.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cerrno>
#endif

#include "net/address.hpp"

namespace ge::net::os {

#if defined(_WIN32)
using SockLen = int;
using Handle = SOCKET;
inline bool startup() {
  static const bool ok = [] {
    WSADATA data;
    return WSAStartup(MAKEWORD(2, 2), &data) == 0;
  }();
  return ok;
}
inline int last_error() { return WSAGetLastError(); }
inline bool would_block(int e) { return e == WSAEWOULDBLOCK; }
inline bool in_progress(int e) { return e == WSAEWOULDBLOCK || e == WSAEINPROGRESS; }
// An ICMP "port unreachable" from an earlier send surfaces on the next receive; it is skipped.
inline bool unreachable(int e) { return e == WSAECONNRESET; }
inline bool valid(Handle h) { return h != INVALID_SOCKET; }
inline void close_handle(intptr_t h) { closesocket(static_cast<SOCKET>(h)); }
inline bool set_nonblocking(Handle h) {
  u_long nb = 1;
  return ioctlsocket(h, FIONBIO, &nb) == 0;
}
constexpr int kNoSignal = 0;
#else
using SockLen = socklen_t;
using Handle = int;
inline bool startup() { return true; }
inline int last_error() { return errno; }
inline bool would_block(int e) { return e == EAGAIN || e == EWOULDBLOCK; }
inline bool in_progress(int e) { return e == EINPROGRESS; }
inline bool unreachable(int e) { return e == ECONNREFUSED; }
inline bool valid(Handle h) { return h >= 0; }
inline void close_handle(intptr_t h) { ::close(static_cast<int>(h)); }
inline bool set_nonblocking(Handle h) {
  return fcntl(h, F_SETFL, fcntl(h, F_GETFL, 0) | O_NONBLOCK) == 0;
}
#if defined(MSG_NOSIGNAL)
constexpr int kNoSignal = MSG_NOSIGNAL;  // a write to a closed stream is an error, not SIGPIPE
#else
constexpr int kNoSignal = 0;  // Apple's: SO_NOSIGPIPE on the socket instead
#endif
#endif

inline Handle handle_of(intptr_t h) { return static_cast<Handle>(h); }

inline std::string os_error(const char* what, int code) {
  return std::string(what) + " failed (OS error " + std::to_string(code) + ")";
}

// The address as the OS's struct; false for a family the OS has no socket for.
inline bool to_sockaddr(const Address& a, sockaddr_storage& out, SockLen& len) {
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

inline Address from_sockaddr(const sockaddr_storage& s) {
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

// A socket's own address, as the OS bound it.
inline Address local_of(Handle h) {
  sockaddr_storage bound;
  SockLen blen = sizeof bound;
  std::memset(&bound, 0, sizeof bound);
  getsockname(h, reinterpret_cast<sockaddr*>(&bound), &blen);
  return from_sockaddr(bound);
}

// One family a socket: an IPv6 socket never answers IPv4 mapped addresses.
inline void v6_only(Handle h, const sockaddr_storage& sa) {
  if (sa.ss_family != AF_INET6) return;
  int only = 1;
  setsockopt(h, IPPROTO_IPV6, IPV6_V6ONLY, reinterpret_cast<const char*>(&only), sizeof only);
}

// Let an IPv4 datagram socket send to a broadcast address (a LAN's discovery probe).
inline void allow_broadcast(Handle h, const sockaddr_storage& sa) {
  if (sa.ss_family != AF_INET) return;
  int on = 1;
  setsockopt(h, SOL_SOCKET, SO_BROADCAST, reinterpret_cast<const char*>(&on), sizeof on);
}

}  // namespace ge::net::os
