// THE FLOORS DOWN A GRID COLUMN for the nav's build (nav.cpp's `build_nav`): the player's box
// lowered down the column at (x, y), each floor it comes to rest on, and the pockets beside it: a
// floor between walls narrower than the box plus the column's offset off its middle (boss2's
// escape pods, 44 units across, the columns 16 off the middle) gets no node from the column,
// whose box fits nowhere on it. The box slid 8 or 16 off the column fits; one that meets a wall
// again within `kPocketPast` on along the slide stands in a slot, a pocket (one on a sill or a
// ledge, open on past it, does not). A pocket's floor is kept only where no neighbouring column's
// floor is open to it (`nav_merge_pockets`).
//
// ge:summary The nav's floor columns: the floors down a grid column and the pockets beside it
// ge:keywords nav column_floors floor-column pocket escape-pod slot nav_merge_pockets boss2
// ge:skill shooterbots bots
// ge:test tests/shooterbots_nav_test.cpp
#pragma once

#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

#include "shooterbots/nav.hpp"

namespace ge::shooterbots {

constexpr float kNavFloorNormal = 0.7f;  // a plane this upright is floor a player stands on
constexpr float kScanStep = 8;           // the column's search for free space below a floor, units
constexpr float kPocketSlide[2] = {8, 16};  // the slides tried, units
constexpr float kPocketPast = 32;           // a slot's wall this near on along the slide, units

// Whether a player's box passes from `a` to `b` (both standing, a step up allowed): the same open
// floor, no wall between.
bool open_between(const ge::shooter::MoveWorld& world, ge::shooter::Vec3f a, ge::shooter::Vec3f b);

// The floors down one grid column at (x, y), from `top` to `bottom`, top first; with `pockets`,
// the floors of the pockets beside it too, kept apart for the caller to weigh against its
// neighbours' (`nav_merge_pockets`).
void column_floors(const ge::shooter::MoveWorld& world, float x, float y, float top, float bottom,
                   std::vector<ge::shooter::Vec3f>& out,
                   std::vector<ge::shooter::Vec3f>* pockets = nullptr);

// Each pocket (its column's index, column-major over `rows`, and its floor) into its column's
// floors, top first, where no floor of the column or its eight neighbours (nor an earlier pocket)
// stands open to it at its height: a space the grid misses whole. A strip along a wall, which the
// next column's floor reaches, is that floor.
void nav_merge_pockets(const ge::shooter::MoveWorld& world, int32_t cols, int32_t rows,
                       const std::vector<std::pair<size_t, ge::shooter::Vec3f>>& pockets,
                       std::vector<std::vector<ge::shooter::Vec3f>>& col_floors);

}  // namespace ge::shooterbots
