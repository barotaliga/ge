// LAB `net-config`: configs over a session (engine/net/session_config.hpp, M21c), under each
// condition preset. A dedicated host runs a server cfg; four clients join. The host changes a
// setting (`sv_maxrate`) and each client's copy is timed against its round trip; each client
// changes its own `rate` and the host's reading of it is timed; the host stuffs `quit`, a player's
// setting and a command the game allows, and each client's console runs only the last; one client
// asks for 30% fake loss (`net_fakeloss`) with cheats off (refused), then with the host's
// `sv_cheats 1` (the loss the host measures on its stream rises), then the host turns cheats off
// and the loss falls back.
//
// ge:summary Lab: server info followed in a round trip, user info read, stuffed text, fake loss
// ge:keywords lab net config server-info user-info stuffed-text quit refused sv_cheats cheat
// ge:keywords net_fakeloss net_fakelag fake-link round-trip follow server-cfg net-config
// ge:skill net console labs
// ge:test tests/net_session_config_test.cpp
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "console/console.hpp"
#include "lab/chart.hpp"
#include "lab/lab.hpp"
#include "net/conditions.hpp"
#include "net/net_cvars.hpp"
#include "net/node.hpp"
#include "net/session.hpp"
#include "net/session_config.hpp"
#include "net/sim_network.hpp"
#include "net/transport.hpp"

