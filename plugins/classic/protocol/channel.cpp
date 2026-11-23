// The protocol family's net channel (channel.hpp).
//
// ge:summary The net channel's header, reliable resend and rate, defined
// ge:keywords channel_transmit channel_process channel_reliable peek_qport
// ge:skill classic shooter net
// ge:test tests/classic_protocol_channel_test.cpp
#include "classic/protocol/channel.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>

namespace ge::classic::protocol {

namespace {

// A packet sent is shown: showpackets 1 or 2, or Quake II's cl_showpackets.
bool shows_sent(const ChannelTrace& t) { return t.packets == 1 || t.packets == 2 || t.sent; }

// One `showpackets` line.
void packet_line(const Channel& c, const char* arrow, uint32_t seq, bool rel, uint32_t ack,
                 bool ack_rel, size_t bytes, double now) {
  char ms[32];
  std::snprintf(ms, sizeof ms, "%.1f", now * 1000);
  std::string line = ms;
  line.append(" ").append(arrow).append(" s=").append(std::to_string(seq));
  line.append(rel ? "(1)" : "(0)").append(" a=").append(std::to_string(ack));
  line.append(ack_rel ? "(1) " : "(0) ").append(std::to_string(bytes));
  c.trace->print(line);
}

void write_qport(gn::Writer& w, const Channel& c) {
  if (c.qport_bytes == 2) gn::write_u16(w, c.qport);
  if (c.qport_bytes == 1) gn::write_u8(w, static_cast<uint8_t>(c.qport));
}

void skip_qport(gn::Reader& r, const Channel& c) {
  if (c.qport_bytes == 2) gn::read_u16(r);
  if (c.qport_bytes == 1) gn::read_u8(r);
}

// The bytes a newer channel's datagram carries after its header: `packet_len`, within a packet.
size_t piece_room(const Channel& c) {
  return std::min(c.packet_len, kMaxPacket - kServerHeader - 2 - c.qport_bytes);
}

// The newer channel's next piece of `frag_out`: the sequence advances after the last one.
size_t send_piece(Channel& c, double now, uint8_t* out) {
  const size_t left = c.frag_out.size() - c.frag_out_at;
  const size_t piece = std::min(left, piece_room(c));
  const bool more = piece < left;
  gn::Writer w = gn::make_writer(out, kMaxPacket);
  gn::write_u32(w, c.outgoing_sequence | 0x40000000u | (c.frag_out_reliable ? 0x80000000u : 0u));
  gn::write_u32(w, c.incoming_sequence | (c.incoming_reliable_sequence ? 0x80000000u : 0u));
  if (c.client) write_qport(w, c);
  gn::write_u16(w, static_cast<uint16_t>(c.frag_out_at | (more ? 0x8000u : 0u)));
  gn::write_bytes(w, c.frag_out.data() + c.frag_out_at, piece);
  if (c.trace != nullptr && c.trace->print && shows_sent(*c.trace))
    packet_line(c, "-->", c.outgoing_sequence, c.frag_out_reliable, c.incoming_sequence,
                c.incoming_reliable_sequence, w.size, now);
  c.frag_out_at += piece;
  c.pieces_out++;
  if (!more) {
    c.frag_out.clear();
    c.frag_out_at = 0;
    c.outgoing_sequence++;
  }
  c.sent++;
  c.bytes_out += w.size * static_cast<uint64_t>(c.copies);
  if (c.clear_time < now) c.clear_time = now;
  c.clear_time += static_cast<double>(w.size) * c.copies * c.rate_s_per_byte;
  return w.size;
}

size_t newer_transmit(Channel& c, const uint8_t* unreliable, size_t size, double now,
                      uint8_t* out) {
  if (!c.frag_out.empty()) return send_piece(c, now, out);
  bool send_reliable = false;
  if (!c.big_reliable.empty() && c.incoming_acknowledged > c.last_reliable_sequence &&
      c.incoming_reliable_acknowledged != c.reliable_sequence) {
    send_reliable = true;
    c.resent++;
  }
  if (c.big_reliable.empty() && !c.big_pending.empty()) {
    c.big_reliable.swap(c.big_pending);
    c.big_pending.clear();
    c.reliable_sequence = !c.reliable_sequence;
    send_reliable = true;
  }
  const size_t rel = send_reliable ? c.big_reliable.size() : 0;
  if (send_reliable) c.last_reliable_sequence = c.outgoing_sequence;
  const size_t room = piece_room(c);
  if (size > room || (send_reliable && rel + size > room)) {
    c.frag_out.assign(c.big_reliable.begin(), c.big_reliable.begin() + static_cast<long>(rel));
    if (rel + size <= kBigMessage)
      c.frag_out.insert(c.frag_out.end(), unreliable, unreliable + size);
    else
      c.dumped++;
    c.frag_out_at = 0;
    c.frag_out_reliable = send_reliable;
    return send_piece(c, now, out);
  }
  gn::Writer w = gn::make_writer(out, kMaxPacket);
  gn::write_u32(w, c.outgoing_sequence | (send_reliable ? 0x80000000u : 0u));
  gn::write_u32(w, c.incoming_sequence | (c.incoming_reliable_sequence ? 0x80000000u : 0u));
  if (c.client) write_qport(w, c);
  if (send_reliable) gn::write_bytes(w, c.big_reliable.data(), rel);
  if (size > 0) gn::write_bytes(w, unreliable, size);
  if (c.trace != nullptr && c.trace->print && shows_sent(*c.trace))
    packet_line(c, "-->", c.outgoing_sequence, send_reliable, c.incoming_sequence,
                c.incoming_reliable_sequence, w.size, now);
  c.outgoing_sequence++;
  c.sent++;
  c.bytes_out += w.size * static_cast<uint64_t>(c.copies);
  if (c.clear_time < now) c.clear_time = now;
  c.clear_time += static_cast<double>(w.size) * c.copies * c.rate_s_per_byte;
  return w.size;
}

}  // namespace

void channel_setup(Channel& c, bool client, uint16_t qport, double now) {
  ChannelTrace* trace = c.trace;
  c = Channel{};
  c.trace = trace;
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
  if (c.fragments) {
    if (c.big_pending.size() + size > kBigMessage) {
      c.overflow = true;
      return false;
    }
    c.big_pending.insert(c.big_pending.end(), data, data + size);
    return true;
  }
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
  size_t n = c.reliable.size + c.big_pending.size() + c.big_reliable.size();
  for (int i = 0; i < c.pending_count; i++) n += c.pending[i].size;
  return n;
}

size_t channel_unreliable_room(const Channel& c, bool* grows) {
  const size_t whole = kMaxPacket - kServerHeader - (c.client ? 2 : 0);
  *grows = false;
  if (c.fragments) return whole;
  size_t carried = 0;
  if (c.reliable.size > 0) {
    if (c.incoming_acknowledged > c.last_reliable_sequence &&
        c.incoming_reliable_acknowledged != c.reliable_sequence)
      carried = c.reliable.size;  // a resend
  } else {
    carried = c.pending_count > 0 ? c.pending[0].size : 0;
    *grows = c.pending_count <= 1;  // a new message joins the first, or is the first
  }
  return carried < whole ? whole - carried : 0;
}

bool channel_can_packet(const Channel& c, double now) {
  if (c.rate_s_per_byte == 0) return true;
  return c.clear_time < now + kRateBurstBytes * c.rate_s_per_byte;
}

size_t channel_transmit(Channel& c, const uint8_t* unreliable, size_t size, double now,
                        uint8_t* out) {
  if (c.fragments) return newer_transmit(c, unreliable, size, now, out);
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
  if (c.client) write_qport(w, c);
  if (send_reliable) {
    gn::write_bytes(w, c.reliable.data, c.reliable.size);
    c.last_reliable_sequence = c.outgoing_sequence;
  }
  if (size > 0 && w.size + size <= kMaxPacket) gn::write_bytes(w, unreliable, size);
  if (c.trace != nullptr && c.trace->print && shows_sent(*c.trace))
    packet_line(c, "-->", c.outgoing_sequence, send_reliable, c.incoming_sequence,
                c.incoming_reliable_sequence, w.size, now);
  c.outgoing_sequence++;
  c.sent++;
  c.bytes_out += w.size * static_cast<uint64_t>(c.copies);
  if (c.clear_time < now) c.clear_time = now;
  c.clear_time += static_cast<double>(w.size) * c.copies * c.rate_s_per_byte;
  return w.size;
}

bool channel_process(Channel& c, const uint8_t* data, size_t size, double now, size_t& payload) {
  const size_t header = c.client ? kServerHeader : kServerHeader + c.qport_bytes;
  if (size < header) return false;
  gn::Reader r = gn::make_reader(data, size);
  uint32_t sequence = gn::read_u32(r);
  uint32_t ack = gn::read_u32(r);
  if (!c.client) skip_qport(r, c);  // the qport: the endpoint found the client by it already
  const bool reliable_message = (sequence >> 31) != 0;
  const bool reliable_ack = (ack >> 31) != 0;
  sequence &= 0x7fffffffu;
  ack &= 0x7fffffffu;
  const bool traced = c.trace != nullptr && c.trace->print;
  if (sequence <= c.incoming_sequence) {
    c.stale++;
    if (traced && c.trace->drop)
      c.trace->print(c.trace->where + ":Out of order packet " + std::to_string(sequence) + " at " +
                     std::to_string(c.incoming_sequence));
    return false;
  }
  c.last_drop = sequence - (c.incoming_sequence + 1);
  if (traced && c.trace->drop && c.last_drop > 0)
    c.trace->print(c.trace->where + ":Dropped " + std::to_string(c.last_drop) + " packets at " +
                   std::to_string(sequence));
  if (traced && (c.trace->packets == 1 || c.trace->packets == 3))
    packet_line(c, "<--", sequence, reliable_message, ack, reliable_ack, size, now);
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

bool channel_read(Channel& c, const uint8_t* data, size_t size, double now,
                  std::span<const uint8_t>& payload) {
  if (!c.fragments) {
    size_t at = 0;
    if (!channel_process(c, data, size, now, at)) return false;
    payload = std::span<const uint8_t>(data + at, size - at);
    return true;
  }
  const size_t header = c.client ? kServerHeader : kServerHeader + c.qport_bytes;
  if (size < header) return false;
  gn::Reader r = gn::make_reader(data, size);
  uint32_t sequence = gn::read_u32(r);
  uint32_t ack = gn::read_u32(r);
  if (!c.client) skip_qport(r, c);
  const bool reliable_message = (sequence >> 31) != 0;
  const bool piece = ((sequence >> 30) & 1u) != 0;
  const bool reliable_ack = (ack >> 31) != 0;
  sequence &= 0x3fffffffu;
  ack &= 0x3fffffffu;
  const bool traced = c.trace != nullptr && c.trace->print;
  if (sequence <= c.incoming_sequence) {
    c.stale++;
    if (traced && c.trace->drop)
      c.trace->print(c.trace->where + ":Out of order packet " + std::to_string(sequence) + " at " +
                     std::to_string(c.incoming_sequence));
    return false;
  }
  // The acknowledgement counts from every piece, even one thrown away below.
  if (reliable_ack == c.reliable_sequence) c.big_reliable.clear();
  c.incoming_acknowledged = ack;
  c.incoming_reliable_acknowledged = reliable_ack;
  c.last_received = now;
  if (piece) {
    if (gn::remaining(r) < 2) return false;
    const uint16_t word = gn::read_u16(r);
    const size_t offset = word & 0x7fffu;
    const bool more = (word & 0x8000u) != 0;
    if (sequence != c.frag_in_sequence) {
      c.frag_in.clear();
      c.frag_in_sequence = sequence;
    }
    const size_t len = size - r.at;
    if (offset != c.frag_in.size() || c.frag_in.size() + len > kBigMessage) {
      c.pieces_lost++;  // out of order, past a gap, or too big: the message is lost
      if (traced && c.trace->drop)
        c.trace->print(c.trace->where + (offset < c.frag_in.size() ? ":Out of order fragment"
                                                                   : ":Dropped fragment(s)"));
      return false;
    }
    c.frag_in.insert(c.frag_in.end(), data + r.at, data + size);
    if (more) return false;
    payload = std::span<const uint8_t>(c.frag_in.data(), c.frag_in.size());
  } else {
    payload = std::span<const uint8_t>(data + r.at, size - r.at);
  }
  c.last_drop = sequence - (c.incoming_sequence + 1);
  if (traced && c.trace->drop && c.last_drop > 0)
    c.trace->print(c.trace->where + ":Dropped " + std::to_string(c.last_drop) + " packets at " +
                   std::to_string(sequence));
  if (traced && (c.trace->packets == 1 || c.trace->packets == 3))
    packet_line(c, "<--", sequence, reliable_message, ack, reliable_ack, size, now);
  c.dropped += c.last_drop;
  c.incoming_sequence = sequence;
  if (reliable_message) c.incoming_reliable_sequence = !c.incoming_reliable_sequence;
  c.accepted++;
  c.bytes_in += size;
  return true;
}

bool channel_sending_pieces(const Channel& c) { return !c.frag_out.empty(); }

bool peek_qport(const uint8_t* data, size_t size, uint16_t& qport) {
  if (size < kClientHeader) return false;
  qport = static_cast<uint16_t>(data[8] | data[9] << 8);
  return true;
}

bool is_out_of_band(const uint8_t* data, size_t size) {
  return size >= 4 && data[0] == 0xff && data[1] == 0xff && data[2] == 0xff && data[3] == 0xff;
}

}  // namespace ge::classic::protocol
