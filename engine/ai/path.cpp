// The heap, the graph search and the grid search (path.hpp).
//
// ge:summary Heap, Search and grid_path: the sift, the back-pointers, the octile A*
// ge:keywords ai path heap search grid_path octile
// ge:skill bots
// ge:test tests/ai_path_test.cpp
#include "ai/path.hpp"

#include <algorithm>
#include <cstdlib>

namespace ge::ai {

void Heap::push(int32_t id, double key) {
  size_t i = ids_.size();
  ids_.push_back(id);
  keys_.push_back(key);
  while (i > 0) {
    const size_t p = (i - 1) >> 1;
    if (keys_[p] <= key) break;
    ids_[i] = ids_[p];
    keys_[i] = keys_[p];
    i = p;
  }
  ids_[i] = id;
  keys_[i] = key;
}

int32_t Heap::pop(double& left_at) {
  const int32_t top = ids_[0];
  left_at = keys_[0];
  const int32_t id = ids_.back();
  const double key = keys_.back();
  ids_.pop_back();
  keys_.pop_back();
  const size_t n = ids_.size();
  if (n > 0) {
    size_t i = 0;
    for (;;) {
      const size_t l = 2 * i + 1;
      if (l >= n) break;
      const size_t c = l + 1 < n && keys_[l + 1] < keys_[l] ? l + 1 : l;
      if (keys_[c] >= key) break;
      ids_[i] = ids_[c];
      keys_[i] = keys_[c];
      i = c;
    }
    ids_[i] = id;
    keys_[i] = key;
  }
  return top;
}

void Search::reset(size_t nodes) {
  cost_.assign(nodes, kUnreached);
  prev_.assign(nodes, -1);
  heap_.clear();
  expanded_ = 0;
}

void Search::seed(int32_t node, double cost, double heuristic) {
  cost_[static_cast<size_t>(node)] = cost;
  prev_[static_cast<size_t>(node)] = -1;
  heap_.push(node, cost + heuristic);
}

void Search::reach(int32_t node, double cost, int32_t from, double heuristic) {
  cost_[static_cast<size_t>(node)] = cost;
  prev_[static_cast<size_t>(node)] = from;
  heap_.push(node, cost + heuristic);
}

std::vector<int32_t> Search::path_to(int32_t node) const {
  std::vector<int32_t> path;
  if (node < 0 || !reached(node)) return path;
  for (int32_t k = node; k >= 0; k = prev(k)) path.push_back(k);
  std::reverse(path.begin(), path.end());
  return path;
}

GridPath grid_path(const Grid& grid, int32_t from, int32_t to,
                   const std::function<double(int32_t, int32_t, double)>& step, double least_per_m,
                   Search& search) {
  GridPath out;
  const auto cells = static_cast<size_t>(grid.cols) * static_cast<size_t>(grid.rows);
  search.reset(cells);
  if (from < 0 || to < 0 || static_cast<size_t>(from) >= cells ||
      static_cast<size_t>(to) >= cells) {
    return out;
  }
  const int32_t ti = to % grid.cols;
  const int32_t tj = to / grid.cols;
  constexpr double kDiagonal = 1.4142135623730951;
  // The octile distance to the goal, m, times the least a metre can cost.
  const auto heuristic = [&](int32_t k) {
    const int32_t di = std::abs(k % grid.cols - ti);
    const int32_t dj = std::abs(k / grid.cols - tj);
    const int32_t lo = std::min(di, dj);
    const int32_t hi = std::max(di, dj);
    return least_per_m * grid.cell * ((kDiagonal - 1) * lo + hi);
  };
  search.seed(from, 0, heuristic(from));
  const auto expand = [&](int32_t a) {
    const int32_t i = a % grid.cols;
    const int32_t j = a / grid.cols;
    for (int32_t dj = -1; dj <= 1; dj++) {
      for (int32_t di = -1; di <= 1; di++) {
        if (di == 0 && dj == 0) continue;
        const int32_t ii = i + di;
        const int32_t jj = j + dj;
        if (ii < 0 || jj < 0 || ii >= grid.cols || jj >= grid.rows) continue;
        const int32_t b = jj * grid.cols + ii;
        const double length = grid.cell * (di != 0 && dj != 0 ? kDiagonal : 1);
        const double c = step(a, b, length);
        if (c == kUnreached) continue;
        const double cost = search.cost(a) + c;
        if (!search.improves(b, cost)) continue;
        search.reach(b, cost, a, heuristic(b));
      }
    }
  };
  const int32_t got = search.run(expand, [&](int32_t k) { return k == to; }, heuristic);
  out.expanded = search.expanded();
  if (got < 0) return out;
  out.cells = search.path_to(got);
  out.cost = search.cost(got);
  return out;
}

}  // namespace ge::ai
