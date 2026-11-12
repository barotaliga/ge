// Tests: the classic face of our forward (games/shooters_edge/net/classic_forward.hpp) on the
// simulated network: a classic client with `prx` set plays through one of ours and through a chain
// of two, the server seeing the forwarder's mark; the refusals come back in words (no `prx`, a
// destination not allowed, a server that never answers); `ping`, `status` and `pingstatus` are
// answered, the last with each server's ping, and a host asking twice in a second gets one answer;
// a client's `drop` frees it.
//
// ge:summary Tests: classic clients chain through our forward: one hop, two, refusals, queries
// ge:keywords test classic-forward forward prx chain pingstatus status refusal drop
// ge:skill net shooter testing
#include <memory>
#include <string>
#include <vector>

#include "check.hpp"
#include "forward/forward.hpp"
#include "net/conditions.hpp"
#include "net/sim_network.hpp"
#include "shooters_edge/net/classic_client.hpp"
#include "shooters_edge/net/classic_forward.hpp"
#include "shooters_edge/net/classic_info.hpp"
#include "shooters_edge/net/classic_server.hpp"
#include "shooters_edge/server/mod_match.hpp"
#include "shooters_edge/server/test_mod.hpp"

namespace cn = shooters_edge::net;
namespace gn = ge::net;
namespace fw = ge::forward;
namespace sv = shooters_edge::server;

namespace {

// Node 0 the server, nodes 1 and 2 forwarders, node 3 the client, node 4 a stranger.
struct Rig {
  ge::progs::Progs progs;
  std::unique_ptr<sv::QcServer> qc;
  sv::RulesRunner rules;
  gn::SimNetwork net;
  gn::Transport st, ct, raw;
  gn::Transport port[2];
  fw::Forward fwd[2];
  cn::ClassicFace face[2];
  cn::ClassicServer server;
  cn::ClassicClientEnd client;
  uint16_t next_port[2] = {40000, 40000};
  int64_t now = 0;
  bool started = false;

  gn::Address server_at() const { return gn::sim_address(0, cn::kPortServer); }
  gn::Address hop_at(int i) const {
    return gn::sim_address(static_cast<uint32_t>(i + 1), cn::kForwardPort);
  }

  bool start(std::string& error) {
    if (!sv::build_test_mod(progs, error)) return false;
    sv::QcServerOptions so;
    so.loop.max_edicts = 1024;
    qc = sv::make_qc_server(progs, so, error);
    if (!qc) return false;
    sv::MapSpec map;
    map.name = "arena";
    map.models = sv::arena_models();
    map.entities = sv::kArenaEntities;
    if (!sv::load_map(*qc, map, error)) return false;
    gn::LinkConditions lan;
    gn::conditions_preset("lan", lan);
    net = gn::make_sim_network(1, lan);
    gn::open_sim_transport(st, net, server_at());
    gn::open_sim_transport(ct, net, gn::sim_address(3, cn::kPortClient));
    gn::open_sim_transport(raw, net, gn::sim_address(4, cn::kPortClient));
    rules = sv::qc_runner_over(*qc);
    if (!cn::classic_start(server, rules, st, cn::ServerConfig{}, error)) return false;
    for (int i = 0; i < 2; i++) {
      gn::open_sim_transport(port[i], net, hop_at(i));
      fw::ForwardConfig fc;
      fc.name = "hop" + std::to_string(i + 1);
      fc.allow = {server_at(), hop_at(1)};
      fc.seed = static_cast<uint64_t>(i) + 7;
      const auto node = static_cast<uint32_t>(i + 1);
      fw::forward_init(fwd[i], fc, port[i], [this, i, node](gn::Transport& out) {
        return gn::open_sim_transport(out, net, gn::sim_address(node, next_port[i]++));
      });
      face[i].probe_us = 50000;
      face[i].every_us = 1000000;
      cn::classic_forward_attach(fwd[i], face[i]);
    }
    return true;
  }
  void connect(const std::string& prx, int first_hop = 0) {
    cn::ClientConfig cc;
    cc.name = "runner";
    cc.qport = 4242;
    cc.seed = 3;
    cc.prx = prx;
    cn::client_start(client, ct, hop_at(first_hop), cc, 0);
    started = true;
  }
  void tick() {
    now += 13000;
    gn::sim_advance(net, now);
    cn::classic_frame(server, now);
    if (started) cn::client_tick(client, now);
    for (fw::Forward& f : fwd) fw::forward_update(f, now);
  }
  void run(int ticks) {
    for (int i = 0; i < ticks; i++) tick();
  }
  void ask(const std::string& text, const gn::Address& to) {
    const std::string d = "\xff\xff\xff\xff" + text;
    gn::transport_send(raw, to, reinterpret_cast<const uint8_t*>(d.data()), d.size(), now);
  }
  std::vector<std::string> answers() {
    std::vector<std::string> out;
    uint8_t buf[4096];
    gn::Address from;
    int n;
    while ((n = gn::transport_receive(raw, buf, sizeof buf, from, now)) >= 0)
      out.emplace_back(reinterpret_cast<const char*>(buf), static_cast<size_t>(n));
    return out;
  }
  std::string served_userinfo() const {
    for (const auto& c : server.clients)
      if (c.state != cn::ConnState::free) return c.userinfo;
    return "";
  }
};

}  // namespace

