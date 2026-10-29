// engine/net/node.hpp with security and the flood guards (M17i) over the simulated network: clients
// with the matchmaker's tokens connect and talk; no token, a forged one, an expired one, one for
// another host and one replayed from another address are refused; a payload captured and sent
// again, or changed, is dropped; a disconnect cannot be forged; a stranger's request flood and a
// peer's payload flood are cut at their rates while the honest clients connect.
//
// ge:summary Test: secure nodes: tokens, sealed payloads, replays and forgeries refused, guards
// ge:keywords test net node security token replay forged aead flood-guard throttle
// ge:skill net testing
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "check.hpp"
#include "net/capture.hpp"
#include "net/node.hpp"

using namespace ge::net;

namespace {

constexpr uint64_t kHostId = 7;

Key master_key() {
  Key k{};
  for (size_t i = 0; i < k.size(); i++) k[i] = static_cast<uint8_t>(5 * i + 11);
  return k;
}

uint32_t protocol() { return protocol_id("secure-test", 1); }

struct End {
  Transport t;
  Node n;
};

std::unique_ptr<End> make_end(SimNetwork& net, uint32_t node, bool host, bool guards = false) {
  auto e = std::make_unique<End>();
  open_sim_transport(e->t, net, sim_address(node, 1));
  NodeConfig c;
  c.protocol = protocol();
  c.accept = host;
  c.max_peers = host ? 8 : 1;
  c.channels.count = 2;
  c.channels.delivery[0] = Delivery::reliable;
  c.channels.delivery[1] = Delivery::unreliable;
  c.seed = node + 300;
  if (host) {
    c.security.on = true;
    c.security.host_id = kHostId;
    c.security.key = host_key(master_key(), kHostId);
  }
  if (guards) c.guards = default_guards();
  node_init(e->n, c, e->t);
  return e;
}

ConnectToken token_for(uint64_t client, uint64_t host = kHostId, int64_t expire_us = 60000000,
                       const Key& master = master_key()) {
  TokenGrant g;
  g.client_id = client;
  g.host_id = host;
  g.expire_us = expire_us;
  g.user[0] = static_cast<uint8_t>(client);
  uint8_t random[kTokenRandomBytes];
  for (size_t i = 0; i < sizeof random; i++) random[i] = static_cast<uint8_t>(client * 31 + i);
  return token_make(master, protocol(), g, random);
}

int64_t step(SimNetwork& net, const std::vector<End*>& ends, int64_t now, int ms,
             std::vector<std::vector<Event>>* seen = nullptr) {
  for (int i = 0; i < ms; i += 10) {
    now += 10000;
    sim_advance(net, now);
    for (size_t k = 0; k < ends.size(); k++) {
      node_update(ends[k]->n, now);
      if (seen) {
        for (const Event& e : ends[k]->n.events) (*seen)[k].push_back(e);
      }
    }
    for (End* e : ends) node_flush(e->n, now);
  }
  return now;
}

int messages(const std::vector<Event>& events) {
  int n = 0;
  for (const Event& e : events) n += e.type == EventType::message;
  return n;
}

}  // namespace

TEST(secure_clients_with_tokens_connect_and_talk) {
  LinkConditions c;
  c.latency_ms = 20;
  SimNetwork net = make_sim_network(1, c);
  auto host = make_end(net, 0, true);
  std::vector<std::unique_ptr<End>> clients;
  std::vector<End*> all{host.get()};
  for (int i = 0; i < 4; i++) {
    clients.push_back(make_end(net, static_cast<uint32_t>(i + 1), false));
    all.push_back(clients.back().get());
    CHECK(node_connect_secure(clients[i]->n, sim_address(0, 1), token_for(100 + i), 0) == 0);
  }
  int64_t now = step(net, all, 0, 300);
  CHECK(node_connected_count(host->n) == 4);
  for (int i = 0; i < 4; i++) CHECK(peer_connected(clients[i]->n, 0));
  // The host knows each client by its token.
  for (const Peer& p : host->n.peers) {
    if (p.state != PeerState::connected) continue;
    CHECK(p.secure && p.client_id >= 100 && p.client_id < 104);
    CHECK(p.user[0] == static_cast<uint8_t>(p.client_id));
  }
  // Messages each way, and what crosses the wire never shows them.
  Capture cap = make_capture();
  clients[0]->t.capture = &cap;
  const char* secret = "the quick brown fox";
  for (int i = 0; i < 4; i++) {
    node_send(clients[i]->n, 0, 0, reinterpret_cast<const uint8_t*>(secret), std::strlen(secret));
  }
  node_broadcast(host->n, 0, reinterpret_cast<const uint8_t*>(secret), std::strlen(secret));
  std::vector<std::vector<Event>> seen(all.size());
  step(net, all, now, 300, &seen);
  CHECK(messages(seen[0]) == 4);
  for (int i = 1; i <= 4; i++) CHECK(messages(seen[i]) == 1);
  const std::string wire(cap.bytes.begin(), cap.bytes.end());
  CHECK(cap.packets > 0 && wire.find("quick brown") == std::string::npos);
  CHECK(host->n.stats.forged == 0 && host->n.stats.replayed == 0);
}

