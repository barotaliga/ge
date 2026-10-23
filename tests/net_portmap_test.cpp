// engine/net/portmap.hpp against the simulated router (sim_router.hpp): each of PCP, NAT-PMP and
// UPnP IGD opens a mapping, renews its lease and closes it, and the forward lets a client outside
// straight in; a NAT-PMP-only router moves the client off PCP at once; a router that gives only
// permanent UPnP leases is asked again with 0; no router fails, and a mismatched address is
// refused; two players asking for one port get two; a host behind a symmetric NAT with a forward
// is reached directly through the rendezvous by a client behind another; the messages keep their
// RFC layouts; NAT-PMP works over real loopback UDP, and UPnP over loopback UDP and TCP.
//
// ge:summary Test: port mapping by PCP, NAT-PMP and UPnP on the simulated router; leases; reach
// ge:keywords test net portmap pcp nat-pmp upnp igd ssdp soap lease renew forward sim-router
// ge:skill net testing
#include "net/portmap.hpp"

#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "check.hpp"
#include "net/node.hpp"
#include "net/portmap_wire.hpp"
#include "net/reach.hpp"
#include "net/rendezvous.hpp"
#include "net/sim_router.hpp"

using namespace ge::net;
namespace pw = ge::net::portmap_wire;

namespace {

constexpr uint32_t kHome = 1, kPublic = 11, kGateway = 21, kOutside = 50;
constexpr int64_t kTick = 10000;

struct Lan {
  SimNetwork net = make_sim_network(7, {});
  int nat = -1;
  SimRouter router;
  Transport mapper;  // the client's own socket for the router's answers
  Portmap pm;
  bool ok = false;
};

std::unique_ptr<Lan> make_lan(uint8_t speaks, NatKind kind = NatKind::symmetric,
                              bool permanent_only = false, const LinkConditions& link = {}) {
  auto l = std::make_unique<Lan>();
  l->net = make_sim_network(7, link);
  NatConfig nc;
  nc.kind = kind;
  nc.public_node = kPublic;
  nc.inside = {kHome, kHome + 1};
  nc.gateway = kGateway;
  l->nat = sim_add_nat(l->net, nc);
  SimRouterConfig rc;
  rc.nat = l->nat;
  rc.protocols = speaks;
  rc.permanent_only = permanent_only;
  l->ok = sim_router_init(l->router, l->net, rc, 0) &&
          open_sim_transport(l->mapper, l->net, sim_address(kHome, 5350));
  return l;
}

PortmapConfig config_for(uint16_t internal, uint32_t lifetime = 3600) {
  PortmapConfig c;
  c.gateway = sim_address(kGateway, pw::kServerPort);
  c.internal_port = internal;
  c.lifetime_s = lifetime;
  c.seed = 3;
  return c;
}

// Run the LAN until `done` or `until_us`; the time reached.
template <typename Done>
int64_t run(Lan& l, int64_t from_us, int64_t until_us, Done done) {
  int64_t now = from_us;
  for (; now <= until_us; now += kTick) {
    sim_advance(l.net, now);
    sim_router_update(l.router, now);
    portmap_update(l.pm, now);
    if (done()) break;
  }
  return now;
}

// Whether a datagram from outside to `to` reaches the inside socket at kHome:7000.
bool straight_in(Lan& l, const Address& to, int64_t now) {
  Transport game, outside;
  open_sim_transport(game, l.net, sim_address(kHome, 7000));
  open_sim_transport(outside, l.net, sim_address(kOutside, 9000));
  const uint8_t m[2] = {4, 2};
  transport_send(outside, to, m, sizeof m, now);
  sim_advance(l.net, now + 1000000);
  uint8_t buf[16];
  Address from;
  const bool got = transport_receive(game, buf, sizeof buf, from, now + 1000000) == 2;
  close_transport(game);
  close_transport(outside);
  return got;
}

}  // namespace

