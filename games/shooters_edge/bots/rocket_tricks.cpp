// The maps' rocket-jump tricks (rocket_tricks.hpp).
//
// ge:summary The maps' rocket-jump tricks as rows, and as the nav's
// ge:keywords rocket-tricks tricks rocket_tricks nav_tricks
// ge:skill bots
// ge:test tests/shooters_edge_team_test.cpp
#include "shooters_edge/bots/rocket_tricks.hpp"

namespace shooters_edge::bots {

namespace {

// map, from, to, pitch, run, jumps, flip, level: the se-rj lab's tricks, most made first.
constexpr RocketTrick kTricks[] = {
    {"dm2", {2000, -450, 115}, {1922, -704, 185}, 47, 374, 24, true, 0},
    {"dm2", {2255, -188, 31}, {2612, -369, 121}, 44, 479, 24, true, 2},
    {"dm2", {2500, -1237, 36}, {2604, -685, 121}, 47, 511, 21, true, 1},
    {"dm2", {1970, -515, 121}, {1871, -642, 184}, 56, 334, 17, true, 1},
    {"dm2", {2186, -2053, 99}, {2199, -2431, 201}, 50, 381, 17, true, 2},
    {"dm2", {1953, -506, 112}, {1404, -526, 185}, 43, 488, 12, true, 2},
    {"dm2", {1701, -546, 44}, {1910, -656, 184}, 80, 392, 10, true, 1},
    {"dm2", {2219, -1707, 30}, {1688, -1521, 25}, 36, 441, 7, true, 2},
    {"dm2", {1953, -509, 109}, {1505, -619, 184}, 41, 470, 6, true, 0},
    {"dm2", {1771, -1707, 30}, {2306, -1694, 24}, 29, 443, 6, true, 2},
    {"dm2", {2172, -1683, 31}, {1669, -1574, 24}, 33, 500, 5, true, 0},
    {"dm2", {2242, -186, 33}, {2594, -259, 121}, 36, 470, 5, true, 2},
    {"dm2", {1968, -529, 140}, {1960, -699, 184}, 65, 303, 4, true, 0},
    {"dm2", {1901, -603, 213}, {1930, -743, 185}, 44, 444, 4, true, 1},
    {"dm2", {1689, -549, 46}, {1317, -465, 185}, 80, 400, 4, false, 1},
    {"dm2", {2358, -722, 64}, {2403, -844, 99}, 45, 349, 3, true, 1},
    {"dm2", {1971, -495, 120}, {1814, -548, 35}, 55, 408, 3, true, 0},
    {"dm2", {1985, -520, 128}, {1811, -703, 313}, 77, 331, 3, true, 1},
    {"dm2", {2019, -405, 107}, {1913, -593, 185}, 42, 368, 3, false, 1},
    {"dm2", {2160, -2035, 94}, {2187, -2343, 88}, 50, 360, 3, true, 2},
    {"dm2", {2260, -1677, 31}, {1724, -1694, 25}, 27, 376, 3, true, 0},
    {"dm2", {2181, -2079, 98}, {2370, -2469, 142}, 49, 382, 3, true, 0},
    {"dm3", {1132, 711, -265}, {1133, 663, 81}, 70, 556, 3, true, 1},
    {"dm3", {430, -249, 61}, {806, 158, 57}, 31, 398, 3, true, 0},
    {"e1m2", {771, 674, 225}, {685, 629, 223}, 20, 318, 3, true, 0},
};

}  // namespace

std::span<const RocketTrick> rocket_tricks(std::string_view name) {
  size_t first = 0, n = 0;
  for (size_t i = 0; i < std::size(kTricks); i++) {
    if (name != kTricks[i].map) continue;
    if (n == 0) first = i;
    n++;
  }
  return {kTricks + first, n};
}

std::vector<ge::shooter::NavTrick> nav_tricks(std::span<const RocketTrick> tricks) {
  std::vector<ge::shooter::NavTrick> out;
  out.reserve(tricks.size());
  for (const RocketTrick& t : tricks) {
    ge::shooter::NavTrick n;
    n.from = t.from;
    n.to = t.to;
    n.back = t.flip;
    n.pitch = t.pitch;
    n.run = t.run;
    n.level = t.level;
    out.push_back(n);
  }
  return out;
}

}  // namespace shooters_edge::bots
