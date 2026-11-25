// The harm the nav graph knows: the nodes over lava or slime the map read marks, the links whose
// line passes over such a drop (`brink`), and those with harm a stride to either side
// (`brink_side`), so a bot keeps its footing near them.
//
// ge:summary The nav graph's brinks: links over or beside lava and slime, marked from the harm
// ge:keywords nav brink brink-side harm lava slime nav_mark_harm nav_mark_side_harm nav_harm_near
// ge:keywords nav_falls_in_harm
// ge:skill shooterbots bots
// ge:test tests/shooterbots_brink_test.cpp
#include <span>

#include "core/math.hpp"
#include "shooterbots/nav.hpp"

namespace ge::shooterbots {

namespace {

constexpr float kStepHeight = 18;  // the move climbs a step this high without a jump

float across(Vec3f a, Vec3f b) {
  const float dx = b.x - a.x, dy = b.y - a.y;
  return root_of(dx * dx + dy * dy);
}

}  // namespace

bool nav_harm_near(const NavGraph& g, Vec3f p, float reach) {
  if (g.harm.size() != g.nodes.size() || g.set.spacing <= 0 || g.cols == 0) return false;
  const int32_t ci = static_cast<int32_t>((p.x - g.lo.x) / g.set.spacing);
  const int32_t cj = static_cast<int32_t>((p.y - g.lo.y) / g.set.spacing);
  const int32_t span = static_cast<int32_t>(reach / g.set.spacing) + 1;
  for (int32_t i = ci - span; i <= ci + span; i++) {
    if (i < 0 || i >= g.cols) continue;
    for (int32_t j = cj - span; j <= cj + span; j++) {
      if (j < 0 || j >= g.rows) continue;
      const size_t c =
          static_cast<size_t>(i) * static_cast<size_t>(g.rows) + static_cast<size_t>(j);
      for (int32_t k = 0; k < g.col_n[c]; k++) {
        const auto n = static_cast<size_t>(g.col_first[c]) + static_cast<size_t>(k);
        if (!g.harm[n]) continue;
        const Vec3f o = g.nodes[n].origin;
        if (o.z < p.z - kStepHeight && across(o, p) <= reach) return true;
      }
    }
  }
  return false;
}

int64_t nav_mark_harm(NavGraph& g, std::span<const uint8_t> harm, float reach) {
  if (harm.size() != g.nodes.size()) return 0;
  g.harm.assign(harm.begin(), harm.end());
  int64_t marked = 0;
  for (size_t a = 0; a < g.nodes.size(); a++) {
    const NavNode& n = g.nodes[a];
    for (int32_t k = 0; k < n.links; k++) {
      NavLink& l = g.links[static_cast<size_t>(n.first_link) + static_cast<size_t>(k)];
      if ((l.kind != NavKind::walk && l.kind != NavKind::drop) || harm[a] ||
          harm[static_cast<size_t>(l.to)])
        continue;
      // Along the link every 16 units, at the higher end's height (a drop's lip is its start).
      const Vec3f A = n.origin, B = g.nodes[static_cast<size_t>(l.to)].origin;
      const float len = across(A, B);
      const int steps = 1 + static_cast<int>(len / 16);
      for (int q = 0; q <= steps && !l.brink; q++) {
        const float f = static_cast<float>(q) / static_cast<float>(steps);
        const Vec3f p{A.x + (B.x - A.x) * f, A.y + (B.y - A.y) * f, A.z > B.z ? A.z : B.z};
        l.brink = nav_harm_near(g, p, reach);
      }
      if (l.brink) marked++;
    }
  }
  return marked;
}

namespace {

// Whether the player's box swept from `p` to `to` (stopped by a wall; a stair's step climbed: the
// sweep tried a step and two higher too, the furthest kept), then dropped to the floor (400 units
// at most), stands in lava or slime.
bool side_falls_in_(const MoveWorld& w, Vec3f p, Vec3f to) {
  HullTrace go;
  go.fraction = -1;
  for (const float up : {0.f, kStepHeight, 2 * kStepHeight}) {
    const HullTrace t = player_trace(w, {p.x, p.y, p.z + up}, {to.x, to.y, p.z + up});
    if (!t.startsolid && !t.allsolid && t.fraction > go.fraction + 0.01f) go = t;
    if (go.fraction >= 1) break;
  }
  if (go.fraction < 0) return false;
  const Vec3f e = go.endpos;
  const HullTrace down = player_trace(w, e, {e.x, e.y, e.z - 400});
  if (down.fraction >= 1 || down.startsolid) return false;
  const int c =
      water_contents(w, {down.endpos.x, down.endpos.y, down.endpos.z + kPlayerMins.z + 2});
  return c == kContentsLava || c == kContentsSlime;
}

}  // namespace

bool nav_falls_in_harm(const MoveWorld& world, Vec3f p, Vec3f to) {
  return side_falls_in_(world, p, to);
}

int64_t nav_mark_side_harm(NavGraph& g, const MoveWorld& world, float side, float near) {
  if (g.harm.size() != g.nodes.size()) return 0;
  int64_t marked = 0;
  for (size_t a = 0; a < g.nodes.size(); a++) {
    const NavNode& n = g.nodes[a];
    if (g.harm[a] || !nav_harm_near(g, n.origin, near)) continue;
    for (int32_t k = 0; k < n.links; k++) {
      NavLink& l = g.links[static_cast<size_t>(n.first_link) + static_cast<size_t>(k)];
      if ((l.kind != NavKind::walk && l.kind != NavKind::drop) || g.harm[static_cast<size_t>(l.to)])
        continue;
      const Vec3f A = n.origin, B = g.nodes[static_cast<size_t>(l.to)].origin;
      const float len = across(A, B);
      if (len < 1) continue;
      // Across the link: its right hand, flat.
      const float rx = (B.y - A.y) / len, ry = -(B.x - A.x) / len;
      const int steps = 1 + static_cast<int>(len / 16);
      for (int q = 0; q <= steps && !l.brink_side; q++) {
        const float f = static_cast<float>(q) / static_cast<float>(steps);
        // A step over the line's height: over a ramp's rise and a stair's tread.
        const Vec3f p{A.x + (B.x - A.x) * f, A.y + (B.y - A.y) * f,
                      A.z + (B.z - A.z) * f + kStepHeight};
        for (const float s : {side, -side})
          if (!l.brink_side && side_falls_in_(world, p, {p.x + rx * s, p.y + ry * s, p.z}))
            l.brink_side = true;
      }
      if (l.brink_side) marked++;
    }
  }
  return marked;
}

}  // namespace ge::shooterbots
