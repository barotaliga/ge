// The rates over a transport (rates.hpp).
//
// ge:summary Packets, bytes and a caller's count a second, by one-second windows
// ge:keywords rates rate-meter rates_step
// ge:skill net
// ge:test tests/net_rates_test.cpp
#include "net/rates.hpp"

namespace ge::net {

void rates_step(Rates& r, double now_s, const Transport& t, uint64_t count, double window_s) {
  const auto start = [&] {
    r.from_s = now_s;
    r.sp = t.sent_packets;
    r.sb = t.sent_bytes;
    r.rp = t.received_packets;
    r.rb = t.received_bytes;
    r.n = count;
  };
  // A first step, or counts that went back (another transport, a count restarted): begin again.
  if (r.from_s < 0 || now_s < r.from_s || t.sent_packets < r.sp || t.sent_bytes < r.sb ||
      t.received_packets < r.rp || t.received_bytes < r.rb || count < r.n) {
    start();
    return;
  }
  const double span = now_s - r.from_s;
  if (span < window_s) return;
  const auto per_s = [span](uint64_t now, uint64_t then) {
    return static_cast<double>(now - then) / span;
  };
  r.sent_packets = per_s(t.sent_packets, r.sp);
  r.sent_bytes = per_s(t.sent_bytes, r.sb);
  r.received_packets = per_s(t.received_packets, r.rp);
  r.received_bytes = per_s(t.received_bytes, r.rb);
  r.counted = per_s(count, r.n);
  start();
}

void rates_reset(Rates& r) { r = Rates{}; }

}  // namespace ge::net
