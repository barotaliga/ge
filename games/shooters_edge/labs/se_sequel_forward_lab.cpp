// LAB `se-sequel-forward`: THE SEQUEL'S CLIENT THROUGH OUR HOPS (engine/forward, engine/proxy).
// Without `--server` our sequel server runs on the simulated network (deterministic: the smoke run)
// and three clients join it: one through a chain of two of our forwarders, one through our near
// and far proxies over a lossy link, and one straight over the same lossy link. With
// `--server=<host:port>` one client joins a live sequel server over UDP by the wall clock, through
// the forwarders (`--forward=<port>`) or the proxies (`--proxy=<port>`) opened on loopback. It
// answers whether the route was dialled, whether the client got through the signon and played,
// what the server saw as the client's address, and how much of the server's traffic the client
// lost with the proxy and without it.
//
// ge:summary Lab: the sequel client through two forwarders or the proxies, to any sequel server
// ge:keywords lab se-sequel-forward sequel forward proxy hops chain route dial lossy-link
// ge:keywords client-loss live-server
// ge:skill net shooter labs
// ge:test tests/shooters_edge_sequel_net_test.cpp
#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "brush/map.hpp"
#include "brush/read.hpp"
#include "brush/write.hpp"
#include "core/num_text.hpp"
#include "forward/forward.hpp"
#include "lab/lab.hpp"
#include "net/conditions.hpp"
#include "net/sim_network.hpp"
#include "net/transport.hpp"
#include "proxy/proxy.hpp"
#include "shooters_edge/net/sequel_client.hpp"
#include "shooters_edge/net/sequel_net_server.hpp"

