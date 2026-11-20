// Tests: the protocol family's net channel (plugins/classic/protocol/channel.hpp): the header's
// sequences and the qport, a stale or duplicated packet dropped and the loss counted, the reliable
// message resent until the peer's bit matches and then cleared, the pending queue taken in order
// and its overflow, the rate's clear time holding a packet back, and out-of-band packets told
// apart, and the trace that shows each packet and each loss (showpackets, showdrop).
//
// ge:summary Tests: the classic net channel's sequences, reliable resend, qport, loss and rate
// ge:keywords test classic-protocol net-channel reliable resend qport rate stale loss
// ge:skill shooter net testing
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "check.hpp"
#include "classic/protocol/channel.hpp"

namespace sp = ge::classic::protocol;

namespace {

struct Wire {
  uint8_t data[sp::kMaxPacket];
  size_t size = 0;
};

Wire send(sp::Channel& c, const std::string& unreliable, double now) {
  Wire w;
  w.size = sp::channel_transmit(c, reinterpret_cast<const uint8_t*>(unreliable.data()),
                                unreliable.size(), now, w.data);
  return w;
}

// Delivers a packet; the payload as text ("" when stale).
std::string deliver(sp::Channel& c, const Wire& w, double now, bool* ok = nullptr) {
  size_t at = 0;
  const bool accepted = sp::channel_process(c, w.data, w.size, now, at);
  if (ok) *ok = accepted;
  if (!accepted) return {};
  return std::string(reinterpret_cast<const char*>(w.data + at), w.size - at);
}

}  // namespace

TEST(the_header_carries_sequences_and_a_clients_qport) {
  sp::Channel client, server;
  sp::channel_setup(client, true, 1234, 0);
  sp::channel_setup(server, false, 0, 0);
  sp::channel_set_rate(client, 0);
  const Wire w = send(client, "hello", 0);
  CHECK(w.size == sp::kClientHeader + 5);
  uint16_t qport = 0;
  CHECK(sp::peek_qport(w.data, w.size, qport) && qport == 1234);
  CHECK(!sp::is_out_of_band(w.data, w.size));
  const uint8_t oob[] = {0xff, 0xff, 0xff, 0xff, 'k'};
  CHECK(sp::is_out_of_band(oob, sizeof oob));
  CHECK(deliver(server, w, 0) == "hello");
  CHECK(server.incoming_sequence == 1);
}

TEST(stale_packets_are_dropped_and_losses_counted) {
  sp::Channel a, b;
  sp::channel_setup(a, false, 0, 0);
  sp::channel_setup(b, true, 7, 0);
  sp::channel_set_rate(a, 0);
  const Wire p1 = send(a, "1", 0);
  const Wire p2 = send(a, "2", 0);
  const Wire p3 = send(a, "3", 0);
  const Wire p4 = send(a, "4", 0);
  CHECK(deliver(b, p1, 0) == "1");
  CHECK(deliver(b, p4, 0) == "4");
  CHECK(b.last_drop == 2);
  bool ok = true;
  deliver(b, p3, 0, &ok);
  CHECK(!ok);  // older than the newest
  deliver(b, p4, 0, &ok);
  CHECK(!ok);  // a duplicate
  CHECK(b.dropped == 2);
  (void)p2;
}

TEST(the_reliable_message_is_resent_until_acknowledged) {
  sp::Channel s, c;
  sp::channel_setup(s, false, 0, 0);
  sp::channel_setup(c, true, 9, 0);
  sp::channel_set_rate(s, 0);
  sp::channel_set_rate(c, 0);
  const std::string msg = "reliable!";
  CHECK(sp::channel_reliable(s, reinterpret_cast<const uint8_t*>(msg.data()), msg.size()));
  const Wire first = send(s, "", 0);
  CHECK((first.data[3] & 0x80) != 0);  // the reliable bit
  // Lost. The next packet reaches the client, whose answer acknowledges a packet past the carrier
  // with the old reliable bit: the server resends.
  const Wire second = send(s, "", 0);
  CHECK((second.data[3] & 0x80) == 0);
  deliver(c, second, 0);
  deliver(s, send(c, "", 0), 0);
  const Wire again = send(s, "u", 0);
  CHECK((again.data[3] & 0x80) != 0);
  CHECK(deliver(c, again, 0) == msg + "u");
  // The client's next packet carries the matching bit: the in-flight buffer clears.
  deliver(s, send(c, "", 0), 0);
  const Wire after = send(s, "x", 0);
  CHECK((after.data[3] & 0x80) == 0);
  CHECK(sp::channel_reliable_backlog(s) == 0);
  CHECK(deliver(c, after, 0) == "x");
}

