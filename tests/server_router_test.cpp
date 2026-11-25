// engine/server/router: a server's game port forwarded by the home router (sv_open_port), against
// the simulated router: the forward made and its public address said, the forward taken down with
// the server, a router that answers nothing said with what to do, and sv_gateway's address read.
//
// ge:summary Test: the server's port forwarded by the simulated home router, said and taken down
// ge:keywords test sv_open_port sv_gateway router port-forward upnp nat-pmp pcp
// ge:skill net testing
#include <string>

#include "check.hpp"
#include "net/portmap_wire.hpp"
#include "net/sim_network.hpp"
#include "net/sim_router.hpp"
#include "server/router.hpp"

namespace {

namespace gn = ge::net;
namespace sv = ge::server;

constexpr uint32_t kHome = 1, kPublic = 11, kGateway = 21;
constexpr int64_t kTick = 10000;

struct Home {
  gn::SimNetwork net = gn::make_sim_network(7, {});
  gn::SimRouter router;
  gn::Transport sock;
  sv::ServerRouter r;
  bool ok = false;
};

void make_home(Home& h, uint8_t speaks) {
  gn::NatConfig nc;
  nc.kind = gn::NatKind::symmetric;
  nc.public_node = kPublic;
  nc.inside = {kHome, kHome + 1};
  nc.gateway = kGateway;
  const int nat = gn::sim_add_nat(h.net, nc);
  gn::SimRouterConfig rc;
  rc.nat = nat;
  rc.protocols = speaks;
  h.ok = gn::sim_router_init(h.router, h.net, rc, 0) &&
         gn::open_sim_transport(h.sock, h.net, gn::sim_address(kHome, 5350));
}

int64_t run(Home& h, int64_t from, int64_t until) {
  int64_t now = from;
  for (; now <= until; now += kTick) {
    gn::sim_advance(h.net, now);
    gn::sim_router_update(h.router, now);
    sv::router_frame(h.r, now);
  }
  return now;
}

}  // namespace

TEST(the_router_forwards_the_game_port_says_where_and_takes_it_down) {
  Home h;
  make_home(h, gn::kPortmapAll);
  CHECK(h.ok);
  sv::router_open(h.r, h.sock, gn::sim_address(kGateway, gn::portmap_wire::kServerPort), 27500, 3,
                  0);
  int64_t t = run(h, 0, 5000000);
  CHECK(h.r.pm.state == gn::PortmapState::mapped);
  CHECK(gn::sim_router_forwards(h.router, t) == 1);
  CHECK_MSG(h.r.said.size() == 1 && h.r.said[0].starts_with("The router forwards UDP 27500 to "
                                                            "this server (pcp): players reach "
                                                            "it at "),
            h.r.said.empty() ? std::string("nothing said") : h.r.said[0]);
  sv::router_close(h.r, t);
  t = run(h, t + kTick, t + 2000000);
  CHECK(sv::router_done(h.r));
  CHECK(gn::sim_router_forwards(h.router, t) == 0);
}

TEST(a_router_that_answers_nothing_is_said_with_what_to_do) {
  Home h;
  make_home(h, 0);  // speaks none of the three
  sv::router_open(h.r, h.sock, gn::sim_address(kGateway, gn::portmap_wire::kServerPort), 27910, 3,
                  0);
  run(h, 0, 60000000);
  CHECK(h.r.pm.state == gn::PortmapState::failed);
  CHECK_MSG(h.r.said.size() == 1 &&
                h.r.said[0].find("open it in the router's settings by hand") != std::string::npos,
            h.r.said.empty() ? std::string("nothing said") : h.r.said[0]);
  sv::router_close(h.r, 60000000);
  CHECK(sv::router_done(h.r));
}

TEST(sv_gateway_names_the_router_with_its_port_or_the_default) {
  gn::Address a;
  CHECK(sv::router_gateway("192.168.1.1", a));
  CHECK(gn::to_string(a) == "192.168.1.1:5351");
  CHECK(sv::router_gateway("10.0.0.1:5350", a) && a.port == 5350);
  CHECK(!sv::router_gateway("not a router", a));
}
