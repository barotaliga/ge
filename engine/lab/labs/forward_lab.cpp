// LAB `forward`: A BETTER PATH THROUGH FORWARDERS (engine/forward). A player's direct path to a
// server is given each network preset; the forwarders sit on good legs (the player to the first,
// a backbone between them, the last beside the server). For each preset the lab sends a game's
// datagrams (one every 20 ms, the server echoing each) straight, through one forwarder and through
// a chain of two, and answers how long the route took to set up, the round trip and the share
// lost each way. It also sends a forwarder every kind of ask a stranger can send and checks that
// no answer is bigger than its ask. All on a virtual clock off one seed.
//
// ge:summary Lab: a game's datagrams direct, through one forwarder and through two, per preset
// ge:keywords lab forward forwarder chain better-path round-trip loss route-setup amplification
// ge:skill net labs
// ge:test tests/forward_test.cpp
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "forward/forward.hpp"
#include "lab/chart.hpp"
#include "lab/lab.hpp"
#include "lab/stats.hpp"
#include "net/conditions.hpp"
#include "net/sim_network.hpp"
#include "net/status.hpp"
#include "net/stun.hpp"

namespace {

namespace gl = ge::lab;
namespace fw = ge::forward;
namespace net = ge::net;

std::string fmt(double v, int digits) {
  char b[32];
  std::snprintf(b, sizeof b, "%.*f", digits, v);
  return b;
}

std::vector<std::string> split(const std::string& s) {
  std::vector<std::string> out;
  size_t at = 0;
  while (at <= s.size()) {
    const size_t comma = s.find(',', at);
    const std::string part =
        s.substr(at, comma == std::string::npos ? std::string::npos : comma - at);
    if (!part.empty()) out.push_back(part);
    if (comma == std::string::npos) break;
    at = comma + 1;
  }
  return out;
}

constexpr uint32_t kPlayer = 0, kF1 = 1, kF2 = 2, kServer = 3;

struct PathRun {
  std::vector<double> rtt_ms;
  int sent = 0;
  double setup_ms = 0;
  bool routed = true;
};

// The player's datagrams over `hops` forwarders (0, 1 or 2) for `seconds`, the server echoing.
PathRun run_path(const net::LinkConditions& direct, int hops, int seconds, uint32_t seed) {
  net::SimNetwork sim = net::make_sim_network(seed, direct);
  net::LinkConditions leg;  // a good leg: near and clean
  leg.latency_ms = 8;
  leg.jitter_ms = 1;
  net::LinkConditions backbone;
  backbone.latency_ms = 12;
  backbone.jitter_ms = 1;
  net::sim_set_path(sim, kPlayer, kF1, leg);
  net::sim_set_path(sim, kPlayer, kF2, leg);
  net::sim_set_path(sim, kF1, kF2, backbone);
  net::sim_set_path(sim, kF1, kServer, backbone);
  net::sim_set_path(sim, kF2, kServer, leg);
  const net::Address f1_at = net::sim_address(kF1, 30000), f2_at = net::sim_address(kF2, 30000);
  const net::Address server_at = net::sim_address(kServer, 27500);
  net::Transport player, server, f1_port, f2_port;
  net::open_sim_transport(player, sim, net::sim_address(kPlayer, 5000));
  net::open_sim_transport(server, sim, server_at);
  net::open_sim_transport(f1_port, sim, f1_at);
  net::open_sim_transport(f2_port, sim, f2_at);
  uint16_t next_up = 40000;
  const auto opener = [&](uint32_t node) {
    return [&sim, &next_up, node](net::Transport& t) {
      return net::open_sim_transport(t, sim, net::sim_address(node, next_up++));
    };
  };
  fw::Forward f1, f2;
  fw::ForwardConfig c;
  c.name = "f1";
  c.allow = {f2_at, server_at};
  c.seed = seed + 1;
  fw::forward_init(f1, c, f1_port, opener(kF1));
  c.name = "f2";
  c.allow = {server_at};
  c.seed = seed + 2;
  fw::forward_init(f2, c, f2_port, opener(kF2));
  std::vector<net::Address> chain;
  if (hops >= 1) chain.push_back(hops == 1 ? f2_at : f1_at);
  if (hops >= 2) chain.push_back(f2_at);
  chain.push_back(server_at);
  fw::Dial d;
  int64_t now = 0;
  fw::dial_start(d, chain, seed + 3, now);
  PathRun run;
  std::vector<int64_t> sent_at;
  const int64_t step = 1000;
  int64_t next_send = 0, end = -1;
  uint8_t buf[net::kMaxDatagram];
  for (int i = 0; i < 4000000 && (end < 0 || now < end); i++) {
    fw::dial_update(d, player, now);
    if (d.state == fw::DialState::refused || d.state == fw::DialState::timed_out) {
      run.routed = false;
      break;
    }
    if (d.state == fw::DialState::routed && end < 0) {
      run.setup_ms = static_cast<double>(now - d.started_us) / 1000.0;
      end = now + int64_t{seconds} * 1000000 + 2000000;
      next_send = now;
    }
    if (end >= 0 && now >= next_send && now < end - 2000000) {
      uint8_t m[16] = {};
      const auto seq = static_cast<uint32_t>(sent_at.size());
      for (int b = 0; b < 4; b++) m[b] = static_cast<uint8_t>(seq >> (8 * b));
      sent_at.push_back(now);
      net::transport_send(player, d.first, m, sizeof m, now);
      next_send += 20000;
    }
    fw::forward_update(f1, now);
    fw::forward_update(f2, now);
    now += step;
    net::sim_advance(sim, now);
    net::Address from;
    int n = 0;
    while ((n = net::transport_receive(server, buf, sizeof buf, from, now)) >= 0)
      net::transport_send(server, from, buf, static_cast<size_t>(n), now);  // the echo
    while ((n = net::transport_receive(player, buf, sizeof buf, from, now)) >= 0) {
      if (fw::dial_read(d, buf, static_cast<size_t>(n), from, now) || n != 16) continue;
      uint32_t seq = 0;
      for (int b = 0; b < 4; b++) seq |= static_cast<uint32_t>(buf[b]) << (8 * b);
      if (seq < sent_at.size())
        run.rtt_ms.push_back(static_cast<double>(now - sent_at[seq]) / 1000.0);
    }
  }
  run.sent = static_cast<int>(sent_at.size());
  fw::forward_close(f1);
  fw::forward_close(f2);
  for (net::Transport* t : {&player, &server, &f1_port, &f2_port}) net::close_transport(*t);
  return run;
}

// Every ask a stranger can send a forwarder: the largest answer over its ask.
double worst_answer_ratio(uint32_t seed, int& asks, int& answers) {
  net::SimNetwork sim = net::make_sim_network(seed, net::LinkConditions{});
  net::Transport port, asker, up;
  net::open_sim_transport(port, sim, net::sim_address(1, 30000));
  net::open_sim_transport(asker, sim, net::sim_address(0, 6000));
  fw::Forward f;
  fw::ForwardConfig c;
  c.open = true;
  c.seed = seed;
  uint16_t next_up = 40000;
  fw::forward_init(f, c, port, [&](net::Transport& t) {
    return net::open_sim_transport(t, sim, net::sim_address(1, next_up++));
  });
  std::vector<std::vector<uint8_t>> list;
  uint8_t route[fw::kForwardRouteSize] = {'g', 'e', 'F', 'W', 1};
  list.emplace_back(route, route + sizeof route);
  list.emplace_back(route, route + 13);
  uint8_t alive[29] = {'g', 'e', 'F', 'W', 4};
  list.emplace_back(alive, alive + sizeof alive);
  uint8_t status[net::kStatusAskSize];
  net::status_ask(fw::forward_protocol(), 1, 0, status);
  list.emplace_back(status, status + sizeof status);
  uint8_t stun[net::kStunRequestSize];
  const uint8_t id[12] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
  net::stun_request(stun, id);
  list.emplace_back(stun, stun + sizeof stun);
  for (size_t n = 1; n < 64; n += 7) list.emplace_back(n, static_cast<uint8_t>(n));
  double worst = 0;
  int64_t now = 0;
  asks = static_cast<int>(list.size());
  answers = 0;
  for (const auto& a : list) {
    net::transport_send(asker, port.local, a.data(), a.size(), now);
    now += 1000;
    net::sim_advance(sim, now);
    fw::forward_update(f, now);
    now += 1000;
    net::sim_advance(sim, now);
    uint8_t buf[net::kMaxDatagram];
    net::Address from;
    int n = 0;
    while ((n = net::transport_receive(asker, buf, sizeof buf, from, now)) >= 0) {
      answers++;
      worst = std::max(worst, static_cast<double>(n) / static_cast<double>(a.size()));
    }
  }
  fw::forward_close(f);
  net::close_transport(port);
  net::close_transport(asker);
  (void)up;
  return worst;
}

void run(gl::Lab& lab) {
  const int seconds = static_cast<int>(lab.arg_num("seconds"));
  const auto seed = static_cast<uint32_t>(lab.arg_num("seed"));
  gl::LabTable table{"each preset as the player's direct path; the forwarders on good legs",
                     {"direct path", "route", "set up ms", "rtt p50 ms", "rtt p95 ms", "lost %"},
                     {}};
  std::vector<gl::Sample> samples;
  bool all_routed = true;
  for (const std::string& name : split(lab.arg("presets"))) {
    net::LinkConditions direct;
    if (!net::conditions_preset(name, direct)) {
      lab.fail("unknown preset '" + name + "'",
               "next: pick from lan, broadband, wifi, mobile, bad, awful "
               "(engine/net/conditions.cpp); skill: net.");
      return;
    }
    for (int hops = 0; hops <= 2; hops++) {
      const PathRun r = run_path(direct, hops, seconds, seed);
      all_routed = all_routed && r.routed;
      const gl::Summary s = gl::summarize(r.rtt_ms);
      const double lost =
          r.sent > 0 ? 100.0 * (1.0 - static_cast<double>(r.rtt_ms.size()) / r.sent) : 100.0;
      const char* route = hops == 0 ? "direct" : hops == 1 ? "one forwarder" : "two forwarders";
      table.rows.push_back({name, route, hops == 0 ? "-" : fmt(r.setup_ms, 0), fmt(s.p50, 0),
                            fmt(s.p95, 0), fmt(lost, 1)});
      const std::string key = name + (hops == 0 ? ".direct" : hops == 1 ? ".one" : ".two");
      lab.metric(key + ".rtt-p50-ms", s.p50, "ms");
      lab.metric(key + ".lost-pct", lost, "%");
      samples.push_back({name + " " +
                             (hops == 0   ? "direct"
                              : hops == 1 ? "1 hop"
                                          : "2 hops"),
                         r.rtt_ms});
    }
  }
  lab.table(table);
  int asks = 0, answers = 0;
  const double worst = worst_answer_ratio(seed, asks, answers);
  lab.metric("answer-over-ask-max", worst, "ratio");
  lab.note("a stranger's " + std::to_string(asks) + " asks drew " + std::to_string(answers) +
           " answers; the largest was " + fmt(worst, 2) + " of its ask");
  const gl::ChartFrame frame{
      "ROUND TRIP: DIRECT AGAINST THROUGH FORWARDERS",
      "a datagram every 20 ms for " + lab.arg("seconds") + " s; seed " + lab.arg("seed"), "",
      "round trip (ms)"};
  lab.shot({"rtt",
            frame.title,
            "Each preset as the player's own path to the server, against the same game's datagrams "
            "through one forwarder and through a chain of two on good legs.",
            "the forwarded boxes low and tight while the direct path's rise with the preset",
            {{"presets", lab.arg("presets")}, {"seed", lab.arg("seed")}}},
           gl::to_picture(gl::box_chart(frame, samples).plot));
  if (!all_routed)
    lab.fail("a route was not set up",
             "next: run tests/forward_test.cpp and read the forwarders' stats; skill: net.");
  if (worst > 1.0)
    lab.fail("a forwarder answered a stranger with more bytes than it was sent",
             "next: find the ask in worst_answer_ratio and cap its answer; skill: net.");
}

const gl::LabSpec kSpec{
    "forward",
    gl::LabKind::sim,
    "engine",
    "engine/forward (the hop, the dial, chains)",
    "net",
    "Through one forwarder or a chain of two on good legs, how does a game's round trip and loss "
    "compare with the player's own path under each preset, and does any answer outgrow its ask?",
    {
        {"presets", "broadband,mobile,bad,awful", "the direct path's presets, comma-separated"},
        {"seconds", "10", "how long each path is measured"},
        {"seed", "7", "the network's stream"},
    },
    "--presets=bad --seconds=3",
    run,
};
GE_LAB(kSpec);

}  // namespace
