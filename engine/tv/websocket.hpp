// WEBSOCKET VIEWERS (RFC 6455) for a TV node: a browser (the hub's web viewer, docs/hub.md) or any
// client that speaks WebSocket watches over a plain TCP connection. The handshake is the HTTP
// upgrade (the key's SHA-1 in base64 back as the accept); then every TV message (node.hpp's wire)
// is one binary frame each way. A client's frames are masked, the node's are not; a ping is
// answered with a pong; a close is answered and the link dropped. A frame larger than
// `kWsMostFrame` or a broken handshake drops the link.
//
// `TvWeb` is the listener and its links, run beside a TV host (net_link.hpp): its links are the
// node's viewers with link numbers from `kWebLinks` up, so the host's one update serves both.
//
// ge:summary WebSocket viewers for a TV node: the upgrade, frames, ping and close, over TCP
// ge:keywords tv websocket ws rfc6455 browser web-viewer handshake upgrade sha1 base64 frame mask
// ge:keywords ping pong close hub tv_web
// ge:skill replay net
// ge:test tests/tv_web_test.cpp
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "net/address.hpp"
#include "net/tcp_socket.hpp"
#include "tv/node.hpp"

namespace ge::tv {

inline constexpr int kWebLinks = 1 << 16;                // the first link number a web viewer takes
inline constexpr size_t kWsMostFrame = size_t{1} << 20;  // a bigger frame drops the link
inline constexpr size_t kWsMostHandshake = 8192;

std::array<uint8_t, 20> sha1(std::span<const uint8_t> bytes);
std::string base64(std::span<const uint8_t> bytes);
// The `Sec-WebSocket-Accept` for a client's `Sec-WebSocket-Key`.
std::string ws_accept(std::string_view key);

// The HTTP upgrade request at the start of `in`: 1 and the response in `reply` with the request's
// length in `used`; 0 when it is not whole yet; -1 with `error` when it is not an upgrade.
int ws_handshake(std::span<const uint8_t> in, std::string& reply, size_t& used, std::string& error);

enum class WsOp : uint8_t { more = 0, text = 1, binary = 2, close = 8, ping = 9, pong = 10 };

struct WsFrame {
  WsOp op = WsOp::binary;
  bool fin = true;
  std::vector<uint8_t> payload;  // unmasked
  size_t size = 0;               // the frame's bytes in the stream
};

// A frame (`mask` a client's 4 bytes, or none from a server).
std::vector<uint8_t> ws_frame(WsOp op, std::span<const uint8_t> payload,
                              const uint8_t* mask = nullptr);
// The frame at the start of `in`: 1 and `out`; 0 when it is not whole; -1 when it breaks the rules
// (too big, a reserved bit).
int ws_parse(std::span<const uint8_t> in, WsFrame& out);

struct WebLink {
  int link = -1;
  net::TcpStream stream;
  std::vector<uint8_t> in;
  bool upgraded = false;
  bool closing = false;
};

struct TvWeb {
  net::TcpListener listener;
  std::vector<WebLink> links;
  int next_link = kWebLinks;
  int64_t refused = 0;  // links dropped for a broken handshake or frame
};

// Listen at `bind` (port 0: any free one); false with `error`.
bool tv_web_listen(TvWeb& web, const net::Address& bind, std::string& error);
void tv_web_close(TvWeb& web);
// Accept, read, upgrade, and hand every whole message to `node` (joining and leaving it too).
void tv_web_receive(TvWeb& web, Node& node);
// A message to a web viewer; false when its link will take no more now (or it is gone).
bool tv_web_send(TvWeb& web, int link, std::span<const uint8_t> msg);
// Push what is queued to the OS.
void tv_web_flush(TvWeb& web);

}  // namespace ge::tv
