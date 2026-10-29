// The attackers of a secure traffic session (net/traffic_attack.hpp).
//
// ge:summary The attackers' datagrams each tick: forged and stolen tokens, replays, the flood
// ge:keywords net attack forged-token stolen-token replay forgery flood flooder
// ge:skill net
// ge:test tests/net_traffic_test.cpp
#include "net/traffic_attack.hpp"

#include <cmath>
#include <cstring>

#include "net/traffic.hpp"

namespace ge::net {

namespace {

void draw_bytes(core::Rng& rng, uint8_t* out, size_t n) {
  for (size_t i = 0; i < n; i++) out[i] = static_cast<uint8_t>(core::int_in(rng, 0, 255));
}

uint64_t draw64(core::Rng& rng) {
  uint8_t b[8];
  draw_bytes(rng, b, 8);
  uint64_t v = 0;
  for (int i = 0; i < 8; i++) v |= static_cast<uint64_t>(b[i]) << (8 * i);
  return v;
}

// How many of a rate are due by `now_us`: the attacks start at the first tick.
uint64_t due_by(int64_t now_us, double per_s) {
  if (per_s <= 0 || now_us < 0) return 0;
  return static_cast<uint64_t>(std::floor(static_cast<double>(now_us) * per_s / 1e6));
}

// A padded request from `from`, carrying `sealed` (a token's sealed part) or none.
void send_request(SimNetwork& net, const Address& from, const Address& to, uint32_t protocol,
                  uint64_t salt, const uint8_t* sealed) {
  uint8_t out[kMaxPacket];
  Writer w = make_writer(out, sizeof out);
  write_handshake(w, protocol, PacketType::request, {salt, 0, 0, 0, 0});
  if (sealed) std::memcpy(out + kRequestTokenAt, sealed, kSealedTokenBytes);
  sim_send(net, from, to, out, w.size);
}

// The forgers' and the thieves' addresses: a few each, bound so an answer would be counted.
constexpr uint32_t kForgers = 8;
constexpr uint32_t kThieves = 8;

}  // namespace

bool any_attack(const TrafficAttack& a) {
  return a.forged_tokens_per_s > 0 || a.stolen_tokens_per_s > 0 || a.replays_per_s > 0 ||
         a.forgeries_per_s > 0 || a.flood_per_s > 0 || (a.flooders > 0 && a.flooder_packets_per_s > 0);
}

Key traffic_master(uint32_t seed) {
  core::Rng rng = core::make_rng(seed ^ 0x6d61746bu);
  Key k{};
  draw_bytes(rng, k.data(), k.size());
  return k;
}

ConnectToken traffic_token(const Key& master, uint64_t client_id, int64_t expire_us,
                           core::Rng& rng) {
  TokenGrant g;
  g.client_id = client_id;
  g.host_id = 1;
  g.expire_us = expire_us;
  uint8_t random[kTokenRandomBytes];
  draw_bytes(rng, random, sizeof random);
  return token_make(master, traffic_protocol(), g, random);
}

void attackers_init(Attackers& a, const TrafficPlan& plan, SimNetwork& net, const Address& host,
                    uint32_t first_node) {
  a.plan = &plan;
  a.master = traffic_master(plan.seed);
  a.wrong = traffic_master(plan.seed + 1);
  a.host = host;
  a.rng = core::make_rng(plan.seed ^ 0x61747461u);
  a.first_node = first_node;
  a.seen = make_capture();
  for (uint32_t k = 0; k < kForgers + kThieves; k++) sim_bind(net, sim_address(first_node + k, 1));
  const TrafficAttack& at = plan.attack;
  const int64_t expire = static_cast<int64_t>(std::floor((plan.seconds + plan.drain_seconds) * 1e6)) +
                         60000000;
  for (int i = 0; i < at.flooders; i++) {
    auto f = std::make_unique<Attackers::Flooder>();
    open_sim_transport(f->transport, net,
                       sim_address(first_node + kForgers + kThieves + static_cast<uint32_t>(i), 1));
    NodeConfig c;
    c.protocol = traffic_protocol();
    c.max_peers = 1;
    c.channels.count = 3;
    c.channels.delivery[kTrafficInputs] = Delivery::sequenced;
    c.channels.delivery[kTrafficSnapshots] = Delivery::unreliable;
    c.channels.delivery[kTrafficEvents] = Delivery::reliable;
    c.seed = plan.seed * 7919ull + static_cast<uint64_t>(i) + 1;
    node_init(f->node, c, f->transport);
    node_connect_secure(f->node, host, traffic_token(a.master, 100000 + static_cast<uint64_t>(i),
                                                     expire, a.rng),
                        0);
    a.flooders.push_back(std::move(f));
  }
}

void attackers_tick(Attackers& a, SimNetwork& net, int64_t now_us,
                    const std::vector<ConnectToken>& tokens, AttackTally& tally) {
  const TrafficAttack& at = a.plan->attack;
  const uint32_t protocol = traffic_protocol();
  const bool sending = now_us < static_cast<int64_t>(std::floor(a.plan->seconds * 1e6));

  // What the forgers and the thieves were answered.
  Datagram d;
  for (uint32_t k = 0; k < kForgers + kThieves; k++) {
    while (sim_receive(net, sim_address(a.first_node + k, 1), d)) tally.answered++;
  }
  // The honest client 0's last sealed payload this tick (its transport's capture).
  if (a.seen.packets > 0) {
    std::vector<CapturedDatagram> got;
    if (read_capture(a.seen.bytes.data(), a.seen.bytes.size(), got, nullptr)) {
      for (const CapturedDatagram& g : got) {
        if (g.to == a.host && g.data.size() > kSealedHeader &&
            g.data[4] == static_cast<uint8_t>(PacketType::payload)) {
          a.payload = g.data;
        }
      }
    }
    a.seen = make_capture();
  }
  if (!sending) return;

  for (uint64_t k = a.sent[0]; k < due_by(now_us, at.forged_tokens_per_s); k++) {
    const ConnectToken t = traffic_token(a.wrong, 1 + k, now_us + 30000000, a.rng);
    send_request(net, sim_address(a.first_node + static_cast<uint32_t>(k % kForgers), 1), a.host,
                 protocol, draw64(a.rng), t.sealed.data());
    a.sent[0] = k + 1;
    tally.forged_tokens++;
  }
  for (uint64_t k = a.sent[1]; !tokens.empty() && k < due_by(now_us, at.stolen_tokens_per_s); k++) {
    const ConnectToken& t = tokens[k % tokens.size()];
    send_request(net,
                 sim_address(a.first_node + kForgers + static_cast<uint32_t>(k % kThieves), 1),
                 a.host, protocol, draw64(a.rng), t.sealed.data());
    a.sent[1] = k + 1;
    tally.stolen_tokens++;
  }
  // Replays and forgeries come from client 0's own address, as a spoofer sends them.
  const Address client0 = sim_address(1, 1);
  for (uint64_t k = a.sent[2]; !a.payload.empty() && k < due_by(now_us, at.replays_per_s); k++) {
    sim_send(net, client0, a.host, a.payload.data(), a.payload.size());
    a.sent[2] = k + 1;
    tally.replays++;
  }
  for (uint64_t k = a.sent[3]; !a.payload.empty() && k < due_by(now_us, at.forgeries_per_s);
       k++) {
    std::vector<uint8_t> f = a.payload;
    f[12] ^= static_cast<uint8_t>(1 + k % 127);  // the counter far ahead: the window lets it in
    f[kSealedHeader + k % (f.size() - kSealedHeader)] ^= 0x20;
    sim_send(net, client0, a.host, f.data(), f.size());
    a.sent[3] = k + 1;
    tally.forgeries++;
  }
  const uint32_t spread = at.flood_addresses < 1 ? 1 : static_cast<uint32_t>(at.flood_addresses);
  const uint32_t flood_base = a.first_node + kForgers + kThieves + 1000;
  for (uint64_t k = a.sent[4]; k < due_by(now_us, at.flood_per_s); k++) {
    send_request(net, sim_address(flood_base + static_cast<uint32_t>(k % spread), 1), a.host,
                 protocol, k, nullptr);
    a.sent[4] = k + 1;
    tally.flood++;
  }

  // The flooders: clients in good standing that send many times what a client does. Their
  // messages are shorter than the traffic's header, so the host's tallies skip them.
  const uint8_t junk[8] = {9};
  for (auto& f : a.flooders) {
    node_update(f->node, now_us);
    if (!peer_connected(f->node, 0)) continue;
    const uint64_t want = due_by(now_us, at.flooder_packets_per_s);
    for (uint64_t k = a.flood_sent; k < want; k++) {
      node_send(f->node, 0, kTrafficSnapshots, junk, sizeof junk);
      node_flush(f->node, now_us);
      tally.flooder_packets++;
    }
  }
  a.flood_sent = due_by(now_us, at.flooder_packets_per_s);
}

void attackers_flush(Attackers& a, int64_t now_us) {
  for (auto& f : a.flooders) node_flush(f->node, now_us);
}

}  // namespace ge::net
