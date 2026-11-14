// WebSocket (websocket.hpp): base64 for the handshake (SHA-1 is sha1.hpp), the upgrade and frames.
//
// ge:summary WebSocket defined: base64, the upgrade, frames
// ge:keywords websocket sha1 base64 handshake frame
// ge:skill net
// ge:test tests/tv_web_test.cpp
#include "net/websocket.hpp"

#include <cstring>

namespace ge::net {

namespace {

std::string lower(std::string_view s) {
  std::string o(s);
  for (char& c : o) c = static_cast<char>(c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c);
  return o;
}

std::string_view trim(std::string_view s) {
  while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) s.remove_prefix(1);
  while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r'))
    s.remove_suffix(1);
  return s;
}

}  // namespace

std::string base64(std::span<const uint8_t> b) {
  static const char* k = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::string o;
  for (size_t i = 0; i < b.size(); i += 3) {
    const uint32_t v = static_cast<uint32_t>(b[i]) << 16 |
                       (i + 1 < b.size() ? static_cast<uint32_t>(b[i + 1]) << 8 : 0) |
                       (i + 2 < b.size() ? b[i + 2] : 0);
    o += k[(v >> 18) & 63];
    o += k[(v >> 12) & 63];
    o += i + 1 < b.size() ? k[(v >> 6) & 63] : '=';
    o += i + 2 < b.size() ? k[v & 63] : '=';
  }
  return o;
}

std::string ws_accept(std::string_view key) {
  const std::string s = std::string(key) + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
  const auto h = sha1({reinterpret_cast<const uint8_t*>(s.data()), s.size()});
  return base64(h);
}

int ws_handshake(std::span<const uint8_t> in, std::string& reply, size_t& used, std::string& error,
                 std::string* path) {
  const std::string_view text(reinterpret_cast<const char*>(in.data()), in.size());
  const size_t end = text.find("\r\n\r\n");
  if (end == std::string_view::npos) {
    if (in.size() > kWsMostHandshake) {
      error = "the upgrade request is longer than " + std::to_string(kWsMostHandshake) + " bytes";
      return -1;
    }
    return 0;
  }
  used = end + 4;
  std::string_view head = text.substr(0, end);
  if (head.rfind("GET ", 0) != 0) {
    error = "not an HTTP GET";
    return -1;
  }
  if (path != nullptr) {
    const std::string_view line = head.substr(4, head.find("\r\n") - 4);
    *path = std::string(line.substr(0, line.find(' ')));
  }
  std::string key;
  bool upgrade = false;
  size_t at = head.find("\r\n");
  while (at != std::string_view::npos) {
    const size_t next = head.find("\r\n", at + 2);
    const std::string_view line = head.substr(
        at + 2, next == std::string_view::npos ? std::string_view::npos : next - at - 2);
    const size_t colon = line.find(':');
    if (colon != std::string_view::npos) {
      const std::string name = lower(trim(line.substr(0, colon)));
      const std::string_view value = trim(line.substr(colon + 1));
      if (name == "sec-websocket-key") key = std::string(value);
      if (name == "upgrade" && lower(value) == "websocket") upgrade = true;
    }
    at = next;
  }
  if (!upgrade || key.empty()) {
    error = "not a WebSocket upgrade (no Upgrade: websocket or no key)";
    return -1;
  }
  reply =
      "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
      "Sec-WebSocket-Accept: " +
      ws_accept(key) + "\r\n\r\n";
  return 1;
}

std::vector<uint8_t> ws_frame(WsOp op, std::span<const uint8_t> payload, const uint8_t* mask) {
  std::vector<uint8_t> f;
  f.push_back(static_cast<uint8_t>(0x80 | static_cast<uint8_t>(op)));
  const uint8_t m = mask != nullptr ? 0x80 : 0;
  const size_t n = payload.size();
  if (n < 126) {
    f.push_back(static_cast<uint8_t>(m | n));
  } else if (n <= 0xffff) {
    f.push_back(static_cast<uint8_t>(m | 126));
    f.push_back(static_cast<uint8_t>(n >> 8));
    f.push_back(static_cast<uint8_t>(n));
  } else {
    f.push_back(static_cast<uint8_t>(m | 127));
    for (int i = 7; i >= 0; i--)
      f.push_back(static_cast<uint8_t>(static_cast<uint64_t>(n) >> (8 * i)));
  }
  if (mask != nullptr) f.insert(f.end(), mask, mask + 4);
  const size_t at = f.size();
  f.insert(f.end(), payload.begin(), payload.end());
  if (mask != nullptr)
    for (size_t i = 0; i < n; i++) f[at + i] ^= mask[i % 4];
  return f;
}

int ws_parse(std::span<const uint8_t> in, WsFrame& out) {
  if (in.size() < 2) return 0;
  if ((in[0] & 0x70) != 0) return -1;
  out.fin = (in[0] & 0x80) != 0;
  out.op = static_cast<WsOp>(in[0] & 0x0f);
  const bool masked = (in[1] & 0x80) != 0;
  uint64_t n = in[1] & 0x7f;
  size_t at = 2;
  if (n == 126) {
    if (in.size() < 4) return 0;
    n = static_cast<uint64_t>(in[2]) << 8 | in[3];
    at = 4;
  } else if (n == 127) {
    if (in.size() < 10) return 0;
    n = 0;
    for (int i = 0; i < 8; i++) n = n << 8 | in[2 + static_cast<size_t>(i)];
    at = 10;
  }
  if (n > kWsMostFrame) return -1;
  uint8_t mask[4] = {};
  if (masked) {
    if (in.size() < at + 4) return 0;
    std::memcpy(mask, in.data() + at, 4);
    at += 4;
  }
  if (in.size() < at + n) return 0;
  out.payload.assign(in.begin() + static_cast<std::ptrdiff_t>(at),
                     in.begin() + static_cast<std::ptrdiff_t>(at + n));
  if (masked)
    for (size_t i = 0; i < out.payload.size(); i++) out.payload[i] ^= mask[i % 4];
  out.size = at + static_cast<size_t>(n);
  return 1;
}

}  // namespace ge::net
