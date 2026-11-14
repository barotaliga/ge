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
#include "net/websocket.hpp"
#include "tv/node.hpp"

namespace ge::tv {

inline constexpr int kWebLinks = 1 << 16;  // the first link number a web viewer takes
// The codec is net/websocket.hpp's, named here as the node's viewers have always used it.
using net::base64;
using net::kWsMostFrame;
using net::kWsMostHandshake;
using net::sha1;
using net::ws_accept;
using net::ws_frame;
using net::ws_handshake;
using net::ws_parse;
using net::WsFrame;
using net::WsOp;

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
