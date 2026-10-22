// The packet's writers, readers and its one-line description.
//
// ge:summary Writing, reading and describing the engine's packets
// ge:keywords net packet write read describe handshake payload message
// ge:skill net
// ge:test tests/net_packet_test.cpp
#include "net/packet.hpp"

#include <cstdio>

namespace ge::net {

const char* packet_type_name(PacketType t) {
  switch (t) {
    case PacketType::request:
      return "request";
    case PacketType::challenge:
      return "challenge";
    case PacketType::response:
      return "response";
    case PacketType::accept:
      return "accept";
    case PacketType::deny:
      return "deny";
    case PacketType::payload:
      return "payload";
    case PacketType::disconnect:
      return "disconnect";
  }
  return "unknown";
}

uint32_t protocol_id(std::string_view game, uint32_t version) {
  uint32_t h = 0x811c9dc5u;
  const auto mix = [&h](uint8_t b) {
    h ^= b;
    h *= 0x01000193u;
  };
  for (const char c : game) mix(static_cast<uint8_t>(c));
  for (int i = 0; i < 4; i++) mix(static_cast<uint8_t>(version >> (8 * i)));
  return h;
}

void write_handshake(Writer& w, uint32_t protocol, PacketType type, const Handshake& h) {
  write_u32(w, protocol);
  write_u8(w, static_cast<uint8_t>(type));
  switch (type) {
    case PacketType::request:
      write_u64(w, h.salt);
      write_pad_to(w, kMaxPacket);
      break;
    case PacketType::challenge:
      write_u64(w, h.salt);
      write_u64(w, h.value);
      break;
    case PacketType::response:
      write_u64(w, h.salt);
      write_u64(w, h.value);
      write_pad_to(w, kMaxPacket);
      break;
    case PacketType::accept:
      write_u64(w, h.salt);
      write_u64(w, h.value);
      write_u8(w, h.slot);
      write_u8(w, h.most);
      break;
    case PacketType::deny:
      write_u64(w, h.salt);
      write_u8(w, h.reason);
      break;
    case PacketType::disconnect:
      write_u64(w, h.value);
      break;
    case PacketType::payload:
      w.overflow = true;  // a payload is written by write_payload_header and its messages
      break;
  }
}

void write_payload_header(Writer& w, uint32_t protocol, const PayloadHeader& h) {
  write_u32(w, protocol);
  write_u8(w, static_cast<uint8_t>(PacketType::payload));
  write_u64(w, h.key);
  write_u16(w, h.seq);
  write_u16(w, h.ack);
  write_u32(w, h.ack_bits);
}

void write_message(Writer& w, uint8_t channel, bool has_id, uint16_t id, const uint8_t* data,
                   size_t size, bool more) {
  write_u8(
      w, static_cast<uint8_t>(channel | (has_id ? kMessageHasId : 0) | (more ? kMessageMore : 0)));
  if (has_id) write_u16(w, id);
  write_varint(w, size);
  write_bytes(w, data, size);
}

bool read_packet_start(Reader& r, uint32_t& protocol, PacketType& type) {
  protocol = read_u32(r);
  const uint8_t t = read_u8(r);
  if (r.bad || t < 1 || t > 7) return false;
  type = static_cast<PacketType>(t);
  return true;
}

bool read_handshake(Reader& r, PacketType type, Handshake& h) {
  switch (type) {
    case PacketType::request:
      h.salt = read_u64(r);
      return !r.bad && r.size >= kMaxPacket;  // an unpadded request is refused: no amplification
    case PacketType::challenge:
      h.salt = read_u64(r);
      h.value = read_u64(r);
      break;
    case PacketType::response:
      h.salt = read_u64(r);
      h.value = read_u64(r);
      return !r.bad && r.size >= kMaxPacket;
    case PacketType::accept:
      h.salt = read_u64(r);
      h.value = read_u64(r);
      h.slot = read_u8(r);
      h.most = read_u8(r);
      break;
    case PacketType::deny:
      h.salt = read_u64(r);
      h.reason = read_u8(r);
      break;
    case PacketType::disconnect:
      h.value = read_u64(r);
      break;
    case PacketType::payload:
      return false;
  }
  return !r.bad;
}

bool read_payload_header(Reader& r, PayloadHeader& h) {
  h.key = read_u64(r);
  h.seq = read_u16(r);
  h.ack = read_u16(r);
  h.ack_bits = read_u32(r);
  return !r.bad;
}

bool read_message(Reader& r, MessageView& m) {
  if (remaining(r) == 0) return false;
  const uint8_t c = read_u8(r);
  m.channel = c & kMessageChannel;
  m.has_id = (c & kMessageHasId) != 0;
  m.more = (c & kMessageMore) != 0;
  m.id = m.has_id ? read_u16(r) : 0;
  const uint64_t n = read_varint(r);
  if (r.bad || m.channel >= kMaxChannels || (m.more && !m.has_id) || n > remaining(r)) {
    r.bad = true;
    return false;
  }
  m.size = static_cast<size_t>(n);
  m.data = read_view(r, m.size);
  return !r.bad;
}

std::string describe_packet(const uint8_t* data, size_t size) {
  Reader r = make_reader(data, size);
  uint32_t protocol = 0;
  PacketType type{};
  char b[160];
  if (!read_packet_start(r, protocol, type)) {
    std::snprintf(b, sizeof b, "not a packet (%zu bytes)", size);
    return b;
  }
  if (type != PacketType::payload) {
    Handshake h;
    const bool ok = read_handshake(r, type, h);
    switch (type) {
      case PacketType::accept:
        std::snprintf(b, sizeof b, "accept salt=%016llx slot=%u of %u", (unsigned long long)h.salt,
                      h.slot, h.most);
        break;
      case PacketType::deny:
        std::snprintf(b, sizeof b, "deny salt=%016llx reason=%s", (unsigned long long)h.salt,
                      h.reason == static_cast<uint8_t>(DenyReason::full) ? "full" : "closed");
        break;
      case PacketType::disconnect:
        std::snprintf(b, sizeof b, "disconnect");
        break;
      default:
        std::snprintf(b, sizeof b, "%s salt=%016llx %zuB", packet_type_name(type),
                      (unsigned long long)h.salt, size);
        break;
    }
    return std::string(b) + (ok ? "" : " (malformed)");
  }
  PayloadHeader h;
  if (!read_payload_header(r, h)) return "payload (cut short)";
  std::string msgs;
  int count = 0;
  MessageView m;
  while (read_message(r, m)) {
    if (m.has_id) {
      std::snprintf(b, sizeof b, " [ch%u#%u %zuB%s]", m.channel, m.id, m.size, m.more ? "+" : "");
    } else {
      std::snprintf(b, sizeof b, " [ch%u %zuB]", m.channel, m.size);
    }
    msgs += b;
    count++;
  }
  std::snprintf(b, sizeof b, "payload seq=%u ack=%u bits=%08x %d msg%s", h.seq, h.ack, h.ack_bits,
                count, count == 1 ? "" : "s");
  return std::string(b) + (count ? msgs : " (keepalive)") + (r.bad ? " (malformed)" : "");
}

}  // namespace ge::net
