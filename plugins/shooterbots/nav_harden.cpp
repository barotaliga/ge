// Jumps over harm hardened (`nav_harden_jumps`): each jump link over or beside lava or slime flown
// again past its target and slower, removed or priced when a flight that misses would land in
// it; a jump off a moving platform flown with the platform standing at its stop.
//
// ge:summary The nav graph's jumps over harm re-proven past the target and slower, priced or cut
// ge:keywords nav harden jump overshoot harm lava slime nav_harden_jumps moving-platform stop
// ge:skill shooterbots bots
#include <cmath>
#include <span>

#include "core/math.hpp"
#include "shooter/world.hpp"
#include "shooterbots/nav.hpp"

namespace ge::shooterbots {

namespace {

constexpr float kStepHeight = 18;  // the move climbs a step this high without a jump

float across(Vec3f a, Vec3f b) {
  const float dx = b.x - a.x, dy = b.y - a.y;
  return root_of(dx * dx + dy * dy);
}

// The floor nodes within `far` across of `p` (the columns round it), by the grid.
template <class F>
void floor_near(const NavGraph& g, Vec3f p, float far, F&& each) {
  const auto span = static_cast<int32_t>(std::ceil(far / g.set.spacing));
  const auto ci = static_cast<int32_t>((p.x - g.lo.x) / g.set.spacing);
  const auto cj = static_cast<int32_t>((p.y - g.lo.y) / g.set.spacing);
  for (int32_t i = ci - span; i <= ci + span; i++) {
    if (i < 0 || i >= g.cols) continue;
    for (int32_t j = cj - span; j <= cj + span; j++) {
      if (j < 0 || j >= g.rows) continue;
      const size_t c =
          static_cast<size_t>(i) * static_cast<size_t>(g.rows) + static_cast<size_t>(j);
      for (int32_t k = 0; k < g.col_n[c]; k++) each(g.col_first[c] + k);
    }
  }
}

}  // namespace

NavHardenCounts nav_harden_jumps(const MoveWorld& world, const MoveSettings& move, NavGraph& g,
                                 std::span<const uint8_t> harm, float slow, float risk_s,
                                 float beside, float overshoot, std::span<const NavMover> movers) {
  NavHardenCounts out;
  if (harm.size() != g.nodes.size()) return out;
  // Whether a harm node stands in a grid column under the line from `a` to `b`, below both.
  const auto over_harm = [&](Vec3f a, Vec3f b) {
    const float len = across(a, b);
    const int steps = static_cast<int>(len / (g.set.spacing * 0.5f)) + 1;
    const float low = (a.z < b.z ? a.z : b.z) - kStepHeight;
    for (int i = 1; i < steps; i++) {
      const Vec3f p = a + (b - a) * (static_cast<float>(i) / static_cast<float>(steps));
      const auto ci = static_cast<int32_t>((p.x - g.lo.x) / g.set.spacing);
      const auto cj = static_cast<int32_t>((p.y - g.lo.y) / g.set.spacing);
      if (ci < 0 || ci >= g.cols || cj < 0 || cj >= g.rows) continue;
      const size_t c =
          static_cast<size_t>(ci) * static_cast<size_t>(g.rows) + static_cast<size_t>(cj);
      for (int32_t k = 0; k < g.col_n[c]; k++) {
        const int32_t n = g.col_first[c] + k;
        if (harm[static_cast<size_t>(n)] && g.nodes[static_cast<size_t>(n)].origin.z < low)
          return true;
      }
    }
    return false;
  };
  // Whether a harm node lower than `p` stands within `beside` across of it (a landing at an edge
  // over it: the momentum carries a jumper on past where the proof stood still).
  const auto harm_beside = [&](Vec3f p) {
    bool found = false;
    floor_near(g, p, beside, [&](int32_t m) {
      const Vec3f o = g.nodes[static_cast<size_t>(m)].origin;
      found = found ||
              (harm[static_cast<size_t>(m)] && o.z < p.z - kStepHeight && across(o, p) <= beside);
    });
    return found;
  };
  std::vector<NavLink> links;
  links.reserve(g.links.size());
  for (size_t a = 0; a < g.nodes.size(); a++) {
    NavNode& n = g.nodes[a];
    const int32_t first = static_cast<int32_t>(links.size());
    for (int32_t k = 0; k < n.links; k++) {
      NavLink l = g.links[static_cast<size_t>(n.first_link) + static_cast<size_t>(k)];
      // An updraft's ride is the push's, not the jump's: nothing to overshoot.
      const bool leap = (l.kind == NavKind::jump && !l.updraft) || nav_blast(l.kind);
      const Vec3f to = g.nodes[static_cast<size_t>(l.to)].origin;
      if (leap && (over_harm(n.origin, to) || harm_beside(to))) {
        out.over++;
        // Off a moving platform: flown with it where it stops (boss1's ferries to the buttons'
        // ledges over lava: flown without it, every jump off one fell in).
        MoveWorld there_world;
        const MoveWorld* on = &world;
        for (const NavMover& m : movers)
          if (l.mover >= 0 && m.ent == l.mover && l.stop >= 0 &&
              static_cast<size_t>(l.stop) < m.stops.size() && world.count < kMaxSolids) {
            there_world = world;
            there_world.solids[there_world.count++] =
                mover_solid(m, m.stops[static_cast<size_t>(l.stop)]);
            on = &there_world;
          }
        // Flown again at a point `overshoot` past the target: a flight that ends in the harm, or
        // falls well below the target, lands at an edge over it.
        Vec3f dir{to.x - n.origin.x, to.y - n.origin.y, 0};
        normalize(dir);
        const NavProof past =
            prove_link(*on, move, n.origin, to + dir * overshoot,
                       l.kind == NavKind::jump ? NavKind::jump : NavKind::walk, l.speed, g.set);
        out.proofs++;
        const int c = water_contents(*on, {past.end.x, past.end.y, past.end.z - 22});
        if (l.kind == NavKind::jump &&
            (c == kContentsLava || c == kContentsSlime || past.end.z < to.z - 2 * kStepHeight)) {
          out.removed++;
          continue;
        }
        if (l.kind == NavKind::jump && l.speed > 0) {
          NavProof pr;
          if (slow > 0) {
            out.proofs++;
            pr = prove_link(*on, move, n.origin, to, NavKind::jump, l.speed * slow, g.set);
          }
          if (!pr.reached) {
            out.removed++;
            continue;
          }
        }
        l.time_s += risk_s;
      }
      links.push_back(l);
    }
    n.first_link = first;
    n.links = static_cast<int32_t>(links.size()) - first;
  }
  g.links = std::move(links);
  return out;
}

}  // namespace ge::shooterbots