TEST(portmap_maps_renews_and_closes_by_each_protocol) {
  for (PortmapProto p : {PortmapProto::pcp, PortmapProto::natpmp, PortmapProto::upnp}) {
    auto l = make_lan(portmap_bit(p));
    CHECK(l->ok);
    portmap_init(l->pm, config_for(7000, 20), l->mapper);
    portmap_start(l->pm, 0);
    int64_t t = run(*l, 0, 30000000, [&] { return l->pm.state != PortmapState::asking; });
    CHECK_MSG(l->pm.state == PortmapState::mapped && l->pm.proto == p,
              "a protocol the router speaks did not map: the protocol is the loop's; skill: net");
    CHECK(sim_node(l->pm.external) == kPublic && l->pm.external.port == 7000);
    CHECK(sim_router_forwards(l->router, t) == 1);
    CHECK(straight_in(*l, l->pm.external, t));
    // The lease is 20 s: renewed every 10, still held a minute on.
    t = run(*l, t + kTick, t + 60000000, [] { return false; });
    CHECK(l->pm.state == PortmapState::mapped && l->pm.stats.renewals >= 5);
    CHECK(sim_router_forwards(l->router, t) == 1);
    portmap_close(l->pm, t);
    t = run(*l, t + kTick, t + 20000000, [&] { return l->pm.state == PortmapState::closed; });
    CHECK(l->pm.state == PortmapState::closed);
    CHECK(sim_router_forwards(l->router, t) == 0);
    CHECK(!straight_in(*l, sim_address(kPublic, 7000), t));
  }
}

TEST(portmap_falls_back_from_pcp_at_once_on_a_nat_pmp_router) {
  auto l = make_lan(portmap_bit(PortmapProto::natpmp));
  portmap_init(l->pm, config_for(7000), l->mapper);
  portmap_start(l->pm, 0);
  run(*l, 0, 10000000, [&] { return l->pm.state != PortmapState::asking; });
  CHECK(l->pm.state == PortmapState::mapped && l->pm.proto == PortmapProto::natpmp);
  CHECK(l->pm.stats.fallbacks == 1);
  // UNSUPP_VERSION came back on PCP's first ask: no PCP timeout was waited out.
  CHECK(l->pm.mapped_us < 200000);
}

TEST(portmap_asks_a_permanent_upnp_lease_where_only_one_is_given) {
  auto l = make_lan(portmap_bit(PortmapProto::upnp), NatKind::symmetric, true);
  portmap_init(l->pm, config_for(7000), l->mapper);
  portmap_start(l->pm, 0);
  const int64_t t = run(*l, 0, 30000000, [&] { return l->pm.state != PortmapState::asking; });
  CHECK(l->pm.state == PortmapState::mapped && l->pm.permanent && l->pm.lease_s == 0);
  CHECK(straight_in(*l, l->pm.external, t));
}

TEST(portmap_fails_with_no_router_and_is_refused_a_wrong_address) {
  auto none = make_lan(0);
  portmap_init(none->pm, config_for(7000), none->mapper);
  portmap_start(none->pm, 0);
  run(*none, 0, 60000000, [&] { return none->pm.state != PortmapState::asking; });
  CHECK(none->pm.state == PortmapState::failed && none->pm.why == PortmapWhy::no_router);
  CHECK(none->pm.stats.fallbacks == 2);  // PCP, then NAT-PMP, then UPnP, each timed out

  auto l = make_lan(portmap_bit(PortmapProto::pcp));
  PortmapConfig c = config_for(7000);
  c.protocols = portmap_bit(PortmapProto::pcp);
  c.local = sim_address(kHome + 1, 0);  // not the address the router sees: another NAT between
  portmap_init(l->pm, c, l->mapper);
  portmap_start(l->pm, 0);
  run(*l, 0, 10000000, [&] { return l->pm.state != PortmapState::asking; });
  CHECK(l->pm.state == PortmapState::failed && l->pm.why == PortmapWhy::refused);
  CHECK(l->router.stats.refused == 1);
}

