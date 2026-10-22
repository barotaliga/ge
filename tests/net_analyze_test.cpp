// engine/net/analyze.hpp: a hand-made capture across the sequence wrap reads back its duplicate,
// its reorder, its resend, its acks and their delays; a fake-traffic session's capture taken at
// the host reads back the session's own numbers (every client a connection, the round trip of
// whole ticks on a LAN, the link's loss on a bad one); and an empty capture is empty.
//
// ge:summary Test: a capture analyzed: wrap, duplicates, reorders, resends, acks; a session's
// ge:keywords test net analyze capture stats rtt loss resend wrap
// ge:skill net testing
#include "net/analyze.hpp"

#include <cmath>
#include <string>
#include <vector>

#include "check.hpp"
#include "net/packet.hpp"
#include "net/traffic.hpp"

using namespace ge::net;

namespace {

const uint32_t kProtocol = protocol_id("analyze-test", 1);

CapturedDatagram payload(int64_t at_ms, const Address& from, const Address& to, uint16_t seq,
                         uint16_t ack, uint32_t bits, int reliable_id = -1) {
  uint8_t buf[kMaxPacket];
  Writer w = make_writer(buf, sizeof buf);
  write_payload_header(w, kProtocol, PayloadHeader{7, seq, ack, bits});
  if (reliable_id >= 0) {
    const uint8_t data[4] = {1, 2, 3, 4};
    write_message(w, 2, true, static_cast<uint16_t>(reliable_id), data, sizeof data);
  }
  CapturedDatagram d;
  d.at_us = at_ms * 1000;
  d.from = from;
  d.to = to;
  d.data.assign(buf, buf + w.size);
  return d;
}

}  // namespace

TEST(a_hand_made_capture_reads_back_across_the_wrap) {
  const Address a = sim_address(1, 1), b = sim_address(2, 1);
  std::vector<CapturedDatagram> ds = {
      payload(0, a, b, 65534, 0, 0),
      payload(10, a, b, 65535, 0, 0),
      payload(20, a, b, 1, 0, 0, 7),  // 0 is never sent: the next after 65535 is 1
      payload(30, a, b, 3, 0, 0, 7),  // the reliable message again: a resend
      payload(40, a, b, 2, 0, 0),     // behind 3: reordered
      payload(50, a, b, 3, 0, 0, 7),  // 3 again: duplicated, its message not a second resend
      // b acks 3, then 2 (bit 0), 1 (bit 1) and 65535 (bit 3): 65534 is not acked yet.
      payload(90, b, a, 1, 3, 0b1011),
  };
  const CaptureAnalysis an = analyze_capture(ds, 0.1);
  CHECK(an.protocol == kProtocol && an.foreign == 0 && an.datagrams == ds.size());
  CHECK(an.connections.size() == 1);
  const DirectionStats& ab = an.connections[0].ab;
  CHECK(ab.from == a && ab.to == b && ab.packets == 6);
  CHECK_MSG(ab.missing == 0, "missing " + std::to_string(ab.missing) + ", want 0 (0 is skipped)");
  CHECK(ab.duplicated == 1 && ab.reordered == 1);
  CHECK_MSG(ab.resends == 1, "resends " + std::to_string(ab.resends));
  CHECK(ab.acked == 4 && ab.ack_delay_ms.size() == 4);
  // Acked in the order the ack names them: 3 (sent at 30), 2 (40), 1 (20), 65535 (10).
  CHECK(ab.ack_delay_ms[0] == 60 && ab.ack_delay_ms[1] == 50 && ab.ack_delay_ms[2] == 70 &&
        ab.ack_delay_ms[3] == 80);
  // 65534 is within the ack bits' reach of the newest ack: not settled, so not lost.
  CHECK(ab.settled == 4 && ab.loss == 0);
  CHECK(ab.messages == 3);
}

TEST(a_foreign_packet_is_counted_not_read) {
  const Address a = sim_address(1, 1), b = sim_address(2, 1);
  std::vector<CapturedDatagram> ds = {payload(0, a, b, 1, 0, 0), payload(10, a, b, 2, 0, 0)};
  CapturedDatagram junk;
  junk.at_us = 20000;
  junk.from = b;
  junk.to = a;
  junk.data = {1, 2, 3};
  ds.push_back(junk);
  const CaptureAnalysis an = analyze_capture(ds);
  CHECK(an.foreign == 1 && an.connections.size() == 1 && an.connections[0].ab.packets == 2);
}

TEST(an_empty_capture_is_empty) {
  const CaptureAnalysis an = analyze_capture({});
  CHECK(an.datagrams == 0 && an.connections.empty() && !an.local_known && an.t_s.empty());
}

TEST(a_session_s_capture_reads_back_its_numbers) {
  for (const char* preset : {"lan", "bad"}) {
    TrafficPlan plan;
    plan.clients = 5;
    plan.seconds = 6;
    plan.drain_seconds = 3;
    plan.capture = true;
    conditions_preset(preset, plan.conditions);
    const TrafficReport r = run_traffic(plan);
    std::vector<CapturedDatagram> ds;
    std::string error;
    CHECK_MSG(read_capture(r.capture.bytes.data(), r.capture.bytes.size(), ds, &error), error);
    const CaptureAnalysis an = analyze_capture(ds);
    const std::string at = std::string(preset) + ": ";
    CHECK_MSG(an.local_known && an.local == sim_address(0, 1), at + "the host took the capture");
    CHECK_MSG(an.connections.size() == 5, at + std::to_string(an.connections.size()) + " conns");
    CHECK(!an.t_s.empty() && an.t_s.size() == an.out_kbps.size() &&
          an.t_s.size() == an.rtt_ms.size() && an.t_s.size() == an.loss_pct.size());
    double sent = 0, lost = 0;
    uint64_t resends = 0;
    for (const ConnectionStats& c : an.connections) {
      const DirectionStats& out = sent_by(c, an.local);
      const DirectionStats& in = received_by(c, an.local);
      CHECK(out.from == an.local && in.to == an.local);
      CHECK_MSG(
          out.by_type[static_cast<int>(PacketType::accept)] >= 1,
          at + std::to_string(out.by_type[static_cast<int>(PacketType::accept)]) + " accepts");
      const double rtt = percentile_of(out.ack_delay_ms, 50);
      // Each way is the latency rounded up to whole 60 Hz ticks, and more on a bad link.
      const double tick = 1000.0 / plan.tick_hz;
      const double least = 2 * std::ceil(plan.conditions.latency_ms / tick) * tick;
      CHECK_MSG(rtt >= least - 0.01 && rtt <= least + plan.conditions.jitter_ms * 2 + 3 * tick,
                at + "rtt p50 " + std::to_string(rtt) + " ms against " + std::to_string(least));
      sent += static_cast<double>(out.settled);
      lost += out.loss * static_cast<double>(out.settled);
      resends += out.resends;
    }
    const double loss = lost / sent;
    if (plan.conditions.loss == 0) {
      CHECK(loss == 0 && resends == 0);
    } else {
      CHECK_MSG(std::fabs(loss - r.loss_total) < 0.02, at + "loss " + std::to_string(loss) +
                                                           " against the session's " +
                                                           std::to_string(r.loss_total));
      CHECK(resends > 0);
    }
  }
}
