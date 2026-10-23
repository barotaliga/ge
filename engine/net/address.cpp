// Addresses parsed and printed by hand, with no OS call, so the simulated network and the tools
// need no socket library: IPv4 dotted quads, IPv6 in brackets with `::` (printed as RFC 5952
// writes it: lower case, the longest run of zero groups folded), and `sim:<node>:<port>`.
//
// ge:summary Address parsing and printing without the OS: IPv4, bracketed IPv6, sim nodes
// ge:keywords net address parse format ipv6 rfc5952 dotted-quad hash
// ge:skill net
// ge:test tests/net_address_test.cpp
#include "net/address.hpp"

#include <cstdio>
#include <cstring>

namespace ge::net {

namespace {

size_t used_bytes(Family f) {
  switch (f) {
    case Family::ipv4:
    case Family::sim:
      return 4;
    case Family::ipv6:
      return 16;
    case Family::none:
      break;
  }
  return 0;
}

// A decimal number in [0, max] filling `text` exactly.
bool parse_uint(std::string_view text, uint32_t max, uint32_t& out) {
  if (text.empty() || text.size() > 10) return false;
  uint64_t v = 0;
  for (const char c : text) {
    if (c < '0' || c > '9') return false;
    v = v * 10 + static_cast<uint64_t>(c - '0');
  }
  if (v > max) return false;
  out = static_cast<uint32_t>(v);
  return true;
}

bool parse_v4(std::string_view text, uint8_t out[4]) {
  for (int i = 0; i < 4; i++) {
    const size_t dot = i < 3 ? text.find('.') : text.size();
    if (dot == std::string_view::npos) return false;
    uint32_t v = 0;
    if (!parse_uint(text.substr(0, dot), 255, v)) return false;
    out[i] = static_cast<uint8_t>(v);
    text.remove_prefix(i < 3 ? dot + 1 : dot);
  }
  return text.empty();
}

int hex_digit(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

// Colon-separated hex groups into `groups`, at most `most`; false on anything else.
bool parse_groups(std::string_view text, uint16_t* groups, int most, int& count) {
  count = 0;
  if (text.empty()) return true;
  while (true) {
    const size_t colon = text.find(':');
    const std::string_view g = text.substr(0, colon);
    if (g.empty() || g.size() > 4 || count == most) return false;
    uint32_t v = 0;
    for (const char c : g) {
      const int d = hex_digit(c);
      if (d < 0) return false;
      v = v * 16 + static_cast<uint32_t>(d);
    }
    groups[count++] = static_cast<uint16_t>(v);
    if (colon == std::string_view::npos) return true;
    text.remove_prefix(colon + 1);
  }
}

bool parse_v6(std::string_view text, uint8_t out[16]) {
  uint16_t head[8], tail[8];
  int nh = 0, nt = 0;
  const size_t fold = text.find("::");
  if (fold == std::string_view::npos) {
    if (!parse_groups(text, head, 8, nh) || nh != 8) return false;
  } else {
    if (text.find("::", fold + 1) != std::string_view::npos) return false;
    if (!parse_groups(text.substr(0, fold), head, 7, nh)) return false;
    if (!parse_groups(text.substr(fold + 2), tail, 7 - nh, nt)) return false;
  }
  uint16_t g[8] = {};
  for (int i = 0; i < nh; i++) g[i] = head[i];
  for (int i = 0; i < nt; i++) g[8 - nt + i] = tail[i];
  for (size_t i = 0; i < 8; i++) {
    out[2 * i] = static_cast<uint8_t>(g[i] >> 8);
    out[2 * i + 1] = static_cast<uint8_t>(g[i] & 0xff);
  }
  return true;
}

}  // namespace

bool operator==(const Address& a, const Address& b) {
  return a.family == b.family && a.port == b.port &&
         std::memcmp(a.bytes, b.bytes, used_bytes(a.family)) == 0;
}

Address ipv4(uint8_t a, uint8_t b, uint8_t c, uint8_t d, uint16_t port) {
  Address r;
  r.family = Family::ipv4;
  r.bytes[0] = a;
  r.bytes[1] = b;
  r.bytes[2] = c;
  r.bytes[3] = d;
  r.port = port;
  return r;
}

Address loopback_v4(uint16_t port) { return ipv4(127, 0, 0, 1, port); }

Address sim_address(uint32_t node, uint16_t port) {
  Address r;
  r.family = Family::sim;
  for (int i = 0; i < 4; i++) r.bytes[i] = static_cast<uint8_t>(node >> (8 * i));
  r.port = port;
  return r;
}

uint32_t sim_node(const Address& a) {
  uint32_t n = 0;
  for (int i = 0; i < 4; i++) n |= static_cast<uint32_t>(a.bytes[i]) << (8 * i);
  return n;
}

void sim_to_ipv4(const Address& a, uint8_t out[4]) {
  const uint32_t n = sim_node(a);
  out[0] = 198;
  out[1] = static_cast<uint8_t>(18 + ((n >> 16) & 1));
  out[2] = static_cast<uint8_t>(n >> 8);
  out[3] = static_cast<uint8_t>(n);
}

Address ipv4_or_sim(const uint8_t ip[4], uint16_t port) {
  Address a;
  if (ip[0] == 198 && (ip[1] & 0xfe) == 18) {
    a = sim_address(static_cast<uint32_t>((ip[1] & 1) << 16 | ip[2] << 8 | ip[3]), port);
  } else {
    a.family = Family::ipv4;
    for (int i = 0; i < 4; i++) a.bytes[i] = ip[i];
    a.port = port;
  }
  return a;
}

bool parse_address(std::string_view text, Address& out) {
  out = Address{};
  uint32_t port = 0;
  if (text.substr(0, 4) == "sim:") {
    text.remove_prefix(4);
    const size_t colon = text.find(':');
    uint32_t node = 0;
    if (colon == std::string_view::npos || !parse_uint(text.substr(0, colon), 0xffffffffu, node) ||
        !parse_uint(text.substr(colon + 1), 0xffff, port)) {
      return false;
    }
    out = sim_address(node, static_cast<uint16_t>(port));
    return true;
  }
  if (!text.empty() && text[0] == '[') {
    const size_t close = text.find("]:");
    if (close == std::string_view::npos || !parse_v6(text.substr(1, close - 1), out.bytes) ||
        !parse_uint(text.substr(close + 2), 0xffff, port)) {
      out = Address{};
      return false;
    }
    out.family = Family::ipv6;
    out.port = static_cast<uint16_t>(port);
    return true;
  }
  const size_t colon = text.rfind(':');
  if (colon == std::string_view::npos || !parse_v4(text.substr(0, colon), out.bytes) ||
      !parse_uint(text.substr(colon + 1), 0xffff, port)) {
    out = Address{};
    return false;
  }
  out.family = Family::ipv4;
  out.port = static_cast<uint16_t>(port);
  return true;
}

std::string to_string(const Address& a) {
  char b[64];
  switch (a.family) {
    case Family::none:
      return "none";
    case Family::ipv4:
      std::snprintf(b, sizeof b, "%u.%u.%u.%u:%u", a.bytes[0], a.bytes[1], a.bytes[2], a.bytes[3],
                    a.port);
      return b;
    case Family::sim:
      std::snprintf(b, sizeof b, "sim:%u:%u", sim_node(a), a.port);
      return b;
    case Family::ipv6:
      break;
  }
  uint16_t g[8];
  for (size_t i = 0; i < 8; i++) {
    g[i] = static_cast<uint16_t>(a.bytes[2 * i] << 8 | a.bytes[2 * i + 1]);
  }
  // The longest run of two or more zero groups (the first on a tie) is folded to "::".
  int best = -1, best_len = 1;
  for (int i = 0; i < 8;) {
    int j = i;
    while (j < 8 && g[j] == 0) j++;
    if (j - i > best_len) {
      best = i;
      best_len = j - i;
    }
    i = j == i ? i + 1 : j;
  }
  std::string s = "[";
  for (int i = 0; i < 8; i++) {
    if (i == best) {
      s += "::";
      i += best_len - 1;
      continue;
    }
    if (i > 0 && i != best + best_len) s += ':';
    std::snprintf(b, sizeof b, "%x", g[i]);
    s += b;
  }
  std::snprintf(b, sizeof b, "]:%u", a.port);
  return s + b;
}

uint64_t address_hash(const Address& a) {
  uint64_t h = 0xcbf29ce484222325ull;
  const auto mix = [&h](uint8_t v) {
    h ^= v;
    h *= 0x100000001b3ull;
  };
  mix(static_cast<uint8_t>(a.family));
  for (size_t i = 0; i < used_bytes(a.family); i++) mix(a.bytes[i]);
  mix(static_cast<uint8_t>(a.port));
  mix(static_cast<uint8_t>(a.port >> 8));
  return h;
}

}  // namespace ge::net
