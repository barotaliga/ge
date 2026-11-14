// WEBSOCKET (RFC 6455) over a TCP stream, for whatever a browser page talks to: a TV node's
// viewers (tv/websocket.hpp) and the forwarder's face for a page without WebTransport
// (forward/websocket.hpp). The handshake is the HTTP upgrade (the key's SHA-1 in base64 back as the
// accept); then each message is one frame. A client's frames are masked, a server's are not. A
// frame larger than `kWsMostFrame` or a reserved bit breaks the rules.
//
// ge:summary WebSocket: the upgrade, frames, masks; base64 for the accept
// ge:keywords websocket ws rfc6455 browser handshake upgrade sha1 base64 frame mask ping pong close
// ge:skill net
// ge:test tests/tv_web_test.cpp
#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "net/sha1.hpp"

namespace ge::net {

inline constexpr size_t kWsMostFrame = size_t{1} << 20;  // a bigger frame drops the link
inline constexpr size_t kWsMostHandshake = 8192;

std::string base64(std::span<const uint8_t> bytes);
// The `Sec-WebSocket-Accept` for a client's `Sec-WebSocket-Key`.
std::string ws_accept(std::string_view key);

// The HTTP upgrade request at the start of `in`: 1 and the response in `reply` with the request's
// length in `used` (and its path, `/play?to=...`, in `path` when given); 0 when it is not whole
// yet; -1 with `error` when it is not an upgrade.
int ws_handshake(std::span<const uint8_t> in, std::string& reply, size_t& used, std::string& error,
                 std::string* path = nullptr);

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

}  // namespace ge::net
