// THE RATES: what a transport sent and received a second, measured over whole windows of about a
// second (the counts the transport keeps, net/transport.hpp), and a caller's own count beside them
// (the moves a game sent). The figures are the last finished window's, so a macro or a meter
// reads steady numbers rather than one frame's burst; a window longer than two seconds (a stall)
// is measured as it was.
//
// ge:summary Packets, bytes and a caller's count a second over a transport, by one-second windows
// ge:keywords rates rate-meter packets-per-second bytes-per-second uprate dnrate pps mps
// ge:keywords cl_pps cl_mps net_uprate net_dnrate
// ge:skill net
// ge:test tests/net_rates_test.cpp
#pragma once

#include <cstdint>

#include "net/transport.hpp"

namespace ge::net {

struct Rates {
  // The last finished window's figures, a second:
  double sent_packets = 0, sent_bytes = 0, received_packets = 0, received_bytes = 0, counted = 0;
  // The window being measured: when it began (s, negative before the first step) and the counts
  // then.
  double from_s = -1;
  uint64_t sp = 0, sb = 0, rp = 0, rb = 0, n = 0;
};

// Steps the meter at `now_s` with the transport's counts and the caller's running count; a window
// of at least `window_s` closes and becomes the figures.
void rates_step(Rates& r, double now_s, const Transport& t, uint64_t count, double window_s = 1);
// Back to nothing measured (a new connection).
void rates_reset(Rates& r);

}  // namespace ge::net
