// THE SIMULATED ROUTER: the control plane of a simulated NAT (sim_network.hpp), answering what a
// home router answers on its LAN face (`NatConfig::gateway`) so the port-mapping client
// (portmap.hpp) is verified headless: PCP and NAT-PMP on port 5351, SSDP on 1900, and UPnP's
// description and SOAP control on its HTTP port (one datagram each way, the simulated network's
// stand-in for TCP). A mapping it grants is a forward on its NAT (`sim_nat_forward`), held until
// the lease runs out, renewed by asking again and taken down by a lease of 0.
//
// What it speaks is configured, as routers differ: any mix of the three; a NAT-PMP-only router
// answers PCP with its own version and UNSUPP_VERSION, as RFC 6887 asks; `permanent_only` refuses
// every UPnP lease but 0, as many do (error 725); a public port another inside address holds is
// given elsewhere by PCP and NAT-PMP and refused by UPnP (718). It answers only its own LAN.
//
//   sim_router_init(r, net, config)     every frame, after sim_advance: sim_router_update(r, now)
//
// ge:summary The simulated router: PCP, NAT-PMP, SSDP and UPnP answered, forwards on its NAT
// ge:keywords net sim-router router gateway portmap pcp nat-pmp upnp igd ssdp soap forward lease
// ge:skill net
// ge:test tests/net_portmap_test.cpp
#pragma once

#include <cstdint>

#include "net/portmap.hpp"
#include "net/sim_network.hpp"
#include "net/transport.hpp"

namespace ge::net {

struct SimRouterConfig {
  int nat = 0;                      // the NAT it runs (sim_add_nat's index)
  uint8_t protocols = kPortmapAll;  // what it answers (portmap_bit)
  uint32_t max_lifetime_s = 7200;   // a longer lease asked for is cut to this
  bool permanent_only = false;      // UPnP: only a lease of 0 (error 725 otherwise)
  uint16_t http_port = 5000;        // UPnP's description and control
};

struct SimRouterStats {
  uint64_t pcp = 0, natpmp = 0, ssdp = 0, upnp = 0;  // requests answered, by protocol
  uint64_t mapped = 0, deleted = 0, refused = 0;
};

struct SimRouter {
  SimRouterConfig config;
  SimNetwork* net = nullptr;
  Transport control, ssdp, http;  // ports 5351, 1900 and http_port on the gateway
  int64_t started_us = 0;
  SimRouterStats stats;
};

// Bind the router's ports on its NAT's gateway; false when the NAT has none or a port is taken.
bool sim_router_init(SimRouter& r, SimNetwork& net, const SimRouterConfig& config, int64_t now_us);
void sim_router_update(SimRouter& r, int64_t now_us);
// The forwards its NAT holds now (each a mapping it granted and has not seen expire).
int sim_router_forwards(const SimRouter& r, int64_t now_us);

}  // namespace ge::net
