// Tests: a live broadcast played as a growing demo (games/shooters_edge/client/tv_live.hpp). A
// recording broadcast a second at a time waits for its lead, opens, grows, holds its last frame
// while the next second has not come, and ends only once the broadcast has; a refusal, a broken
// wire and an address that does not read are said in a player's words. Over loopback UDP, the
// window's end joins a TV node and plays its broadcast to the end.
//
// ge:summary Tests: a live broadcast played as a growing demo: lead, open, grow, hold, end
// ge:keywords test tv live broadcast growing-demo hold lead tv_watch
// ge:skill net replay testing
#include <chrono>
#include <string>
#include <vector>

#include "check.hpp"
#include "shooters_edge/client/demo_write.hpp"
#include "shooters_edge/client/recording_tv.hpp"
#include "shooters_edge/client/timedemo.hpp"
#include "shooters_edge/client/tv_live.hpp"
#include "net/address.hpp"
#include "net/transport.hpp"
#include "tv/net_link.hpp"
#include "tv/node.hpp"

namespace cl = shooters_edge::client;

namespace {

// A client recording of `frames` frames, broadcast in `seconds` one-second blocks: the file after
// each second (the header alone first), the broadcast's end last.
std::vector<std::vector<uint8_t>> broadcast(int frames, int seconds) {
  const std::vector<uint8_t> demo =
      cl::write_client_demo(cl::loop_script("halls", {0, 0, 24}, 64, frames));
  cl::RecordingBroadcast b;
  cl::recording_broadcast_begin(b);
  cl::recording_broadcast_tick(b, 0);  // the broadcast's clock starts
  std::vector<std::vector<uint8_t>> out{b.out};
  const size_t piece = demo.size() / static_cast<size_t>(seconds) + 1;
  for (int s = 0; s < seconds; s++) {
    const size_t at = std::min(demo.size(), piece * static_cast<size_t>(s));
    const size_t to = std::min(demo.size(), at + piece);
    cl::recording_broadcast_add(b, std::span<const uint8_t>(demo).subspan(at, to - at));
    cl::recording_broadcast_tick(b, 1000 * (s + 1));
    out.push_back(b.out);
  }
  cl::recording_broadcast_end(b);
  out.push_back(b.out);
  return out;
}

}  // namespace

TEST(a_broadcast_waits_for_its_lead_then_grows_and_holds_until_it_ends) {
  const std::vector<std::vector<uint8_t>> files = broadcast(72 * 4, 4);
  cl::TvLive live;
  ge::tv::Watch watch;
  cl::Timedemo t;
  t.paced = true;
  std::string line, error;
  CHECK(cl::tv_live_take(live, watch, t, line, error) == cl::TvLiveTake::waiting);
  watch.have_header = true;
  watch.stream = files[0];
  CHECK(cl::tv_live_take(live, watch, t, line, error) == cl::TvLiveTake::waiting);
  watch.stream = files[1];
  CHECK(cl::tv_live_take(live, watch, t, line, error) == cl::TvLiveTake::waiting);
  CHECK(!t.active);
  watch.stream = files[2];
  CHECK_MSG(cl::tv_live_take(live, watch, t, line, error) == cl::TvLiveTake::opened,
            line + " " + error);
  CHECK(t.active && live.open && live.unpacker.blocks == cl::kLiveLead);
  const size_t opened = t.demo.bytes.size();

  // Played flat out past what has come: the end holds the last frame while the broadcast goes on.
  int frames = 0;
  cl::TimedemoStep st = cl::TimedemoStep::frame;
  for (int i = 0; i < 100000 && st != cl::TimedemoStep::end; i++) {
    st = cl::playdemo_step(t, 0.05, 1.0, error);
    CHECK_MSG(st != cl::TimedemoStep::error, error);
    if (st == cl::TimedemoStep::error) return;
    const cl::TimedemoStep held = cl::tv_live_hold(live, watch, st, t);
    if (st == cl::TimedemoStep::end) {
      CHECK(held == cl::TimedemoStep::frame);
      break;
    }
    frames++;
  }
  CHECK(st == cl::TimedemoStep::end && frames > 0);
  const double held_at = t.clock;
  CHECK(held_at == t.demo.state.time);

  watch.stream = files[3];
  CHECK(cl::tv_live_take(live, watch, t, line, error) == cl::TvLiveTake::playing);
  CHECK(t.demo.bytes.size() > opened);
  watch.stream = files.back();
  watch.ended = true;
  CHECK(cl::tv_live_take(live, watch, t, line, error) == cl::TvLiveTake::playing);
  st = cl::TimedemoStep::frame;
  for (int i = 0; i < 100000 && st != cl::TimedemoStep::end; i++) {
    st = cl::tv_live_hold(live, watch, cl::playdemo_step(t, 0.05, 1.0, error), t);
    CHECK_MSG(st != cl::TimedemoStep::error, error);
    if (st == cl::TimedemoStep::error) return;
  }
  CHECK(st == cl::TimedemoStep::end);  // ended: the end is the end
  CHECK(t.demo.state.time > held_at);
}

