// ge:summary Watching the ways in: the spots seen from where a bot stands, and the one watched now
// ge:keywords watch watch_spots watch_choose doorway first-seen line-of-sight ways-in
// ge:skill bots
// ge:test tests/shooters_edge_fight_test.cpp
#include "shooters_edge/bots/watch.hpp"

#include <cmath>

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "shooter/trace.hpp"
#include "shooters_edge/bots/map_plan.hpp"

namespace shooters_edge::bots {

namespace {

// Two spots this near in bearing from the eye are one, degrees.
constexpr double kSameSpot_deg = 20;
// A spot is watched this long before the next, s (drawn between).
constexpr double kWatchMin_s = 1.5, kWatchMax_s = 4;
// A spot this near the eye is no doorway (it stands in it), units.
constexpr float kTooNear = 96;
// Where a head shows over a node, units above its origin.
constexpr float kHead = 22;

double bearing(sh::Vec3f from, sh::Vec3f to) {
  return ge::core::libm::atan2(static_cast<double>(to.y - from.y),
                               static_cast<double>(to.x - from.x)) *
         (180.0 / ge::core::kPi);
}

double apart(double a, double b) {
  double d = std::fmod(a - b, 360.0);
  if (d > 180) d -= 360;
  if (d < -180) d += 360;
  return std::fabs(d);
}

bool sees(const server::RulesCommon& s, int me, sh::Vec3f eye, sh::Vec3f p) {
  return sh::trace_move(s.sv, eye, {}, {}, p, sh::kTraceNoMonsters, me).fraction >= 1;
}

}  // namespace

void watch_spots(Fighter& f, const server::RulesCommon& s, int me, int32_t node, sh::Vec3f eye) {
  f.watch.clear();
  f.watch_node = node;
  f.watch_pick = -1;
  if (!f.plan || node < 0) return;
  const int32_t a = plan_area_of(*f.plan, node);
  if (a < 0) return;
  const sh::NavGraph& g = *f.bot.pilot.pilot.g;
  for (const int32_t entry : f.plan->areas[static_cast<size_t>(a)].entries) {
    const sh::NavRoute r = sh::nav_route(g, entry, node, f.watch_search);
    if (r.nodes.size() < 2) continue;
    // From him back along the way in: the last node still in sight.
    int32_t spot = -1;
    for (size_t k = r.nodes.size() - 1; k-- > 0;) {
      const sh::Vec3f o = g.nodes[static_cast<size_t>(r.nodes[k])].origin;
      if (!sees(s, me, eye, {o.x, o.y, o.z + kHead})) break;
      spot = r.nodes[k];
    }
    if (spot < 0) continue;
    const sh::Vec3f o = g.nodes[static_cast<size_t>(spot)].origin;
    const sh::Vec3f at{o.x, o.y, o.z + kHead};
    const float dx = at.x - eye.x, dy = at.y - eye.y;
    if (dx * dx + dy * dy < kTooNear * kTooNear) continue;
    bool same = false;
    for (const sh::Vec3f& w : f.watch)
      same = same || apart(bearing(eye, w), bearing(eye, at)) < kSameSpot_deg;
    if (!same) f.watch.push_back(at);
  }
}

int32_t watch_choose(Fighter& f, sh::Vec3f eye, double t, const sh::Vec3f* heard) {
  if (f.watch.empty()) return f.watch_pick = -1;
  const auto n = static_cast<int32_t>(f.watch.size());
  if (heard) {
    // The spot nearest the sound's bearing, held a while as any.
    int32_t best = 0;
    for (int32_t k = 1; k < n; k++)
      if (apart(bearing(eye, f.watch[static_cast<size_t>(k)]), bearing(eye, *heard)) <
          apart(bearing(eye, f.watch[static_cast<size_t>(best)]), bearing(eye, *heard)))
        best = k;
    if (best != f.watch_pick) {
      f.watch_pick = best;
      f.watch_until = t + kWatchMax_s;
    }
    return f.watch_pick;
  }
  if (f.watch_pick < 0 || f.watch_pick >= n || t >= f.watch_until) {
    int32_t next = ge::core::int_in(f.rng, 0, n - 1);
    if (n > 1 && next == f.watch_pick) next = (next + 1) % n;
    f.watch_pick = next;
    f.watch_until = t + ge::core::range(f.rng, kWatchMin_s, kWatchMax_s);
  }
  return f.watch_pick;
}

}  // namespace shooters_edge::bots
