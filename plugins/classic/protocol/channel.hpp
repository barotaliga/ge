// THE NET CHANNEL of the classic protocol family: what every in-band packet carries, on either
// side, in Quake's protocol and Quake II's alike. No socket here: `channel_transmit`
// returns a datagram's bytes and `channel_process` reads one, so a game's endpoints (its server
// and its client) own the sending.
//
// THE HEADER: u32 sequence (bit 31 set when the packet carries the reliable message); u32 the last
// sequence received (bit 31 the even/odd bit of the last reliable message received); from a
// client, a u16 qport after it, so the server finds the client when a NAT moves its port. A packet
// whose first word is 0xFFFFFFFF is out of band (a game's server reads those), never a
// channel's.
//
// ONE RELIABLE MESSAGE IN FLIGHT: it is resent while the peer has acknowledged something newer
// than the packet that last carried it but its reliable bit does not match ours yet; when the bit
// matches, the in-flight buffer is cleared. A new reliable message is taken from the pending
// queue only when none is in flight, and taking it flips our reliable bit. The reliable part goes
// first in a packet, then as much of the unreliable datagram as fits. Pending reliable data waits
// in up to kBackBuffers messages (a full queue is an overflow: the endpoint drops the client).
//
// RECEIVING: a sequence not newer than the last one is stale or a duplicate and is dropped; the
// packets lost before an accepted one are `sequence - (last + 1)`.
//
// THE RATE: the channel keeps a clear time; sending n bytes adds `n x copies / rate` seconds to
// it (from now when it lies in the past); a packet may go when the clear time is before now plus
// 400 bytes' worth. A rate of 0 is unlimited (loopback). `copies` is 1 + the `dupe` user info key.
//
// QUAKE II'S LATER PROTOCOLS (35 and 36) change two things, each a field of the channel set by the
// endpoint after `channel_setup`. A client's qport is one byte, or none when it is 0 (the server
// then knows the client by its address and port alone): `qport_bytes` 2, 1 or 0. And protocol 36
// may ask for the NEWER CHANNEL (`fragments`): sequences of 30 bits, bit 30 of the first word marks
// a fragment; reliable data gathers in one buffer of up to kBigMessage bytes (past it: an
// overflow), sent whole as the next reliable message once the last one is acknowledged. A
// datagram whose reliable and unreliable parts together pass `packet_len` (or whose unreliable part
// alone does) goes in fragments: the reliable part then the unreliable one (left out when the two
// pass kBigMessage, `dumped`), cut into pieces of `packet_len` bytes that share one sequence, each
// with a word after the header (bits 0 to 14 the piece's offset, bit 15 another follows), the
// first piece at once and one more at each later transmit (the caller's unreliable data that
// time is not sent). The receiver joins the pieces in order; a piece out of order or past a gap
// throws the message away, and a lost piece loses the whole datagram (its reliable part goes again
// as a new one). Acknowledgements are taken from every piece.
//
// SHOWING THE PACKETS (`trace`, a client's `showpackets` and `showdrop`): each packet sent is a
// line `<ms> --> s=<sequence>(<reliable>) a=<last received>(<its reliable bit>) <bytes>`, each
// accepted `<ms> <-- ...` the same (`packets` 1 both ways, 2 sent only, 3 received only), the
// clock in milliseconds to a tenth; `drop` says `<where>:Out of order packet <n> at <newest>` for a
// stale one and `<where>:Dropped <n> packets at <sequence>` for a gap.
//
// ge:summary The classic net channel: sequences, the reliable bit and resend, qport, rate, choke
// ge:keywords net-channel netchan sequence ack reliable-bit resend qport rate clear-time choke
// ge:keywords backbuf dupe channel_transmit channel_process channel_can_packet drop-count
// ge:keywords showpackets showdrop channel-trace
// ge:skill classic shooter net
// ge:test tests/classic_protocol_channel_test.cpp
#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <vector>

#include "classic/protocol/wire.hpp"

