// A live TV broadcast played as a growing demo (tv_live.hpp).
//
// ge:summary A live broadcast's stream unpacked onto a growing demo; the window's TV client
// ge:keywords tv live broadcast growing-demo tv_watch hold
// ge:skill net replay
// ge:test tests/shooters_edge_tv_live_test.cpp
#include "shooters_edge/client/tv_live.hpp"

#include <chrono>
#include <utility>

#include "shooters_edge/client/demo.hpp"

namespace shooters_edge::client {

TvLiveTake tv_live_take(TvLive& live, const ge::tv::Watch& watch, Timedemo& t, std::string& line,
                        std::string& error) {
  if (!watch.refused.empty()) {
    line = "The broadcast turned this viewer away: " + watch.refused;
    return TvLiveTake::failed;
  }
  if (!watch.error.empty()) {
    line = "The broadcast sent something this viewer cannot read.";
    error = watch.error;
    return TvLiveTake::failed;
  }
  if (!watch.have_header) return TvLiveTake::waiting;
  std::vector<uint8_t>& into = live.open ? t.demo.bytes : live.opening;
  std::string cut;
  if (!unpack_more(live.unpacker, watch.stream, into, cut)) {
    line = "This broadcast cannot be shown here.";
    error = cut;
    return TvLiveTake::failed;
  }
  if (live.open) return TvLiveTake::playing;
  if (live.unpacker.blocks < kLiveLead || live.opening.empty()) return TvLiveTake::waiting;
  if (!open_demo(std::move(live.opening), t.demo, error)) {
    line = "This broadcast cannot be shown here.";
    return TvLiveTake::failed;
  }
  t.start = t.demo;
  t.active = true;
  live.open = true;
  return TvLiveTake::opened;
}

TimedemoStep tv_live_hold(const TvLive& live, const ge::tv::Watch& watch, TimedemoStep step,
                          Timedemo& t) {
  if (step != TimedemoStep::end || !live.open || watch.ended) return step;
  t.clock = t.demo.state.time;  // ran out before the next block: hold the last frame
  return TimedemoStep::frame;
}

bool tv_watch_start(TvWatch& w, const std::string& node, std::string& error) {
  ge::net::Address at;
  const std::string text = node.find(':') == std::string::npos ? node + ":27900" : node;
  if (!ge::net::parse_address(text, at)) {
    error = "'" + node + "' is no address; a broadcast is host:port";
    return false;
  }
  ge::net::Address any = ge::net::ipv4(0, 0, 0, 0, 0);
  if (!ge::net::open_udp_transport(w.socket, any, &error)) return false;
  const uint64_t seed =
      static_cast<uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count());
  ge::tv::tv_client_init(w.client, w.socket, at, "player", false, seed, 0);
  w.live = TvLive{};
  w.node = node;
  w.on = true;
  return true;
}

TvLiveTake tv_watch_frame(TvWatch& w, Timedemo& t, int64_t now_us, std::string& line,
                          std::string& error) {
  if (!w.on) return TvLiveTake::waiting;
  ge::tv::tv_client_update(w.client, now_us);
  return tv_live_take(w.live, w.client.watch, t, line, error);
}

bool tv_watch_feed(TvWatch& w, Timedemo& t, int64_t now_us, std::string& why) {
  std::string error;
  if (!w.on && !w.node.empty()) {
    const std::string node = std::exchange(w.node, std::string());
    if (!tv_watch_start(w, node, error)) {
      why = "The broadcast could not be joined: " + error;
      return false;
    }
  }
  if (!w.on) return true;
  t.paced = t.hear = t.smooth = true;  // watched as a played demo is, its failure said as one's
  if (t.name.empty()) t.name = t.found = "the broadcast on " + w.node;
  std::string line;
  const TvLiveTake got = tv_watch_frame(w, t, now_us, line, error);
  if (got == TvLiveTake::failed) {
    why = error.empty() ? line : line + " (" + error + ")";
    tv_watch_stop(w);
    return false;
  }
  return true;
}

void tv_watch_stop(TvWatch& w) {
  if (!w.on) return;
  ge::net::close_transport(w.socket);
  w.on = false;
  w.node.clear();
}

}  // namespace shooters_edge::client
