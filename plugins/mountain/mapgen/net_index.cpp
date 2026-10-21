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

NetIndex::NetIndex() : side_(grid_side()), grid_(size_t(side_) * size_t(side_)) {}

const std::vector<int>* NetIndex::list(int c, int r) const {
  const int gc = c + kGridMargin;
  const int gr = r + kGridMargin;
  if (gc >= 0 && gc < side_ && gr >= 0 && gr < side_) {
    const std::vector<int>& l = grid_[size_t(gr) * size_t(side_) + size_t(gc)];
    return l.empty() ? nullptr : &l;
  }
  const auto it = cells_.find(ge::core::cell_key(c, r));
  return it == cells_.end() ? nullptr : &it->second;
}

std::vector<int>& NetIndex::slot(int c, int r) {
  const int gc = c + kGridMargin;
  const int gr = r + kGridMargin;
  if (gc >= 0 && gc < side_ && gr >= 0 && gr < side_)
    return grid_[size_t(gr) * size_t(side_) + size_t(gc)];
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
    for (int c = c0; c <= c1; c++) {
      for (int r = r0; r <= r1; r++) slot(c, r).push_back(g);
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