namespace ge::classic::protocol {

constexpr int kBackBuffers = 8;
constexpr size_t kBigMessage = 32768;  // the newer channel's reliable buffer and joined message
constexpr double kRateBurstBytes = 400;

struct ChannelTrace {
  int packets = 0;    // showpackets: 0 none, 1 both ways, 2 sent, 3 received
  bool drop = false;  // showdrop
  std::string where;  // the peer's address, for the drop lines
  std::function<void(const std::string&)> print;
};

struct MsgBuf {
  uint16_t size = 0;
  uint8_t data[kMaxMsgLen];
};

struct Channel {
  bool client = false;  // a client's channel writes its qport; a server's reads it
  uint16_t qport = 0;

  uint32_t outgoing_sequence = 1;  // the next packet's
  uint32_t incoming_sequence = 0;  // the newest accepted
  uint32_t incoming_acknowledged = 0;
  bool incoming_reliable_acknowledged = false;  // the peer's bit for our reliable message
  bool incoming_reliable_sequence = false;      // our bit for the peer's reliable messages
  bool reliable_sequence = false;               // our reliable message's bit
  uint32_t last_reliable_sequence = 0;          // the packet that last carried it

  MsgBuf reliable;               // in flight (size 0: none)
  MsgBuf pending[kBackBuffers];  // waiting, oldest first
  int pending_count = 0;
  bool overflow = false;

  double rate_s_per_byte = 1.0 / 2500;  // 0: unlimited
  int copies = 1;
  double clear_time = 0;  // s

  // Counts.
  uint32_t last_drop = 0;  // packets lost before the newest accepted
  uint64_t dropped = 0, stale = 0, accepted = 0, sent = 0, resent = 0, choked = 0;
  uint64_t bytes_in = 0, bytes_out = 0;
  double last_received = 0;       // s
  ChannelTrace* trace = nullptr;  // kept by `channel_setup`
  // Quake II's later protocols (above); `channel_setup` resets them.
  uint8_t qport_bytes = 2;
  bool fragments = false;
  size_t packet_len = kMaxPacket;  // the newer channel's piece and the datagram's payload, at most
  std::vector<uint8_t> big_pending, big_reliable;  // waiting, in flight
  std::vector<uint8_t> frag_out;                   // the datagram going out in pieces
  size_t frag_out_at = 0;
  bool frag_out_reliable = false;
  std::vector<uint8_t> frag_in;  // the pieces joined so far
  uint32_t frag_in_sequence = 0;
  uint64_t dumped = 0, pieces_out = 0, pieces_lost = 0;
};

void channel_setup(Channel& c, bool client, uint16_t qport, double now);
// The rate in bytes a second (0: unlimited) and the duplicate copies.
void channel_set_rate(Channel& c, int bytes_per_second, int dupe = 0);

// Appends to the pending reliable data; false (and `overflow`) when no room is left.
bool channel_reliable(Channel& c, const uint8_t* data, size_t size);
// Bytes of reliable data waiting or in flight.
size_t channel_reliable_backlog(const Channel& c);

bool channel_can_packet(const Channel& c, double now);

// One packet with `size` bytes of unreliable data after the reliable part when they fit; the
// datagram's length into `out` (kMaxPacket bytes). The caller sends it `c.copies` times.
size_t channel_transmit(Channel& c, const uint8_t* unreliable, size_t size, double now,
                        uint8_t* out);

// Reads a packet's header; false when it is stale (or too short). On true the payload starts at
// `payload` (after the qport on a server's channel).
bool channel_process(Channel& c, const uint8_t* data, size_t size, double now, size_t& payload);

// As `channel_process`, the payload handed back whole: on the newer channel a fragment's message
// once its last piece is in (false until then).
bool channel_read(Channel& c, const uint8_t* data, size_t size, double now,
                  std::span<const uint8_t>& payload);
// The newer channel is still sending a datagram's pieces (the next transmit sends one).
bool channel_sending_pieces(const Channel& c);
// The qport a client's packet carries (after the 8 header bytes); false when too short.
bool peek_qport(const uint8_t* data, size_t size, uint16_t& qport);
// True when a datagram is out of band (its first word 0xFFFFFFFF).
bool is_out_of_band(const uint8_t* data, size_t size);

}  // namespace ge::classic::protocol
