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
#include <span>
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

// A server sizes its frame to the room the packet has after the reliable data it carries: a long
// message waiting takes its share, and the frame that fits rides with it rather than being lost.
TEST(the_room_for_a_frame_is_what_the_reliable_part_leaves) {
  sp::Channel s;
  sp::channel_setup(s, false, 0, 0);
  sp::channel_set_rate(s, 0);
  const size_t whole = sp::kMaxPacket - sp::kServerHeader;
  bool grows = false;
  CHECK(sp::channel_unreliable_room(s, &grows) == whole && grows);
  std::vector<uint8_t> long_one(1000, 'r');
  CHECK(sp::channel_reliable(s, long_one.data(), long_one.size()));
  const size_t room = sp::channel_unreliable_room(s, &grows);
  CHECK(room == whole - 1000 && grows);
  const std::string frame(room, 'f');
  const Wire w = send(s, frame, 0);
  CHECK(w.size == sp::kMaxPacket);  // the reliable part and the whole frame in one packet
  // In flight and not yet due again: the next packet carries no reliable part.
  CHECK(sp::channel_unreliable_room(s, &grows) == whole && !grows);
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
  // Quake II's cl_showpackets shows each packet sent with showpackets 0 or 3.
  sent.packets = 3;
  send(a, "4", 3);
  CHECK(lines.size() == 4);
  sent.sent = true;
  send(a, "5", 3);
  CHECK(lines.size() == 5 && lines[4].find("3000.0 --> s=5") == 0);
}

namespace {

// Delivers through `channel_read`; the payload as text ("" until a message is whole).
std::string read_whole(sp::Channel& c, const Wire& w, double now, bool* ok = nullptr) {
  std::span<const uint8_t> p;
  const bool whole = sp::channel_read(c, w.data, w.size, now, p);
  if (ok) *ok = whole;
  return whole ? std::string(reinterpret_cast<const char*>(p.data()), p.size()) : std::string();
}

void newer(sp::Channel& c, bool client, uint16_t qport, uint8_t bytes) {
  sp::channel_setup(c, client, qport, 0);
  c.qport_bytes = bytes;
  c.fragments = true;
  c.packet_len = 100;
}

}  // namespace

TEST(a_later_protocols_qport_is_one_byte_or_none) {
  for (const uint8_t bytes : {uint8_t(1), uint8_t(0)}) {
    sp::Channel client, server;
    sp::channel_setup(client, true, bytes == 1 ? 77 : 0, 0);
    sp::channel_setup(server, false, bytes == 1 ? 77 : 0, 0);
    client.qport_bytes = server.qport_bytes = bytes;
    const Wire w = send(client, "move", 0);
    CHECK(w.size == size_t(8 + bytes + 4));
    if (bytes == 1) CHECK(int(w.data[8]) == 77);
    CHECK(deliver(server, w, 0) == std::string("move"));
  }
}

TEST(the_newer_channel_sends_a_big_datagram_in_pieces) {
  sp::Channel server, client;
  newer(server, false, 9, 1);
  newer(client, true, 9, 1);
  std::string big;
  for (int i = 0; i < 260; i++) big += static_cast<char>('a' + i % 26);
  const Wire first = send(server, big, 0);
  CHECK((first.data[3] & 0x40) != 0);  // a piece
  CHECK(sp::channel_sending_pieces(server));
  CHECK(read_whole(client, first, 0) == std::string());
  const Wire second = send(server, "ignored", 0.01);
  CHECK(read_whole(client, second, 0.01) == std::string());
  const Wire third = send(server, "ignored", 0.02);
  CHECK(!sp::channel_sending_pieces(server));
  bool ok = false;
  CHECK(read_whole(client, third, 0.02, &ok) == big);
  CHECK(ok);
  CHECK(server.outgoing_sequence == 2u);  // the pieces shared one sequence
  CHECK(client.incoming_sequence == 1u);
}

TEST(a_lost_piece_loses_the_datagram_and_the_reliable_part_goes_again) {
  sp::Channel server, client;
  newer(server, false, 0, 0);
  newer(client, true, 0, 0);
  std::string rel(150, 'r');
  CHECK(sp::channel_reliable(server, reinterpret_cast<const uint8_t*>(rel.data()), rel.size()));
  const Wire a = send(server, "", 0);
  CHECK((a.data[3] & 0xC0) == 0xC0);  // reliable and a piece
  const Wire lost = send(server, "", 0.01);
  (void)lost;
  CHECK(read_whole(client, a, 0) == std::string());
  // A later datagram reaches the client, which acknowledges it without the reliable bit.
  const Wire later = send(server, "x", 0.015);
  CHECK(read_whole(client, later, 0.015) == std::string("x"));
  const Wire ack = send(client, "", 0.02);
  read_whole(server, ack, 0.02);
  const Wire again = send(server, "", 0.1);  // the reliable message resent, again in pieces
  CHECK((again.data[3] & 0xC0) == 0xC0);
  CHECK(read_whole(client, again, 0.1) == std::string());
  const Wire end = send(server, "", 0.11);
  CHECK(read_whole(client, end, 0.11) == rel);
  CHECK(server.resent >= 1);
}

TEST(the_newer_channel_gathers_reliable_data_and_overflows_past_its_buffer) {
  sp::Channel server;
  newer(server, false, 0, 0);
  std::vector<uint8_t> chunk(4000, 1);
  for (int i = 0; i < 8; i++) CHECK(sp::channel_reliable(server, chunk.data(), chunk.size()));
  CHECK(!server.overflow);
  CHECK(!sp::channel_reliable(server, chunk.data(), chunk.size()));
  CHECK(server.overflow);
}

TEST(a_piece_past_a_gap_is_thrown_away) {
  sp::Channel server, client;
  newer(server, false, 0, 0);
  newer(client, true, 0, 0);
  const std::string big(250, 'z');
  const Wire a = send(server, big, 0);
  const Wire b = send(server, "", 0.01);
  const Wire c = send(server, "", 0.02);
  (void)a;
  CHECK(read_whole(client, b, 0.01) == std::string());  // the first piece never came
  CHECK(read_whole(client, c, 0.02) == std::string());
  CHECK(client.pieces_lost >= 1);
  CHECK(client.incoming_sequence == 0u);
}