TEST(portmap_gives_two_players_asking_for_one_port_two) {
  auto l = make_lan(portmap_bit(PortmapProto::pcp));
  Transport second;
  open_sim_transport(second, l->net, sim_address(kHome + 1, 5350));
  Portmap other;
  portmap_init(l->pm, config_for(7000), l->mapper);
  portmap_init(other, config_for(7000), second);
  portmap_start(l->pm, 0);
  portmap_start(other, 0);
  for (int64_t now = kTick; now < 5000000; now += kTick) {
    sim_advance(l->net, now);
    sim_router_update(l->router, now);
    portmap_update(l->pm, now);
    portmap_update(other, now);
  }
  CHECK(l->pm.state == PortmapState::mapped && other.state == PortmapState::mapped);
  CHECK(l->pm.external.port != other.external.port);
  CHECK(sim_router_forwards(l->router, 5000000) == 2);
}

TEST(portmap_lets_a_client_reach_a_symmetric_host_directly) {
  LinkConditions lc;
  lc.latency_ms = 20;
  auto l = make_lan(kPortmapAll, NatKind::symmetric, false, lc);
  const Address server = sim_address(100, 3478);
  NatConfig cn;  // the client's own symmetric NAT
  cn.kind = NatKind::symmetric;
  cn.public_node = 12;
  cn.inside = {2};
  sim_add_nat(l->net, cn);
  Transport st, ht, ct;
  open_sim_transport(st, l->net, server);
  open_sim_transport(ht, l->net, sim_address(kHome, 7000));
  open_sim_transport(ct, l->net, sim_address(2, 7000));
  Rendezvous rv;
  rendezvous_init(rv, {}, st);
  NodeConfig nc;
  nc.protocol = protocol_id("portmap-test", 1);
  nc.channels.count = 1;
  nc.channels.delivery[0] = Delivery::reliable;
  nc.keep_foreign = true;
  Node host, client;
  nc.accept = true;
  nc.seed = 1;
  node_init(host, nc, ht);
  nc.accept = false;
  nc.seed = 2;
  node_init(client, nc, ct);
  ReachConfig cfg;
  cfg.rendezvous = server;
  cfg.session = 42;
  ReachHost h;
  reach_host_init(h, cfg, host);
  ReachClient c;
  reach_client_init(c, cfg, client);
  portmap_init(l->pm, config_for(7000), l->mapper);
  portmap_start(l->pm, 0);
  for (int64_t now = kTick;
       now <= 20000000 && c.state != ReachState::connected && c.state != ReachState::failed;
       now += kTick) {
    sim_advance(l->net, now);
    sim_router_update(l->router, now);
    portmap_update(l->pm, now);
    if (l->pm.state == PortmapState::mapped) reach_host_forward(h, l->pm.external.port, now);
    rendezvous_update(rv, now);
    node_update(host, now);
    node_update(client, now);
    reach_host_update(h, now);
    if (now == 2000000) reach_client_start(c, now);
    reach_client_update(c, now);
    node_flush(host, now);
    node_flush(client, now);
  }
  CHECK_MSG(c.state == ReachState::connected && c.path == ReachPath::direct,
            "a forwarded host behind a symmetric NAT was not reached directly by a client "
            "behind another: the pair needs the relay without a forward; skill: net");
  CHECK(c.host_address == l->pm.external);
  rendezvous_close(rv);
}

