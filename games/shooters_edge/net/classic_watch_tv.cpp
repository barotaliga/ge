// setv --watch: a server's spectator view broadcast live (classic_watch_tv.hpp).
//
// ge:summary setv --watch defined: the watcher feeding a TV host with WebSocket viewers, listed
// ge:keywords watch-tv setv live broadcast tv-node watcher retry
// ge:skill net replay
// ge:test tests/shooters_edge_classic_watch_tv_test.cpp
#include "shooters_edge/net/classic_watch_tv.hpp"

#include <span>
#include <string>
#include <utility>

#include "net/node.hpp"
#include "net/packet.hpp"
#include "tv/node.hpp"

namespace shooters_edge::net {

namespace {

constexpr uint32_t kListedVersion = 1;

void say(WatchTv& w, std::string line) { w.log.push_back(std::move(line)); }

// A new broadcast: a fresh node (its viewers let go: they join this one anew) and a watcher.
void begin(WatchTv& w, int64_t now_us) {
  for (size_t i = 0; i < w.host.net.peers.size(); ++i) {
    if (w.host.net.peers[i].state == gn::PeerState::connected)
      gn::node_disconnect(w.host.net, static_cast<int>(i), now_us);
  }
  if (w.has_web) {
    const gn::Address at = w.web.listener.local;
    ge::tv::tv_web_close(w.web);
    std::string error;
    w.has_web = ge::tv::tv_web_listen(w.web, at, error);
    if (!w.has_web) say(w, "the WebSocket viewers' port did not open again: " + error);
  }
  ge::tv::Config c;
  c.name = w.config.name;
  c.delay_ms = static_cast<int64_t>(w.config.delay_s * 1000);
  c.max_viewers = w.config.max_viewers;
  w.host.tv = ge::tv::Node{};
  w.host.tv.config = c;
  w.watcher = std::make_unique<ClassicWatcher>();
  ClientConfig cc;
  cc.name = w.config.watcher;
  cc.password = w.config.server_password;
  cc.fetch_looks = false;  // the viewers have their own
  cc.seed = static_cast<int>(w.config.seed % 100000) + w.broadcasts;
  cc.qport =
      static_cast<uint16_t>(1 + (w.config.seed + static_cast<uint64_t>(w.broadcasts)) % 65000);
  watcher_start(*w.watcher, w.up, w.server, cc, {{"title", w.config.name}}, now_us);
  w.broadcasts++;
  w.retry_at_us = -1;
  say(w,
      "watching " + gn::to_string(w.server) + " (broadcast " + std::to_string(w.broadcasts) + ")");
}

}  // namespace

bool watch_tv_open(WatchTv& w, const gn::Address& bind, const gn::Address& server,
                   const WatchTvConfig& config, std::string& error) {
  gn::Address any = bind;
  any.port = 0;
  if (!gn::open_udp_transport(w.up, any, &error)) return false;
  if (!gn::open_udp_transport(w.down, bind, &error)) return false;
  if (config.web_port != 0) {
    gn::Address at = bind;
    at.port = config.web_port;
    if (!ge::tv::tv_web_listen(w.web, at, error)) return false;
    w.has_web = true;
  }
  watch_tv_start(w, server, config, 0);
  return true;
}

void watch_tv_start(WatchTv& w, const gn::Address& server, const WatchTvConfig& config,
                    int64_t now_us) {
  w.config = config;
  w.server = server;
  ge::tv::tv_host_init(w.host, {}, w.down, config.max_viewers, config.seed);
  w.host.source = gn::to_string(server);
  w.host.public_protocol = gn::protocol_id("shooters_edge", 1);
  if (w.has_web) w.host.web = &w.web;
  if (!config.masters.empty())
    ge::tv::tv_host_list(w.host, config.masters, kListedVersion, config.seed + 1);
  begin(w, now_us);
}

void watch_tv_update(WatchTv& w, int64_t now_us) {
  if (w.retry_at_us >= 0 && now_us >= w.retry_at_us) begin(w, now_us);
  if (w.watcher && w.retry_at_us < 0) {
    const ClientStage was = w.watcher->client.stage;
    watcher_tick(*w.watcher, now_us);
    const std::span<const uint8_t> fresh = client::recording_broadcast_fresh(w.watcher->cast);
    if (!fresh.empty() && !ge::tv::tv_feed(w.host.tv, fresh, now_us / 1000)) {
      say(w, "the broadcast stopped: " + w.host.tv.error);
      watcher_stop(*w.watcher);
    }
    if (was != ClientStage::active && w.watcher->client.stage == ClientStage::active)
      say(w, "in the match on " + gn::to_string(w.server));
    if (w.watcher->ended) {
      const std::string why = w.watcher->client.refused.empty()
                                  ? std::string(client_stage_name(was))
                                  : w.watcher->client.refused;
      ge::tv::tv_end(w.host.tv);
      say(w, "the watcher left " + gn::to_string(w.server) + " (" + why + "); again in " +
                 std::to_string(static_cast<int>(w.config.retry_s)) + " s");
      w.retry_at_us = now_us + static_cast<int64_t>(w.config.retry_s * 1e6);
    }
  }
  ge::tv::tv_host_update(w.host, now_us);
}

std::string watch_tv_status(const WatchTv& w) {
  const ge::tv::Node& n = w.host.tv;
  std::string s = "tv: watching " + gn::to_string(w.server) + ", broadcast " +
                  std::to_string(w.broadcasts) +
                  (w.retry_at_us >= 0 ? " (waiting to join again)"
                   : n.have_header    ? ""
                                      : " (joining)") +
                  ", viewers " + std::to_string(ge::tv::tv_viewer_count(n)) + ", sent " +
                  std::to_string(n.bytes_out / 1024) + " KiB";
  return s;
}

void watch_tv_close(WatchTv& w, int64_t now_us) {
  if (w.watcher && !w.watcher->ended) watcher_stop(*w.watcher);
  ge::tv::tv_host_unlist(w.host, now_us);
  if (w.has_web) ge::tv::tv_web_close(w.web);
  gn::close_transport(w.down);
  gn::close_transport(w.up);
}

}  // namespace shooters_edge::net
