// TV over engine/net (net_link.hpp): the host's, the client's and the relay's updates.
//
// ge:summary TV over engine/net: the host's, the client's and the relay's updates
// ge:keywords tv net host client relay update
// ge:skill replay net
// ge:test tests/tv_net_test.cpp
#include "tv/net_link.hpp"

#include <algorithm>

namespace ge::tv {

namespace {

constexpr int kChannel = 0;

int64_t ms_of(int64_t us) { return us / 1000; }

}  // namespace

net::NodeConfig tv_node_config(bool host, int max_peers, uint64_t seed) {
  net::NodeConfig c;
  c.protocol = net::protocol_id("tv", kWireVersion);
  c.accept = host;
  c.max_peers = std::clamp(max_peers, 1, net::kMaxPeers);
  c.channels.count = 1;
  c.channels.delivery[0] = net::Delivery::reliable;
  c.channels.largest[0] = static_cast<uint32_t>(net::kMaxBigMessage);
  c.seed = seed;
  return c;
}

void tv_host_init(TvHost& h, const Config& config, net::Transport& transport, int max_peers,
                  uint64_t seed) {
  h.tv = Node{};
  h.tv.config = config;
  net::node_init(h.net, tv_node_config(true, max_peers, seed), transport);
}

void tv_host_update(TvHost& h, int64_t now_us) {
  net::node_update(h.net, now_us);
  for (const net::Event& e : h.net.events) {
    if (e.type == net::EventType::connected) {
      tv_join(h.tv, e.peer);
    } else if (e.type == net::EventType::disconnected) {
      tv_leave(h.tv, e.peer);
    } else if (e.type == net::EventType::message) {
      const uint8_t* d = net::event_data(h.net, e);
      if (!tv_receive(h.tv, e.peer, {d, e.size})) {
        tv_leave(h.tv, e.peer);
        net::node_disconnect(h.net, e.peer, now_us);
      }
    }
  }
  tv_update(h.tv, ms_of(now_us), [&](int link, std::span<const uint8_t> m) {
    return net::node_send(h.net, link, kChannel, m.data(), m.size());
  });
  net::node_flush(h.net, now_us);
}

void tv_client_init(TvClient& c, net::Transport& transport, const net::Address& to,
                    std::string_view name, bool relay, uint64_t seed, int64_t now_us) {
  c.watch = Watch{};
  c.name = std::string(name);
  c.relay = relay;
  net::node_init(c.net, tv_node_config(false, 1, seed), transport);
  c.peer = net::node_connect(c.net, to, now_us);
}

void tv_client_update(TvClient& c, int64_t now_us) {
  c.fresh.clear();
  net::node_update(c.net, now_us);
  for (const net::Event& e : c.net.events) {
    if (e.peer != c.peer) continue;
    if (e.type == net::EventType::message) {
      std::vector<uint8_t> fresh;
      if (!watch_receive(c.watch, {net::event_data(c.net, e), e.size}, &fresh)) continue;
      if (!fresh.empty()) c.fresh.push_back(std::move(fresh));
    } else if (e.type == net::EventType::disconnected || e.type == net::EventType::connect_failed) {
      if (c.watch.error.empty() && !c.watch.ended)
        c.watch.error = std::string("the TV link closed: ") + net::reason_name(e.reason);
    }
  }
  if (net::peer_connected(c.net, c.peer)) {
    if (!c.greeted) {
      const auto hello = tv_hello(c.name, c.relay);
      c.greeted = net::node_send(c.net, c.peer, kChannel, hello.data(), hello.size());
    }
    size_t sent = 0;
    while (c.greeted && sent < c.outbox.size() &&
           net::node_send(c.net, c.peer, kChannel, c.outbox[sent].data(), c.outbox[sent].size()))
      sent++;
    c.outbox.erase(c.outbox.begin(), c.outbox.begin() + static_cast<std::ptrdiff_t>(sent));
  }
  net::node_flush(c.net, now_us);
}

void tv_client_say(TvClient& c, std::string_view text) {
  c.outbox.push_back(tv_chat(c.name, text));
}

void tv_relay_init(TvRelay& r, const Config& config, net::Transport& up_transport,
                   const net::Address& to, net::Transport& down_transport, int max_peers,
                   uint64_t seed, int64_t now_us) {
  Config c = config;
  c.relay = true;
  tv_client_init(r.up, up_transport, to, config.name, true, seed, now_us);
  r.up.watch.keep = false;
  tv_host_init(r.down, c, down_transport, max_peers, seed + 1);
}

void tv_relay_update(TvRelay& r, int64_t now_us) {
  tv_client_update(r.up, now_us);
  for (const auto& bytes : r.up.fresh) tv_feed(r.down.tv, bytes, ms_of(now_us));
  if (r.up.watch.ended) tv_end(r.down.tv);
  // The chat: the node above's lines go down, this node's viewers' lines go up.
  for (ChatLine& line : r.up.watch.chat) r.down.tv.down.push_back(std::move(line));
  r.up.watch.chat.clear();
  for (const ChatLine& line : r.down.tv.up) r.up.outbox.push_back(tv_chat(line.who, line.text));
  r.down.tv.up.clear();
  const int64_t under = tv_viewer_count(r.down.tv);
  if (under != r.reported) {
    r.up.outbox.push_back(tv_count(under));
    r.reported = under;
  }
  r.down.tv.tree = r.up.watch.viewers;
  tv_host_update(r.down, now_us);
}

}  // namespace ge::tv
