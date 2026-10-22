// THE ENGINE'S PACKET: the wire format every node speaks (docs/networking.md § The wire), written
// and read here and nowhere else, and decoded into one line for the tools. A packet is the
// game's protocol id (32 bits), a type byte and the type's body:
//
//   request     client salt (64), zeros to kMaxPacket
//   challenge   client salt, cookie (64): what only the host can make from the client's address
//   response    client salt, cookie, zeros to kMaxPacket
//   accept      client salt, session key (64), slot (8), the host's maximum (8)
//   deny        client salt, reason (8)
//   payload     session key, sequence (16), ack (16), ack bits (32), then messages
//   disconnect  session key
//
// A message is its channel byte (the top bit set when a 16-bit id follows: a reliable or a
// sequenced channel's; the next bit set on every piece of a big message but its last), the id,
// its length as a varint and its bytes. Little endian throughout.
//
// ge:summary The wire format: packet types, the payload header, messages; describe_packet
// ge:keywords net packet protocol wire-format header handshake request challenge response
// ge:keywords accept deny payload disconnect message channel ack ack-bits sequence
// ge:keywords protocol-id describe inspect decode
// ge:skill net
// ge:test tests/net_packet_test.cpp
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include "net/wire.hpp"

namespace ge::net {

// The largest packet a node sends: inside the smallest path MTU games meet (IPv6's 1280 less
// its 40-byte header and UDP's 8, with room to spare), so IP never fragments it.
constexpr size_t kMaxPacket = 1200;
constexpr size_t kPayloadHeader = 4 + 1 + 8 + 2 + 2 + 4;
// The largest message: one alone in a packet, with its channel byte, id and a 2-byte length.
constexpr size_t kMaxMessage = kMaxPacket - kPayloadHeader - 5;
constexpr int kMaxChannels = 8;

enum class PacketType : uint8_t {
  request = 1,
  challenge = 2,
  response = 3,
  accept = 4,
  deny = 5,
  payload = 6,
  disconnect = 7,
};
const char* packet_type_name(PacketType t);

enum class DenyReason : uint8_t { full = 1, closed = 2 };

// The game's protocol id: FNV-1a 32 of its name and version, so builds that cannot play
// together never parse each other's packets.
uint32_t protocol_id(std::string_view game, uint32_t version);

// Everything after the type byte of a packet that is not a payload.
struct Handshake {
  uint64_t salt = 0;   // the connecting node's salt (every type but disconnect)
  uint64_t value = 0;  // challenge, response: the cookie; accept, disconnect: the session key
  uint8_t slot = 0;    // accept
  uint8_t most = 0;    // accept: the host's maximum of peers
  uint8_t reason = 0;  // deny
};

struct PayloadHeader {
  uint64_t key = 0;
  uint16_t seq = 0;
  uint16_t ack = 0;
  uint32_t ack_bits = 0;  // bit i: sequence ack - 1 - i was received
};

// A packet that is not a payload, whole; `w` must hold kMaxPacket.
void write_handshake(Writer& w, uint32_t protocol, PacketType type, const Handshake& h);
void write_payload_header(Writer& w, uint32_t protocol, const PayloadHeader& h);

constexpr uint8_t kMessageHasId = 0x80;
constexpr uint8_t kMessageMore = 0x40;     // a big message's piece, and more pieces follow
constexpr uint8_t kMessageChannel = 0x3f;  // the channel's bits of the channel byte
inline size_t message_wire_size(bool has_id, size_t size) {
  return 1 + (has_id ? 2 : 0) + varint_size(size) + size;
}
void write_message(Writer& w, uint8_t channel, bool has_id, uint16_t id, const uint8_t* data,
                   size_t size, bool more = false);

struct MessageView {
  uint8_t channel = 0;
  bool has_id = false;
  bool more = false;  // a big message's piece with more to follow (connection.hpp)
  uint16_t id = 0;
  const uint8_t* data = nullptr;
  size_t size = 0;
};

// The packet's protocol id and type; false when it is too short or the type unknown.
bool read_packet_start(Reader& r, uint32_t& protocol, PacketType& type);
bool read_handshake(Reader& r, PacketType type, Handshake& h);
bool read_payload_header(Reader& r, PayloadHeader& h);
// The next message of a payload; false at the end or on a malformed one (`r.bad`).
bool read_message(Reader& r, MessageView& m);

// One line a packet, for `ge net inspect` and the labs:
// "payload seq=17 ack=15 bits=fffffffd 3 msgs [ch0 24B] [ch2#5 12B+] [ch1#88 8B]" (a + marks
// a big message's piece with more to follow).
std::string describe_packet(const uint8_t* data, size_t size);

}  // namespace ge::net
