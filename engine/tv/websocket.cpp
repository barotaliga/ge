// WebSocket viewers (websocket.hpp): SHA-1 and base64 for the handshake, the upgrade, frames, and
// the links run beside a TV node.
//
// ge:summary WebSocket viewers: SHA-1, base64, the upgrade, frames, the links beside a TV node
// ge:keywords tv websocket sha1 base64 handshake frame link
// ge:skill replay net
// ge:test tests/tv_web_test.cpp
#include "tv/websocket.hpp"

#include <algorithm>
#include <cstring>

namespace ge::tv {

namespace {

uint32_t rol(uint32_t v, int n) { return (v << n) | (v >> (32 - n)); }

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

WebLink* link_of(TvWeb& web, int link) {
  for (WebLink& l : web.links)
    if (l.link == link) return &l;
  return nullptr;
}

}  // namespace

std::array<uint8_t, 20> sha1(std::span<const uint8_t> bytes) {
  uint32_t h[5] = {0x67452301u, 0xEFCDAB89u, 0x98BADCFEu, 0x10325476u, 0xC3D2E1F0u};
  std::vector<uint8_t> m(bytes.begin(), bytes.end());
  const uint64_t bits = static_cast<uint64_t>(bytes.size()) * 8;
  m.push_back(0x80);
  while (m.size() % 64 != 56) m.push_back(0);
  for (int i = 7; i >= 0; i--) m.push_back(static_cast<uint8_t>(bits >> (8 * i)));
  for (size_t at = 0; at < m.size(); at += 64) {
    uint32_t w[80];
    for (int i = 0; i < 16; i++) {
      const size_t k = at + static_cast<size_t>(i) * 4;
      w[i] = static_cast<uint32_t>(m[k]) << 24 | static_cast<uint32_t>(m[k + 1]) << 16 |
             static_cast<uint32_t>(m[k + 2]) << 8 | m[k + 3];
    }
    for (int i = 16; i < 80; i++) w[i] = rol(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
    uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4];
    for (int i = 0; i < 80; i++) {
      uint32_t f = 0, k = 0;
      if (i < 20) {
        f = (b & c) | (~b & d);
        k = 0x5A827999u;
      } else if (i < 40) {
        f = b ^ c ^ d;
        k = 0x6ED9EBA1u;
      } else if (i < 60) {
        f = (b & c) | (b & d) | (c & d);
        k = 0x8F1BBCDCu;
      } else {
        f = b ^ c ^ d;
        k = 0xCA62C1D6u;
      }
      const uint32_t t = rol(a, 5) + f + e + k + w[i];
      e = d;
      d = c;
      c = rol(b, 30);
      b = a;
      a = t;
    }
    h[0] += a;
    h[1] += b;
    h[2] += c;
    h[3] += d;
    h[4] += e;
  }
  std::array<uint8_t, 20> out{};
  for (size_t i = 0; i < 5; i++)
    for (size_t j = 0; j < 4; j++) out[i * 4 + j] = static_cast<uint8_t>(h[i] >> (24 - 8 * j));
  return out;
}

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

int ws_handshake(std::span<const uint8_t> in, std::string& reply, size_t& used,
                 std::string& error) {
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

bool tv_web_listen(TvWeb& web, const net::Address& bind, std::string& error) {
  return net::tcp_listen(web.listener, bind, &error);
}

void tv_web_close(TvWeb& web) {
  for (WebLink& l : web.links) net::tcp_close(l.stream);
  web.links.clear();
  net::tcp_close_listener(web.listener);
}

void tv_web_receive(TvWeb& web, Node& node) {
  for (;;) {
    WebLink l;
    if (!net::tcp_accept(web.listener, l.stream)) break;
    l.link = web.next_link++;
    web.links.push_back(std::move(l));
  }
  for (WebLink& l : web.links) {
    net::tcp_pump(l.stream);
    net::tcp_take_bytes(l.stream, l.in);
    if (!l.upgraded) {
      std::string reply, error;
      size_t used = 0;
      const int got = ws_handshake(l.in, reply, used, error);
      if (got < 0) {
        l.closing = true;
        web.refused++;
        continue;
      }
      if (got == 0) continue;
      net::tcp_send_bytes(l.stream, reinterpret_cast<const uint8_t*>(reply.data()), reply.size());
      l.in.erase(l.in.begin(), l.in.begin() + static_cast<std::ptrdiff_t>(used));
      l.upgraded = true;
      tv_join(node, l.link);
    }
    size_t at = 0;
    while (!l.closing) {
      WsFrame f;
      const int got = ws_parse({l.in.data() + at, l.in.size() - at}, f);
      if (got == 0) break;
      if (got < 0) {
        l.closing = true;
        web.refused++;
        break;
      }
      at += f.size;
      if (f.op == WsOp::ping) {
        const auto pong = ws_frame(WsOp::pong, f.payload);
        net::tcp_send_bytes(l.stream, pong.data(), pong.size());
      } else if (f.op == WsOp::close) {
        const auto bye = ws_frame(WsOp::close, {});
        net::tcp_send_bytes(l.stream, bye.data(), bye.size());
        l.closing = true;
      } else if (f.op == WsOp::binary && f.fin) {
        if (!tv_receive(node, l.link, f.payload)) l.closing = true;
      }
    }
    l.in.erase(l.in.begin(), l.in.begin() + static_cast<std::ptrdiff_t>(at));
  }
  for (WebLink& l : web.links) {
    if (l.closing || l.stream.closed) {
      if (l.upgraded) tv_leave(node, l.link);
      net::tcp_pump(l.stream);
      net::tcp_close(l.stream);
    }
  }
  std::erase_if(web.links, [](const WebLink& l) { return l.closing || l.stream.closed; });
}

bool tv_web_send(TvWeb& web, int link, std::span<const uint8_t> msg) {
  WebLink* l = link_of(web, link);
  if (l == nullptr || !l->upgraded || l->closing) return false;
  const auto f = ws_frame(WsOp::binary, msg);
  if (l->stream.out.size() - l->stream.out_head + f.size() > net::kStreamBacklog) return false;
  return net::tcp_send_bytes(l->stream, f.data(), f.size());
}

void tv_web_flush(TvWeb& web) {
  for (WebLink& l : web.links) net::tcp_pump(l.stream);
}

}  // namespace ge::tv
