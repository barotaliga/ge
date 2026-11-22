// The UDP sockets over each OS's API (os_socket.hpp).
//
// ge:summary UDP sockets over BSD sockets and Winsock; the OS's entropy
// ge:keywords net udp socket winsock sendto recvfrom
// ge:skill net
// ge:test tests/net_udp_socket_test.cpp
#include "net/udp_socket.hpp"

#include <cstdlib>
#include <cstring>
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
  allow_broadcast(h, sa);
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

bool resolve_ipv4(std::string_view text, uint16_t port, Address& out, std::string* error) {
  std::string host(text);
  const size_t colon = host.rfind(':');
  if (colon != std::string::npos) {
    const std::string digits = host.substr(colon + 1);
    char* end = nullptr;
    const unsigned long p = std::strtoul(digits.c_str(), &end, 10);
    if (digits.empty() || *end != 0 || p == 0 || p > 65535) {
      if (error) *error = "'" + std::string(text) + "' has no port a resolver reads (1..65535)";
      return false;
    }
    port = static_cast<uint16_t>(p);
    host.resize(colon);
  }
  if (!startup()) {
    if (error) *error = "WSAStartup failed: Winsock 2.2 is not available";
    return false;
  }
  addrinfo hints;
  std::memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_DGRAM;
  addrinfo* found = nullptr;
  const int r = getaddrinfo(host.c_str(), nullptr, &hints, &found);
  if (r != 0 || found == nullptr) {
    if (error) *error = "'" + host + "' does not resolve to an IPv4 address";
    return false;
  }
  sockaddr_storage sa;
  std::memset(&sa, 0, sizeof sa);
  std::memcpy(&sa, found->ai_addr, found->ai_addrlen < sizeof sa ? found->ai_addrlen : sizeof sa);
  freeaddrinfo(found);
  out = from_sockaddr(sa);
  out.port = port;
  return true;
}

bool host_addresses(std::string& name, std::string& canonical, std::vector<HostAddress>& out,
                    bool ipv6, std::string* error) {
  out.clear();
  canonical.clear();
  char host[256] = {};
  if (!startup() || gethostname(host, sizeof host - 1) != 0) {
    if (error) *error = "Couldn't get system host name";
    return false;
  }
  name = host;
  addrinfo hints;
  std::memset(&hints, 0, sizeof hints);
  hints.ai_family = ipv6 ? AF_UNSPEC : AF_INET;
  hints.ai_socktype = SOCK_DGRAM;
  hints.ai_flags = AI_CANONNAME;
  addrinfo* found = nullptr;
  const int r = getaddrinfo(host, nullptr, &hints, &found);
  if (r != 0 || found == nullptr) {
    if (error) *error = "Couldn't resolve " + name + ": " + gai_strerror(r);
    return false;
  }
  if (found->ai_canonname != nullptr) canonical = found->ai_canonname;
  for (const addrinfo* a = found; a != nullptr; a = a->ai_next) {
    sockaddr_storage sa;
    std::memset(&sa, 0, sizeof sa);
    std::memcpy(&sa, a->ai_addr, a->ai_addrlen < sizeof sa ? a->ai_addrlen : sizeof sa);
    HostAddress h;
    h.address = from_sockaddr(sa);
    char back[NI_MAXHOST] = {};
    if (getnameinfo(a->ai_addr, static_cast<SockLen>(a->ai_addrlen), back, sizeof back, nullptr, 0,
                    NI_NAMEREQD) == 0)
      h.reverse = back;
    out.push_back(std::move(h));
  }
  freeaddrinfo(found);
  return true;
}

std::string showip_text(const std::string& canonical, const std::vector<HostAddress>& addresses,
                        const std::vector<std::pair<std::string, Address>>& sockets) {
  std::string out;
  if (!canonical.empty()) out += "Hostname: " + canonical + "\n";
  for (const HostAddress& h : addresses) {
    Address bare = h.address;
    bare.port = 0;
    std::string text = to_string(bare);
    if (const size_t colon = text.rfind(":0");
        colon != std::string::npos && colon + 2 == text.size())
      text.resize(colon);  // the address alone
    if (bare.family == Family::ipv6 && text.size() > 1 && text.front() == '[')
      text = text.substr(1, text.find(']') - 1);
    out += (bare.family == Family::ipv6 ? "IP6     : " : "IP      : ") + text;
    if (!h.reverse.empty()) out += " (" + h.reverse + ")";
    out += "\n";
  }
  for (const auto& [what, at] : sockets) out += what + " socket bound to " + to_string(at) + "\n";
  return out;
}

}  // namespace ge::net
