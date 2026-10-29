// Hosting from inside a game (net/hosting.hpp): LAN answers, pings, the forward, the list.
//
// ge:summary Hosting: the advert kept current, probes answered, the forward, advertise and unlist
// ge:keywords net hosting advertise unlist ping forward listed invite lan
// ge:skill net
// ge:test tests/net_hosting_test.cpp
#include "net/hosting.hpp"

#include <cstring>

#include "net/reach_wire.hpp"
#include "net/wire.hpp"

namespace ge::net {

namespace {

bool same(const Advert& a, const Advert& b) {
  return a.port == b.port && a.seated == b.seated && a.max_players == b.max_players &&
         a.phase == b.phase && a.flags == b.flags && std::strcmp(a.name, b.name) == 0;
}

void advertise(Hosting& h, bool listed, int64_t now_us) {
  uint8_t out[reach_wire::kMaxSize + kProbeSize];
  Writer w = make_writer(out, sizeof out);
  const bool fresh = !same(h.advert, h.advertised) || listed != h.advertised_listed ||
                     h.advertised_us < 0 || now_us - h.advertised_us >= h.config.advertise_us;
  if (fresh) {
    h.serial++;
    h.acked = false;
    h.advertised_us = now_us;
  }
  reach_wire::write_start(w, reach_wire::kAdvertise);
  write_u64(w, h.config.session);
  write_u32(w, h.serial);
  write_u32(w, h.config.protocol);
  write_u8(w, listed ? 1 : 0);
  write_advert_body(w, h.advert);
  write_pad_to(w, reach_wire::kMaxSize);
  // From the node's own socket: the address the session registered from, the only one the
  // rendezvous takes an advertise from.
  transport_send(*h.node->transport, h.config.rendezvous, out, w.size, now_us);
  h.advertised = h.advert;
  h.advertised_listed = listed;
  h.sent_us = now_us;
  h.stats.advertised++;
}

// The rendezvous's acknowledgement of the last advertise, among the node's foreign datagrams.
void read_acks(Hosting& h) {
  for (const Foreign& f : h.node->foreign) {
    if (f.from != h.config.rendezvous) continue;
    Reader r = make_reader(h.node->foreign_bytes.data() + f.offset, f.size);
    reach_wire::Kind kind{};
    if (!reach_wire::read_start(r, kind) || kind != reach_wire::kAdvertised) continue;
    const uint64_t id = read_u64(r);
    const uint32_t serial = read_u32(r);
    if (!r.bad && id == h.config.session && serial == h.serial && !h.acked) {
      h.acked = true;
      h.stats.acked++;
    }
  }
}

// An advertise is due: the advert changed, its refresh came round, or the last went unanswered.
bool due(const Hosting& h, bool listed, int64_t now_us) {
  if (h.sent_us < 0 || !same(h.advert, h.advertised) || listed != h.advertised_listed) return true;
  if (now_us - h.advertised_us >= h.config.advertise_us) return true;
  return !h.acked && now_us - h.sent_us >= h.config.ask_us;
}

uint64_t draw64(uint64_t seed) {
  core::Rng rng = core::make_rng(static_cast<uint32_t>(seed ^ (seed >> 32)) ^ 0x5e551017u);
  const auto hi = static_cast<uint64_t>(core::next(rng) * 4294967296.0);
  const auto lo = static_cast<uint64_t>(core::next(rng) * 4294967296.0);
  return (hi << 32 | lo) | 1;
}

}  // namespace

const char* visibility_name(Visibility v) {
  switch (v) {
    case Visibility::lan:
      return "lan";
    case Visibility::invite:
      return "invite";
    case Visibility::listed:
      return "listed";
  }
  return "?";
}

bool visibility_of(std::string_view name, Visibility& out) {
  for (Visibility v : {Visibility::lan, Visibility::invite, Visibility::listed}) {
    if (name == visibility_name(v)) {
      out = v;
      return true;
    }
  }
  return false;
}

const char* hosting_state_name(HostingState s) {
  switch (s) {
    case HostingState::idle:
      return "idle";
    case HostingState::open:
      return "open";
    case HostingState::closing:
      return "closing";
    case HostingState::closed:
      return "closed";
  }
  return "?";
}

void hosting_start(Hosting& h, const HostingConfig& config, Node& node, SessionHost& session,
                   Transport* lan, Transport* router, int64_t now_us) {
  h.config = config;
  h.node = &node;
  h.session = &session;
  h.lan = lan;
  h.router = router;
  h.state = HostingState::open;
  h.started_us = now_us;
  h.stats = HostingStats{};
  h.forwarded = 0;
  h.advertised = Advert{};
  h.advertised_listed = false;
  h.serial = 0;
  h.acked = false;
  h.advertised_us = h.sent_us = -1;
  h.unlist_left = 0;
  node.config.keep_foreign = true;  // the pings and the rendezvous's answers arrive there
  h.registering = config.visibility != Visibility::lan && config.rendezvous.family != Family::none;
  if (h.registering && !h.config.session) h.config.session = draw64(config.seed);
  if (!h.registering) h.config.session = 0;
  if (h.registering) {
    ReachConfig rc = config.reach;
    rc.rendezvous = config.rendezvous;
    rc.session = h.config.session;
    reach_host_init(h.reach, rc, node);
  }
  h.mapping = h.registering && config.open_port && router;
  if (h.mapping) {
    PortmapConfig pc = config.portmap;
    if (!pc.internal_port) pc.internal_port = node.transport->local.port;
    portmap_init(h.portmap, pc, *router);
    portmap_start(h.portmap, now_us);
  }
  h.advert = advert_of(session, config.protocol, node.transport->local.port, config.name.c_str());
  h.advert.session = h.config.session;
  h.advert.flags = config.locked ? kAdvertLocked : 0;
}

void hosting_update(Hosting& h, int64_t now_us) {
  if (!h.node || h.state == HostingState::idle || h.state == HostingState::closed) return;
  const bool open = h.state == HostingState::open;
  h.advert.seated = session_player_count(*h.session, false);
  h.advert.phase = h.session->phase;
  // Pings on the node's own socket, and the LAN's probes on the discovery socket.
  for (const Foreign& f : h.node->foreign) {
    uint8_t out[kProbeSize];
    const size_t n =
        lan_answer_probe(h.node->foreign_bytes.data() + f.offset, f.size, h.advert, out);
    if (n && open && transport_send(*h.node->transport, f.from, out, n, now_us)) h.stats.pings++;
  }
  if (h.lan) {
    if (open)
      h.stats.lan_answers += static_cast<uint64_t>(lan_answer(*h.lan, h.advert, now_us));
    else
      lan_answer(*h.lan, Advert{}, now_us);  // read and drop: protocol 0 answers nothing
  }
  if (h.mapping) {
    portmap_update(h.portmap, now_us);
    const uint16_t port = h.portmap.state == PortmapState::mapped ? h.portmap.external.port : 0;
    if (open && h.registering && port != h.forwarded) reach_host_forward(h.reach, port, now_us);
    h.forwarded = port;
  }
  if (h.registering) read_acks(h);
  if (open && h.registering) {
    reach_host_update(h.reach, now_us);
    if (h.config.visibility == Visibility::listed && h.reach.registered && due(h, true, now_us))
      advertise(h, true, now_us);
  }
  if (h.state == HostingState::closing) {
    if (h.unlist_left > 0 && !h.acked && now_us - h.sent_us >= h.config.ask_us) {
      advertise(h, false, now_us);
      h.unlist_left--;
    }
    // The forward comes down only once the unlist is in: the rendezvous takes it only from the
    // address the session registered from, which is the forward's.
    const bool unlisted = h.acked || h.unlist_left <= 0;
    if (unlisted && h.mapping && h.portmap.state == PortmapState::mapped)
      portmap_close(h.portmap, now_us);
    const bool down = !h.mapping || h.portmap.state == PortmapState::closed ||
                      h.portmap.state == PortmapState::failed ||
                      h.portmap.state == PortmapState::idle;
    if (down && unlisted) h.state = HostingState::closed;
  }
}

void hosting_stop(Hosting& h, int64_t now_us) {
  if (h.state != HostingState::open) return;
  h.state = HostingState::closing;
  h.node->closed = true;
  if (h.registering && h.config.visibility == Visibility::listed && h.reach.registered) {
    advertise(h, false, now_us);
    h.unlist_left = h.config.unlist_tries - 1;
  }
  if (h.mapping && h.portmap.state != PortmapState::mapped) h.mapping = false;  // never got one
  hosting_update(h, now_us);
}

uint64_t hosting_invite(const Hosting& h) { return h.config.session; }

}  // namespace ge::net