TEST(portmap_messages_keep_their_layouts) {
  uint8_t b[pw::kPcpMapSize];
  // NAT-PMP's map request (RFC 6886 § 3.3): 12 bytes, big endian.
  pw::NatpmpMap m{7000, 7001, 3600};
  CHECK(pw::natpmp_map_request(b, m) == 12);
  CHECK(b[0] == 0 && b[1] == 1 && b[4] == 0x1b && b[5] == 0x58 && b[6] == 0x1b && b[7] == 0x59);
  CHECK(b[8] == 0 && b[9] == 0 && b[10] == 0x0e && b[11] == 0x10);
  pw::NatpmpAnswer a;
  a.op = pw::kOpMapUdp;
  a.epoch_s = 5;
  a.map = m;
  CHECK(pw::natpmp_answer(b, a) == 16 && b[1] == 129);
  pw::NatpmpAnswer back;
  CHECK(pw::natpmp_read_answer(b, 16, back) && back.map.external_port == 7001 && back.epoch_s == 5);
  // PCP's MAP (RFC 6887 § 11.1): 60 bytes, the client's address IPv4-mapped.
  pw::PcpMap p;
  const uint8_t ip[4] = {192, 168, 1, 20};
  std::memcpy(p.client_ip, ip, 4);
  p.lifetime_s = 7200;
  p.internal_port = 7000;
  for (int i = 0; i < 12; i++) p.nonce[i] = static_cast<uint8_t>(i);
  CHECK(pw::pcp_map_write(b, p) == 60);
  CHECK(b[0] == 2 && b[1] == 1 && b[18] == 0xff && b[19] == 0xff && b[20] == 192 && b[23] == 20);
  CHECK(b[36] == 17 && b[40] == 0x1b && b[41] == 0x58);
  pw::PcpMap q;
  CHECK(pw::pcp_map_read(b, 60, q) && !q.answer && q.internal_port == 7000 && q.client_ip[3] == 20);
  // SSDP and UPnP's text.
  std::string location;
  CHECK(pw::is_ssdp_search(pw::ssdp_search()));
  CHECK(pw::ssdp_location(pw::ssdp_answer("http://192.168.1.1:5000/rootDesc.xml"), location));
  Address host;
  std::string path;
  CHECK(pw::parse_http_url(location, host, path) && path == "/rootDesc.xml");
  CHECK(host == ipv4(192, 168, 1, 1, 5000));
  CHECK(pw::xml_value("<u:X><NewExternalIPAddress>1.2.3.4</NewExternalIPAddress>",
                      "NewExternalIPAddress") == "1.2.3.4");
  int status = 0;
  std::string_view body;
  CHECK(!pw::http_answer_whole("HTTP/1.1 200 OK\r\nContent-Length: 5\r\n\r\nab", false, status,
                               body));
  CHECK(pw::http_answer_whole("HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nab", false, status,
                              body) &&
        status == 200 && body == "ab");
  CHECK(pw::soap_error(pw::soap_fault(725, "x")) == 725);
}

TEST(portmap_nat_pmp_over_real_loopback_udp) {
  Transport router, mapper;
  std::string err;
  if (!open_udp_transport(router, loopback_v4(0), &err) ||
      !open_udp_transport(mapper, loopback_v4(0), &err)) {
    ge::test::skip("no loopback UDP socket here: " + err);
    return;
  }
  PortmapConfig c;
  c.gateway = router.local;
  c.internal_port = 7000;
  c.protocols = portmap_bit(PortmapProto::natpmp);
  c.first_wait_us = 50000;
  Portmap pm;
  portmap_init(pm, c, mapper);
  portmap_start(pm, 0);
  // A minimal NAT-PMP answerer: its external address 203.0.113.7, every port granted as asked.
  int64_t now = 0;
  for (int i = 0; i < 200000 && pm.state == PortmapState::asking; i++) {
    now += 100;
    uint8_t buf[64];
    Address from;
    const int n = transport_receive(router, buf, sizeof buf, from, now);
    if (n > 0) {
      uint8_t op = 0;
      pw::NatpmpMap m;
      if (pw::natpmp_read_request(buf, static_cast<size_t>(n), op, m)) {
        pw::NatpmpAnswer a;
        a.op = op;
        const uint8_t ext[4] = {203, 0, 113, 7};
        std::memcpy(a.ip, ext, 4);
        a.map = m;
        uint8_t out[16];
        transport_send(router, from, out, pw::natpmp_answer(out, a), now);
      }
    }
    portmap_update(pm, now);
  }
  CHECK_MSG(pm.state == PortmapState::mapped, "NAT-PMP over loopback UDP did not map; skill: net");
  CHECK(pm.external == ipv4(203, 0, 113, 7, 7000));
  close_transport(router);
  close_transport(mapper);
}

