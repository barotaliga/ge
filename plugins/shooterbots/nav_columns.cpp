// The nav's floor columns (nav_columns.hpp): the floors down a grid column, the pockets beside it
// and the pockets merged into the grid's floors.
//
// ge:summary The nav's floor columns: the floors down a grid column and the pockets beside it
// ge:keywords nav column_floors floor-column pocket escape-pod slot nav_merge_pockets
// ge:skill shooterbots bots
// ge:test tests/shooterbots_nav_test.cpp
#include "shooterbots/nav_columns.hpp"

#include <algorithm>
#include <cmath>
#include <span>

#include "shooter/brush_hull.hpp"
#include "shooter/world.hpp"

namespace ge::shooterbots {

using ge::shooter::box_free;
using ge::shooter::HullTrace;
using ge::shooter::kPlayerMaxs;
using ge::shooter::kPlayerMins;
using ge::shooter::MoveWorld;
using ge::shooter::player_trace;
using ge::shooter::Vec3f;

constexpr float kFloorNormal = kNavFloorNormal;

namespace {

constexpr float kPocketDirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};

}  // namespace

// Whether a player's box passes from `a` to `b` (both standing, a step up allowed): the same open
// floor, no wall between.
bool open_between(const MoveWorld& world, Vec3f a, Vec3f b) {
  const float up = std::max(a.z, b.z) + 1;
  const HullTrace tr = player_trace(world, {a.x, a.y, up}, {b.x, b.y, up});
  return !tr.startsolid && tr.fraction >= 1;
}

// The floors down one grid column at (x, y), top first; with `pockets`, the floors of the pockets
// beside it too (`kPocketSlide`), kept apart for the caller to weigh against its neighbours'.
void column_floors(const MoveWorld& world, float x, float y, float top, float bottom,
                   std::vector<Vec3f>& out, std::vector<Vec3f>* pockets) {
  out.clear();
  if (pockets != nullptr) pockets->clear();
  float z = top;
  float slide_below = top + 1;  // no slide again above a pocket's floor already found
  while (z > bottom) {
    Vec3f start{x, y, z};
    if (!box_free(world, start)) {
      // A POCKET (`kPocketSlide`): the box free a slide off the column, not on it.
      bool slid = false;
      Vec3f past{};  // on along the slide, where it was needed: a wall before it makes a slot
      for (const float d : pockets != nullptr && z < slide_below
                               ? std::span<const float>(kPocketSlide)
                               : std::span<const float>{})
        for (const auto& dir : kPocketDirs)
          if (!slid && box_free(world, {x + dir[0] * d, y + dir[1] * d, z})) {
            start = {x + dir[0] * d, y + dir[1] * d, z};
            past = {start.x + dir[0] * kPocketPast, start.y + dir[1] * kPocketPast, z};
            slid = true;
          }
      if (!slid) {
        z -= kScanStep;
        continue;
      }
      // The pocket's floor is kept; the column's own scan goes on down as it would have.
      const HullTrace tr = player_trace(world, start, {start.x, start.y, bottom});
      if (!tr.startsolid && tr.fraction < 1 && tr.plane.normal.z >= kFloorNormal &&
          player_trace(world, start, past).fraction < 1)
        pockets->push_back(tr.endpos);
      slide_below = tr.startsolid || tr.fraction == 1 ? bottom : tr.endpos.z;
      z -= kScanStep;
      continue;
    }
    const HullTrace tr = player_trace(world, start, {x, y, bottom});
    if (tr.startsolid || tr.fraction == 1) break;
    if (tr.plane.normal.z >= kFloorNormal) out.push_back(tr.endpos);
    // Below this floor: past the player's box under it, then scan for free space again.
    z = tr.endpos.z - (kPlayerMaxs.z - kPlayerMins.z) - 1;
  }
}

void nav_merge_pockets(const MoveWorld& world, int32_t cols, int32_t rows,
                       const std::vector<std::pair<size_t, Vec3f>>& pockets,
                       std::vector<std::vector<Vec3f>>& col_floors) {
  auto col_of = [&](int32_t i, int32_t j) {
    return static_cast<size_t>(i) * static_cast<size_t>(rows) + static_cast<size_t>(j);
  };
  for (const auto& [c, p] : pockets) {
    const int32_t ci = static_cast<int32_t>(c / static_cast<size_t>(rows));
    const int32_t cj = static_cast<int32_t>(c % static_cast<size_t>(rows));
    bool known = false;
    for (int32_t di = -1; di <= 1 && !known; di++)
      for (int32_t dj = -1; dj <= 1 && !known; dj++) {
        const int32_t ni = ci + di, nj = cj + dj;
        if (ni < 0 || nj < 0 || ni >= cols || nj >= rows) continue;
        for (const Vec3f f : col_floors[col_of(ni, nj)])
          known = known || (std::fabs(f.z - p.z) < 8 && open_between(world, f, p));
      }
    if (known) continue;
    std::vector<Vec3f>& at = col_floors[c];
    at.push_back(p);
    std::sort(at.begin(), at.end(), [](Vec3f a, Vec3f b) { return a.z > b.z; });
  }
}

}  // namespace ge::shooterbots
