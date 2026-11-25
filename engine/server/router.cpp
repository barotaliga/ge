// The server's game port opened on the home router, defined: router.hpp's header states it.
//
// ge:summary The server's game port forwarded by the home router: the gateway, the ask, the lines
// ge:keywords sv_open_port sv_gateway router_open router_frame default-gateway proc-net-route
// ge:skill net
// ge:test tests/server_router_test.cpp
#include "server/router.hpp"

#include <fstream>
#include <sstream>

#include "net/portmap_wire.hpp"

namespace ge::server {

namespace net = ge::net;

namespace {

// The default route's gateway from Linux's route table (hex, the address's bytes low first).
bool linux_gateway(net::Address& out) {
  std::ifstream f("/proc/net/route");
  if (!f) return false;
  std::string line;
  std::getline(f, line);  // the heading
  while (std::getline(f, line)) {
    std::istringstream in(line);
    std::string iface, dest, gw, flags;
    if (!(in >> iface >> dest >> gw >> flags)) continue;
    if (dest != "00000000") continue;
    const unsigned long g = std::stoul(gw, nullptr, 16);
    const unsigned long fl = std::stoul(flags, nullptr, 16);
    if ((fl & 0x2UL) == 0 || g == 0) continue;  // RTF_GATEWAY: a route through a router
    out = net::ipv4(static_cast<uint8_t>(g & 0xff), static_cast<uint8_t>((g >> 8) & 0xff),
                    static_cast<uint8_t>((g >> 16) & 0xff), static_cast<uint8_t>((g >> 24) & 0xff),
                    net::portmap_wire::kServerPort);
    return true;
  }
  return false;
}

void say(ServerRouter& r, std::string line) { r.said.push_back(std::move(line)); }

}  // namespace

bool router_gateway(const std::string& text, net::Address& out) {
  if (!text.empty()) {
    const std::string with_port = text.find(':') == std::string::npos
                                      ? text + ":" + std::to_string(net::portmap_wire::kServerPort)
                                      : text;
    return net::parse_address(with_port, out);
  }
  return linux_gateway(out);
}

void router_open(ServerRouter& r, net::Transport& transport, const net::Address& gateway,
                 uint16_t port, uint64_t seed, int64_t now_us) {
  net::PortmapConfig pc;
  pc.gateway = gateway;
  pc.internal_port = port;
  pc.external_port = port;
  pc.lifetime_s = 3600;
  pc.seed = seed;
  pc.description = "game server";
  if (gateway.family == net::Family::none)  // no router known: UPnP's search alone
    pc.protocols = net::portmap_bit(net::PortmapProto::upnp);
  r.transport = &transport;
  r.port = port;
  r.on = true;
  r.told = net::PortmapState::asking;
  net::portmap_init(r.pm, pc, transport);
  net::portmap_start(r.pm, now_us);
}

void router_frame(ServerRouter& r, int64_t now_us) {
  if (!r.on) return;
  net::portmap_update(r.pm, now_us);
  const net::PortmapState s = r.pm.state;
  if (s == r.told) return;
  const std::string udp = "UDP " + std::to_string(r.port);
  if (s == net::PortmapState::mapped) {
    say(r, std::string("The router forwards ") + udp + " to this server (" +
               net::portmap_proto_name(r.pm.proto) + "): players reach it at " +
               net::to_string(r.pm.external) + ".");
  } else if (s == net::PortmapState::failed && r.pm.why == net::PortmapWhy::lease_lost) {
    say(r, "The router dropped the forward of " + udp + "; asking it again.");
    net::portmap_start(r.pm, now_us);
  } else if (s == net::PortmapState::failed) {
    say(r, std::string("The router did not forward ") + udp + " (" +
               net::portmap_why_name(r.pm.why) +
               "): open it in the router's settings by hand, or set sv_gateway to the router's "
               "address.");
  }
  r.told = r.pm.state;
}

void router_close(ServerRouter& r, int64_t now_us) {
  if (!r.on) return;
  if (r.pm.state == net::PortmapState::mapped) {
    net::portmap_close(r.pm, now_us);
    r.told = r.pm.state;
  } else {
    r.on = false;
  }
}

bool router_done(const ServerRouter& r) {
  return !r.on || r.pm.state == net::PortmapState::closed ||
         r.pm.state == net::PortmapState::failed || r.pm.state == net::PortmapState::idle;
}

}  // namespace ge::server