TEST(portmap_upnp_over_real_loopback_udp_and_tcp) {
  Transport ssdp, mapper;
  TcpListener web;
  std::string err;
  if (!open_udp_transport(ssdp, loopback_v4(0), &err) ||
      !open_udp_transport(mapper, loopback_v4(0), &err) || !tcp_listen(web, loopback_v4(0), &err)) {
    ge::test::skip("no loopback sockets here: " + err);
    return;
  }
  PortmapConfig c;
  c.ssdp = ssdp.local;
  c.internal_port = 7000;
  c.protocols = portmap_bit(PortmapProto::upnp);
  c.first_wait_us = 200000;
  Portmap pm;
  portmap_init(pm, c, mapper);
  portmap_start(pm, 0);
  const std::string service(pw::kIgdServices[1]);
  const std::string description =
      "<root><device><serviceList><service><serviceType>" + service +
      "</serviceType><controlURL>/ctl</controlURL></service></serviceList></device></root>";
  // A minimal internet gateway: SSDP on UDP, its description and SOAP on TCP, one request a stream.
  TcpStream stream;
  std::vector<uint8_t> got;
  int added = 0;
  int64_t now = 0;
  for (int i = 0; i < 400000 && pm.state == PortmapState::asking; i++) {
    now += 50;
    uint8_t buf[1500];
    Address from;
    const int n = transport_receive(ssdp, buf, sizeof buf, from, now);
    if (n > 0 && pw::is_ssdp_search({reinterpret_cast<const char*>(buf), static_cast<size_t>(n)})) {
      const std::string a = pw::ssdp_answer(pw::http_url(web.local, "/rootDesc.xml"));
      transport_send(ssdp, from, reinterpret_cast<const uint8_t*>(a.data()), a.size(), now);
    }
    if (stream.handle == -1 && tcp_accept(web, stream)) got.clear();
    if (stream.handle != -1) {
      tcp_pump(stream);
      tcp_take_bytes(stream, got);
      const std::string_view text(reinterpret_cast<const char*>(got.data()), got.size());
      const size_t end = text.find("\r\n\r\n");
      const std::string_view length = pw::http_header(text, "Content-Length");
      if (end != std::string_view::npos &&
          text.size() >= end + 4 + (length.empty() ? 0 : std::stoul(std::string(length)))) {
        std::string answer;
        if (text.substr(0, 4) == "GET ") {
          answer = pw::http_answer(200, "text/xml", description);
        } else if (pw::soap_action(text) == "AddPortMapping") {
          added++;
          answer = pw::soap_answer(service, "AddPortMapping", "");
        } else {
          answer = pw::soap_answer(service, "GetExternalIPAddress",
                                   "<NewExternalIPAddress>203.0.113.7</NewExternalIPAddress>");
        }
        tcp_send_bytes(stream, reinterpret_cast<const uint8_t*>(answer.data()), answer.size());
        for (int k = 0; k < 1000 && !stream.out.empty(); k++) tcp_pump(stream);
        tcp_close(stream);
      }
    }
    portmap_update(pm, now);
  }
  CHECK_MSG(pm.state == PortmapState::mapped,
            "UPnP over loopback UDP and TCP did not map: its state and step are in pm; skill: net");
  CHECK(added == 1 && pm.external == ipv4(203, 0, 113, 7, 7000));
  tcp_close_listener(web);
  close_transport(ssdp);
  close_transport(mapper);
}