namespace {

namespace gl = ge::lab;
namespace gn = ge::net;
namespace con = ge::console;

constexpr int64_t kTick = 10000;  // every node updates at 100 Hz
constexpr int kClients = 4;

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

std::string fmt(double v, int digits) {
  char b[32];
  std::snprintf(b, sizeof b, "%.*f", digits, v);
  return b;
}

gn::NodeConfig node_config(bool host, uint64_t seed) {
  gn::NodeConfig c;
  c.protocol = gn::protocol_id("net-config", 1);
  c.accept = host;
  c.max_peers = host ? kClients : 1;
  c.channels.count = 2;
  c.channels.delivery[0] = gn::Delivery::reliable;
  c.channels.delivery[1] = gn::Delivery::unreliable;
  c.timeout_us = 3000000;
  c.seed = seed;
  return c;
}

struct Client {
  gn::Transport t;
  gn::Node n;
  gn::SessionClient s;
  con::Console console{false};
  gn::ReplicateConfig net;
  gn::NetCvars cvars;
  gn::FakeLinkCvars fake;
  gn::FakeLinkCvars applied;
  gn::FakeLink link;
  gn::ClientConfig config;
  int hellos = 0, quits = 0;
  double follow_ms = -1, follow_rtt_ms = 0, user_ms = -1;
  bool refused_cheat = false, took_cheat = false, cheat_reset = false;
};

struct Outcome {
  double follow_ms = 0, follow_rtt_ms = 0;  // the slowest client's, and its round trip then
  double follow_ratio = 0;                  // the most any client took over its round trip
  double user_ms = 0;                       // the slowest
  uint64_t stuffed = 0, refused = 0;
  int hellos = 0, quits = 0;
  bool refused_cheat = false, took_cheat = false, cheat_reset = false;
  double loss_off = 0, loss_on = 0, loss_back = 0;  // the host's measure of client 1's stream
  std::vector<double> t_s, loss;
};

Outcome play(const gn::LinkConditions& link, uint32_t seed, const std::string& cfg) {
  Outcome o;
  gn::SimNetwork net = gn::make_sim_network(seed, link);
  gn::Transport host_t;
  gn::open_sim_transport(host_t, net, gn::sim_address(0, 1));
  auto host_n = std::make_unique<gn::Node>();
  gn::node_init(*host_n, node_config(true, uint64_t{seed} * 7 + 1), host_t);
  auto host = std::make_unique<gn::SessionHost>();
  gn::SessionConfig sc;
  sc.max_players = kClients;
  sc.seed = uint64_t{seed} * 13 + 5;
  gn::session_host_init(*host, sc, *host_n, 0);
  con::Console server(true);
  gn::ReplicateConfig server_net;
  gn::NetCvars server_cvars{&server_net};
  gn::add_net_cvars(server.cvars(), server_cvars);
  server.execute(cfg);  // the server's cfg, before anyone joins

  std::vector<std::unique_ptr<Client>> cs;
  for (int i = 0; i < kClients; i++) {
    auto c = std::make_unique<Client>();
    gn::open_sim_transport(c->t, net, gn::sim_address(static_cast<uint32_t>(i + 1), 1));
    gn::node_init(c->n, node_config(false, uint64_t{seed} * 1000 + static_cast<uint64_t>(i)), c->t);
    gn::session_client_init(c->s, c->n, 0);
    c->cvars.config = &c->net;
    gn::add_net_cvars(c->console.cvars(), c->cvars);
    gn::add_fake_link_cvars(c->console.cvars(), c->fake);
    Client* self = c.get();
    c->console.add_command(
        "hello", "a line a server may stuff: says hello",
        [self](con::Console&, const con::Args&) { self->hellos++; }, con::kStuffable);
    c->console.add_command("quit", "leaves the game",
                           [self](con::Console&, const con::Args&) { self->quits++; });
    cs.push_back(std::move(c));
  }
  const gn::Address host_at = gn::sim_address(0, 1);
  Client& tester = *cs[0];
  int64_t changed_at = -1, user_at = -1;
  int64_t now = 0;
  // The tester's stream, a packet a tick carrying the second it was sent in, counted by that.
  double sent_in[16] = {}, got_in[16] = {};
  const int64_t end = 15500000;  // the stream stops at 15 s; its last packets land
  while (now < end) {
    now += kTick;
    const int64_t ms = now / 1000;
    for (int i = 0; i < kClients; i++) {
      if (ms == 50 * i + 10) {
        char name[16];
        std::snprintf(name, sizeof name, "player %d", i + 1);
        gn::session_client_join(cs[i]->s, host_at, name, 0, now);
      }
    }
    if (ms == 2000) {
      server.execute("sv_maxrate 40000");
      changed_at = now;
    }
    if (ms == 3000) {
      for (auto& c : cs) c->console.execute("rate 15000");
      user_at = now;
    }
    if (ms == 4000) {
      const std::string stuff = "quit\nrate 1\nhello\n";
      gn::session_host_stuff(*host, gn::kNoPlayer, stuff.data(), stuff.size());
    }
    if (ms == 5000) {
      tester.console.execute("net_fakeloss 30");
      tester.refused_cheat = tester.fake.loss_pct == 0;
    }
    if (ms == 8000) server.execute("sv_cheats 1");
    if (ms == 8500) {
      tester.console.execute("net_fakeloss 30");
      tester.took_cheat = tester.fake.loss_pct == 30;
    }
    if (ms == 12000) server.execute("sv_cheats 0");

    gn::sim_advance(net, now);
    gn::node_update(*host_n, now);
    gn::session_host_update(*host, now);
    for (const gn::Event& e : host_n->events)
      if (e.type == gn::EventType::message && e.channel == 1 && e.size == 1 &&
          host_n->event_bytes[e.offset] < 16)
        got_in[host_n->event_bytes[e.offset]]++;
    gn::session_host_follow_cvars(*host, server.cvars());
    if (user_at >= 0) {
      bool all = true;
      for (int seat = 0; seat < kClients; seat++)
        all = all && gn::session_user_value(*host, seat, "rate") == "15000";
      if (all && o.user_ms == 0) o.user_ms = static_cast<double>(now - user_at) / 1000.0;
    }
    for (auto& c : cs) {
      gn::node_update(c->n, now);
      gn::session_client_update(c->s, now);
      gn::session_client_follow_cvars(c->s, c->console, c->config);
      const con::Cvar* rate = c->console.cvars().find("sv_maxrate");
      if (changed_at >= 0 && c->follow_ms < 0 && rate != nullptr && rate->value == "40000") {
        c->follow_ms = static_cast<double>(now - changed_at) / 1000.0;
        c->follow_rtt_ms = c->n.peers[0].conn.stats.rtt_ms;
      }
    }
    // The tester's fake link put on its sends whenever it moved (it moves back by itself when
    // the server turns cheats off).
    if (gn::fake_link_follow(tester.t, tester.link, tester.fake, tester.applied,
                             uint64_t{seed} * 31 + 7) &&
        ms > 12000 && tester.fake.loss_pct == 0)
      tester.cheat_reset = true;
    if (ms >= 4000 && ms < 15000 && tester.s.state == gn::ClientState::seated) {
      const auto second = static_cast<uint8_t>(ms / 1000);
      gn::node_send(tester.n, tester.s.peer, 1, &second, 1);
      sent_in[second]++;
    }
    gn::node_flush(*host_n, now);
    for (auto& c : cs) gn::node_flush(c->n, now);
  }
  for (auto& c : cs) {
    if (c->follow_ms > o.follow_ms) o.follow_ms = c->follow_ms, o.follow_rtt_ms = c->follow_rtt_ms;
    if (c->follow_rtt_ms > 0 && c->follow_ms / c->follow_rtt_ms > o.follow_ratio)
      o.follow_ratio = c->follow_ms / c->follow_rtt_ms;
    if (c->follow_ms < 0) o.follow_ms = -1;
    o.stuffed += c->config.stuffed;
    o.refused += c->config.refused;
    o.hellos += c->hellos;
    o.quits += c->quits;
  }
  o.refused_cheat = tester.refused_cheat;
  o.took_cheat = tester.took_cheat;
  o.cheat_reset = tester.cheat_reset;
  // Cheats off and refused (5 to 8 s), on and taken (9 to 12 s), off again (13 to 15 s).
  const auto share = [&](int from, int to) {
    double sent = 0, got = 0;
    for (int k = from; k < to; k++) sent += sent_in[k], got += got_in[k];
    return sent > 0 ? 100.0 * (sent - got) / sent : 0;
  };
  for (int k = 4; k < 15; k++) {
    o.t_s.push_back(k + 0.5);
    o.loss.push_back(share(k, k + 1));
  }
  o.loss_off = share(5, 8);
  o.loss_on = share(9, 12);
  o.loss_back = share(13, 15);
  return o;
}

void run(gl::Lab& lab) {
  const auto seed = static_cast<uint32_t>(lab.arg_num("seed"));
  const std::string cfg = lab.arg("cfg");
  gl::LabTable table{"configs over a session, each preset",
                     {"preset", "sv_ change followed (ms)", "round trip then (ms)",
                      "over the round trip", "user info read (ms)", "stuffed", "refused", "hello",
                      "quit", "fake loss, cheats off", "on", "off again"},
                     {}};
  gl::LineChart chart{
      {"FAKE LOSS ON ONE CLIENT'S STREAM AS THE HOST MEASURES IT",
       "a packet a tick, the share of each second's lost (by when it was sent)", "t (s)",
       "lost (%)"},
      {},
      {{5, "fakeloss refused"}, {8, "sv_cheats 1"}, {8.5, "fakeloss 30"}, {12, "sv_cheats 0"}},
      {},
      {},
      false,
      false,
      false};
  for (const std::string& name : split(lab.arg("presets"))) {
    gn::LinkConditions link;
    if (!gn::conditions_preset(name, link)) {
      lab.fail("unknown preset '" + name + "'",
               "Pick from lan, broadband, wifi, mobile, bad, awful (engine/net/conditions.cpp); "
               "skill: net.");
      continue;
    }
    const Outcome o = play(link, seed, cfg);
    table.rows.push_back({name, fmt(o.follow_ms, 0), fmt(o.follow_rtt_ms, 0),
                          fmt(o.follow_ratio, 2), fmt(o.user_ms, 0), std::to_string(o.stuffed),
                          std::to_string(o.refused), std::to_string(o.hellos),
                          std::to_string(o.quits), fmt(o.loss_off, 1), fmt(o.loss_on, 1),
                          fmt(o.loss_back, 1)});
    const std::string k = name + "-";
    lab.metric(k + "follow-ms", o.follow_ms, "ms");
    lab.metric(k + "follow-over-rtt", o.follow_ratio, "ratio");
    lab.metric(k + "user-ms", o.user_ms, "ms");
    lab.metric(k + "loss-off", o.loss_off, "%");
    lab.metric(k + "loss-on", o.loss_on, "%");
    lab.metric(k + "loss-back", o.loss_back, "%");
    chart.traces.push_back({name, o.t_s, o.loss});
    if (o.follow_ms < 0 || o.follow_ratio > 1)
      lab.fail(name + ": a client followed the server's sv_maxrate in " + fmt(o.follow_ms, 0) +
                   " ms, past its round trip of " + fmt(o.follow_rtt_ms, 0) + " ms",
               "Read session_host_follow_cvars and the welcome in engine/net/session_host.cpp; "
               "skill: net.");
    if (o.quits != 0 || o.hellos != kClients || o.refused != uint64_t{2} * kClients)
      lab.fail(name + ": stuffed text ran " + std::to_string(o.quits) + " quits and " +
                   std::to_string(o.hellos) + " hellos with " + std::to_string(o.refused) +
                   " refused (want 0, " + std::to_string(kClients) + ", " +
                   std::to_string(2 * kClients) + ")",
               "A server may run only kStuffable commands and set only its own cvars: read "
               "Console::run in engine/console/console.cpp; skill: console.");
    if (!o.refused_cheat || !o.took_cheat || !o.cheat_reset)
      lab.fail(name + ": net_fakeloss did not follow sv_cheats (refused off " +
                   std::string(o.refused_cheat ? "yes" : "no") + ", taken on " +
                   (o.took_cheat ? "yes" : "no") + ", reset off " + (o.cheat_reset ? "yes" : "no") +
                   ")",
               "Read the kCheat rule in engine/console/cvar.cpp and the server info's sv_cheats; "
               "skill: console.");
    if (o.loss_on < o.loss_off + 15)
      lab.fail(name + ": 30% fake loss moved the measured loss only from " + fmt(o.loss_off, 1) +
                   "% to " + fmt(o.loss_on, 1) + "%",
               "Read with_fake_link in engine/net/net_cvars.cpp; skill: net.");
  }
  lab.table(std::move(table));
  if (!chart.traces.empty() && lab.wants("fakeloss")) {
    const gl::Chart c = gl::line_chart(chart);
    const std::string svg = gl::to_svg(c.plot);
    lab.attach("fakeloss.svg", std::vector<uint8_t>(svg.begin(), svg.end()));
    lab.shot(
        {"fakeloss",
         chart.frame.title,
         "The share of one client's stream lost each second, as the host counts it: the "
         "client asks for 30% fake loss with cheats off (refused), the server turns cheats "
         "on, the client asks again (taken), the server turns cheats off (the loss goes "
         "back by itself).",
         "each line flat at its preset's own loss until 8.5 s, about 30 points higher until "
         "12 s, then back down at once.",
         {{"presets", lab.arg("presets")}, {"cfg", lab.arg("cfg")}, {"seed", lab.arg("seed")}}},
        gl::to_picture(c.plot));
  }
  lab.note(
      "sv_ change followed: from the host setting sv_maxrate to the slowest client's copy "
      "of it; over the round trip: the most any client took against its smoothed round "
      "trip then (at most 1, M21c's bound). stuffed: the stuffed messages the clients' "
      "consoles ran (one each: quit, rate 1, hello); refused: the lines they would not (quit and a "
      "player's own rate, two a client); hello: the one the game allows. fake loss: the "
      "share of client 1's stream lost, cheats off and refused, on and taken, off again.");
}

const gl::LabSpec kSpec{
    "net-config",
    gl::LabKind::sim,
    "engine",
    "engine/net (session_config, net_cvars) over engine/console",
    "net",
    "Over each condition, does a client follow a server's setting within a round trip, does the "
    "server read a client's own, does a stuffed quit get refused, and does fake loss move the "
    "link only while the server allows cheats?",
    {
        {"presets", "lan,broadband,wifi,mobile,bad", "condition presets, comma-separated"},
        {"cfg", "sv_maxrate 30000\nsv_maxupdaterate 60",
         "the server's cfg, run before anyone joins"},
        {"seed", "1", "the network's, the nodes' and the tokens' stream"},
    },
    "--presets=lan,bad",
    run,
};
GE_LAB(kSpec);

}  // namespace
