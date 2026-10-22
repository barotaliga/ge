// engine/net/packet.hpp: every packet type round trips, messages read back, an unpadded request
// is refused, and describe_packet says what a packet is.
//
// ge:summary Test: the wire format's packets, messages, protocol id and description
// ge:keywords test net packet handshake payload message describe protocol-id
// ge:skill net testing
#include "net/packet.hpp"

#include "check.hpp"

using namespace ge::net;

TEST(packet_handshakes_round_trip) {
  const uint32_t proto = protocol_id("test", 1);
  CHECK(proto == protocol_id("test", 1) && proto != protocol_id("test", 2));
  for (const PacketType t : {PacketType::request, PacketType::challenge, PacketType::response,
                             PacketType::accept, PacketType::deny, PacketType::disconnect}) {
    uint8_t buf[kMaxPacket];
    Writer w = make_writer(buf, sizeof buf);
    const Handshake h{0x1122334455667788ull, 0x99aabbccddeeff00ull, 3, 32, 1};
    write_handshake(w, proto, t, h);
    CHECK(!w.overflow);
    if (t == PacketType::request || t == PacketType::response) CHECK(w.size == kMaxPacket);
    Reader r = make_reader(buf, w.size);
    uint32_t p = 0;
    PacketType back{};
    CHECK(read_packet_start(r, p, back) && p == proto && back == t);
    Handshake g;
    CHECK(read_handshake(r, t, g));
    if (t != PacketType::disconnect) CHECK(g.salt == h.salt);
    if (t == PacketType::challenge || t == PacketType::response || t == PacketType::accept ||
        t == PacketType::disconnect) {
      CHECK(g.value == h.value);
    }
    if (t == PacketType::accept) CHECK(g.slot == 3 && g.most == 32);
    if (t == PacketType::deny) CHECK(g.reason == 1);
    CHECK(describe_packet(buf, w.size).find(packet_type_name(t)) == 0);
  }
}

TEST(packet_refuses_an_unpadded_request) {
  uint8_t buf[64];
  Writer w = make_writer(buf, sizeof buf);
  write_u32(w, 1);
  write_u8(w, static_cast<uint8_t>(PacketType::request));
  write_u64(w, 5);
  Reader r = make_reader(buf, w.size);
  uint32_t p;
  PacketType t;
  CHECK(read_packet_start(r, p, t));
  Handshake h;
  CHECK(!read_handshake(r, t, h));
}

TEST(packet_payload_messages_round_trip) {
  uint8_t buf[kMaxPacket];
  Writer w = make_writer(buf, sizeof buf);
  write_payload_header(w, 9, {42, 17, 15, 0xfffffffd});
  CHECK(w.size == kPayloadHeader);
  const uint8_t a[24] = {1}, b[12] = {2};
  write_message(w, 0, false, 0, a, sizeof a);
  write_message(w, 2, true, 5, b, sizeof b);
  CHECK(w.size == kPayloadHeader + message_wire_size(false, 24) + message_wire_size(true, 12));
  CHECK(describe_packet(buf, w.size) ==
        "payload seq=17 ack=15 bits=fffffffd 2 msgs [ch0 24B] [ch2#5 12B]");
  Reader r = make_reader(buf, w.size);
  uint32_t p;
  PacketType t;
  read_packet_start(r, p, t);
  PayloadHeader h;
  CHECK(read_payload_header(r, h) && h.key == 42 && h.seq == 17 && h.ack == 15);
  MessageView m;
  CHECK(read_message(r, m) && m.channel == 0 && !m.has_id && m.size == 24 && m.data[0] == 1);
  CHECK(read_message(r, m) && m.channel == 2 && m.has_id && m.id == 5 && m.data[0] == 2);
  CHECK(!read_message(r, m) && !r.bad);
  // A length past the end is malformed.
  buf[kPayloadHeader + 1] = 100;
  Reader bad = make_reader(buf, w.size);
  read_packet_start(bad, p, t);
  read_payload_header(bad, h);
  CHECK(!read_message(bad, m) && bad.bad);
  CHECK(describe_packet(buf, 3) == "not a packet (3 bytes)");
  CHECK(kMaxMessage + kPayloadHeader + message_wire_size(true, kMaxMessage) - kMaxMessage <=
        kMaxPacket);
}

TEST(packet_a_big_message_s_piece_carries_its_more_bit) {
  uint8_t buf[kMaxPacket];
  Writer w = make_writer(buf, sizeof buf);
  write_payload_header(w, 9, {42, 3, 0, 0});
  const uint8_t a[8] = {7}, b[4] = {8};
  write_message(w, 2, true, 11, a, sizeof a, true);
  write_message(w, 2, true, 12, b, sizeof b);
  CHECK(describe_packet(buf, w.size) ==
        "payload seq=3 ack=0 bits=00000000 2 msgs [ch2#11 8B+] "
        "[ch2#12 4B]");
  Reader r = make_reader(buf, w.size);
  uint32_t p;
  PacketType t;
  PayloadHeader h;
  read_packet_start(r, p, t);
  read_payload_header(r, h);
  MessageView m;
  CHECK(read_message(r, m) && m.channel == 2 && m.more && m.id == 11);
  CHECK(read_message(r, m) && m.channel == 2 && !m.more && m.id == 12);
  // A piece without an id is malformed: only a reliable channel's messages come in pieces.
  Writer w2 = make_writer(buf, sizeof buf);
  write_payload_header(w2, 9, {42, 4, 0, 0});
  write_u8(w2, 1 | kMessageMore);
  write_varint(w2, 1);
  write_u8(w2, 0);
  Reader r2 = make_reader(buf, w2.size);
  read_packet_start(r2, p, t);
  read_payload_header(r2, h);
  CHECK(!read_message(r2, m) && r2.bad);
}
