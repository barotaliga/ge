// Port of game5 `engine/analysis/crossings.ts`.
//
// ge:summary The segment-pair test off a 16 m spatial hash
// ge:keywords analysis selfCrossings
// ge:oracle game5/engine/analysis/crossings.ts
// ge:skill mountain
// ge:test tests/mountain_generate_test.cpp
#include "mountain/analysis/crossings.hpp"

#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <unordered_set>

#include "core/js.hpp"

namespace ge::mountain::analysis {

namespace js = ge::core::js;
using mapgen::TrackPoint;

namespace {

constexpr double kCell = 16;

double orient(const TrackPoint& a, const TrackPoint& b, const TrackPoint& c) {
  return (b.x - a.x) * (c.z - a.z) - (b.z - a.z) * (c.x - a.x);
}

bool crosses(const TrackPoint& a, const TrackPoint& b, const TrackPoint& c, const TrackPoint& d) {
  return orient(a, b, c) * orient(a, b, d) < 0 && orient(c, d, a) * orient(c, d, b) < 0;
}

}  // namespace

int self_crossings(const std::vector<TrackPoint>& points) {
  const size_t n = points.size();
  // The count is a set of pairs, so the hash's iteration order cannot reach it.
  std::unordered_map<int64_t, std::vector<size_t>> cells;
  for (size_t i = 0; i + 1 < n; i++) {
    const TrackPoint& a = points[i];
    const TrackPoint& b = points[i + 1];
    const auto c0 = int64_t(std::floor(js::min(a.x, b.x) / kCell));
    const auto c1 = int64_t(std::floor(js::max(a.x, b.x) / kCell));
    const auto r0 = int64_t(std::floor(js::min(a.z, b.z) / kCell));
    const auto r1 = int64_t(std::floor(js::max(a.z, b.z) / kCell));
    for (int64_t c = c0; c <= c1; c++) {
      for (int64_t r = r0; r <= r1; r++) cells[c * 8192 + r].push_back(i);
    }
  }
  std::unordered_set<uint64_t> seen;
  int count = 0;
  for (const auto& [key, list] : cells) {
    for (size_t p = 0; p < list.size(); p++) {
      for (size_t q = p + 1; q < list.size(); q++) {
        const size_t i = std::min(list[p], list[q]);
        const size_t j = std::max(list[p], list[q]);
        if (j - i <= 1) continue;
        if (!seen.insert(uint64_t(i) * n + j).second) continue;
        if (crosses(points[i], points[i + 1], points[j], points[j + 1])) count++;
      }
    }
  }
  return count;
}

}  // namespace ge::mountain::analysis
