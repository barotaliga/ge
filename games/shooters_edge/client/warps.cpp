// The players who arrived or left this frame (warps.hpp).
//
// ge:summary Who warped: arrivals and departures read off the players' moves, the shown share
// ge:keywords warps watch_warps warp_shown arrival_shown jump respawn teleport
// ge:skill render shooter
// ge:test tests/shooters_edge_warps_test.cpp
#include "shooters_edge/client/warps.hpp"

#include <algorithm>

namespace shooters_edge::client {

namespace {

double ease(double x) {
  x = std::clamp(x, 0.0, 1.0);
  return x * x * (3 - 2 * x);
}

}  // namespace

void watch_warps(WarpWatch& w, std::span<const Body> now, double t, double dt,
                 std::vector<Warp>& began) {
  std::erase_if(w.live,
                [&](const Warp& v) { return t - v.t0 >= (v.arriving ? kArriveS : kLeaveS); });
  const float reach = kJumpUnits + kJumpUnitsPerS * float(std::max(dt, 0.0));
  const auto start = [&](int entity, const sh::Vec3f& at, bool arriving) {
    // A second warp of the same kind for the same entity replaces the first.
    std::erase_if(w.live,
                  [&](const Warp& v) { return v.entity == entity && v.arriving == arriving; });
    const Warp v{entity, at, t, arriving};
    w.live.push_back(v);
    began.push_back(v);
  };
  std::unordered_map<int, Body> next;
  for (const Body& b : now) {
    next[b.entity] = b;
    const auto was = w.last.find(b.entity);
    if (was == w.last.end()) {
      start(b.entity, b.origin, true);
      continue;
    }
    const sh::Vec3f& a = was->second.origin;
    const float dx = b.origin.x - a.x, dy = b.origin.y - a.y, dz = b.origin.z - a.z;
    if (dx * dx + dy * dy + dz * dz <= reach * reach) continue;
    if (was->second.alive) start(b.entity, a, false);
    start(b.entity, b.origin, true);
  }
  w.last = std::move(next);
}

double warp_shown(const Warp& warp, double t) {
  if (warp.arriving) return ease((t - warp.t0) / kArriveS);
  return 1 - ease((t - warp.t0) / kLeaveS);
}

double arrival_shown(const WarpWatch& w, int entity, double t) {
  for (const Warp& v : w.live)
    if (v.entity == entity && v.arriving) return warp_shown(v, t);
  return 1;
}

}  // namespace shooters_edge::client

namespace shooters_edge::client {

double arrival_time(double share) {
  share = std::clamp(share, 0.0, 1.0);
  // The ease rises everywhere: halve its domain until it holds the share.
  double lo = 0, hi = 1;
  for (int i = 0; i < 30; i++) {
    const double mid = 0.5 * (lo + hi);
    (ease(mid) < share ? lo : hi) = mid;
  }
  return 0.5 * (lo + hi) * kArriveS;
}

}  // namespace shooters_edge::client
