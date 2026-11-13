// engine/net/transport.hpp's pipe: a transport to one peer whose datagrams the caller carries (a
// browser page's WebTransport session). Two pipes joined by queues in memory stand in for the page
// and the far end: a datagram goes into the pipe whatever address it is sent to, one received is
// reported from the pipe's peer, and two nodes connect and trade a reliable message over them as
// they would over UDP.
//
// ge:summary Test: the pipe transport, and two nodes over a pair of pipes
// ge:keywords test net transport pipe datagram-pipe webtransport node
// ge:skill net testing
#include <deque>
#include <string>
#include <vector>

#include "check.hpp"
#include "net/node.hpp"
#include "net/transport.hpp"

using namespace ge::net;

namespace {

// One direction of the line: what one end sent, waiting for the other to take it.
struct Line {
  std::deque<std::vector<uint8_t>> queue;
  bool open = true;
};

struct End {
  Line* out = nullptr;
  Line* in = nullptr;
};

bool end_send(void* ctx, const uint8_t* data, size_t size) {
  End& e = *static_cast<End*>(ctx);
  if (!e.out->open) return false;
  e.out->queue.emplace_back(data, data + size);
  return true;
}

int end_receive(void* ctx, uint8_t* buffer, size_t capacity) {
  End& e = *static_cast<End*>(ctx);
  if (e.in->queue.empty()) return -1;
  std::vector<uint8_t> d = std::move(e.in->queue.front());
  e.in->queue.pop_front();
  if (d.size() > capacity) return -1;  // as a socket would: too big for the buffer, dropped
  std::copy(d.begin(), d.end(), buffer);
  return static_cast<int>(d.size());
}

DatagramPipe pipe_of(End& e, const Address& peer) {
  DatagramPipe p;
  p.send = end_send;
  p.receive = end_receive;
  p.ctx = &e;
  p.peer = peer;
  return p;
}

}  // namespace

TEST(pipe_refuses_missing_calls) {
  Transport t;
  CHECK(!open_pipe_transport(t, DatagramPipe{}));
  CHECK(t.kind == TransportKind::none);
}

TEST(pipe_sends_to_its_peer_and_hears_from_it) {
  Line ab, ba;
  End a{&ab, &ba}, b{&ba, &ab};
  const Address server = ipv4(10, 0, 0, 7, 27500);
  Transport t;
  CHECK(open_pipe_transport(t, pipe_of(a, server)));
  CHECK(t.kind == TransportKind::pipe);
  const uint8_t m[3] = {1, 2, 3};
  // Any address: the pipe has one peer.
  CHECK(transport_send(t, ipv4(192, 168, 1, 1, 9), m, sizeof m, 0));
  CHECK(ab.queue.size() == 1 && ab.queue.front() == std::vector<uint8_t>(m, m + 3));
  uint8_t buf[16];
  Address from;
  CHECK(transport_receive(t, buf, sizeof buf, from, 0) == -1);  // nothing yet: at once
  ba.queue.push_back({9, 8});
  CHECK(transport_receive(t, buf, sizeof buf, from, 0) == 2);
  CHECK(buf[0] == 9 && buf[1] == 8);
  CHECK(from == server);
  // A pipe that cannot send counts the failure, as a socket's would.
  ab.open = false;
  CHECK(!transport_send(t, server, m, sizeof m, 0));
  CHECK(t.send_failures == 1);
  close_transport(t);
  CHECK(t.kind == TransportKind::none);
  (void)b;
}

TEST(pipe_two_nodes_talk) {
  Line up, down;
  End page{&up, &down}, far{&down, &up};
  const Address server = ipv4(10, 0, 0, 7, 27500), player = ipv4(10, 0, 0, 9, 50000);
  Transport ct, ht;
  CHECK(open_pipe_transport(ct, pipe_of(page, server)));
  CHECK(open_pipe_transport(ht, pipe_of(far, player)));
  NodeConfig hc;
  hc.protocol = protocol_id("pipe-test", 1);
  hc.accept = true;
  hc.max_peers = 2;
  hc.channels.count = 1;
  hc.channels.delivery[0] = Delivery::reliable;
  hc.seed = 7;
  NodeConfig cc = hc;
  cc.accept = false;
  cc.max_peers = 1;
  cc.seed = 8;
  Node host, client;
  node_init(host, hc, ht);
  node_init(client, cc, ct);
  node_connect(client, server, 0);
  bool sent = false, heard = false;
  for (int64_t t = 0; t < 3000000 && !heard; t += 1000) {
    node_update(host, t);
    node_update(client, t);
    for (const Event& e : host.events) {
      if (e.type == EventType::message && e.size == 5)
        heard = std::string(reinterpret_cast<const char*>(event_data(host, e)), 5) == "hello";
    }
    if (!sent && peer_connected(client, 0))
      sent = node_send(client, 0, 0, reinterpret_cast<const uint8_t*>("hello"), 5);
    node_flush(host, t);
    node_flush(client, t);
  }
  CHECK(heard);
  close_transport(ct);
  close_transport(ht);
}