TEST(a_refusal_and_a_broken_wire_are_said_to_the_player) {
  cl::TvLive live;
  cl::Timedemo t;
  std::string line, error;
  ge::tv::Watch refused;
  refused.refused = "the broadcast is full";
  CHECK(cl::tv_live_take(live, refused, t, line, error) == cl::TvLiveTake::failed);
  CHECK(line.find("the broadcast is full") != std::string::npos);
  ge::tv::Watch broken;
  broken.error = "a block of 9 bytes past its end";
  line.clear();
  CHECK(cl::tv_live_take(live, broken, t, line, error) == cl::TvLiveTake::failed);
  CHECK(error == broken.error && line.find('`') == std::string::npos);
  ge::tv::Watch garbage;
  garbage.have_header = true;
  garbage.stream = {'n', 'o', 't', ' ', 'a', ' ', 'f', 'i', 'l', 'e', 0, 0, 0, 0, 0, 0};
  line.clear();
  CHECK(cl::tv_live_take(live, garbage, t, line, error) == cl::TvLiveTake::failed);
  CHECK(line == "This broadcast cannot be shown here.");
}

TEST(a_node_that_does_not_read_is_refused_and_no_broadcast_does_nothing) {
  cl::TvWatch w;
  cl::Timedemo t;
  std::string why;
  CHECK(cl::tv_watch_feed(w, t, 0, why) && why.empty() && !w.on && !t.paced);
  w.node = "not an address:x";
  CHECK(!cl::tv_watch_feed(w, t, 0, why));
  CHECK(why.starts_with("The broadcast could not be joined: ") && !w.on && w.node.empty());
  CHECK(cl::tv_watch_feed(w, t, 0, why));  // given up: nothing more
}

TEST(the_window_joins_a_node_over_loopback_and_plays_its_broadcast) {
  namespace gn = ge::net;
  const std::vector<std::vector<uint8_t>> files = broadcast(72 * 3, 3);
  gn::Transport port;
  std::string error;
  CHECK_MSG(gn::open_udp_transport(port, gn::ipv4(127, 0, 0, 1, 27987), &error), error);
  if (!error.empty()) return;
  ge::tv::TvHost host;
  ge::tv::tv_host_init(host, ge::tv::Config{}, port, 4, 9);
  CHECK(ge::tv::tv_feed(host.tv, files.back(), 0));
  ge::tv::tv_end(host.tv);

  const auto now_us = [] {
    return static_cast<int64_t>(std::chrono::duration_cast<std::chrono::microseconds>(
                                    std::chrono::steady_clock::now().time_since_epoch())
                                    .count());
  };
  // The window's clock is far from zero: the link must start on it, not on zero.
  cl::TvWatch w{"127.0.0.1:27987"};
  cl::Timedemo t;
  std::string why;
  const int64_t start = now_us();
  bool ended = false;
  while (now_us() - start < 10'000'000 && !ended) {
    ge::tv::tv_host_update(host, now_us());
    CHECK_MSG(cl::tv_watch_feed(w, t, now_us(), why), why);
    if (!why.empty()) break;
    ended = t.active && w.client.watch.ended;
  }
  CHECK(w.on && t.active && t.paced && w.live.open);
  CHECK(t.name == "the broadcast on 127.0.0.1:27987");
  CHECK(ended);
  cl::TimedemoStep st = cl::TimedemoStep::frame;
  for (int i = 0; i < 100000 && st != cl::TimedemoStep::end; i++) {
    st = cl::tv_live_hold(w.live, w.client.watch, cl::playdemo_step(t, 0.05, 1.0, error), t);
    CHECK_MSG(st != cl::TimedemoStep::error, error);
    if (st == cl::TimedemoStep::error) break;
  }
  CHECK(st == cl::TimedemoStep::end && t.demo.state.time > 2.0);
  cl::tv_watch_stop(w);
  CHECK(!w.on);
  gn::close_transport(port);
}
