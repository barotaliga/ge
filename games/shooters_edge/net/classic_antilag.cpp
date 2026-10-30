// Lag compensation's history and rewind arithmetic (classic_antilag.hpp).
//
// ge:summary The antilag ring, the target time and a player's position at a time, defined
// ge:keywords antilag_record antilag_target antilag_position
// ge:skill net shooter
// ge:test tests/shooters_edge_classic_antilag_test.cpp
#include "shooters_edge/net/classic_antilag.hpp"

namespace shooters_edge::net {

void antilag_record(AntilagHistory& h, int slot, double time, sh::Vec3f origin) {
  if (slot < 0 || slot >= kAntilagPlayers) return;
  AntilagRing& r = h.players[slot];
  r.samples[r.head] = {time, origin};
  r.head = (r.head + 1) % kAntilagSamples;
  if (r.count < kAntilagSamples) r.count++;
}

void antilag_clear(AntilagHistory& h, int slot) {
  if (slot >= 0 && slot < kAntilagPlayers) h.players[slot] = AntilagRing{};
}

double antilag_target(double acked_frame_time, double ping_s, int maxfps, double now) {
  if (maxfps < 20 || maxfps > 1000) maxfps = 77;
  const double lead = ping_s < 0.02 ? 1.0 / maxfps : kAntilagLead;
  const double t = acked_frame_time + lead;
  return t > now ? now : t;
}

bool antilag_position(const AntilagRing& ring, double t, sh::Vec3f velocity, sh::Vec3f& out) {
  if (ring.count == 0) return false;
  auto at = [&](int i) -> const AntilagSample& {  // i = 0 the oldest
    return ring.samples[(ring.head - ring.count + i + kAntilagSamples) % kAntilagSamples];
  };
  const AntilagSample& newest = at(ring.count - 1);
  if (t >= newest.time) {
    double gap = t - newest.time;
    if (gap > kAntilagLead) gap = kAntilagLead;
    if (ring.count == 1) gap = t > newest.time ? gap : 0;
    out = newest.origin + velocity * static_cast<float>(gap);
    return true;
  }
  if (ring.count == 1 || t <= at(0).time) {
    out = ring.count == 1 ? newest.origin : at(0).origin;
    return true;
  }
  for (int i = ring.count - 1; i > 0; i--) {
    const AntilagSample& a = at(i - 1);
    const AntilagSample& b = at(i);
    if (t >= a.time && t <= b.time) {
      const double span = b.time - a.time;
      const float f = span > 0 ? static_cast<float>((t - a.time) / span) : 1.0f;
      out = a.origin + (b.origin - a.origin) * f;
      return true;
    }
  }
  out = newest.origin;
  return true;
}

}  // namespace shooters_edge::net
