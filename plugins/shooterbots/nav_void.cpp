// The void under a map's player hull: a map whose player hull was compiled unsealed leaves the
// space outside its rooms open to the player's box while the point hull (what is drawn) is rock
// there. A floor the nav finds in that space is a ledge inside the rock; a bot sent to it walks
// into the wall and falls out of the world (Dissolution of Eternity's r1m4, west of the hall). A
// pocket the point hull fills but the player's box fits is not always the void: a small closed one
// is part of the map (e1m2's under its plat), so a floor is the void only when its buried space
// runs on to the edge of the world or further than any pocket.
//
// ge:summary The nav's void test: a floor buried in the map's rock whose open player hull runs on
// ge:summary to the world's edge, kept out of the graph
// ge:keywords nav void unsealed-hull buried-floor player-hull point-hull nav_floor_in_void
// ge:skill shooterbots bots
// ge:test tests/shooterbots_nav_test.cpp
#include <array>
#include <cstdint>
#include <deque>

#include "shooterbots/nav.hpp"

namespace ge::shooterbots {

namespace {

namespace sh = ge::shooter;

constexpr float kVoidCell = 16;         // the flood's grid, units (a player hull's wall is thicker)
constexpr size_t kPocketCells = 20000;  // a closed pocket holds fewer cells than this

bool rock(const sh::Hull& h, Vec3f p) { return sh::point_contents(h, p) == sh::kContentsSolid; }

// Whether the player's box at `p` lies wholly in the point hull's rock (its corners, edges' middles
// and centre at the feet, the middle and the head).
bool buried(const sh::Hull& point, Vec3f p) {
  for (const float dx : {-15.0f, 0.0f, 15.0f})
    for (const float dy : {-15.0f, 0.0f, 15.0f})
      for (const float dz : {-23.0f, 0.0f, 31.0f})
        if (!rock(point, {p.x + dx, p.y + dy, p.z + dz})) return false;
  return true;
}

}  // namespace

bool nav_floor_in_void(const MoveWorld& world, Vec3f p, NavVoid& cache) {
  const Solid& w = world.solids[0];
  if (world.count < 1 || w.point_hull.nodes == nullptr || w.clip_hull.nodes == nullptr)
    return false;
  if (!buried(w.point_hull, p)) return false;
  const Vec3f lo = w.mins, hi = w.maxs;
  const auto nx = static_cast<int64_t>((hi.x - lo.x) / kVoidCell) + 1;
  const auto ny = static_cast<int64_t>((hi.y - lo.y) / kVoidCell) + 1;
  const auto nz = static_cast<int64_t>((hi.z - lo.z) / kVoidCell) + 1;
  if (cache.cell.size() != static_cast<size_t>(nx * ny * nz))
    cache.cell.assign(static_cast<size_t>(nx * ny * nz), 0);
  auto at = [&](int64_t i, int64_t j, int64_t k) {
    return static_cast<size_t>((i * ny + j) * nz + k);
  };
  auto centre = [&](int64_t i, int64_t j, int64_t k) {
    return Vec3f{lo.x + static_cast<float>(i) * kVoidCell, lo.y + static_cast<float>(j) * kVoidCell,
                 lo.z + static_cast<float>(k) * kVoidCell};
  };
  // The cell under the floor's origin, or the first open one beside it.
  const int64_t ci = static_cast<int64_t>((p.x - lo.x) / kVoidCell);
  const int64_t cj = static_cast<int64_t>((p.y - lo.y) / kVoidCell);
  const int64_t ck = static_cast<int64_t>((p.z - lo.z) / kVoidCell);
  std::array<int64_t, 3> seed{-1, -1, -1};
  for (int64_t di = 0; di <= 1 && seed[0] < 0; di++)
    for (int64_t dj = 0; dj <= 1 && seed[0] < 0; dj++)
      for (int64_t dk = 0; dk <= 1 && seed[0] < 0; dk++) {
        const int64_t i = ci + di, j = cj + dj, k = ck + dk;
        if (i < 0 || j < 0 || k < 0 || i >= nx || j >= ny || k >= nz) continue;
        const Vec3f c = centre(i, j, k);
        if (!rock(w.clip_hull, c) && rock(w.point_hull, c)) seed = {i, j, k};
      }
  if (seed[0] < 0) return false;
  if (const uint8_t known = cache.cell[at(seed[0], seed[1], seed[2])]; known != 0)
    return known == 2;
  // Flood the player hull's open cells that lie in the point hull's rock; reaching the world's edge
  // or outgrowing a pocket is the void. Every cell it reached is then known either way.
  std::deque<std::array<int64_t, 3>> todo{seed};
  std::vector<size_t> reached{at(seed[0], seed[1], seed[2])};
  cache.cell[reached.back()] = 3;  // in this flood
  bool open_void = false;
  constexpr int64_t kStep[6][3] = {{1, 0, 0},  {-1, 0, 0}, {0, 1, 0},
                                   {0, -1, 0}, {0, 0, 1},  {0, 0, -1}};
  while (!todo.empty() && !open_void) {
    const std::array<int64_t, 3> c = todo.front();
    todo.pop_front();
    for (const auto& s : kStep) {
      const int64_t i = c[0] + s[0], j = c[1] + s[1], k = c[2] + s[2];
      if (i < 0 || j < 0 || k < 0 || i >= nx || j >= ny || k >= nz) {
        open_void = true;
        break;
      }
      uint8_t& mark = cache.cell[at(i, j, k)];
      if (mark == 3) continue;
      if (mark != 0) {  // a flood before this one reached it
        open_void = mark == 2;
        if (open_void) break;
        continue;
      }
      const Vec3f n = centre(i, j, k);
      if (rock(w.clip_hull, n) || !rock(w.point_hull, n)) continue;
      mark = 3;
      reached.push_back(at(i, j, k));
      todo.push_back({i, j, k});
    }
    if (reached.size() > kPocketCells) open_void = true;
  }
  for (const size_t r : reached) cache.cell[r] = open_void ? 2 : 1;
  return open_void;
}

}  // namespace ge::shooterbots
