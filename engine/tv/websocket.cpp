// WebSocket viewers (websocket.hpp): base64 for the handshake (SHA-1 is net/sha1.hpp), the upgrade,
// frames, and the links run beside a TV node.
//
// ge:summary WebSocket viewers: base64, the upgrade, frames, the links beside a TV node
// ge:keywords tv websocket sha1 base64 handshake frame link
// ge:skill replay net
// ge:test tests/tv_web_test.cpp
#include "tv/websocket.hpp"

#include <algorithm>
#include <cstring>

namespace ge::tv {

namespace {

WebLink* link_of(TvWeb& web, int link) {
  for (WebLink& l : web.links)
    if (l.link == link) return &l;
  return nullptr;
}

}  // namespace

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
