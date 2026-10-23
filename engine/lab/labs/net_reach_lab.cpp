// LAB `net-reach`: a client reaching a host through the rendezvous (engine/net/rendezvous.hpp,
// reach.hpp) for every pair of NAT kinds (none, full cone, restricted, port-restricted,
// symmetric), once under each condition preset. Each pair registers, joins, punches and, where no
// hole opens, falls back on the relay; then a reliable message crosses each way on the path
// found. It answers whether every pair connects, by the path RFC 4787's behaviours predict
// (direct where a hole can open, the relay where it cannot), and how long a player waits for it on
// each preset; and it checks the server's STUN answer names a NAT's public face. The connect times
// are drawn as a heatmap, a row a host kind and a column a client kind. Everything runs on a
// virtual clock off one seed, so the lab is a determinism test too. Then, under each preset, a
// host's router speaking each port-mapping protocol (PCP, NAT-PMP, UPnP IGD: portmap.hpp) opens
// its port, and a client behind a symmetric NAT reaches it directly by that forward.
//
// ge:summary Lab: every pair of NAT kinds reaching a host: direct or relayed, how long, each preset
// ge:keywords lab net reach nat nat-traversal hole-punching relay rendezvous stun matrix preset
// ge:keywords full-cone restricted port-restricted symmetric connect-time portmap upnp nat-pmp pcp
// ge:skill net labs
// ge:test tests/net_reach_test.cpp
#include <cmath>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "lab/chart.hpp"
#include "lab/lab.hpp"
#include "net/portmap.hpp"
#include "net/reach.hpp"
#include "net/rendezvous.hpp"
#include "net/sim_router.hpp"
#include "net/stun.hpp"

