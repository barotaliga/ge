// PATHS: the one cheapest-path search every bot and every generator step shares, so none writes
// its own heap.
//
// - THE HEAP (`Heap`): a binary heap of node ids by key. Equal keys leave in the order the sift
//   puts them, which is a pure function of the pushes: a search over it is deterministic to the
//   bit, and two searches given the same pushes expand the same nodes in the same order.
// - THE GRAPH SEARCH (`Search`): Dijkstra over nodes 0..n-1 (or A* with a heuristic), the caller
//   expanding each node it pops. A caller seeds the start nodes (`seed`), and for each step out of
//   a node asks whether it is cheaper (`improves`) before any dearer check of its own, then takes
//   it (`reach`), keeping what else it tracks per node beside the search's cost and back-pointer.
//   A node popped again at a dearer key than it was reached at since is skipped: every step from
//   it was weighed at that cheaper cost already. `run` stops at the first node `done` accepts (or
//   when the heap is empty); `path_to` reads the way back to a seed.
// - THE GRID SEARCH (`grid_path`): A* over a grid's cells and their eight neighbours, each step
//   costed by the caller (infinite where it may not go), the heuristic the octile distance times
//   the least cost a metre can have (so it never overestimates, and the path is the cheapest).
// - STRING-PULLING (`string_pull`): a path's points cut to the fewest that keep every straight leg
//   clear by the caller's test: from each kept point, on to the furthest point it reaches clear.
//
// No transcendental, no allocation once the search's arrays are sized (`reset` keeps them).
//
// ge:summary Paths: the deterministic binary heap, graph search, grid A*, string-pulling
// ge:keywords ai path search heap binary-heap dijkstra a-star astar graph grid octile cost
// ge:keywords string-pulling string_pull smoothing route back-pointer expand relax
// ge:skill bots
// ge:test tests/ai_path_test.cpp
#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <span>
#include <utility>
#include <vector>

namespace ge::ai {

// A binary heap of node ids by key, the least key on top.
class Heap {
 public:
  [[nodiscard]] bool empty() const { return ids_.empty(); }
  [[nodiscard]] size_t size() const { return ids_.size(); }
  void clear() {
    ids_.clear();
    keys_.clear();
  }
  void push(int32_t id, double key);
  // The node on top, off the heap; `left_at` the key it left with.
  int32_t pop(double& left_at);

 private:
  std::vector<int32_t> ids_;
  std::vector<double> keys_;
};

inline constexpr double kUnreached = std::numeric_limits<double>::infinity();

// The cheapest ways from the seeds to every node a caller's expansion reaches.
class Search {
 public:
  explicit Search(size_t nodes = 0) { reset(nodes); }
  // Every node unreached, the heap empty; the arrays kept where they are big enough.
  void reset(size_t nodes);

  // A start: `node` at `cost`, with no way back.
  void seed(int32_t node, double cost = 0, double heuristic = 0);
  // Whether `cost` is cheaper than the way `node` is reached at now.
  [[nodiscard]] bool improves(int32_t node, double cost) const {
    return cost < cost_[static_cast<size_t>(node)];
  }
  // `node` reached at `cost` from `from`, pushed at `cost + heuristic`.
  void reach(int32_t node, double cost, int32_t from, double heuristic = 0);

  // Pops until `done(node)` accepts one (returned) or the heap is empty (-1), calling
  // `expand(node)` on every node popped at the key it was reached at. `heuristic(node)` is the
  // same estimate `seed` and `reach` were given (none: Dijkstra).
  template <class Expand, class Done>
  int32_t run(Expand&& expand, Done&& done);
  template <class Expand, class Done, class Heuristic>
  int32_t run(Expand&& expand, Done&& done, Heuristic&& heuristic);
  // Every node the expansion reaches: no goal.
  template <class Expand>
  void run_all(Expand&& expand) {
    run(expand, [](int32_t) { return false; });
  }

  [[nodiscard]] double cost(int32_t node) const { return cost_[static_cast<size_t>(node)]; }
  [[nodiscard]] int32_t prev(int32_t node) const { return prev_[static_cast<size_t>(node)]; }
  [[nodiscard]] bool reached(int32_t node) const { return cost(node) != kUnreached; }
  [[nodiscard]] size_t nodes() const { return cost_.size(); }
  // How many nodes were expanded by the last runs since `reset`.
  [[nodiscard]] size_t expanded() const { return expanded_; }
  // The nodes from a seed to `node`, in order; empty where it was never reached.
  [[nodiscard]] std::vector<int32_t> path_to(int32_t node) const;

 private:
  std::vector<double> cost_;
  std::vector<int32_t> prev_;
  Heap heap_;
  size_t expanded_ = 0;
};

template <class Expand, class Done>
int32_t Search::run(Expand&& expand, Done&& done) {
  return run(expand, done, [](int32_t) { return 0.0; });
}

template <class Expand, class Done, class Heuristic>
int32_t Search::run(Expand&& expand, Done&& done, Heuristic&& heuristic) {
  while (!heap_.empty()) {
    double key = 0;
    const int32_t a = heap_.pop(key);
    if (key > cost_[static_cast<size_t>(a)] + heuristic(a)) continue;
    if (done(a)) return a;
    expanded_++;
    expand(a);
  }
  return -1;
}

// A grid of `cols` x `rows` cells `cell` m on a side, cell (i, j) the node `j * cols + i`.
struct Grid {
  int32_t cols = 0;
  int32_t rows = 0;
  double cell = 1;
};

// What a grid search found: the cells from the start to the goal (empty: no way), its cost, and
// how many cells it expanded.
struct GridPath {
  std::vector<int32_t> cells;
  double cost = kUnreached;
  size_t expanded = 0;
};

// The cheapest way from cell `from` to cell `to` over the eight neighbours, a step from cell `a`
// to its neighbour `b` costing `step(a, b, length_m)` (kUnreached: barred). `least_per_m` is the
// least any metre can cost (0: Dijkstra; never more than any step's cost over its length).
GridPath grid_path(const Grid& grid, int32_t from, int32_t to,
                   const std::function<double(int32_t a, int32_t b, double length)>& step,
                   double least_per_m, Search& search);

// The fewest of `points` that keep every leg clear: the first and the last kept, and from each
// kept point the furthest later one `clear(a, b)` passes (indices into `points`).
template <class Point>
std::vector<Point> string_pull(std::span<const Point> points,
                               const std::function<bool(size_t a, size_t b)>& clear);

template <class Point>
std::vector<Point> string_pull(std::span<const Point> points,
                               const std::function<bool(size_t a, size_t b)>& clear) {
  std::vector<Point> out;
  if (points.empty()) return out;
  size_t at = 0;
  out.push_back(points[0]);
  while (at + 1 < points.size()) {
    size_t next = at + 1;
    for (size_t b = points.size() - 1; b > at + 1; b--) {
      if (clear(at, b)) {
        next = b;
        break;
      }
    }
    out.push_back(points[next]);
    at = next;
  }
  return out;
}

}  // namespace ge::ai