TEST(secure_host_refuses_missing_forged_expired_and_others_tokens) {
  SimNetwork net = make_sim_network(2, {});
  auto host = make_end(net, 0, true);
  auto plain = make_end(net, 1, false);   // no token at all
  auto forger = make_end(net, 2, false);  // a token under another master key
  auto late = make_end(net, 3, false);    // an expired token
  auto lost = make_end(net, 4, false);    // a token for another host
  auto tamper = make_end(net, 5, false);  // a token with a byte changed
  Key other = master_key();
  other[0] ^= 1;
  node_connect(plain->n, sim_address(0, 1), 0);
  node_connect_secure(forger->n, sim_address(0, 1), token_for(1, kHostId, 60000000, other), 0);
  node_connect_secure(late->n, sim_address(0, 1), token_for(2, kHostId, 5000), 0);
  node_connect_secure(lost->n, sim_address(0, 1), token_for(3, kHostId + 1), 0);
  ConnectToken changed = token_for(4);
  changed.sealed[60] ^= 4;
  node_connect_secure(tamper->n, sim_address(0, 1), changed, 0);
  std::vector<End*> all{host.get(), plain.get(), forger.get(),
                        late.get(), lost.get(),  tamper.get()};
  std::vector<std::vector<Event>> seen(all.size());
  step(net, all, 0, 6000, &seen);
  CHECK(node_connected_count(host->n) == 0);
  for (size_t k = 1; k < all.size(); k++) {
    CHECK(seen[k].size() == 1 && seen[k][0].reason == Reason::connect_timed_out);
  }
  // A refused token is never answered: no client got as far as a challenge.
  CHECK(host->n.stats.bad_tokens > 0 && host->n.stats.expired_tokens > 0);
  CHECK(host->n.stats.bad_cookies == 0);
}

TEST(secure_host_refuses_a_token_replayed_from_another_address) {
  SimNetwork net = make_sim_network(3, {});
  auto host = make_end(net, 0, true);
  auto honest = make_end(net, 1, false);
  auto thief = make_end(net, 2, false);
  const ConnectToken t = token_for(55);
  node_connect_secure(honest->n, sim_address(0, 1), t, 0);
  std::vector<End*> all{host.get(), honest.get(), thief.get()};
  int64_t now = step(net, all, 0, 200);
  CHECK(peer_connected(honest->n, 0));
  // The thief has the whole token (keys too) and asks from its own address: refused, and the
  // honest client stays.
  node_connect_secure(thief->n, sim_address(0, 1), t, now);
  std::vector<std::vector<Event>> seen(all.size());
  step(net, all, now, 6000, &seen);
  CHECK(!peer_connected(thief->n, 0));
  CHECK(host->n.stats.replayed_tokens > 0);
  CHECK(node_connected_count(host->n) == 1 && peer_connected(honest->n, 0));
}