namespace {

namespace gl = ge::lab;
namespace gn = ge::net;

constexpr int64_t kTick = 10000;  // every node updates at 100 Hz
constexpr int kKinds = 5;         // none, then the four NAT kinds
const gn::Address kServer = gn::sim_address(100, 3478);

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

const char* kind_name(int k) {
  return k == 0 ? "none" : gn::nat_kind_name(static_cast<gn::NatKind>(k - 1));
}

// The path RFC 4787's behaviours predict: a symmetric NAT gives the pair a port the other side
// never sent to, which a port-restricted or symmetric NAT there filters out.
gn::ReachPath expected(int host, int client) {
  const auto sym = [](int k) { return k == 1 + static_cast<int>(gn::NatKind::symmetric); };
  const auto hard = [&](int k) {
    return sym(k) || k == 1 + static_cast<int>(gn::NatKind::port_restricted);
  };
  return (sym(host) && hard(client)) || (sym(client) && hard(host)) ? gn::ReachPath::relay
                                                                    : gn::ReachPath::direct;
}

struct End {
  gn::Transport t;
  gn::Node n;
};

std::unique_ptr<End> make_end(gn::SimNetwork& net, uint32_t node, int kind, bool host,
                              uint64_t seed) {
  if (kind > 0) {
    gn::NatConfig nc;
    nc.kind = static_cast<gn::NatKind>(kind - 1);
    nc.public_node = node + 10;
    nc.inside = {node};
    gn::sim_add_nat(net, nc);
  }
  auto e = std::make_unique<End>();
  gn::open_sim_transport(e->t, net, gn::sim_address(node, 7000));
  gn::NodeConfig c;
  c.protocol = gn::protocol_id("net-reach", 1);
  c.accept = host;
  c.channels.count = 1;
  c.channels.delivery[0] = gn::Delivery::reliable;
  c.keep_foreign = true;
  c.seed = seed * 1000 + node;
  gn::node_init(e->n, c, e->t);
  return e;
}

struct Outcome {
  gn::ReachState state = gn::ReachState::idle;
  gn::ReachWhy why = gn::ReachWhy::none;
  gn::ReachPath path = gn::ReachPath::none;
  double connect_ms = -1;
  bool both_ways = false;
  gn::ReachStats client, host;
  uint64_t relayed_bytes = 0, nat_dropped = 0;
};

Outcome reach(const gn::LinkConditions& link, int host_kind, int client_kind, uint32_t seed) {
  gn::SimNetwork net = gn::make_sim_network(seed, link);
  gn::Transport st;
  gn::open_sim_transport(st, net, kServer);
  gn::Rendezvous rv;
  gn::RendezvousConfig rc;
  rc.seed = seed;
  gn::rendezvous_init(rv, rc, st);
  auto host = make_end(net, 1, host_kind, true, seed);
  auto client = make_end(net, 2, client_kind, false, seed);
  gn::ReachConfig cfg;
  cfg.rendezvous = kServer;
  cfg.session = 42;
  cfg.seed = uint64_t{seed} * 7 + 1;
  gn::ReachHost h;
  gn::reach_host_init(h, cfg, host->n);
  gn::ReachClient c;
  gn::reach_client_init(c, cfg, client->n);
  Outcome o;
  bool said = false, host_heard = false, client_heard = false;
  for (int64_t now = kTick; now <= 30000000; now += kTick) {
    gn::sim_advance(net, now);
    gn::rendezvous_update(rv, now);
    gn::node_update(host->n, now);
    gn::node_update(client->n, now);
    gn::reach_host_update(h, now);
    if (now == 1000000) gn::reach_client_start(c, now);  // the host has registered by then
    gn::reach_client_update(c, now);
    for (const gn::Event& e : host->n.events) host_heard |= e.type == gn::EventType::message;
    for (const gn::Event& e : client->n.events) client_heard |= e.type == gn::EventType::message;
    if (c.state == gn::ReachState::connected && !said) {
      const uint8_t m[5] = {'h', 'e', 'l', 'l', 'o'};
      gn::node_send(client->n, c.peer, 0, m, sizeof m);
      for (int p = 0; p < static_cast<int>(host->n.peers.size()); p++) {
        if (gn::peer_connected(host->n, p)) gn::node_send(host->n, p, 0, m, sizeof m);
      }
      said = true;
    }
    gn::node_flush(host->n, now);
    gn::node_flush(client->n, now);
    if (c.state == gn::ReachState::failed || (host_heard && client_heard)) break;
  }
  o.state = c.state;
  o.why = c.why;
  o.path = c.path;
  if (c.state == gn::ReachState::connected)
    o.connect_ms = static_cast<double>(c.connected_us) / 1e3;
  o.both_ways = host_heard && client_heard;
  o.client = c.stats;
  o.host = h.stats;
  o.relayed_bytes = rv.stats.relayed_bytes;
  o.nat_dropped = net.stats.nat_dropped;
  gn::rendezvous_close(rv);
  return o;
}

// A STUN request from behind a NAT of each kind: the answer names the NAT's public port.
bool stun_sees_public_face(int kind) {
  gn::SimNetwork net = gn::make_sim_network(1, {});
  gn::Transport st;
  gn::open_sim_transport(st, net, kServer);
  gn::Rendezvous rv;
  gn::rendezvous_init(rv, {}, st);
  auto e = make_end(net, 1, kind, false, 1);
  const uint8_t id[12] = {7, 7, 7};
  uint8_t req[gn::kStunRequestSize];
  gn::stun_request(req, id);
  gn::transport_send(e->t, kServer, req, sizeof req, 0);
  gn::sim_advance(net, 1000);
  gn::rendezvous_update(rv, 1000);
  gn::sim_advance(net, 2000);
  uint8_t buf[gn::kMaxDatagram];
  gn::Address from, mapped;
  const int n = gn::transport_receive(e->t, buf, sizeof buf, from, 2000);
  if (n <= 0 || !gn::stun_read_answer(buf, static_cast<size_t>(n), id, mapped)) return false;
  return kind == 0 ? mapped == gn::sim_address(1, 7000) : gn::sim_node(mapped) == 11;
}

// OPENING A PORT: a host behind a symmetric NAT whose router speaks only `proto` maps its port
// (asking PCP, NAT-PMP and UPnP in turn), registers the forward with the rendezvous, and a client
// behind another symmetric NAT (a pair that needs the relay without it) connects; the lease of
// 20 s is renewed for a minute, then the mapping is closed.
struct MapOutcome {
  gn::PortmapState state = gn::PortmapState::idle;
  gn::PortmapWhy why = gn::PortmapWhy::none;
  gn::PortmapProto proto = gn::PortmapProto::none;
  double map_ms = -1, connect_ms = -1;
  gn::ReachPath path = gn::ReachPath::none;
  uint64_t fallbacks = 0, renewals = 0;
  bool held = false, closed = false;  // the forward held through the minute; gone after the close
};

MapOutcome map_and_reach(const gn::LinkConditions& link, gn::PortmapProto proto, uint32_t seed) {
  gn::SimNetwork net = gn::make_sim_network(seed, link);
  gn::Transport st;
  gn::open_sim_transport(st, net, kServer);
  gn::Rendezvous rv;
  gn::RendezvousConfig rc;
  rc.seed = seed;
  gn::rendezvous_init(rv, rc, st);
  gn::NatConfig hn;
  hn.kind = gn::NatKind::symmetric;
  hn.public_node = 11;
  hn.inside = {1};
  hn.gateway = 21;
  gn::SimRouterConfig src;
  src.nat = gn::sim_add_nat(net, hn);
  src.protocols = gn::portmap_bit(proto);
  gn::SimRouter router;
  gn::sim_router_init(router, net, src, 0);
  auto host = make_end(net, 1, 0, true, seed);  // its NAT is the router's, added above
  auto client = make_end(net, 2, 1 + static_cast<int>(gn::NatKind::symmetric), false, seed);
  gn::Transport mt;
  gn::open_sim_transport(mt, net, gn::sim_address(1, 5350));
  gn::PortmapConfig pc;
  pc.gateway = gn::sim_address(21, 5351);
  pc.internal_port = 7000;
  pc.lifetime_s = 20;
  pc.seed = seed;
  gn::Portmap pm;
  gn::portmap_init(pm, pc, mt);
  gn::ReachConfig cfg;
  cfg.rendezvous = kServer;
  cfg.session = 42;
  cfg.seed = uint64_t{seed} * 7 + 1;
  gn::ReachHost h;
  gn::reach_host_init(h, cfg, host->n);
  gn::ReachClient c;
  gn::reach_client_init(c, cfg, client->n);
  gn::portmap_start(pm, 0);
  MapOutcome o;
  int64_t client_at = -1, end_at = -1;
  bool held = true;
  for (int64_t now = kTick; now <= 120000000; now += kTick) {
    gn::sim_advance(net, now);
    gn::sim_router_update(router, now);
    gn::rendezvous_update(rv, now);
    gn::node_update(host->n, now);
    gn::node_update(client->n, now);
    gn::portmap_update(pm, now);
    if (pm.state == gn::PortmapState::mapped) {
      gn::reach_host_forward(h, pm.external.port, now);
      if (client_at < 0) client_at = now + 1000000;  // the forward registered by then
      held &= gn::sim_router_forwards(router, now) == 1;
    }
    gn::reach_host_update(h, now);
    if (now == client_at) gn::reach_client_start(c, now);
    gn::reach_client_update(c, now);
    gn::node_flush(host->n, now);
    gn::node_flush(client->n, now);
    if (pm.state == gn::PortmapState::failed) break;
    if (end_at < 0 && client_at >= 0 && now >= client_at + 60000000) {
      end_at = now;
      o.held = held;
      o.renewals = pm.stats.renewals;
      gn::portmap_close(pm, now);
    }
    if (pm.state == gn::PortmapState::closed) {
      o.closed = gn::sim_router_forwards(router, now) == 0;
      break;
    }
  }
  o.state = end_at >= 0 ? gn::PortmapState::mapped : pm.state;
  o.why = pm.why;
  o.proto = pm.proto;
  if (pm.mapped_us > 0) o.map_ms = static_cast<double>(pm.mapped_us) / 1e3;
  o.fallbacks = pm.stats.fallbacks;
  o.path = c.path;
  if (c.state == gn::ReachState::connected)
    o.connect_ms = static_cast<double>(c.connected_us) / 1e3;
  gn::rendezvous_close(rv);
  return o;
}

std::string fmt(double v, int digits) {
  char b[32];
  std::snprintf(b, sizeof b, "%.*f", digits, v);
  return b;
}

void run(gl::Lab& lab) {
  const auto seed = static_cast<uint32_t>(lab.arg_num("seed"));
  std::vector<std::string> names;
  std::vector<double> times;  // a preset's 5 rows of 5, the heatmap's cells
  for (const std::string& name : split(lab.arg("presets"))) {
    gn::LinkConditions link;
    if (!gn::conditions_preset(name, link)) {
      lab.fail("unknown preset '" + name + "'",
               "Pick from lan, broadband, wifi, mobile, bad, awful (engine/net/conditions.cpp); "
               "skill: net.");
      continue;
    }
    std::vector<std::string> columns{"host \\ client"};
    for (int k = 0; k < kKinds; k++) columns.emplace_back(kind_name(k));
    gl::LabTable table{name + ": the path each pair took and when it connected (s)", columns, {}};
    int direct = 0, relayed = 0, failed = 0, wrong = 0;
    double worst = 0;
    for (int hk = 0; hk < kKinds; hk++) {
      std::vector<std::string> row{kind_name(hk)};
      for (int ck = 0; ck < kKinds; ck++) {
        // Each pair its own draws, so one unlucky loss does not repeat down the matrix.
        const Outcome o = reach(link, hk, ck, seed * 31 + static_cast<uint32_t>(hk * kKinds + ck));
        const bool ok = o.state == gn::ReachState::connected && o.both_ways;
        std::string cell;
        if (!ok) {
          failed++;
          cell = std::string("FAIL ") + gn::reach_why_name(o.why);
        } else {
          (o.path == gn::ReachPath::direct ? direct : relayed)++;
          cell = std::string(gn::reach_path_name(o.path)) + " " + fmt(o.connect_ms / 1e3, 2);
          if (o.path != expected(hk, ck)) {
            wrong++;
            cell += " (want " + std::string(gn::reach_path_name(expected(hk, ck))) + ")";
          }
          if (o.connect_ms > worst) worst = o.connect_ms;
        }
        row.push_back(cell);
        times.push_back(ok ? o.connect_ms : std::nan(""));
        lab.metric(name + "-" + kind_name(hk) + "-" + kind_name(ck) + "-ms", ok ? o.connect_ms : -1,
                   "ms");
      }
      table.rows.push_back(std::move(row));
    }
    lab.table(std::move(table));
    names.push_back(name);
    lab.metric(name + "-direct", direct, "count");
    lab.metric(name + "-relayed", relayed, "count");
    lab.metric(name + "-failed", failed, "count");
    lab.metric(name + "-worst-ms", worst, "ms");
    if (failed)
      lab.fail(name + ": " + std::to_string(failed) +
                   " of 25 pairs never connected and traded a "
                   "message",
               "Run tests/net_reach_test.cpp, then read reach_client_update in "
               "engine/net/reach.cpp and the relay in engine/net/rendezvous.cpp; skill: net.");
    if (wrong)
      lab.fail(name + ": " + std::to_string(wrong) +
                   " pairs took another path than their NATs "
                   "predict",
               "Read the filters in map_in (engine/net/sim_network.cpp) and the punch and "
               "retarget in engine/net/reach.cpp; skill: net.");
  }
  for (const std::string& name : split(lab.arg("presets"))) {
    gn::LinkConditions link;
    if (!gn::conditions_preset(name, link)) continue;  // failed above
    gl::LabTable table{name + ": a symmetric host's router opening its port, by what it speaks",
                       {"router speaks", "mapped by", "map (s)", "fallbacks", "renewals",
                        "client path", "connect (s)", "held", "closed"},
                       {}};
    for (gn::PortmapProto p :
         {gn::PortmapProto::pcp, gn::PortmapProto::natpmp, gn::PortmapProto::upnp}) {
      const MapOutcome o = map_and_reach(link, p, seed * 37 + static_cast<uint32_t>(p));
      const bool mapped = o.state == gn::PortmapState::mapped;
      table.rows.push_back({gn::portmap_proto_name(p),
                            mapped ? gn::portmap_proto_name(o.proto)
                                   : std::string("FAIL ") + gn::portmap_why_name(o.why),
                            mapped ? fmt(o.map_ms / 1e3, 2) : "-", std::to_string(o.fallbacks),
                            std::to_string(o.renewals), gn::reach_path_name(o.path),
                            o.connect_ms >= 0 ? fmt(o.connect_ms / 1e3, 2) : "-",
                            o.held ? "yes" : "NO", o.closed ? "yes" : "NO"});
      const std::string key = name + "-" + gn::portmap_proto_name(p);
      lab.metric(key + "-map-ms", o.map_ms, "ms");
      lab.metric(key + "-renewals", static_cast<double>(o.renewals), "count");
      if (!mapped || o.proto != p || o.path != gn::ReachPath::direct || !o.held || !o.closed ||
          o.renewals < 5)
        lab.fail(key +
                     ": the router's protocol did not map, renew, let a client straight in and "
                     "close",
                 "Run tests/net_portmap_test.cpp, then read engine/net/portmap.cpp and "
                 "engine/net/sim_router.cpp; skill: net.");
    }
    lab.table(std::move(table));
  }
  lab.note(
      "Opening a port: the host sits behind a symmetric NAT and the client behind another, a pair "
      "the relay must carry without a forward. The host's router speaks the one protocol of its "
      "row; the host asks PCP, then NAT-PMP, then UPnP, so a fallback is a protocol that timed out "
      "(about 3.75 s each) or, for PCP on a NAT-PMP router, was answered with UNSUPP_VERSION at "
      "once. The lease asked for is 20 s, renewed at half its life for a minute; then the mapping "
      "is closed. The client's path is direct when the forward let it in.");
  gl::LabTable stun{"STUN from behind each NAT kind", {"kind", "public face"}, {}};
  for (int k = 0; k < kKinds; k++) {
    const bool ok = stun_sees_public_face(k);
    stun.rows.push_back({kind_name(k), ok ? "ok" : "FAIL"});
    if (!ok)
      lab.fail(std::string("STUN from behind ") + kind_name(k) + " did not name its public face",
               "Read stun_answer in engine/net/stun.cpp and map_out in "
               "engine/net/sim_network.cpp; skill: net.");
  }
  lab.table(std::move(stun));
  lab.note(
      "A row is the host's NAT, a column the client's; none is a public address. direct: the "
      "client's connect got through a punched hole (or to a public host); relay: it went through "
      "the server's relay port after direct_us (3 s) without an answer, so a relayed pair always "
      "waits at least that long. The time is from the client's join to its node's connected "
      "event; then a reliable message crosses each way. (want ...) marks a pair whose path is "
      "not the one RFC 4787's mapping and filtering predict.");
  if (!names.empty()) {
    std::vector<std::string> rows;
    for (const std::string& n : names) {
      for (int hk = 0; hk < kKinds; hk++) rows.push_back(n + " " + kind_name(hk));
    }
    gl::ChartFrame frame{"CONNECT TIME, HOST NAT x CLIENT NAT, EACH PRESET",
                         "columns: none, full-cone, restricted, port-restricted, symmetric",
                         "client's NAT", "ms"};
    const gl::Chart chart = gl::heatmap_chart(frame, rows, times, 0, kKinds);
    const std::string svg = gl::to_svg(chart.plot);
    lab.attach("connect.svg", std::vector<uint8_t>(svg.begin(), svg.end()));
    lab.shot({"connect",
              chart.title,
              "How long each pair waited to connect: a row a preset and host NAT kind, a column a "
              "client NAT kind (none, full-cone, restricted, port-restricted, symmetric).",
              "the relayed pairs (a symmetric NAT against a port-restricted or symmetric one) "
              "darkest, a little over 3 s; every other cell light, a few round trips.",
              {{"presets", lab.arg("presets")}, {"seed", lab.arg("seed")}}},
             gl::to_picture(chart.plot));
  }
}

const gl::LabSpec kSpec{
    "net-reach",
    gl::LabKind::sim,
    "engine",
    "engine/net (rendezvous, reach, stun, portmap, the simulated NATs and router)",
    "net",
    "Behind every pair of home routers' NAT kinds, does a client reach its host, by a punched "
    "hole where one can open and the relay where none can, and how long does it take? Does each "
    "port-mapping protocol open, keep and close a forward that lets a client straight in?",
    {
        {"presets", "lan,wifi,bad", "condition presets, comma-separated"},
        {"seed", "1", "the network's, the nodes' and the relays' stream"},
    },
    "--presets=lan,bad",
    run,
};
GE_LAB(kSpec);

}  // namespace
