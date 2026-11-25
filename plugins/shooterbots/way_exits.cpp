// The way through's exits kept shut (way_exits.hpp).
//
// ge:summary Exits shut to his routes: links into a level change's box until it is the step
// ge:keywords way_exit_links way_shut_exits exit changelevel trigger crossing early-exit route
// ge:skill shooterbots bots
// ge:test tests/shooters_edge_companion_test.cpp
#include "shooterbots/way_exits.hpp"

#include <algorithm>
#include <cstddef>

namespace ge::shooterbots {

namespace sh = ge::shooter;

namespace {

// A standing player's box about his origin: 16 each way across, 24 below, 32 above.
constexpr float kAcross = 16, kBelow = 24, kAbove = 32;

// Whether origin `p` puts a standing player's box into `lo`..`hi`.
bool in_box(sh::Vec3f p, sh::Vec3f lo, sh::Vec3f hi) {
  return p.x + kAcross > lo.x && p.x - kAcross < hi.x && p.y + kAcross > lo.y &&
         p.y - kAcross < hi.y && p.z + kAbove > lo.z && p.z - kBelow < hi.z;
}

// Whether a player's box carried from `a` to `b` in a straight line passes into `lo`..`hi` (the
// box grown by his and tested against the line: the slabs, axis by axis).
bool sweeps(sh::Vec3f a, sh::Vec3f b, sh::Vec3f lo, sh::Vec3f hi) {
  const float glo[3] = {lo.x - kAcross, lo.y - kAcross, lo.z - kAbove};
  const float ghi[3] = {hi.x + kAcross, hi.y + kAcross, hi.z + kBelow};
  const float pa[3] = {a.x, a.y, a.z}, pb[3] = {b.x, b.y, b.z};
  float t0 = 0, t1 = 1;
  for (int k = 0; k < 3; k++) {
    const float d = pb[k] - pa[k];
    if (d == 0) {
      if (pa[k] <= glo[k] || pa[k] >= ghi[k]) return false;
      continue;
    }
    float ta = (glo[k] - pa[k]) / d, tb = (ghi[k] - pa[k]) / d;
    if (ta > tb) std::swap(ta, tb);
    t0 = std::max(t0, ta);
    t1 = std::min(t1, tb);
    if (t0 >= t1) return false;
  }
  return true;
}

}  // namespace

void way_exit_links(WayThrough& w, const NavGraph& g) {
  w.exit_links.clear();
  for (size_t k = 0; k < w.steps.size(); k++) {
    const WayStep& st = w.steps[k];
    if (st.kind != WayKind::exit) continue;
    for (size_t a = 0; a < g.nodes.size(); a++) {
      const NavNode& n = g.nodes[a];
      if (in_box(n.origin, st.lo, st.hi)) continue;  // out of the box is no way in
      for (int32_t j = 0; j < n.links; j++) {
        const size_t li = static_cast<size_t>(n.first_link) + static_cast<size_t>(j);
        const sh::Vec3f to = g.nodes[static_cast<size_t>(g.links[li].to)].origin;
        if (sweeps(n.origin, to, st.lo, st.hi))
          w.exit_links.push_back(
              {static_cast<int32_t>(li), static_cast<int32_t>(a), static_cast<int32_t>(k)});
      }
    }
  }
}

void way_shut_exits(const WayThrough& w, const NavGraph& g, std::vector<NavExtraLink>& shut) {
  for (const WayThrough::ExitLink& x : w.exit_links) {
    if (x.step == w.step) continue;  // the exit he is on is walked into
    const NavLink& l = g.links[static_cast<size_t>(x.link)];
    shut.push_back({x.from, l.to, l.kind, l.time_s});
  }
}

}  // namespace ge::shooterbots
