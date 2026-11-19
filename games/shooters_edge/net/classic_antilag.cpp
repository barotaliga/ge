// Lag compensation's history and rewind arithmetic (classic_antilag.hpp).
//
// ge:summary The antilag ring, the target time and a player's position at a time, defined
// ge:keywords antilag_record antilag_target antilag_position antilag_place antilag_wanted
// ge:skill net shooter
// ge:test tests/shooters_edge_classic_antilag_test.cpp
#include "shooters_edge/net/classic_antilag.hpp"

#include <cstdlib>

namespace shooters_edge::net {

void antilag_record(AntilagHistory& h, int slot, double time, sh::Vec3f origin, sh::Vec3f mins,
                    sh::Vec3f maxs) {
  if (slot < 0 || slot >= kAntilagPlayers) return;
  AntilagRing& r = h.players[slot];
  if (r.count > 0) {  // a teleport or a respawn: the ring starts afresh
    const AntilagSample& last = r.samples[(r.head + kAntilagSamples - 1) % kAntilagSamples];
    const sh::Vec3f d = origin - last.origin;
    const double dt = time > last.time ? time - last.time : 0;
    const float reach = kAntilagJump + kAntilagSpeed * static_cast<float>(dt);
    if (d.x * d.x + d.y * d.y + d.z * d.z > reach * reach) r = AntilagRing{};
  }
  r.samples[r.head] = {time, origin, mins, maxs};
  r.head = (r.head + 1) % kAntilagSamples;
  if (r.count < kAntilagSamples) r.count++;
}

void antilag_clear(AntilagHistory& h, int slot) {
  if (slot >= 0 && slot < kAntilagPlayers) h.players[slot] = AntilagRing{};
}

double antilag_target(double acked_frame_time, double ping_s, int maxfps, double now, bool exact,
                      double max_back, double back) {
  if (maxfps < 20 || maxfps > 1000) maxfps = 77;
  const double lead = exact ? 0.0 : ping_s < 0.02 ? 1.0 / maxfps : kAntilagLead;
  double t = acked_frame_time + lead - back;
  if (t > now) t = now;
  if (max_back > 0 && t < now - max_back) t = now - max_back;
  return t;
}

bool antilag_wanted(const std::string& cl_antilag) {
  return cl_antilag.empty() || std::atof(cl_antilag.c_str()) > 0;
}

const char* antilag_told(bool wanted) {
  return wanted ? "Your shots now hit the others where your screen showed them.\n"
                : "Your shots now hit the others where the server has them.\n";
}

bool antilag_position(const AntilagRing& ring, double t, sh::Vec3f velocity, sh::Vec3f& out) {
  AntilagPlace p;
  if (!antilag_place(ring, t, velocity, p)) return false;
  out = p.origin;
  return true;
}

bool antilag_place(const AntilagRing& ring, double t, sh::Vec3f velocity, AntilagPlace& out) {
  if (ring.count == 0) return false;
  auto at = [&](int i) -> const AntilagSample& {  // i = 0 the oldest
    return ring.samples[(ring.head - ring.count + i + kAntilagSamples) % kAntilagSamples];
  };
  const AntilagSample& newest = at(ring.count - 1);
  if (t >= newest.time) {
    double gap = t - newest.time;
    if (gap > kAntilagLead) gap = kAntilagLead;
    if (ring.count == 1) gap = t > newest.time ? gap : 0;
    out = {newest.origin + velocity * static_cast<float>(gap), newest.mins, newest.maxs};
    return true;
  }
  if (ring.count == 1 || t <= at(0).time) {
    const AntilagSample& o = ring.count == 1 ? newest : at(0);
    out = {o.origin, o.mins, o.maxs};
    return true;
  }
  for (int i = ring.count - 1; i > 0; i--) {
    const AntilagSample& a = at(i - 1);
    const AntilagSample& b = at(i);
    if (t >= a.time && t <= b.time) {
      const double span = b.time - a.time;
      const float f = span > 0 ? static_cast<float>((t - a.time) / span) : 1.0f;
      out = {a.origin + (b.origin - a.origin) * f, a.mins, a.maxs};
      return true;
    }
  }
  out = {newest.origin, newest.mins, newest.maxs};
  return true;
}

}  // namespace shooters_edge::net
