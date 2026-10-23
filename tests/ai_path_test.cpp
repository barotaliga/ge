// Paths (engine/ai/path.hpp): the heap hands back its keys in order and its ties as a pure
// function of the pushes; the graph search finds the cheapest way on a small graph, skips a node
// popped stale, stops at its goal and reads the way back; the grid search goes round a wall the
// cheapest way, A* with an admissible heuristic finds the same cost as Dijkstra expanding fewer
// cells; and string-pulling keeps the corners a wall needs and drops the rest.
//
// ge:summary Tests: the heap, the graph search, grid A* round a wall, string-pulling
// ge:keywords test ai path heap search dijkstra a-star grid string-pulling wall
// ge:skill bots testing
#include <cmath>
#include <cstdint>
#include <span>
#include <utility>
#include <vector>

#include "ai/path.hpp"
#include "check.hpp"

namespace ai = ge::ai;

TEST(the_heap_hands_back_its_keys_in_order_and_its_ties_the_same_way_every_time) {
  const std::vector<double> keys{5, 1, 4, 1, 3, 9, 2, 6, 5, 3};
  const auto drain = [&] {
    ai::Heap h;
    for (size_t i = 0; i < keys.size(); i++) h.push(static_cast<int32_t>(i), keys[i]);
    std::vector<std::pair<int32_t, double>> out;
    while (!h.empty()) {
      double k = 0;
      const int32_t id = h.pop(k);
      out.emplace_back(id, k);
    }
    return out;
  };
  const auto a = drain();
  CHECK(a.size() == keys.size());
  for (size_t i = 1; i < a.size(); i++) CHECK(a[i - 1].second <= a[i].second);
  CHECK(a == drain());
}

TEST(the_graph_search_finds_the_cheapest_way_stops_at_its_goal_and_reads_it_back) {
  // 0 -> 1 (1), 0 -> 2 (4), 1 -> 2 (1), 2 -> 3 (1), 1 -> 3 (5): cheapest 0 1 2 3 at 3.
  const std::vector<std::vector<std::pair<int32_t, double>>> edges{
      {{1, 1}, {2, 4}}, {{2, 1}, {3, 5}}, {{3, 1}}, {}};
  ai::Search s(edges.size());
  s.seed(0);
  const auto expand = [&](int32_t a) {
    for (const auto& [b, c] : edges[static_cast<size_t>(a)]) {
      const double cost = s.cost(a) + c;
      if (s.improves(b, cost)) s.reach(b, cost, a);
    }
  };
  const int32_t got = s.run(expand, [](int32_t n) { return n == 3; });
  CHECK(got == 3);
  CHECK(s.cost(3) == 3);
  CHECK(s.path_to(3) == (std::vector<int32_t>{0, 1, 2, 3}));
  // Node 2 was pushed at 4 and again at 2: popped once at 2, the stale 4 skipped.
  CHECK(s.expanded() == 3);
  s.reset(edges.size());
  CHECK(!s.reached(3) && s.path_to(3).empty());
}

namespace {

// A 20 x 20 grid of 1 m cells with a wall down column 10 from row 0 to row 16.
constexpr ai::Grid kGrid{20, 20, 1};
bool wall(int32_t k) { return k % 20 == 10 && k / 20 <= 16; }
double step(int32_t, int32_t b, double length) { return wall(b) ? ai::kUnreached : length; }

}  // namespace

TEST(the_grid_search_goes_round_a_wall_and_a_star_matches_dijkstra_with_fewer_cells) {
  ai::Search s;
  const int32_t from = 5 * 20 + 2;
  const int32_t to = 5 * 20 + 18;
  const ai::GridPath dijkstra = ai::grid_path(kGrid, from, to, step, 0, s);
  const ai::GridPath astar = ai::grid_path(kGrid, from, to, step, 1, s);
  CHECK(!astar.cells.empty() && astar.cells.front() == from && astar.cells.back() == to);
  CHECK(std::fabs(astar.cost - dijkstra.cost) < 1e-9);
  CHECK(astar.expanded < dijkstra.expanded);
  bool under = false;
  for (const int32_t k : astar.cells) {
    CHECK(!wall(k));
    under = under || (k % 20 == 10 && k / 20 == 17);
  }
  CHECK(under);
  // Boxed in: no way.
  const auto boxed = [](int32_t, int32_t b, double length) {
    return b % 20 >= 10 ? ai::kUnreached : length;
  };
  CHECK(ai::grid_path(kGrid, from, to, boxed, 1, s).cells.empty());
}

TEST(string_pulling_keeps_the_corners_a_wall_needs_and_drops_the_rest) {
  ai::Search s;
  const ai::GridPath p = ai::grid_path(kGrid, 5 * 20 + 2, 5 * 20 + 18, step, 1, s);
  struct P {
    double x, z;
  };
  std::vector<P> pts;
  pts.reserve(p.cells.size());
  for (const int32_t k : p.cells) {
    const int32_t row = k / 20;
    pts.push_back({k % 20 + 0.5, row + 0.5});
  }
  // Clear where the straight leg never passes a wall cell (sampled every 0.1 m).
  const auto clear = [&](size_t a, size_t b) {
    const double d = std::hypot(pts[b].x - pts[a].x, pts[b].z - pts[a].z);
    const int n = static_cast<int>(d / 0.1) + 1;
    for (int i = 0; i <= n; i++) {
      const double f = static_cast<double>(i) / n;
      const auto x = static_cast<int32_t>(pts[a].x + (pts[b].x - pts[a].x) * f);
      const auto z = static_cast<int32_t>(pts[a].z + (pts[b].z - pts[a].z) * f);
      if (wall(z * 20 + x)) return false;
    }
    return true;
  };
  const std::vector<P> pulled = ai::string_pull<P>(std::span<const P>(pts), clear);
  CHECK(pulled.size() >= 3 && pulled.size() < pts.size());
  CHECK(pulled.front().x == pts.front().x && pulled.back().x == pts.back().x);
  for (size_t i = 1; i < pulled.size(); i++) {
    // Every kept leg is clear of the wall.
    std::vector<P> two{pulled[i - 1], pulled[i]};
    std::swap(pts, two);
    CHECK(clear(0, 1));
    std::swap(pts, two);
  }
}
