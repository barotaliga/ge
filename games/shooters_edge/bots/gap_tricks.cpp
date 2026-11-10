// The maps' gap tricks (gap_tricks.hpp).
//
// ge:summary The maps' gap tricks as rows, and as the nav's
// ge:keywords gap-tricks gap_tricks nav_gap_tricks
// ge:skill bots
// ge:test tests/shooter_nav_gaps_test.cpp
#include "shooters_edge/bots/gap_tricks.hpp"

namespace shooters_edge::bots {

namespace {

// map, from, to, speed, jumps, harm, level: the se-rj lab's gap tricks, most made first.
constexpr GapTrick kTricks[] = {
    {"dm2", {2433, -2008, 121}, {2432, -2279, 121}, 422, 1362, true, 0},
    {"dm2", {2524, -2256, 121}, {2469, -1990, 121}, 430, 1336, true, 0},
    {"dm2", {1837, -404, 185}, {1900, -645, 185}, 387, 1001, false, 0},
    {"dm2", {1779, -1695, 25}, {2102, -1694, 25}, 458, 623, true, 0},
    {"dm2", {1877, -598, 185}, {1817, -380, 185}, 347, 553, false, 0},
    {"dm2", {2185, 90, 25}, {2611, 74, -72}, 448, 313, true, 0},
    {"dm2", {2410, -2256, 121}, {2442, -1985, 120}, 419, 263, true, 0},
    {"dm2", {2547, -2255, 121}, {2585, -1983, 121}, 434, 221, true, 0},
    {"dm2", {2504, -2011, 121}, {2563, -2270, 121}, 403, 128, true, 0},
    {"dm2", {2644, -2209, 153}, {2387, -1996, 121}, 496, 94, true, 1},
    {"dm2", {1546, -621, 185}, {1370, -477, 184}, 369, 90, false, 0},
    {"dm2", {1550, -570, 185}, {1834, -570, 184}, 440, 88, false, 1},
    {"dm2", {2095, -1696, 25}, {1787, -1697, 25}, 437, 60, true, 1},
    {"dm2", {1427, -504, 185}, {1528, -694, 184}, 373, 58, false, 0},
    {"dm2", {2188, 90, 25}, {2529, 273, -68}, 442, 37, true, 0},
    {"dm2", {2435, -2254, 121}, {2332, -1988, 121}, 437, 20, true, 0},
    {"dm2", {2642, -2203, 153}, {2417, -2446, 123}, 504, 19, true, 1},
    {"dm2", {1908, -1032, 344}, {1485, -1291, 98}, 438, 19, false, 0},
    {"dm2", {2173, 78, 25}, {2370, 275, -11}, 431, 19, true, 0},
    {"dm2", {1928, -1021, 345}, {1615, -831, 184}, 404, 15, false, 1},
    {"dm2", {1551, -572, 185}, {1943, -517, 101}, 446, 14, false, 1},
    {"dm2", {2330, -2011, 121}, {2406, -2271, 121}, 403, 13, true, 0},
    {"dm2", {1536, -609, 185}, {1324, -549, 184}, 405, 13, false, 0},
};

}  // namespace

std::span<const GapTrick> gap_tricks(std::string_view name) {
  size_t first = 0, n = 0;
  for (size_t i = 0; i < std::size(kTricks); i++) {
    if (name != kTricks[i].map) continue;
    if (n == 0) first = i;
    n++;
  }
  return {kTricks + first, n};
}

std::vector<ge::shooter::NavGapTrick> nav_gap_tricks(std::span<const GapTrick> tricks) {
  std::vector<ge::shooter::NavGapTrick> out;
  out.reserve(tricks.size());
  for (const GapTrick& t : tricks) {
    ge::shooter::NavGapTrick n;
    n.from = t.from;
    n.to = t.to;
    n.speed = t.speed;
    n.level = t.level;
    out.push_back(n);
  }
  return out;
}

}  // namespace shooters_edge::bots
