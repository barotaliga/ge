// OPENING A PORT on the player's home router, so a host behind it can be reached straight from the
// internet: the router is asked to forward one public port to the game's socket, for a lease the
// client renews at half its life, and to take it down again when the game is done. Three
// protocols, tried in order, each where the config allows it (portmap_wire.hpp):
//
//   PCP      a MAP request to the gateway's port 5351 (a NAT-PMP-only router's answer to it moves
//            the client to NAT-PMP at once)
//   NAT-PMP  the external address and a UDP map, to the same port
//   UPnP     SSDP's search, the router's description over HTTP, then SOAP: AddPortMapping (again
//            with a permanent lease where a router takes no other) and GetExternalIPAddress
//
// The outcome is `mapped` with the protocol, the public address (the router's external address and
// the port it gave) and the lease; or `failed` with why. A mapped host passes its port to the
// rendezvous (`reach_host_forward`), and a client anywhere connects straight to it.
//
// The client has a transport of its own (a second socket: the answers come to it); a UDP one for a
// real router, or a simulated one against `SimRouter` (sim_router.hpp). Over UDP, UPnP's HTTP runs
// on a TCP stream to the router; on the simulated network it is one datagram each way, asked
// again on a timeout as TCP would resend. Like the rest of engine/net it takes the time from the
// caller and draws only off its seed:
//
//   portmap_init(pm, config, transport); portmap_start(pm, now)
//   every frame: portmap_update(pm, now)           when done: portmap_close(pm, now), updated on
//
// ge:summary Opening a port on the home router: PCP, NAT-PMP or UPnP IGD; the lease kept, closed
// ge:keywords net portmap port-forward port-mapping upnp igd nat-pmp pcp lease renew router
// ge:keywords open-port external-address ssdp soap gateway
// ge:skill net
// ge:test tests/net_portmap_test.cpp
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "net/address.hpp"
#include "net/tcp_socket.hpp"
#include "net/transport.hpp"

namespace ge::net {

enum class PortmapProto : uint8_t { none, pcp, natpmp, upnp };
const char* portmap_proto_name(PortmapProto p);
bool portmap_proto(std::string_view name, PortmapProto& out);
constexpr uint8_t portmap_bit(PortmapProto p) {
  return static_cast<uint8_t>(1u << static_cast<int>(p));
}
constexpr uint8_t kPortmapAll = portmap_bit(PortmapProto::pcp) | portmap_bit(PortmapProto::natpmp) |
                                portmap_bit(PortmapProto::upnp);

struct PortmapConfig {
  Address gateway;  // the router's LAN address (PCP and NAT-PMP; UPnP finds its own)
  Address local;    // this machine as the router sees it (none: the transport's own address)
  Address ssdp;     // where UPnP's search goes (none: SSDP's group, or the simulated broadcast)
  uint16_t internal_port = 0;  // the game's socket
  uint16_t external_port = 0;  // the public port asked for (0: the internal one)
  uint32_t lifetime_s = 3600;  // the lease asked for; the router may give less
  uint8_t protocols = kPortmapAll;
  int64_t first_wait_us = 250000;  // an unanswered ask is sent again after this, then doubled
  int tries = 4;                   // asks a step before the protocol is given up
  uint64_t seed = 0;               // PCP's nonce (a real game passes os_entropy())
  std::string description = "ge";  // UPnP's label for the mapping
};

enum class PortmapState : uint8_t { idle, asking, mapped, failed, closing, closed };
const char* portmap_state_name(PortmapState s);
enum class PortmapWhy : uint8_t { none, no_router, refused, lease_lost };
const char* portmap_why_name(PortmapWhy w);

struct PortmapStats {
  uint64_t asks = 0, answers = 0, renewals = 0, fallbacks = 0;
};

struct Portmap {
  PortmapConfig config;
  Transport* transport = nullptr;
  PortmapState state = PortmapState::idle;
  PortmapWhy why = PortmapWhy::none;
  PortmapProto proto = PortmapProto::none;  // the one mapped, or being tried
  Address external;                         // the router's external address and the port given
  uint32_t lease_s = 0;                     // as granted (UPnP: 0 is permanent)
  int64_t started_us = 0, mapped_us = 0;    // mapped_us: how long the first mapping took
  int64_t renew_at_us = 0, expires_us = 0;  // the lease's half-life and its end
  bool renewing = false;
  // The step in flight: its kind, asks left, when it is next sent.
  int step = 0;
  int tries_left = 0;
  int64_t next_ask_us = 0, wait_us = 0;
  uint8_t nonce[12] = {};
  bool have_ip = false, have_port = false;  // NAT-PMP's two answers
  // UPnP: the description's and the control's place, and the HTTP exchange in flight.
  Address http_host;
  std::string description_path, control_path, service;
  std::string request;
  std::vector<uint8_t> answer;
  TcpStream stream;
  bool permanent = false;  // the router gives only permanent leases
  PortmapStats stats;
};

void portmap_init(Portmap& pm, const PortmapConfig& config, Transport& transport);
// Ask the router; `mapped` or `failed` follows in portmap_update.
void portmap_start(Portmap& pm, int64_t now_us);
// Read the answers, send what is due, renew the lease at half its life.
void portmap_update(Portmap& pm, int64_t now_us);
// Ask the router to take the mapping down (a lease of 0); `closed` once it says so or the asks
// run out. Keep calling portmap_update until then.
void portmap_close(Portmap& pm, int64_t now_us);

}  // namespace ge::net