TEST(secure_payloads_replayed_or_changed_are_dropped) {
  SimNetwork net = make_sim_network(4, {});
  auto host = make_end(net, 0, true);
  auto client = make_end(net, 1, false);
  Capture cap = make_capture();
  node_connect_secure(client->n, sim_address(0, 1), token_for(9), 0);
  std::vector<End*> all{host.get(), client.get()};
  int64_t now = step(net, all, 0, 200);
  client->t.capture = &cap;
  const uint8_t m[3] = {1, 2, 3};
  node_send(client->n, 0, 0, m, sizeof m);
  std::vector<std::vector<Event>> seen(all.size());
  now = step(net, all, now, 100, &seen);
  CHECK(messages(seen[0]) == 1);
  client->t.capture = nullptr;

  std::vector<CapturedDatagram> sent;
  std::string error;
  CHECK(read_capture(cap.bytes.data(), cap.bytes.size(), sent, &error));
  int payloads = 0;
  for (const CapturedDatagram& d : sent) {
    if (d.data.size() < 5 || d.data[4] != static_cast<uint8_t>(PacketType::payload)) continue;
    if (!(d.from == client->t.local)) continue;
    payloads++;
    // Sent again from the client's own address (the attacker spoofs it): refused as a replay.
    sim_send(net, d.from, d.to, d.data.data(), d.data.size());
    // And changed: refused as a forgery (a new counter, so the window lets it in to be opened).
    std::vector<uint8_t> forged = d.data;
    forged[12] ^= 1;  // the counter's top byte: far ahead, so the window lets it be opened
    sim_send(net, d.from, d.to, forged.data(), forged.size());
  }
  CHECK(payloads > 0);
  seen.assign(all.size(), {});
  step(net, all, now, 100, &seen);
  CHECK(messages(seen[0]) == 0);
  CHECK(host->n.stats.replayed >= static_cast<uint64_t>(payloads));
  CHECK(host->n.stats.forged >= static_cast<uint64_t>(payloads));
  CHECK(peer_connected(host->n, 0) && peer_connected(client->n, 0));
}

TEST(secure_disconnect_cannot_be_forged) {
  SimNetwork net = make_sim_network(5, {});
  auto host = make_end(net, 0, true);
  auto client = make_end(net, 1, false);
  node_connect_secure(client->n, sim_address(0, 1), token_for(12), 0);
  std::vector<End*> all{host.get(), client.get()};
  int64_t now = step(net, all, 0, 200);
  // An unsealed disconnect with the right session key, as the old protocol took it.
  uint8_t out[kMaxPacket];
  Writer w = make_writer(out, sizeof out);
  write_handshake(w, protocol(), PacketType::disconnect, {0, host->n.peers[0].conn.key, 0, 0, 0});
  sim_send(net, client->t.local, host->t.local, out, w.size);
  now = step(net, all, now, 100);
  CHECK(peer_connected(host->n, 0) && host->n.stats.forged == 1);
  // The client's own goes through.
  node_disconnect(client->n, 0, now);
  std::vector<std::vector<Event>> seen(all.size());
  step(net, all, now, 100, &seen);
  CHECK(!peer_connected(host->n, 0));
  CHECK(seen[0].size() == 1 && seen[0][0].reason == Reason::remote_closed);
}

TEST(guards_cut_a_request_flood_and_a_peer_flood) {
  SimNetwork net = make_sim_network(6, {});
  auto host = make_end(net, 0, true, true);
  auto honest = make_end(net, 1, false, true);
  auto loud = make_end(net, 2, false, true);
  node_connect_secure(honest->n, sim_address(0, 1), token_for(20), 0);
  node_connect_secure(loud->n, sim_address(0, 1), token_for(21), 0);
  // A stranger sends 5000 padded requests a second for a second.
  const Address stranger = sim_address(9, 1);
  sim_bind(net, stranger);
  uint8_t req[kMaxPacket];
  Writer w = make_writer(req, sizeof req);
  write_handshake(w, protocol(), PacketType::request, {123, 0, 0, 0, 0});
  std::vector<End*> all{host.get(), honest.get(), loud.get()};
  int64_t now = 0;
  for (int t = 0; t < 100; t++) {
    for (int k = 0; k < 50; k++) sim_send(net, stranger, host->t.local, req, w.size);
    now = step(net, all, now, 10);
  }
  CHECK(peer_connected(honest->n, 0) && peer_connected(loud->n, 0));
  // One address gets its burst and its rate: a second's 20 and a burst of 20, of the 5000.
  CHECK(host->n.stats.throttled > 4900);
  CHECK(host->n.stats.bad_tokens <= 45);

  // The loud client floods 2000 payloads a second; the host takes its rate and drops the rest.
  const uint8_t m = 1;
  for (int t = 0; t < 100; t++) {
    for (int k = 0; k < 20; k++) {
      node_send(loud->n, 0, 1, &m, 1);
      node_flush(loud->n, now);
    }
    now = step(net, all, now, 10);
  }
  CHECK(host->n.stats.flooded > 1000);
  CHECK(peer_connected(honest->n, 0));
}
