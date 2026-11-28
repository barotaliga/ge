// The nav's SQUEEZES: a corridor whose room for the player's box is narrower than the grid's
// spacing can lie wholly between two rows of columns, and the nav then has no node in it: the room
// it leads from is sealed on the graph (Scourge of Armagon's hip2m3, whose start arena's ways out
// leave the box about 20 units of room, between the columns 32 apart). From each floor, a
// neighbouring column with no floor near its height, the box in the rock there at that height, is
// sampled again a little to either side; a level floor found there, with no node near it, becomes
// a node in the column that holds it, and is itself looked past in turn, so a long narrow way is
// followed to its end.
//
// ge:summary The nav's squeeze nodes: floors in a corridor narrower than the grid, found off the
// ge:summary columns beside a node and followed along the way
// ge:keywords nav squeeze narrow-corridor between-columns grid-aliasing nav_add_squeezes
// ge:skill shooterbots bots
// ge:test tests/shooterbots_nav_test.cpp
#include <cstdint>
#include <deque>
#include <vector>

#include "shooterbots/nav.hpp"

namespace ge::shooterbots {

namespace {

constexpr float kRise = 18;           // a squeeze's floor lies within this of the node's, units
constexpr float kFloorNormal = 0.7f;  // a floor's plane is at least this level
// The sideways samples, units off the neighbouring column's line, nearest first.
constexpr float kSide[] = {0, 8, -8, 16, -16, 24, -24};
constexpr float kSpan = 32;  // the room's middle is looked for this far to either side, units

}  // namespace

int64_t nav_add_squeezes(const MoveWorld& world, const NavGraph& g,
                         std::vector<std::vector<Vec3f>>& col_floors) {
  const float sp = g.set.spacing;
  auto col_at = [&](Vec3f p, int32_t& i, int32_t& j) {
    if (p.x < g.lo.x || p.y < g.lo.y) return false;
    i = static_cast<int32_t>((p.x - g.lo.x) / sp);
    j = static_cast<int32_t>((p.y - g.lo.y) / sp);
    return i < g.cols && j < g.rows;
  };
  auto col_of = [&](int32_t i, int32_t j) {
    return static_cast<size_t>(i) * static_cast<size_t>(g.rows) + static_cast<size_t>(j);
  };
  // A floor within `rise` of height z and three quarters of a column across of p.
  auto known = [&](Vec3f p, float rise) {
    int32_t i = 0, j = 0;
    if (!col_at(p, i, j)) return true;
    for (int32_t di = -1; di <= 1; di++)
      for (int32_t dj = -1; dj <= 1; dj++) {
        if (i + di < 0 || j + dj < 0 || i + di >= g.cols || j + dj >= g.rows) continue;
        for (const Vec3f& f : col_floors[col_of(i + di, j + dj)]) {
          const float dx = f.x - p.x, dy = f.y - p.y, dz = f.z - p.z;
          if (dz <= rise && -dz <= rise && dx * dx + dy * dy < sp * sp * 0.5625f) return true;
        }
      }
    return false;
  };
  std::deque<Vec3f> todo;
  for (const std::vector<Vec3f>& fl : col_floors)
    for (const Vec3f& f : fl) todo.push_back(f);
  NavVoid unsealed;
  int64_t added = 0;
  constexpr float kDir[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
  while (!todo.empty()) {
    const Vec3f f = todo.front();
    todo.pop_front();
    for (const auto& d : kDir) {
      const Vec3f p{f.x + d[0] * sp, f.y + d[1] * sp, f.z};
      // Only a column in the rock at his height: an open one is a drop's edge or a chasm, whose
      // far lips a node beside the column would only invite a jump onto (hip2m3's lava cleft).
      if (known(p, kRise) || box_free(world, {p.x, p.y, f.z + 1})) continue;
      for (const float side : kSide) {
        // Sideways: across the way from f to p.
        const Vec3f q{p.x - d[1] * side, p.y + d[0] * side, f.z + kRise + 1};
        if (!box_free(world, q)) continue;
        const HullTrace tr = player_trace(world, q, {q.x, q.y, f.z - kRise - 1});
        if (tr.startsolid || tr.fraction == 1 || tr.plane.normal.z < kFloorNormal) continue;
        // The middle of a narrow way, across it, on its floor: a node at the way's edge leaves no
        // room for a door's lip that stands in it once open (hip3m1's secret nook).
        const float ax = -d[1], ay = d[0];
        float lo = 0, hi = 0;
        const Vec3f e{tr.endpos.x, tr.endpos.y, tr.endpos.z + 1};
        while (lo > -kSpan && box_free(world, {e.x + ax * (lo - 2), e.y + ay * (lo - 2), e.z}))
          lo -= 2;
        while (hi < kSpan && box_free(world, {e.x + ax * (hi + 2), e.y + ay * (hi + 2), e.z}))
          hi += 2;
        // A room open past kSpan on either side is no narrow way: the floor found stays.
        const float mid = lo > -kSpan && hi < kSpan ? (lo + hi) * 0.5f : 0.0f;
        const Vec3f m{e.x + ax * mid, e.y + ay * mid, q.z};
        const HullTrace mt = player_trace(world, m, {m.x, m.y, f.z - kRise - 1});
        if (mt.startsolid || mt.fraction == 1 || mt.plane.normal.z < kFloorNormal) continue;
        const Vec3f t = mt.endpos;
        int32_t ti = 0, tj = 0;
        if (!col_at(t, ti, tj) || known(t, kRise)) break;
        if (nav_floor_in_void(world, t, unsealed)) break;
        col_floors[col_of(ti, tj)].push_back(t);
        todo.push_back(t);
        added++;
        break;
      }
    }
  }
  return added;
}

}  // namespace ge::shooterbots