TEST(pending_reliable_data_goes_in_order_and_overflows) {
  sp::Channel s, c;
  sp::channel_setup(s, false, 0, 0);
  sp::channel_setup(c, true, 9, 0);
  sp::channel_set_rate(s, 0);
  sp::channel_set_rate(c, 0);
  std::string got;
  for (int i = 0; i < 3; i++) {
    const std::string m = "m" + std::to_string(i);
    CHECK(sp::channel_reliable(s, reinterpret_cast<const uint8_t*>(m.data()), m.size()));
  }
  for (int round = 0; round < 6; round++) {
    got += deliver(c, send(s, "", 0), 0);
    deliver(s, send(c, "", 0), 0);
  }
  CHECK(got == "m0m1m2");
  std::vector<uint8_t> big(sp::kMaxMsgLen - 100, 'z');
  bool refused = false;
  for (int i = 0; i < sp::kBackBuffers + 4 && !refused; i++)
    refused = !sp::channel_reliable(s, big.data(), big.size());
  CHECK(refused && s.overflow);
}

TEST(the_rate_holds_a_packet_back) {
  sp::Channel s;
  sp::channel_setup(s, false, 0, 0);
  sp::channel_set_rate(s, 2500);
  CHECK(sp::channel_can_packet(s, 0));
  const std::string big(1200, 'q');
  send(s, big, 0);
  // 1208 bytes at 2500 bytes/s clear in about 0.48 s; 400 bytes of burst are allowed early.
  CHECK(!sp::channel_can_packet(s, 0.1));
  CHECK(sp::channel_can_packet(s, 0.4));
  sp::channel_set_rate(s, 2500, 1);
  CHECK(s.copies == 2);
}

TEST(the_trace_shows_each_packet_and_each_loss) {
  std::vector<std::string> lines;
  sp::ChannelTrace sent{.packets = 2, .drop = false, .where = "a", .print = {}};
  sp::ChannelTrace got{.packets = 3, .drop = true, .where = "10.0.0.1:27500", .print = {}};
  sent.print = got.print = [&lines](const std::string& l) { lines.push_back(l); };
  sp::Channel a, b;
  a.trace = &sent;
  b.trace = &got;
  sp::channel_setup(a, false, 0, 0);  // the trace kept across the setup
  sp::channel_setup(b, true, 7, 0);
  CHECK(a.trace == &sent && b.trace == &got);
  sp::channel_set_rate(a, 0);
  const Wire p1 = send(a, "1", 0.25);
  CHECK(lines.size() == 1 && lines[0] == "250.0 --> s=1(0) a=0(0) " + std::to_string(p1.size));
  const Wire p2 = send(a, "2", 0.5);
  const Wire p3 = send(a, "3", 0.5);
  lines.clear();
  deliver(b, p1, 1.0);
  CHECK(lines.size() == 1 && lines[0] == "1000.0 <-- s=1(0) a=0(0) " + std::to_string(p1.size));
  deliver(b, p3, 1.5);
  CHECK(lines.size() == 3 && lines[1] == "10.0.0.1:27500:Dropped 1 packets at 3");
  deliver(b, p2, 1.5);
  CHECK(lines.size() == 4 && lines[3] == "10.0.0.1:27500:Out of order packet 2 at 3");
  got.packets = 0;
  got.drop = false;
  deliver(b, p3, 2);
  CHECK(lines.size() == 4);  // off: nothing
}
