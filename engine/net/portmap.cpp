// Opening a port on the home router: the three protocols' steps, the lease, the close
// (net/portmap.hpp).
//
// ge:summary The port-mapping client's steps: PCP, NAT-PMP, UPnP's search, description and SOAP
// ge:keywords net portmap step retry backoff fallback renew lease close http exchange
// ge:skill net
// ge:test tests/net_portmap_test.cpp
#include "net/portmap.hpp"

#include <cstring>
#include <string_view>

#include "core/prng.hpp"
#include "net/portmap_wire.hpp"

namespace ge::net {

namespace {

using namespace portmap_wire;

enum Step : uint8_t { kNone, kPcp, kNatpmp, kSsdp, kDescribe, kAdd, kExternalIp, kDelete };

void ip_of(const Address& a, uint8_t ip[4]) {
  std::memset(ip, 0, 4);
  if (a.family == Family::sim) {
    sim_to_ipv4(a, ip);
  } else if (a.family == Family::ipv4) {
    std::memcpy(ip, a.bytes, 4);
  }
}

Address local_of(const Portmap& pm) {
  return pm.config.local.family != Family::none ? pm.config.local : pm.transport->local;
}

uint16_t wanted_port(const Portmap& pm) {
  return pm.config.external_port ? pm.config.external_port : pm.config.internal_port;
}

bool closing(const Portmap& pm) { return pm.state == PortmapState::closing; }

void send_bytes(Portmap& pm, const Address& to, const void* data, size_t size, int64_t now_us) {
  transport_send(*pm.transport, to, static_cast<const uint8_t*>(data), size, now_us);
  pm.stats.asks++;
}

// The HTTP exchange: over UDP a TCP stream to the router; on the simulated network one datagram.
void http_send(Portmap& pm, int64_t now_us) {
  pm.answer.clear();
  if (pm.transport->kind == TransportKind::sim) {
    send_bytes(pm, pm.http_host, pm.request.data(), pm.request.size(), now_us);
    return;
  }
  tcp_close(pm.stream);
  if (tcp_connect(pm.stream, pm.http_host, nullptr))
    tcp_send_bytes(pm.stream, reinterpret_cast<const uint8_t*>(pm.request.data()),
                   pm.request.size());
  pm.stats.asks++;
}

void send_step(Portmap& pm, int64_t now_us) {
  const uint32_t lifetime = closing(pm) ? 0 : pm.config.lifetime_s;
  switch (pm.step) {
    case kPcp: {
      PcpMap m;
      m.lifetime_s = lifetime;
      ip_of(local_of(pm), m.client_ip);
      std::memcpy(m.nonce, pm.nonce, 12);
      m.internal_port = pm.config.internal_port;
      // A renewal or a delete names the port it holds; a first ask suggests one.
      m.external_port = pm.external.port ? pm.external.port : wanted_port(pm);
      uint8_t out[kPcpMapSize];
      send_bytes(pm, pm.config.gateway, out, pcp_map_write(out, m), now_us);
      break;
    }
    case kNatpmp: {
      uint8_t out[12];
      if (!pm.have_ip && !closing(pm))
        send_bytes(pm, pm.config.gateway, out, natpmp_external_request(out), now_us);
      if (!pm.have_port) {
        NatpmpMap m;
        m.internal_port = pm.config.internal_port;
        m.external_port = closing(pm) ? 0 : (pm.external.port ? pm.external.port : wanted_port(pm));
        m.lifetime_s = lifetime;
        send_bytes(pm, pm.config.gateway, out, natpmp_map_request(out, m), now_us);
      }
      break;
    }
    case kSsdp: {
      const std::string s = ssdp_search();
      const Address to = pm.config.ssdp.family != Family::none ? pm.config.ssdp
                         : pm.transport->kind == TransportKind::sim
                             ? sim_broadcast(kSsdpPort)
                             : ipv4(239, 255, 255, 250, kSsdpPort);
      send_bytes(pm, to, s.data(), s.size(), now_us);
      break;
    }
    case kDescribe:
    case kAdd:
    case kExternalIp:
    case kDelete:
      http_send(pm, now_us);
      break;
    default:
      break;
  }
}

void begin(Portmap& pm, int step, int64_t now_us) {
  pm.step = step;
  pm.tries_left = pm.config.tries - 1;
  pm.wait_us = pm.config.first_wait_us;
  pm.next_ask_us = now_us + pm.wait_us;
  if (step == kNatpmp) pm.have_port = false;
  if (step == kAdd) {
    UpnpAdd a;
    a.external_port = pm.external.port ? pm.external.port : wanted_port(pm);
    a.internal_port = pm.config.internal_port;
    ip_of(local_of(pm), a.internal_ip);
    a.lease_s = pm.permanent ? 0 : pm.config.lifetime_s;
    a.description = pm.config.description;
    pm.request = soap_add(pm.http_host, pm.control_path, pm.service, a);
  } else if (step == kDescribe) {
    pm.request = http_get(pm.http_host, pm.description_path);
  } else if (step == kExternalIp) {
    pm.request = soap_external_ip(pm.http_host, pm.control_path, pm.service);
  } else if (step == kDelete) {
    pm.request = soap_delete(pm.http_host, pm.control_path, pm.service, pm.external.port);
  }
  send_step(pm, now_us);
}

bool allowed(const Portmap& pm, PortmapProto p) {
  if (!(pm.config.protocols & portmap_bit(p))) return false;
  return p == PortmapProto::upnp || pm.config.gateway.family != Family::none;
}

// The next protocol after the one that just failed, or `failed` when none is left.
void next_proto(Portmap& pm, PortmapWhy why, int64_t now_us) {
  tcp_close(pm.stream);
  for (int p = static_cast<int>(pm.proto) + 1; p <= static_cast<int>(PortmapProto::upnp); p++) {
    const auto proto = static_cast<PortmapProto>(p);
    if (!allowed(pm, proto)) continue;
    if (pm.proto != PortmapProto::none) pm.stats.fallbacks++;
    pm.proto = proto;
    pm.external = Address{};
    pm.have_ip = pm.have_port = false;
    begin(pm,
          proto == PortmapProto::pcp      ? kPcp
          : proto == PortmapProto::natpmp ? kNatpmp
                                          : kSsdp,
          now_us);
    return;
  }
  pm.state = PortmapState::failed;
  pm.why = why;
  pm.step = kNone;
}

void mapped(Portmap& pm, uint32_t lease_s, int64_t now_us) {
  tcp_close(pm.stream);
  pm.stats.answers++;
  if (pm.renewing) pm.stats.renewals++;
  if (pm.state == PortmapState::asking) pm.mapped_us = now_us - pm.started_us;
  pm.state = PortmapState::mapped;
  pm.renewing = false;
  pm.lease_s = lease_s;
  pm.step = kNone;
  // Renewed at half the lease, as RFC 6886 and 6887 advise; a permanent one never.
  pm.renew_at_us = lease_s ? now_us + int64_t{lease_s} * 500000 : INT64_MAX;
  pm.expires_us = lease_s ? now_us + int64_t{lease_s} * 1000000 : INT64_MAX;
}

void closed(Portmap& pm) {
  tcp_close(pm.stream);
  pm.state = PortmapState::closed;
  pm.step = kNone;
}

// A step whose asks ran out.
void gave_up(Portmap& pm, int64_t now_us) {
  if (closing(pm)) {
    closed(pm);
  } else if (pm.renewing) {
    tcp_close(pm.stream);
    pm.state = PortmapState::failed;
    pm.why = PortmapWhy::lease_lost;
    pm.step = kNone;
  } else {
    next_proto(pm, PortmapWhy::no_router, now_us);
  }
}

// A step the router refused.
void refused(Portmap& pm, int64_t now_us) {
  if (closing(pm)) {
    closed(pm);
  } else if (pm.renewing) {
    pm.renewing = false;
    pm.state = PortmapState::failed;
    pm.why = PortmapWhy::lease_lost;
    pm.step = kNone;
  } else {
    next_proto(pm, PortmapWhy::refused, now_us);
  }
}

void on_pcp(Portmap& pm, const uint8_t* data, size_t size, int64_t now_us) {
  if (message_version(data, size) == kNatpmpVersion) {
    // A NAT-PMP router's answer to PCP: its own version, so NAT-PMP is asked at once.
    NatpmpAnswer a;
    if (natpmp_read_answer(data, size, a) && a.result == kNatpmpUnsupportedVersion &&
        !pm.renewing && !closing(pm))
      next_proto(pm, PortmapWhy::no_router, now_us);
    return;
  }
  PcpMap m;
  if (!pcp_map_read(data, size, m) || !m.answer || std::memcmp(m.nonce, pm.nonce, 12) != 0) return;
  if (closing(pm)) {
    closed(pm);
    return;
  }
  if (m.result != kPcpOk) {
    refused(pm, now_us);
    return;
  }
  pm.external = ipv4_or_sim(m.external_ip, m.external_port);
  mapped(pm, m.lifetime_s, now_us);
}

void on_natpmp(Portmap& pm, const uint8_t* data, size_t size, int64_t now_us) {
  NatpmpAnswer a;
  if (!natpmp_read_answer(data, size, a)) return;
  if (a.result != kNatpmpOk) {
    refused(pm, now_us);
    return;
  }
  if (a.op == kOpExternal) {
    pm.have_ip = true;
    const uint16_t port = pm.external.port;
    pm.external = ipv4_or_sim(a.ip, port);
  } else if (a.op == kOpMapUdp && a.map.internal_port == pm.config.internal_port) {
    if (closing(pm)) {
      closed(pm);
      return;
    }
    pm.have_port = true;
    pm.external.port = a.map.external_port;
    pm.lease_s = a.map.lifetime_s;
  }
  if (pm.have_ip && pm.have_port) mapped(pm, pm.lease_s, now_us);
}

void on_http(Portmap& pm, int status, std::string_view body, int64_t now_us) {
  tcp_close(pm.stream);
  pm.answer.clear();
  if (pm.step == kDescribe) {
    if (status != 200 || !igd_control(body, pm.service, pm.control_path)) {
      refused(pm, now_us);
      return;
    }
    begin(pm, kAdd, now_us);
  } else if (pm.step == kAdd) {
    if (status != 200) {
      if (soap_error(body) == kUpnpPermanentOnly && !pm.permanent) {
        pm.permanent = true;  // asked again for a lease of 0, the only one it gives
        begin(pm, kAdd, now_us);
        return;
      }
      refused(pm, now_us);
      return;
    }
    pm.external.port = pm.external.port ? pm.external.port : wanted_port(pm);
    if (pm.renewing) {
      mapped(pm, pm.lease_s, now_us);
      return;
    }
    begin(pm, kExternalIp, now_us);
  } else if (pm.step == kExternalIp) {
    uint8_t ip[4];
    if (status != 200 || !parse_ipv4(xml_value(body, "NewExternalIPAddress"), ip)) {
      refused(pm, now_us);
      return;
    }
    const uint16_t port = pm.external.port;
    pm.external = ipv4_or_sim(ip, port);
    mapped(pm, pm.permanent ? 0 : pm.config.lifetime_s, now_us);
  } else if (pm.step == kDelete) {
    closed(pm);
  }
}

void on_datagram(Portmap& pm, const Address& from, const uint8_t* data, size_t size,
                 int64_t now_us) {
  if (pm.step == kPcp && from == pm.config.gateway) {
    on_pcp(pm, data, size, now_us);
  } else if (pm.step == kNatpmp && from == pm.config.gateway) {
    on_natpmp(pm, data, size, now_us);
  } else if (pm.step == kSsdp) {
    std::string location;
    const std::string_view text(reinterpret_cast<const char*>(data), size);
    if (!ssdp_location(text, location) ||
        !parse_http_url(location, pm.http_host, pm.description_path))
      return;
    pm.stats.answers++;
    begin(pm, kDescribe, now_us);
  } else if (pm.step >= kDescribe && pm.transport->kind == TransportKind::sim &&
             from == pm.http_host) {
    int status = 0;
    std::string_view body;
    const std::string_view text(reinterpret_cast<const char*>(data), size);
    if (http_answer_whole(text, true, status, body)) on_http(pm, status, body, now_us);
  }
}

}  // namespace

const char* portmap_proto_name(PortmapProto p) {
  switch (p) {
    case PortmapProto::none:
      return "none";
    case PortmapProto::pcp:
      return "pcp";
    case PortmapProto::natpmp:
      return "natpmp";
    case PortmapProto::upnp:
      return "upnp";
  }
  return "?";
}

bool portmap_proto(std::string_view name, PortmapProto& out) {
  for (int p = 0; p <= static_cast<int>(PortmapProto::upnp); p++) {
    if (name == portmap_proto_name(static_cast<PortmapProto>(p))) {
      out = static_cast<PortmapProto>(p);
      return true;
    }
  }
  return false;
}

const char* portmap_state_name(PortmapState s) {
  switch (s) {
    case PortmapState::idle:
      return "idle";
    case PortmapState::asking:
      return "asking";
    case PortmapState::mapped:
      return "mapped";
    case PortmapState::failed:
      return "failed";
    case PortmapState::closing:
      return "closing";
    case PortmapState::closed:
      return "closed";
  }
  return "?";
}

const char* portmap_why_name(PortmapWhy w) {
  switch (w) {
    case PortmapWhy::none:
      return "none";
    case PortmapWhy::no_router:
      return "no-router";
    case PortmapWhy::refused:
      return "refused";
    case PortmapWhy::lease_lost:
      return "lease-lost";
  }
  return "?";
}

void portmap_init(Portmap& pm, const PortmapConfig& config, Transport& transport) {
  tcp_close(pm.stream);
  pm = Portmap{};
  pm.config = config;
  if (pm.config.tries < 1) pm.config.tries = 1;
  pm.transport = &transport;
  core::Rng rng = core::make_rng(static_cast<uint32_t>(config.seed ^ (config.seed >> 32)));
  for (uint8_t& b : pm.nonce) b = static_cast<uint8_t>(core::next(rng) * 256.0);
}

void portmap_start(Portmap& pm, int64_t now_us) {
  tcp_close(pm.stream);
  pm.state = PortmapState::asking;
  pm.why = PortmapWhy::none;
  pm.proto = PortmapProto::none;
  pm.external = Address{};
  pm.permanent = pm.renewing = false;
  pm.started_us = now_us;
  next_proto(pm, PortmapWhy::no_router, now_us);
}

void portmap_update(Portmap& pm, int64_t now_us) {
  uint8_t buf[kMaxDatagram];
  Address from;
  while (true) {
    const int n = transport_receive(*pm.transport, buf, sizeof buf, from, now_us);
    if (n < 0) break;
    if (pm.step != kNone) on_datagram(pm, from, buf, static_cast<size_t>(n), now_us);
  }
  if (pm.step >= kDescribe && pm.stream.handle != -1) {
    tcp_pump(pm.stream);
    tcp_take_bytes(pm.stream, pm.answer);
    int status = 0;
    std::string_view body;
    const std::string_view text(reinterpret_cast<const char*>(pm.answer.data()), pm.answer.size());
    if (http_answer_whole(text, pm.stream.closed, status, body)) {
      const std::string held(body);  // on_http closes the stream and clears the answer
      on_http(pm, status, held, now_us);
    }
  }
  if (pm.state == PortmapState::mapped && !pm.renewing && now_us >= pm.renew_at_us) {
    pm.renewing = true;
    pm.have_port = false;
    begin(pm,
          pm.proto == PortmapProto::pcp      ? kPcp
          : pm.proto == PortmapProto::natpmp ? kNatpmp
                                             : kAdd,
          now_us);
  }
  if (pm.step == kNone || now_us < pm.next_ask_us) return;
  // A renewal keeps asking until the lease it renews runs out, at the longest wait: a loss burst
  // over a few sparse asks must not cost a lease that still has half its life.
  const bool lease_left = pm.renewing && now_us < pm.expires_us;
  if (pm.tries_left <= 0 && !lease_left) {
    gave_up(pm, now_us);
    return;
  }
  pm.tries_left--;
  if (pm.tries_left >= 0) pm.wait_us *= 2;
  pm.next_ask_us = now_us + pm.wait_us;
  send_step(pm, now_us);
}

void portmap_close(Portmap& pm, int64_t now_us) {
  if (pm.state != PortmapState::mapped) {
    // Nothing held: an ask in flight is dropped, and a later answer is ignored.
    tcp_close(pm.stream);
    pm.state = PortmapState::closed;
    pm.step = kNone;
    return;
  }
  pm.state = PortmapState::closing;
  pm.renewing = false;
  pm.have_port = false;
  begin(pm,
        pm.proto == PortmapProto::pcp      ? kPcp
        : pm.proto == PortmapProto::natpmp ? kNatpmp
                                           : kDelete,
        now_us);
}

}  // namespace ge::net
