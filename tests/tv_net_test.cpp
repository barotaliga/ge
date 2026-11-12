// TV over engine/net (engine/tv/net_link.hpp, trial.hpp) on the simulated network: a source's
// node, a relay under it and viewers on both reach the same stream under every link preset, the
// whole of it from the keyframe block each started at (one that connects late starts later); a
// line said under the relay reaches every viewer; every node counts the whole tree; a delay holds
// every viewer back by about that much; a host listed on a master is found there as a TV node and
// answers a status ask with its name, the match's map and its viewers.
//
// ge:summary Tests TV over the simulated network: source, relay, viewers, every preset, chat, count
// ge:keywords test tv net relay tree simulated-network preset chat viewer-count delay lag
// ge:skill replay net testing
#include <algorithm>
#include <string>

#include "check.hpp"
#include "net/conditions.hpp"
#include "net/master.hpp"
#include "net/sim_network.hpp"
#include "net/status.hpp"
#include "replay/stream.hpp"
#include "tv/net_link.hpp"
#include "tv/trial.hpp"

namespace net = ge::net;
namespace tv = ge::tv;

TEST(every_viewer_gets_the_whole_stream_through_a_relay_under_every_preset) {
  for (const std::string& name : net::preset_names()) {
    tv::TrialConfig c;
    CHECK(net::conditions_preset(name, c.link));
    const tv::TrialResult r = tv::run_tv_trial(c);
    CHECK_MSG(r.all_whole, name + ": " + r.why);
    for (const tv::TrialViewer& v : r.viewers) {
      CHECK_MSG(v.chat == "viewer2: hello from the end of the line",
                name + ": heard '" + v.chat + "'");
      CHECK_MSG(v.told == 3, name + ": told " + std::to_string(v.told));
    }
    CHECK_MSG(r.counted == 3, name + ": counted " + std::to_string(r.counted));
  }
}

TEST(on_a_clean_link_every_viewer_starts_at_the_first_block) {
  tv::TrialConfig c;
  CHECK(net::conditions_preset("lan", c.link));
  const tv::TrialResult r = tv::run_tv_trial(c);
  CHECK(r.all_whole);
  for (const tv::TrialViewer& v : r.viewers) CHECK(v.first == 0 && v.blocks == c.blocks);
}

TEST(a_delay_holds_every_viewer_back_by_about_that_much) {
  tv::TrialConfig c;
  CHECK(net::conditions_preset("lan", c.link));
  c.block_ms = 1000;  // the match at its own speed
  c.blocks = 12;
  c.delay_ms = 3000;
  const tv::TrialResult r = tv::run_tv_trial(c);
  CHECK_MSG(r.all_whole, r.why);
  for (const tv::TrialViewer& v : r.viewers) {
    CHECK(!v.lag_ms.empty());
    const double least = *std::min_element(v.lag_ms.begin(), v.lag_ms.end());
    CHECK_MSG(least >= 2900 && least < 3500, "least lag " + std::to_string(least));
  }
}

TEST(a_listed_host_is_found_on_the_master_and_answers_status) {
  net::SimNetwork sim = net::make_sim_network(3, net::LinkConditions{});
  const net::Address master_at = net::sim_address(1, 27900);
  net::Transport master_port, host_port, asker;
  net::open_sim_transport(master_port, sim, master_at);
  net::open_sim_transport(host_port, sim, net::sim_address(0, 27500));
  net::open_sim_transport(asker, sim, net::sim_address(2, 9000));
  net::Master master;
  net::master_init(master, net::MasterConfig{}, master_port);
  tv::TvHost host;
  tv::Config config;
  config.name = "hall tv";
  tv::tv_host_init(host, config, host_port, 4, 5);
  tv::tv_host_list(host, {master_at}, 2, 6);
  // A match's header, so the status names its game and map.
  std::vector<uint8_t> file;
  ge::replay::StreamWriter w;
  ge::replay::StreamHeader h;
  h.game = "tv-test";
  h.meta = {{"map", "hall"}};
  ge::replay::stream_begin(w, h, file);
  CHECK(tv::tv_feed(host.tv, file, 0));

  int64_t now = 0;
  const auto tick = [&] {
    tv::tv_host_update(host, now);
    now += 20000;
    net::sim_advance(sim, now);
    net::master_update(master, now);
    net::sim_advance(sim, now);
  };
  for (int i = 0; i < 10; i++) tick();
  CHECK(host.beat.acks >= 1);
  CHECK(net::master_count(master, host.net.config.protocol, net::kind_bit(net::NodeKind::tv)) == 1);

  // A status ask: a cookie first, then the record.
  uint64_t cookie = 0;
  net::Status got;
  bool answered = false;
  for (int round = 0; round < 2 && !answered; round++) {
    uint8_t ask[net::kStatusAskSize];
    const size_t n = net::status_ask(host.net.config.protocol, 77, cookie, ask);
    net::transport_send(asker, net::sim_address(0, 27500), ask, n, now);
    for (int i = 0; i < 5; i++) tick();
    uint8_t buf[net::kMaxDatagram];
    net::Address from;
    int got_n = 0;
    while ((got_n = net::transport_receive(asker, buf, sizeof buf, from, now)) >= 0) {
      const auto r = net::status_read_reply(buf, static_cast<size_t>(got_n), 77, cookie, got);
      if (r == net::StatusReply::status) answered = true;
    }
  }
  CHECK(answered);
  CHECK(got.kind == net::NodeKind::tv);
  CHECK(got.name == "hall tv");
  CHECK(got.map == "hall");
  CHECK(got.mode == "tv-test");
  CHECK(host.status_answers >= 2);
}
