// The classic net channel (classic_channel.hpp).
//
// ge:summary The net channel's header, reliable resend and rate, defined
// ge:keywords channel_transmit channel_process channel_reliable peek_qport
// ge:skill net shooter
// ge:test tests/shooters_edge_classic_channel_test.cpp
#include "shooters_edge/net/classic_channel.hpp"

#include <cstring>

namespace shooters_edge::net {

void channel_setup(Channel& c, bool client, uint16_t qport, double now) {
  c = Channel{};
  c.client = client;
  c.qport = qport;
  c.clear_time = now;
  c.last_received = now;
}

void channel_set_rate(Channel& c, int bytes_per_second, int dupe) {
  c.rate_s_per_byte = bytes_per_second > 0 ? 1.0 / bytes_per_second : 0;
  c.copies = 1 + (dupe < 0 ? 0 : dupe > 3 ? 3 : dupe);
}

bool channel_reliable(Channel& c, const uint8_t* data, size_t size) {
  if (size == 0) return true;
  if (size > kMaxMsgLen - 16) {
    c.overflow = true;
    return false;
  }
  if (c.pending_count > 0) {
    MsgBuf& last = c.pending[c.pending_count - 1];
    if (last.size + size <= kMaxMsgLen - 16) {
      std::memcpy(last.data + last.size, data, size);
      last.size = static_cast<uint16_t>(last.size + size);
      return true;
    }
  }
  if (c.pending_count == kBackBuffers) {
    c.overflow = true;
    return false;
  }
  MsgBuf& b = c.pending[c.pending_count++];
  std::memcpy(b.data, data, size);
  b.size = static_cast<uint16_t>(size);
  return true;
}

size_t channel_reliable_backlog(const Channel& c) {
  size_t n = c.reliable.size;
  for (int i = 0; i < c.pending_count; i++) n += c.pending[i].size;
  return n;
}

bool channel_can_packet(const Channel& c, double now) {
  if (c.rate_s_per_byte == 0) return true;
  return c.clear_time < now + kRateBurstBytes * c.rate_s_per_byte;
}

size_t channel_transmit(Channel& c, const uint8_t* unreliable, size_t size, double now,
                        uint8_t* out) {
  bool send_reliable = false;
  if (c.reliable.size > 0 && c.incoming_acknowledged > c.last_reliable_sequence &&
      c.incoming_reliable_acknowledged != c.reliable_sequence) {
    send_reliable = true;
    c.resent++;
  }
  if (c.reliable.size == 0 && c.pending_count > 0) {
    c.reliable = c.pending[0];
    for (int i = 1; i < c.pending_count; i++) c.pending[i - 1] = c.pending[i];
    c.pending_count--;
    c.reliable_sequence = !c.reliable_sequence;
    send_reliable = true;
  }

  gn::Writer w = gn::make_writer(out, kMaxPacket);
  gn::write_u32(w, c.outgoing_sequence | (send_reliable ? 0x80000000u : 0u));
  gn::write_u32(w, c.incoming_sequence | (c.incoming_reliable_sequence ? 0x80000000u : 0u));
  if (c.client) gn::write_u16(w, c.qport);
  if (send_reliable) {
    gn::write_bytes(w, c.reliable.data, c.reliable.size);
    c.last_reliable_sequence = c.outgoing_sequence;
  }
  if (size > 0 && w.size + size <= kMaxPacket) gn::write_bytes(w, unreliable, size);
  c.outgoing_sequence++;
  c.sent++;
  c.bytes_out += w.size * static_cast<uint64_t>(c.copies);
  if (c.clear_time < now) c.clear_time = now;
  c.clear_time += static_cast<double>(w.size) * c.copies * c.rate_s_per_byte;
  return w.size;
}

bool channel_process(Channel& c, const uint8_t* data, size_t size, double now, size_t& payload) {
  const size_t header = c.client ? kServerHeader : kClientHeader;
  if (size < header) return false;
  gn::Reader r = gn::make_reader(data, size);
  uint32_t sequence = gn::read_u32(r);
  uint32_t ack = gn::read_u32(r);
  if (!c.client) gn::read_u16(r);  // the qport: the endpoint found the client by it already
  const bool reliable_message = (sequence >> 31) != 0;
  const bool reliable_ack = (ack >> 31) != 0;
  sequence &= 0x7fffffffu;
  ack &= 0x7fffffffu;
  if (sequence <= c.incoming_sequence) {
    c.stale++;
    return false;
  }
  c.last_drop = sequence - (c.incoming_sequence + 1);
  c.dropped += c.last_drop;
  if (reliable_ack == c.reliable_sequence) c.reliable.size = 0;
  c.incoming_sequence = sequence;
  c.incoming_acknowledged = ack;
  c.incoming_reliable_acknowledged = reliable_ack;
  if (reliable_message) c.incoming_reliable_sequence = !c.incoming_reliable_sequence;
  c.accepted++;
  c.bytes_in += size;
  c.last_received = now;
  payload = r.at;
  return true;
}

bool peek_qport(const uint8_t* data, size_t size, uint16_t& qport) {
  if (size < kClientHeader) return false;
  qport = static_cast<uint16_t>(data[8] | data[9] << 8);
  return true;
}

bool is_out_of_band(const uint8_t* data, size_t size) {
  return size >= 4 && data[0] == 0xff && data[1] == 0xff && data[2] == 0xff && data[3] == 0xff;
}

}  // namespace shooters_edge::net
