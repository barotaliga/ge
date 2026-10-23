// The simulated router's answers (net/sim_router.hpp).
//
// ge:summary The simulated router's answers: PCP, NAT-PMP, SSDP, UPnP SOAP; leases as forwards
// ge:keywords net sim-router pcp nat-pmp upnp ssdp soap forward lease epoch grant delete
// ge:skill net
// ge:test tests/net_portmap_test.cpp
#include "net/sim_router.hpp"

#include <cstring>
#include <string>
#include <string_view>

#include "net/portmap_wire.hpp"

namespace ge::net {

namespace {

using namespace portmap_wire;

constexpr std::string_view kDescriptionPath = "/rootDesc.xml";
constexpr std::string_view kControlPath = "/ctl/IPConn";

SimNat& nat_of(SimRouter& r) { return r.net->nats[static_cast<size_t>(r.config.nat)]; }

bool speaks(const SimRouter& r, PortmapProto p) {
  return (r.config.protocols & portmap_bit(p)) != 0;
}

uint32_t epoch(const SimRouter& r, int64_t now_us) {
  return static_cast<uint32_t>((now_us - r.started_us) / 1000000);
}

void external_ip(SimRouter& r, uint8_t ip[4]) {
  sim_to_ipv4(sim_address(nat_of(r).config.public_node, 0), ip);
}

bool on_lan(SimRouter& r, const Address& from) {
  return from.family == Family::sim && sim_nat_of(*r.net, sim_node(from)) == r.config.nat;
}

void reply(Transport& t, const Address& to, const void* data, size_t size, int64_t now_us) {
  transport_send(t, to, static_cast<const uint8_t*>(data), size, now_us);
}

// A forward for `inside` on `port` (or near it, when `elsewhere` lets it move): the port given,
// 0 when none could be.
uint16_t grant(SimRouter& r, const Address& inside, uint16_t port, int64_t until_us,
               bool elsewhere) {
  SimNat& nat = nat_of(r);
  for (SimNat::Forward& f : nat.forwards) {
    // A mapping this address already holds is renewed where it is (RFC 6886 § 3.3).
    if (f.inside == inside && f.until_us > r.net->now_us) {
      f.until_us = until_us;
      return f.port;
    }
  }
  for (int i = 0; i < (elsewhere ? 64 : 1); i++) {
    const auto p = static_cast<uint16_t>(port + i);
    if (p == 0) continue;
    if (sim_nat_forward(*r.net, r.config.nat, inside, p, until_us)) {
      r.stats.mapped++;
      return p;
    }
  }
  return 0;
}

bool take_down(SimRouter& r, const Address& inside) {
  for (const SimNat::Forward& f : nat_of(r).forwards) {
    if (f.inside == inside) {
      sim_nat_unforward(*r.net, r.config.nat, f.port);
      r.stats.deleted++;
      return true;
    }
  }
  return false;
}

int64_t until(uint32_t lifetime_s, int64_t now_us) {
  return now_us + int64_t{lifetime_s} * 1000000;
}

void on_natpmp(SimRouter& r, const Address& from, const uint8_t* data, size_t size,
               int64_t now_us) {
  uint8_t op = 0;
  NatpmpMap m;
  if (!natpmp_read_request(data, size, op, m)) return;
  NatpmpAnswer a;
  a.op = op;
  a.epoch_s = epoch(r, now_us);
  r.stats.natpmp++;
  if (op == kOpExternal) {
    external_ip(r, a.ip);
  } else if (op != kOpMapUdp) {
    a.result = kNatpmpUnsupportedOpcode;
  } else {
    const Address inside = sim_address(sim_node(from), m.internal_port);
    a.map.internal_port = m.internal_port;
    if (m.lifetime_s == 0) {
      take_down(r, inside);  // a delete of nothing succeeds too (RFC 6886 § 3.4)
    } else {
      const uint32_t life =
          m.lifetime_s < r.config.max_lifetime_s ? m.lifetime_s : r.config.max_lifetime_s;
      const uint16_t want = m.external_port ? m.external_port : m.internal_port;
      a.map.external_port = grant(r, inside, want, until(life, now_us), true);
      a.map.lifetime_s = life;
      if (!a.map.external_port) {
        a.result = kNatpmpNoResources;
        r.stats.refused++;
      }
    }
  }
  uint8_t out[16];
  reply(r.control, from, out, natpmp_answer(out, a), now_us);
}

void on_pcp(SimRouter& r, const Address& from, const uint8_t* data, size_t size, int64_t now_us) {
  PcpMap m;
  if (!pcp_map_read(data, size, m) || m.answer) return;
  r.stats.pcp++;
  PcpMap a = m;
  a.answer = true;
  a.result = kPcpOk;
  a.epoch_s = epoch(r, now_us);
  uint8_t seen[4];
  sim_to_ipv4(from, seen);
  if (std::memcmp(seen, m.client_ip, 4) != 0) {
    a.result = kPcpAddressMismatch;  // another NAT between it and us
  } else if (m.protocol != kProtoUdp) {
    a.result = kPcpUnsupportedProtocol;
  } else {
    const Address inside = sim_address(sim_node(from), m.internal_port);
    if (m.lifetime_s == 0) {
      take_down(r, inside);
      a.external_port = 0;
    } else {
      a.lifetime_s =
          m.lifetime_s < r.config.max_lifetime_s ? m.lifetime_s : r.config.max_lifetime_s;
      const uint16_t want = m.external_port ? m.external_port : m.internal_port;
      a.external_port = grant(r, inside, want, until(a.lifetime_s, now_us), true);
      external_ip(r, a.external_ip);
      if (!a.external_port) a.result = kPcpNoResources;
    }
  }
  if (a.result != kPcpOk) {
    r.stats.refused++;
    a.lifetime_s = 0;
  }
  uint8_t out[kPcpMapSize];
  reply(r.control, from, out, pcp_map_write(out, a), now_us);
}

void on_control(SimRouter& r, const Address& from, const uint8_t* data, size_t size,
                int64_t now_us) {
  const int version = message_version(data, size);
  if (version == kNatpmpVersion && speaks(r, PortmapProto::natpmp)) {
    on_natpmp(r, from, data, size, now_us);
  } else if (version == kPcpVersion && speaks(r, PortmapProto::pcp)) {
    on_pcp(r, from, data, size, now_us);
  } else if (version == kPcpVersion && speaks(r, PortmapProto::natpmp) && size >= 2) {
    // A NAT-PMP router's answer to a version it does not speak: its own, and UNSUPP_VERSION.
    NatpmpAnswer a;
    a.op = static_cast<uint8_t>(data[1] & 0x7f);
    a.result = kNatpmpUnsupportedVersion;
    a.epoch_s = epoch(r, now_us);
    uint8_t out[16];
    reply(r.control, from, out, natpmp_answer(out, a), now_us);
  }
}

std::string description() {
  std::string d =
      "<?xml version=\"1.0\"?><root xmlns=\"urn:schemas-upnp-org:device-1-0\"><device>"
      "<deviceType>";
  d += kIgdDevice;
  d += "</deviceType><friendlyName>ge simulated router</friendlyName><deviceList><device>"
       "<deviceType>urn:schemas-upnp-org:device:WANDevice:1</deviceType><deviceList><device>"
       "<deviceType>urn:schemas-upnp-org:device:WANConnectionDevice:1</deviceType><serviceList>"
       "<service><serviceType>";
  d += kIgdServices[1];
  d += "</serviceType><serviceId>urn:upnp-org:serviceId:WANIPConn1</serviceId><controlURL>";
  d += kControlPath;
  d += "</controlURL><eventSubURL>/evt/IPConn</eventSubURL><SCPDURL>/WANIPCn.xml</SCPDURL>"
       "</service></serviceList></device></deviceList></device></deviceList></device></root>";
  return d;
}

uint32_t number(std::string_view body, std::string_view name, bool& ok) {
  uint32_t v = 0;
  const std::string_view t = xml_value(body, name);
  if (t.empty()) ok = false;
  for (char c : t) {
    if (c < '0' || c > '9') ok = false;
    v = v * 10 + static_cast<uint32_t>(c - '0');
  }
  return v;
}

std::string on_soap(SimRouter& r, const Address& from, std::string_view request, int64_t now_us) {
  const std::string_view action = soap_action(request);
  const size_t at = request.find("\r\n\r\n");
  const std::string_view body = at == std::string_view::npos ? request : request.substr(at + 4);
  const std::string_view service = kIgdServices[1];
  r.stats.upnp++;
  if (action == "GetExternalIPAddress") {
    uint8_t ip[4];
    external_ip(r, ip);
    return soap_answer(service, action,
                       "<NewExternalIPAddress>" + ipv4_text(ip) + "</NewExternalIPAddress>");
  }
  if (action == "AddPortMapping") {
    bool ok = true;
    const uint32_t external = number(body, "NewExternalPort", ok);
    const uint32_t internal = number(body, "NewInternalPort", ok);
    const uint32_t lease = number(body, "NewLeaseDuration", ok);
    uint8_t client[4], seen[4];
    sim_to_ipv4(from, seen);
    if (!ok || external == 0 || external > 65535 || internal == 0 || internal > 65535 ||
        xml_value(body, "NewProtocol") != "UDP" ||
        !parse_ipv4(xml_value(body, "NewInternalClient"), client)) {
      r.stats.refused++;
      return soap_fault(402, "Invalid Args");
    }
    if (std::memcmp(client, seen, 4) != 0) {
      r.stats.refused++;
      return soap_fault(606, "Action not authorized");
    }
    if (r.config.permanent_only && lease != 0) {
      r.stats.refused++;
      return soap_fault(kUpnpPermanentOnly, "OnlyPermanentLeasesSupported");
    }
    const uint32_t life = lease < r.config.max_lifetime_s ? lease : r.config.max_lifetime_s;
    const Address inside = sim_address(sim_node(from), static_cast<uint16_t>(internal));
    const int64_t until_us = lease ? until(life, now_us) : INT64_MAX;
    if (grant(r, inside, static_cast<uint16_t>(external), until_us, false) != external) {
      r.stats.refused++;
      return soap_fault(kUpnpConflict, "ConflictInMappingEntry");
    }
    return soap_answer(service, action, "");
  }
  if (action == "DeletePortMapping") {
    bool ok = true;
    const uint32_t external = number(body, "NewExternalPort", ok);
    for (const SimNat::Forward& f : nat_of(r).forwards) {
      if (ok && f.port == external && sim_node(f.inside) == sim_node(from)) {
        sim_nat_unforward(*r.net, r.config.nat, f.port);
        r.stats.deleted++;
        return soap_answer(service, action, "");
      }
    }
    return soap_fault(kUpnpNoSuchEntry, "NoSuchEntryInArray");
  }
  return soap_fault(401, "Invalid Action");
}

void on_http(SimRouter& r, const Address& from, const uint8_t* data, size_t size, int64_t now_us) {
  const std::string_view request(reinterpret_cast<const char*>(data), size);
  std::string answer;
  if (request.substr(0, 4) == "GET ") {
    const size_t sp = request.find(' ', 4);
    if (sp == std::string_view::npos || request.substr(4, sp - 4) != kDescriptionPath) {
      answer = http_answer(404, "text/plain", "not found");
    } else {
      answer = http_answer(200, "text/xml", description());
    }
  } else if (request.substr(0, 5) == "POST ") {
    answer = on_soap(r, from, request, now_us);
  } else {
    return;
  }
  reply(r.http, from, answer.data(), answer.size(), now_us);
}

}  // namespace

bool sim_router_init(SimRouter& r, SimNetwork& net, const SimRouterConfig& config, int64_t now_us) {
  r = SimRouter{};
  r.config = config;
  r.net = &net;
  r.started_us = now_us;
  if (config.nat < 0 || config.nat >= static_cast<int>(net.nats.size())) return false;
  const uint32_t gw = net.nats[static_cast<size_t>(config.nat)].config.gateway;
  if (gw == kSimNoNode) return false;
  return open_sim_transport(r.control, net, sim_address(gw, kServerPort)) &&
         open_sim_transport(r.ssdp, net, sim_address(gw, kSsdpPort)) &&
         open_sim_transport(r.http, net, sim_address(gw, config.http_port));
}

void sim_router_update(SimRouter& r, int64_t now_us) {
  uint8_t buf[kMaxDatagram];
  Address from;
  int n = 0;
  while ((n = transport_receive(r.control, buf, sizeof buf, from, now_us)) >= 0) {
    if (on_lan(r, from)) on_control(r, from, buf, static_cast<size_t>(n), now_us);
  }
  while ((n = transport_receive(r.ssdp, buf, sizeof buf, from, now_us)) >= 0) {
    const std::string_view text(reinterpret_cast<const char*>(buf), static_cast<size_t>(n));
    if (!on_lan(r, from) || !speaks(r, PortmapProto::upnp) || !is_ssdp_search(text)) continue;
    r.stats.ssdp++;
    const uint32_t gw = nat_of(r).config.gateway;
    const std::string a =
        ssdp_answer(http_url(sim_address(gw, r.config.http_port), kDescriptionPath));
    reply(r.ssdp, from, a.data(), a.size(), now_us);
  }
  while ((n = transport_receive(r.http, buf, sizeof buf, from, now_us)) >= 0) {
    if (on_lan(r, from) && speaks(r, PortmapProto::upnp))
      on_http(r, from, buf, static_cast<size_t>(n), now_us);
  }
}

int sim_router_forwards(const SimRouter& r, int64_t now_us) {
  int n = 0;
  for (const SimNat::Forward& f : r.net->nats[static_cast<size_t>(r.config.nat)].forwards)
    n += f.until_us > now_us ? 1 : 0;
  return n;
}

}  // namespace ge::net