TEST(a_classic_client_plays_through_one_of_our_forwarders) {
  Rig r;
  std::string error;
  CHECK_MSG(r.start(error), error);
  r.connect(gn::to_string(r.server_at()));
  r.run(600);
  CHECK_MSG(r.client.stage == cn::ClientStage::active, std::string("the client is at ") +
                                                           cn::client_stage_name(r.client.stage) +
                                                           ": " + r.client.refused);
  const std::string info = r.served_userinfo();
  CHECK_MSG(cn::info_get(info, "*qwfwd") == "ge 1", info);
  CHECK(cn::info_get(info, "prx").empty());
  CHECK(r.face[0].counts.accepted == 1);
  CHECK(r.fwd[0].clients.size() == 1 && r.fwd[0].clients[0]->bytes_down > 10000);
}

TEST(a_classic_client_plays_through_a_chain_of_two) {
  Rig r;
  std::string error;
  CHECK_MSG(r.start(error), error);
  r.connect(gn::to_string(r.hop_at(1)) + "@" + gn::to_string(r.server_at()));
  r.run(600);
  CHECK_MSG(r.client.stage == cn::ClientStage::active, std::string("the client is at ") +
                                                           cn::client_stage_name(r.client.stage) +
                                                           ": " + r.client.refused);
  CHECK(r.face[0].counts.accepted == 1 && r.face[1].counts.accepted == 1);
  CHECK(r.fwd[1].clients.size() == 1 && r.fwd[1].clients[0]->bytes_down > 10000);
}

TEST(refusals_come_back_in_words) {
  {
    Rig r;
    std::string error;
    CHECK_MSG(r.start(error), error);
    r.connect("");
    r.run(200);
    CHECK_MSG(r.client.refused.find("set prx") != std::string::npos, r.client.refused);
  }
  {
    Rig r;
    std::string error;
    CHECK_MSG(r.start(error), error);
    r.connect("sim:0:27999");
    r.run(200);
    CHECK_MSG(r.client.refused.find("does not forward to") != std::string::npos, r.client.refused);
  }
  {
    Rig r;
    std::string error;
    CHECK_MSG(r.start(error), error);
    r.fwd[0].config.allow.push_back(gn::sim_address(9, cn::kPortServer));
    r.connect("sim:9:27500");
    r.run(1200);  // five asks, 2 s apart
    CHECK_MSG(r.client.refused.find("No answer from") != std::string::npos, r.client.refused);
    CHECK(r.fwd[0].clients.empty());
  }
}

TEST(ping_status_and_pingstatus_are_answered_once_a_second_a_host) {
  Rig r;
  std::string error;
  CHECK_MSG(r.start(error), error);
  r.run(100);  // the probes have reached the server
  r.ask("ping", r.hop_at(0));
  r.ask("status 23\n", r.hop_at(0));
  r.ask("pingstatus", r.hop_at(0));
  r.run(5);
  const std::vector<std::string> a = r.answers();
  CHECK(a.size() == 2);  // the ping, then one of status and pingstatus
  bool ping = false, status = false;
  for (const std::string& s : a) {
    if (s == "l") ping = true;
    if (s.find("\\*version\\qwfwd ge 1") != std::string::npos) status = true;
  }
  CHECK(ping && status);
  r.run(100);
  r.ask("pingstatus", r.hop_at(0));
  r.run(5);
  const std::vector<std::string> b = r.answers();
  CHECK(b.size() == 1);
  if (b.size() != 1) return;
  // The header, `n`, then 8 bytes a server: the server and the second hop, each with its ping.
  CHECK(b[0].size() == 5 + 16);
  if (b[0].size() != 5 + 16) return;
  const auto u16 = [&](size_t at) {
    return static_cast<uint8_t>(b[0][at]) | (static_cast<uint8_t>(b[0][at + 1]) << 8);
  };
  CHECK(u16(5 + 4) == cn::kPortServer);
  CHECK(u16(5 + 6) < 100);  // the server's round trip, ms
  CHECK(u16(13 + 4) == cn::kForwardPort);
  CHECK(u16(13 + 6) < 100);
}

TEST(a_clients_drop_frees_it) {
  Rig r;
  std::string error;
  CHECK_MSG(r.start(error), error);
  r.connect(gn::to_string(r.server_at()));
  r.run(600);
  CHECK(r.fwd[0].clients.size() == 1);
  cn::client_command(r.client, "drop");
  r.run(20);
  CHECK(r.fwd[0].clients.empty());
  CHECK(r.face[0].counts.drops == 1);
}
