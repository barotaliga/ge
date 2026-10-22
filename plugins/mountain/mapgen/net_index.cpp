// The network index's segments and cells.
//
// ge:summary The runs' spatial hash: segments added, cells held in a grid or a map, covers
// ge:keywords net-index add segment covers cell-key
// ge:skill mountain
// ge:test tests/mountain_resort_build_test.cpp
#include "mountain/mapgen/net_index.hpp"

#include <algorithm>

namespace ge::mountain::mapgen {

namespace {

int grid_side() {
  const double size = ge::core::num::max(rules::world::size, resort_rules::massif::size);
  return static_cast<int>(std::ceil(size / NetIndex::kCell)) + 2 * NetIndex::kGridMargin;
}

}  // namespace

NetIndex::NetIndex()
    : side_(grid_side()),
      row_cols_(size_t(side_)),
      col_rows_(size_t(side_)),
      grid_(size_t(side_) * size_t(side_)) {}

std::vector<NetIndex::Span>& NetIndex::slot(int c, int r) {
  const int gc = c + kGridMargin;
  const int gr = r + kGridMargin;
  if (gc >= 0 && gc < side_ && gr >= 0 && gr < side_) {
    const auto widen = [](Range& range, int v) {
      if (range.lo > range.hi) {
        range = {v, v};
      } else {
        range.lo = std::min(range.lo, v), range.hi = std::max(range.hi, v);
      }
    };
    widen(row_cols_[size_t(gr)], c);
    widen(col_rows_[size_t(gc)], r);
    return grid_[size_t(gr) * size_t(side_) + size_t(gc)];
  }
  outside_ = true;
  return cells_[ge::core::cell_key(c, r)];
}

int NetIndex::add(const std::vector<TrackPoint>& points, int rank, int into, double from,
                  bool road) {
  const int run = static_cast<int>(runs.size());
  runs.push_back({&points, rank, into, from, road});
  for (size_t i = 0; i + 1 < points.size(); i++) {
    const TrackPoint& a = points[i];
    const TrackPoint& b = points[i + 1];
    const int g = static_cast<int>(seg_run_.size());
    seg_run_.push_back(run);
    seg_index_.push_back(static_cast<int>(i));
    const double dx = b.x - a.x;
    const double dz = b.z - a.z;
    const double len2 = dx * dx + dz * dz;
    for (const double v : {a.x, a.z, dx, dz, len2 == 0 ? 1.0 : len2, a.s}) seg_geo_.push_back(v);
    const int c0 = static_cast<int>(std::floor(std::min(a.x, b.x) / kCell));
    const int c1 = static_cast<int>(std::floor(std::max(a.x, b.x) / kCell));
    const int r0 = static_cast<int>(std::floor(std::min(a.z, b.z) / kCell));
    const int r1 = static_cast<int>(std::floor(std::max(a.z, b.z) / kCell));
    const double x0 = std::min(a.x, b.x);
    const double x1 = std::max(a.x, b.x);
    const double z0 = std::min(a.z, b.z);
    const double z1 = std::max(a.z, b.z);
    for (int c = c0; c <= c1; c++) {
      for (int r = r0; r <= r1; r++) {
        std::vector<Span>& list = slot(c, r);
        if (!list.empty() && list.back().g1 == g && list.back().g1 - list.back().g0 < kSpan) {
          Span& sp = list.back();
          sp.g1 = g + 1;
          sp.x0 = std::min(sp.x0, x0), sp.x1 = std::max(sp.x1, x1);
          sp.z0 = std::min(sp.z0, z0), sp.z1 = std::max(sp.z1, z1);
        } else {
          list.push_back({g, g + 1, x0, z0, x1, z1});
        }
      }
    }
    if (c0_ > c1_) {
      c0_ = c0, c1_ = c1, r0_ = r0, r1_ = r1;
    } else {
      c0_ = std::min(c0_, c0), c1_ = std::max(c1_, c1);
      r0_ = std::min(r0_, r0), r1_ = std::max(r1_, r1);
    }
  }
  return run;
}

bool NetIndex::covers(double x, double z, double pad, NetHit& out) const {
  std::vector<int> seen;
  const double within = kWidestHalf + pad;
  const auto skip = [&](int r, double) {
    return std::find(seen.begin(), seen.end(), r) != seen.end();
  };
  for (;;) {
    const NetHit& h = nearest(x, z, within, skip, out);
    if (h.distance == std::numeric_limits<double>::infinity()) return false;
    if (h.distance < h.width / 2 + pad) return true;
    seen.push_back(h.run);
  }
}

}  // namespace ge::mountain::mapgen