namespace {

namespace gl = ge::lab;
namespace gn = ge::net;
namespace br = ge::brush;
namespace sh = ge::shooter;
namespace cn = shooters_edge::net;
namespace sv = shooters_edge::server;
namespace fw = ge::forward;
namespace px = ge::proxy;
namespace num = ge::core::num;

br::WorldBrush box(float x0, float y0, float z0, float x1, float y1, float z1) {
  const float lo[3] = {x0, y0, z0}, hi[3] = {x1, y1, z1};
  return br::box_brush(lo, hi, sh::kSqSolid, "world/wall");
}

// A hall of 2048 units with three starts.
std::shared_ptr<const br::Map> hall(std::string& error) {
  br::BrushWorld w;
  w.models.resize(1);
  auto& m = w.models[0];
  m.push_back(box(-1024, -1024, -16, 1024, 1024, 0));
  m.push_back(box(-1040, -1024, -16, -1024, 1024, 256));
  m.push_back(box(1024, -1024, -16, 1040, 1024, 256));
  m.push_back(box(-1040, 1024, -16, 1040, 1040, 256));
  m.push_back(box(-1040, -1040, -16, 1040, -1024, 256));
  for (const char* at : {"-400 -400 24", "400 400 24", "-400 400 24"}) {
    br::Entity e;
    e.pairs = {{"classname", "info_player_deathmatch"}, {"origin", at}, {"angle", "45"}};
    w.entities.push_back(e);
  }
  std::vector<uint8_t> bytes;
  auto map = std::make_shared<br::Map>();
  if (!br::write_brush_world(w, &bytes, &error) || !br::read_bsp(bytes, map.get(), &error))
    return nullptr;
  return map;
}

sh::MoveCmd walk(int tick) {
  sh::MoveCmd m;
  m.forward = 300;
  m.angles.y = static_cast<float>((tick / 100) % 4) * 90.0f;
  return m;
}

double client_loss(const cn::SequelClientEnd& c) {
  const auto all = c.ch.accepted + c.ch.dropped;
  return all == 0 ? 0 : 100.0 * double(c.ch.dropped) / double(all);
}

// The hops: two forwarders chained (the first allows only the second, the second only the
// server), and the near and far proxies with fake loss on the link between them.
struct Hops {
  gn::Transport fa_port, fb_port;
  fw::Forward fa, fb;
  gn::Transport near_client, near_link, far_link;
  gn::FakeLink near_fake, far_fake;
  px::ProxyNear near;
  px::ProxyFar far;
  bool forward = false, proxy = false;
  gn::Address near_at;
};

void open_forwarders(Hops& h, const gn::Address& b, const gn::Address& to,
                     const fw::OpenUpstream& up_a, const fw::OpenUpstream& up_b) {
  fw::ForwardConfig fc;
  fc.name = "first";
  fc.allow = {b};
  fc.seed = 7;
  fw::forward_init(h.fa, fc, h.fa_port, up_a);
  fc.name = "second";
  fc.allow = {to};
  fc.seed = 8;
  fw::forward_init(h.fb, fc, h.fb_port, up_b);
  h.forward = true;
}

void open_proxies(gl::Lab& lab, Hops& h, const gn::Address& near_at, const gn::Address& far_at,
                  const gn::Address& to, const px::OpenUpstream& up, bool fake) {
  // On loopback the link's loss is faked on its two sockets; the simulated network loses it on
  // the path between the proxies' nodes instead.
  if (fake) {
    gn::LinkConditions bad;
    bad.loss = lab.arg_num("link-loss");
    transport_fake(h.near_link, &h.near_fake, bad, 11);
    transport_fake(h.far_link, &h.far_fake, bad, 12);
  }
  px::ProxyConfig pc;
  pc.link.repeat = static_cast<int>(lab.arg_num("copies"));
  pc.link.pack = true;
  pc.allow = {to};
  pc.name = "near";
  pc.seed = 5;
  px::proxy_near_init(h.near, pc, h.near_client, h.near_link, far_at, to);
  pc.name = "far";
  pc.seed = 6;
  px::proxy_far_init(h.far, pc, h.far_link, up);
  h.near_at = near_at;
  h.proxy = true;
}

void update_hops(Hops& h, int64_t now_us) {
  if (h.forward) {
    fw::forward_update(h.fa, now_us);
    fw::forward_update(h.fb, now_us);
  }
  if (h.proxy) {
    px::proxy_near_update(h.near, now_us);
    px::proxy_far_update(h.far, now_us);
  }
}

void close_hops(Hops& h) {
  if (h.forward) {
    fw::forward_close(h.fa);
    fw::forward_close(h.fb);
  }
  if (h.proxy) px::proxy_far_close(h.far);
}

void client_rows(gl::LabTable& t, const std::string& who, const cn::SequelClientEnd& c,
                 const cn::SequelNetClient* seen) {
  t.rows.push_back({who + ": stage", cn::client_stage_name(c.stage)});
  if (!c.refused.empty()) t.rows.push_back({who + ": refused or dropped", c.refused});
  t.rows.push_back(
      {who + ": frames / moves sent", std::to_string(c.frames) + " / " + std::to_string(c.moves)});
  t.rows.push_back({who + ": server packets lost (%)", num::to_fixed(client_loss(c), 1)});
  if (seen != nullptr) {
    t.rows.push_back({who + ": the server saw it at", gn::to_string(seen->address)});
    t.rows.push_back({who + ": moves taken / bad checksums",
                      std::to_string(seen->moves) + " / " + std::to_string(seen->bad_checksums)});
  }
}

void report_forward(gl::Lab& lab, Hops& h, const cn::SequelClientEnd& c,
                    const cn::SequelNetClient* seen, gl::Det det) {
  gl::LabTable t{"through two of our forwarders", {"what", "value"}, {}};
  t.rows.push_back({"route dialled", c.dial.state == fw::DialState::routed ? "yes" : "no"});
  if (det == gl::Det::measured)
    t.rows.push_back({"route's round trip (ms)",
                      c.dial.rtt_us < 0 ? "-" : num::to_fixed(double(c.dial.rtt_us) / 1000, 2)});
  for (fw::Forward* f : {&h.fa, &h.fb})
    for (const auto& hc : f->clients)
      t.rows.push_back({f->config.name + ": packets up / down",
                        std::to_string(hc->packets_up) + " / " + std::to_string(hc->packets_down)});
  client_rows(t, "client", c, seen);
  lab.table(t, det);
  lab.metric("hops.forward_routed", c.dial.state == fw::DialState::routed ? 1 : 0, "bool", det);
  lab.metric("hops.forward_frames", double(c.frames), "frames", det);
  if (c.dial.state != fw::DialState::routed || c.frames == 0)
    lab.fail("the client did not play through the forwarders: " + c.refused,
             "next: run tests/shooters_edge_sequel_net_test.cpp; skill: net.");
}

void report_proxy(gl::Lab& lab, Hops& h, const cn::SequelClientEnd& c,
                  const cn::SequelNetClient* seen, const cn::SequelClientEnd* direct, gl::Det det) {
  gl::LabTable t{"through our proxies", {"what", "value"}, {}};
  t.rows.push_back({"link: loss each way / copies", num::to_fixed(lab.arg_num("link-loss"), 2) +
                                                        " / " +
                                                        std::to_string(h.near.config.link.repeat)});
  if (det == gl::Det::measured)
    t.rows.push_back({"link frames lost (near out / far out)",
                      std::to_string(h.near_fake.lost) + " / " + std::to_string(h.far_fake.lost)});
  uint64_t up = 0, up_game = 0, down = 0, down_game = 0;
  for (const auto& n : h.near.clients) {
    up += n->link.stats.bytes_out;
    up_game += n->link.stats.payload_out;
  }
  for (const auto& f : h.far.sessions) {
    down += f->link.stats.bytes_out;
    down_game += f->link.stats.payload_out;
  }
  t.rows.push_back(
      {"link bytes up / the game's", std::to_string(up) + " / " + std::to_string(up_game)});
  t.rows.push_back(
      {"link bytes down / the game's", std::to_string(down) + " / " + std::to_string(down_game)});
  client_rows(t, "client", c, seen);
  if (direct != nullptr) client_rows(t, "straight over the link", *direct, nullptr);
  lab.table(t, det);
  lab.metric("hops.proxy_frames", double(c.frames), "frames", det);
  lab.metric("hops.client_loss", client_loss(c), "%", det);
  if (direct != nullptr) lab.metric("hops.direct_loss", client_loss(*direct), "%", det);
  if (c.frames == 0)
    lab.fail("the client did not play through the proxies: " + c.refused,
             "next: raise --copies or run tests/shooters_edge_sequel_net_test.cpp; skill: net.");
}

const cn::SequelNetClient* seen_by(const cn::SequelNetServer& s, const std::string& name) {
  for (const auto& c : s.clients)
    if (c.stage != cn::SequelSlot::free && c.name == name) return &c;
  return nullptr;
}

void simulated(gl::Lab& lab) {
  struct Rig {
    sv::RulesRunner rules;
    gn::SimNetwork net;
    gn::Transport st, at, bt, ct;
    cn::SequelNetServer server;
    cn::SequelClientEnd a, b, c;
    Hops hops;
  };
  auto owned = std::make_unique<Rig>();
  Rig& r = *owned;
  std::string error;
  auto map = hall(error);
  r.rules = sv::sequel_runner(sv::make_sequel_server({}));
  r.rules.console("deathmatch 1");
  sv::MapSpec spec;
  spec.name = "hall";
  spec.brushes = map;
  if (map == nullptr || !r.rules.start(spec, error))
    return lab.fail("the sequel's match did not start: " + error,
                    "next: run tests/shooters_edge_sequel_net_test.cpp; skill: net.");
  r.net = gn::make_sim_network(1, gn::LinkConditions{});
  const gn::Address to = gn::sim_address(0, cn::kPortSequel);
  gn::open_sim_transport(r.st, r.net, to);
  if (!cn::sequel_net_start(r.server, r.rules, r.st, cn::SequelNetConfig{}, error))
    return lab.fail(error, "next: skill: net.");
  // Each hop's node opens its upstream sockets from port 40000.
  uint16_t next_up = 40000;
  auto opener = [&r, &next_up](uint32_t node) {
    return [&r, &next_up, node](gn::Transport& t) {
      return gn::open_sim_transport(t, r.net, gn::sim_address(node, next_up++));
    };
  };
  Hops& h = r.hops;
  const gn::Address fa = gn::sim_address(20, 30000), fb = gn::sim_address(21, 30000);
  gn::open_sim_transport(h.fa_port, r.net, fa);
  gn::open_sim_transport(h.fb_port, r.net, fb);
  open_forwarders(h, fb, to, opener(20), opener(21));
  const gn::Address near_at = gn::sim_address(30, 30000), far_at = gn::sim_address(31, 30000);
  gn::open_sim_transport(h.near_client, r.net, near_at);
  gn::open_sim_transport(h.near_link, r.net, gn::sim_address(30, 30001));
  gn::open_sim_transport(h.far_link, r.net, far_at);
  open_proxies(lab, h, near_at, far_at, to, opener(31), false);
  // The clients: through the forwarders, through the proxies, and straight over a link as lossy.
  gn::open_sim_transport(r.at, r.net, gn::sim_address(1, 27901));
  gn::open_sim_transport(r.bt, r.net, gn::sim_address(2, 27901));
  gn::open_sim_transport(r.ct, r.net, gn::sim_address(3, 27901));
  gn::LinkConditions bad;
  bad.loss = lab.arg_num("link-loss");
  gn::sim_set_path(r.net, 30, 31, bad);
  gn::sim_set_path(r.net, 3, 0, bad);
  cn::SequelClientConfig cfg;
  cfg.brain = walk;
  cfg.name = "forwarded";
  cfg.qport = 11;
  cfg.via = {fa, fb};
  cn::sequel_client_start(r.a, r.at, to, cfg, 0);
  cfg.name = "proxied";
  cfg.qport = 12;
  cfg.via = {};
  cn::sequel_client_start(r.b, r.bt, near_at, cfg, 0);
  cfg.name = "straight";
  cfg.qport = 13;
  cn::sequel_client_start(r.c, r.ct, to, cfg, 0);
  const int ticks = static_cast<int>(lab.arg_num("seconds") * 1000.0 / 13.0);
  int64_t now = 0;
  for (int i = 0; i < ticks; i++) {
    now += 13000;
    gn::sim_advance(r.net, now);
    update_hops(h, now);
    cn::sequel_client_tick(r.a, now);
    cn::sequel_client_tick(r.b, now);
    cn::sequel_client_tick(r.c, now);
    cn::sequel_net_frame(r.server, now);
  }
  report_forward(lab, h, r.a, seen_by(r.server, "forwarded"), gl::Det::deterministic);
  report_proxy(lab, h, r.b, seen_by(r.server, "proxied"), &r.c, gl::Det::deterministic);
  cn::sequel_client_stop(r.a);
  cn::sequel_client_stop(r.b);
  cn::sequel_client_stop(r.c);
  close_hops(h);
}

bool open_upstream(gn::Transport& out) {
  return gn::open_udp_transport(out, gn::ipv4(0, 0, 0, 0, 0), nullptr);
}

void live(gl::Lab& lab, const std::string& server) {
  std::string text = server;
  if (text.find(':') == std::string::npos) text += ":27910";
  gn::Address to;
  if (!gn::parse_address(text, to))
    return lab.fail("'" + server + "' is not host:port", "next: --server=127.0.0.1:27910");
  const int forward_port = static_cast<int>(lab.arg_num("forward"));
  const int proxy_port = static_cast<int>(lab.arg_num("proxy"));
  if ((forward_port > 0) == (proxy_port > 0))
    return lab.fail("a live run goes through one kind of hop",
                    "next: add --forward=<port> or --proxy=<port>, not both");
  gn::Transport t;
  std::string error;
  if (!gn::open_udp_transport(t, gn::ipv4(0, 0, 0, 0, 0), &error))
    return lab.fail("the UDP socket did not open: " + error, "next: check the container's network");
  auto hops = std::make_unique<Hops>();
  Hops& h = *hops;
  cn::SequelClientConfig cfg;
  cfg.name = lab.arg("name");
  cfg.brain = walk;
  // A fresh qport each run: a server ignores a connect from the address and qport of a slot in use.
  cfg.qport = static_cast<uint16_t>(
      1024 + std::chrono::steady_clock::now().time_since_epoch().count() % 60000);
  gn::Address first = to;
  auto loop = [](int port) { return gn::ipv4(127, 0, 0, 1, static_cast<uint16_t>(port)); };
  if (forward_port > 0) {
    if (!gn::open_udp_transport(h.fa_port, loop(forward_port), &error) ||
        !gn::open_udp_transport(h.fb_port, loop(forward_port + 1), &error))
      return lab.fail("a forwarder's socket did not open: " + error,
                      "next: pick free ports with --forward=<port>");
    open_forwarders(h, loop(forward_port + 1), to, open_upstream, open_upstream);
    cfg.via = {loop(forward_port), loop(forward_port + 1)};
  } else {
    if (!gn::open_udp_transport(h.near_client, loop(proxy_port), &error) ||
        !gn::open_udp_transport(h.near_link, loop(0), &error) ||
        !gn::open_udp_transport(h.far_link, loop(proxy_port + 1), &error))
      return lab.fail("a proxy's socket did not open: " + error,
                      "next: pick free ports with --proxy=<port>");
    open_proxies(lab, h, loop(proxy_port), loop(proxy_port + 1), to, open_upstream, true);
    first = loop(proxy_port);
  }
  auto c = std::make_unique<cn::SequelClientEnd>();
  cn::sequel_client_start(*c, t, first, cfg, 0);
  const auto start = std::chrono::steady_clock::now();
  const auto seconds = static_cast<int64_t>(lab.arg_num("seconds") * 1e6);
  int64_t now_us = 0, client_us = 0;
  // The client ticks every 13 ms; the hops run every millisecond between, as a hop's own loop
  // would.
  while (now_us < seconds && c->stage != cn::ClientStage::dropped) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    now_us = std::chrono::duration_cast<std::chrono::microseconds>(
                 std::chrono::steady_clock::now() - start)
                 .count();
    update_hops(h, now_us);
    if (now_us - client_us < 13000) continue;
    client_us = now_us;
    cn::sequel_client_tick(*c, now_us);
  }
  if (h.forward)
    report_forward(lab, h, *c, nullptr, gl::Det::measured);
  else
    report_proxy(lab, h, *c, nullptr, nullptr, gl::Det::measured);
  cn::sequel_client_stop(*c);
  close_hops(h);
}

void run(gl::Lab& lab) {
  const std::string& server = lab.arg("server");
  if (server.empty())
    simulated(lab);
  else
    live(lab, server);
}

const gl::LabSpec kSpec{
    "se-sequel-forward",
    gl::LabKind::sim,
    "game:shooters_edge",
    "engine/forward and engine/proxy on the sequel's protocol: our client through our hops",
    "net",
    "Our sequel client through two of our forwarders and through our proxies, to our server "
    "simulated or a live one: is the route dialled, does it play, and how much does the proxy save "
    "it on a lossy link?",
    {
        {"server", "", "a live sequel server, as host:port (empty: ours, simulated)"},
        {"seconds", "10", "how long the clients play"},
        {"name", "edge", "a live client's name"},
        {"forward", "0", "live: through two of our forwarders on this loopback port and the next"},
        {"proxy", "0", "live: through our near proxy on this loopback port, the far on the next"},
        {"link-loss", "0.1", "the share of link frames lost each way, 0..1"},
        {"copies", "3", "copies of each proxy link frame, 1..5"},
    },
    "--seconds=6",
    run,
};
GE_LAB(kSpec);

}  // namespace
