// ONE CONNECTION'S STREAM OF PAYLOADS, after the handshake: the sequence numbers and their acks,
// the round trip and the loss read off them, and the game's channels carried in them.
//
// - ACKS. Every payload carries its sequence, the newest sequence received and 32 bits for the
//   ones before it, so each packet's fate is told by the next few the other side sends. A
//   packet acked is a round-trip sample (smoothed as RFC 6298 does: SRTT and RTTVAR, gains 1/8
//   and 1/4; the sample includes how long the other side held the ack, at most its flush
//   interval). A packet never acked once it can no longer be (more than 32 behind the newest
//   ack, or older than twice the resend timeout) is lost; `loss` is their share, smoothed over
//   about the last 64 packets.
// - CHANNELS (`Delivery`): `unreliable` messages ride the next packet with room, once, or are
//   dropped at the end of the flush; `sequenced` ones likewise, and the receiver drops any older
//   than one it has delivered (an input, a position: only the newest counts); `reliable` ones
//   are kept until a packet carrying them is acked, ride every packet until then once the resend
//   timeout (SRTT + 4 RTTVAR, 20 ms to 1 s) has passed since they last rode, and are delivered
//   once each, in the order sent, per channel.
// - LIMITS. At most kReliableInFlight reliable messages wait for acks across the channels, and a
//   channel's oldest waiting id is never more than kReliableSpan behind its newest: a send past
//   either is refused (`connection_queue` returns false) so the game sees the back-pressure.
//
// Everything is sized at `connection_init`; nothing allocates afterwards. The clock is the
// caller's (`now_us`); nothing here reads one.
//
// ge:summary One connection: sequences, acks, RTT and loss, unreliable/sequenced/reliable channels
// ge:keywords net connection reliability reliable unreliable sequenced ordered channel ack
// ge:keywords ack-bits rtt round-trip ping srtt rttvar rto resend retransmit loss packet-loss
// ge:keywords jitter bandwidth link-stats stats delivery
// ge:skill net
// ge:test tests/net_connection_test.cpp
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "net/packet.hpp"

namespace ge::net {

enum class Delivery : uint8_t { unreliable, sequenced, reliable };
const char* delivery_name(Delivery d);

struct ChannelConfig {
  Delivery delivery[kMaxChannels] = {};
  int count = 1;
};

constexpr int kSentRing = 1024;         // packets remembered for their acks
constexpr int kReliableInFlight = 128;  // reliable messages waiting for acks, every channel
constexpr int kReliableSpan = 128;      // a channel's oldest waiting id to its newest
constexpr int kReliablePerPacket = 32;  // reliable messages one packet may carry

// A link's numbers, kept as the packets go: what the HUD's ping, the scoreboard and the tools read.
struct LinkStats {
  double rtt_ms = 0;      // smoothed round trip (0 until the first sample)
  double rtt_var_ms = 0;  // its mean deviation
  double jitter_ms = 0;   // RFC 3550's: the smoothed change between consecutive samples
  double loss = 0;        // the share of packets lost, about the last 64
  double send_kbps = 0;   // kbit/s out and in, over the last 250 ms window
  double recv_kbps = 0;
  uint64_t packets_sent = 0, packets_received = 0, packets_acked = 0, packets_lost = 0;
  uint64_t bytes_sent = 0, bytes_received = 0;
  uint64_t messages_sent = 0, messages_received = 0;
  uint64_t resends = 0;         // a reliable message sent again
  uint64_t dropped_unsent = 0;  // unreliable messages that found no room before the flush ended
  uint64_t duplicates = 0;      // packets received twice (the second dropped)
  uint64_t stale = 0;           // packets too old to place, and sequenced messages overtaken
  uint64_t rtt_samples = 0;
};

struct Connection {
  ChannelConfig channels;
  uint64_t key = 0;  // the session key every payload carries
  LinkStats stats;

  // Sending. Sequence 0 is never sent: an ack of 0 means nothing has been received yet.
  uint16_t next_seq = 1;
  struct Sent {
    bool used = false, acked = false, final = false;
    uint16_t seq = 0;
    int64_t sent_us = 0;
    uint8_t reliable = 0;
    uint8_t slots[kReliablePerPacket];  // into `out`
    uint8_t chans[kReliablePerPacket];
    uint16_t ids[kReliablePerPacket];
  };
  std::vector<Sent> sent;  // kSentRing, indexed by seq % kSentRing
  uint16_t audit_seq = 1;  // the oldest packet not yet counted acked or lost
  uint16_t newest_acked = 0;
  bool any_acked = false;

  struct Out {
    bool used = false;
    uint8_t channel = 0;
    uint16_t id = 0;
    uint16_t size = 0;
    int64_t last_sent_us = -1;  // -1: never sent
  };
  std::vector<Out> out;            // kReliableInFlight
  std::vector<uint8_t> out_bytes;  // kReliableInFlight * kMaxMessage
  uint16_t next_id[kMaxChannels] = {};
  // Unreliable and sequenced messages queued since the last flush, already encoded.
  std::vector<uint8_t> loose;
  std::vector<uint32_t> loose_ends;  // where each message's encoding ends in `loose`
  size_t loose_next = 0;             // the first message not yet in a packet this flush

  // Receiving.
  bool any_received = false;
  uint16_t recv_newest = 0;
  uint32_t recv_bits = 0;
  bool ack_owed = false;  // a payload arrived since the last packet went out
  uint16_t expect_id[kMaxChannels] = {};
  bool seq_any[kMaxChannels] = {};
  uint16_t seq_newest[kMaxChannels] = {};
  struct Held {
    bool used = false;
    uint8_t channel = 0;
    uint16_t id = 0;
    uint16_t size = 0;
  };
  std::vector<Held> held;           // reliable messages that arrived ahead of their turn
  std::vector<uint8_t> held_bytes;  // kReliableSpan * kMaxMessage

  // The bandwidth windows.
  int64_t window_start_us = 0;
  uint64_t window_sent = 0, window_recv = 0;
  double last_rtt_sample_ms = -1;
};

// Size every buffer for `channels` and start the stream at `now_us` with `key`.
void connection_init(Connection& c, const ChannelConfig& channels, uint64_t key, int64_t now_us);

// Queue a message on `channel`; false when it is too big (kMaxMessage), the channel unknown, or a
// reliable window is full.
bool connection_queue(Connection& c, int channel, const uint8_t* data, size_t size);

// The next packet to send into `out` (kMaxPacket bytes): its size, or 0 when there is nothing to
// send and `force` is false. `force` sends a header-only packet (a keepalive, an ack) when there
// is nothing else. The caller sends it, then calls again while it returns more.
size_t connection_write(Connection& c, uint32_t protocol, uint8_t* out, int64_t now_us, bool force);
// Drop the unreliable and sequenced messages no packet had room for (end of a flush).
void connection_end_flush(Connection& c);

// What a received payload delivered: called once a message, in delivery order.
struct MessageSink {
  void (*fn)(void* ctx, int channel, const uint8_t* data, size_t size);
  void* ctx;
};
// A payload whose header is read and whose key matched; `r` stands at its first message. False
// when the packet is a duplicate, too old, or malformed (nothing of it is delivered).
bool connection_read(Connection& c, const PayloadHeader& h, Reader& r, int64_t now_us,
                     MessageSink sink);

// Count the packets that can no longer be acked as lost, and roll the bandwidth window.
void connection_audit(Connection& c, int64_t now_us);

// The resend timeout now: SRTT + 4 RTTVAR in [20 ms, 1 s], 200 ms before the first sample.
int64_t resend_timeout_us(const Connection& c);

}  // namespace ge::net
