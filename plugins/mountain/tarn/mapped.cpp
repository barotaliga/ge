// A real face's lakes laid on its map (mapped.hpp).
//
// ge:summary The mapped lakes read off a face's hints, dug or filled into its ground, iced
// ge:keywords mapped-lake lay_mapped_lakes mapped_lakes depth_of basin bank shelf centroid
// ge:keywords surface_of
// ge:skill mountain
// ge:test tests/mountain_mapped_test.cpp
#include "mountain/tarn/mapped.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>

#include "core/heightfield.hpp"
#include "core/math.hpp"
#include "mountain/mapgen/real_hints.hpp"
#include "mountain/tarn/tarn.hpp"

namespace ge::mountain::tarn {

namespace {

namespace mg = mapgen;

constexpr double kPi = 3.141592653589793;

double ground(const mg::Level& level, double x, double z) {
  return core::sample_field(level.ground, x, z);
}

// The shore's edges bucketed on a coarse grid, so a cell asks only the edges near it, and its
// crossings of a row, so a row's cells ask whether they are inside by a search.
class ShoreIndex {
 public:
  ShoreIndex(const std::vector<mg::PlanPoint>& s, double pad) : s_(s) {
    x0_ = z0_ = 1e30;
    double x1 = -1e30, z1 = -1e30;
    for (const mg::PlanPoint& p : s) {
      x0_ = std::min(x0_, p.x - pad);
      z0_ = std::min(z0_, p.z - pad);
      x1 = std::max(x1, p.x + pad);
      z1 = std::max(z1, p.z + pad);
    }
    cols_ = std::max(1, static_cast<int>(std::ceil((x1 - x0_) / kBucket)));
    rows_ = std::max(1, static_cast<int>(std::ceil((z1 - z0_) / kBucket)));
    buckets_.resize(static_cast<size_t>(cols_) * static_cast<size_t>(rows_));
    for (size_t i = 0, j = s.size() - 1; i < s.size(); j = i++) {
      const int a0 = col(std::min(s[i].x, s[j].x)), a1 = col(std::max(s[i].x, s[j].x));
      const int b0 = row(std::min(s[i].z, s[j].z)), b1 = row(std::max(s[i].z, s[j].z));
      for (int b = b0; b <= b1; ++b) {
        for (int a = a0; a <= a1; ++a) bucket(a, b).push_back(static_cast<uint32_t>(i));
      }
    }
  }
  // The distance from (x, z) to the nearest edge, m, read out to `limit` (past it: over it).
  double distance(double x, double z, double limit) const {
    const int a = col(x), b = row(z);
    const int n = static_cast<int>(std::ceil(limit / kBucket)) + 1;
    double best = 1e30;
    for (int bb = std::max(0, b - n); bb <= std::min(rows_ - 1, b + n); ++bb) {
      for (int aa = std::max(0, a - n); aa <= std::min(cols_ - 1, a + n); ++aa) {
        for (const uint32_t i : bucket(aa, bb)) best = std::min(best, to_edge2(i, x, z));
      }
    }
    return std::min(std::sqrt(best), limit + 1);
  }
  // The x where the shore crosses the row at `z`, sorted: inside between each pair.
  void crossings(double z, std::vector<double>& xs) const {
    xs.clear();
    for (size_t i = 0, j = s_.size() - 1; i < s_.size(); j = i++) {
      if ((s_[i].z > z) != (s_[j].z > z))
        xs.push_back(s_[i].x + (s_[j].x - s_[i].x) * (z - s_[i].z) / (s_[j].z - s_[i].z));
    }
    std::sort(xs.begin(), xs.end());
  }
  static bool inside(const std::vector<double>& xs, double x) {
    // Even-odd, as `inside_shore` counts: the crossings strictly right of x.
    const auto right = xs.end() - std::upper_bound(xs.begin(), xs.end(), x);
    return right % 2 == 1;
  }

 private:
  static constexpr double kBucket = 32;
  const std::vector<mg::PlanPoint>& s_;
  double x0_, z0_;
  int cols_ = 1, rows_ = 1;
  std::vector<std::vector<uint32_t>> buckets_;
  int col(double x) const {
    return std::clamp(static_cast<int>(std::floor((x - x0_) / kBucket)), 0, cols_ - 1);
  }
  int row(double z) const {
    return std::clamp(static_cast<int>(std::floor((z - z0_) / kBucket)), 0, rows_ - 1);
  }
  std::vector<uint32_t>& bucket(int a, int b) {
    return buckets_[static_cast<size_t>(b) * static_cast<size_t>(cols_) + static_cast<size_t>(a)];
  }
  const std::vector<uint32_t>& bucket(int a, int b) const {
    return buckets_[static_cast<size_t>(b) * static_cast<size_t>(cols_) + static_cast<size_t>(a)];
  }
  double to_edge2(uint32_t i, double x, double z) const {
    const mg::PlanPoint& p = s_[i];
    const mg::PlanPoint& q = s_[i == 0 ? s_.size() - 1 : i - 1];
    const double ex = p.x - q.x, ez = p.z - q.z;
    const double len2 = ex * ex + ez * ez;
    const double t = len2 > 0 ? std::clamp(((x - q.x) * ex + (z - q.z) * ez) / len2, 0.0, 1.0) : 0;
    const double dx = x - (q.x + t * ex), dz = z - (q.z + t * ez);
    return dx * dx + dz * dz;
  }
};

// The ground's `kSurface` share inside the shore, read at every cell of the ground's grid there.
double surface_of(const mg::Level& level, const std::vector<mg::PlanPoint>& s) {
  const core::Heightfield& g = level.ground;
  double x0 = 1e30, x1 = -1e30, z0 = 1e30, z1 = -1e30;
  for (const mg::PlanPoint& p : s) {
    x0 = std::min(x0, p.x);
    x1 = std::max(x1, p.x);
    z0 = std::min(z0, p.z);
    z1 = std::max(z1, p.z);
  }
  std::vector<double> ys, xs;
  const ShoreIndex index(s, 0);
  const int c0 = std::max(0, static_cast<int>(std::floor((x0 - g.origin_x) / g.cell)));
  const int c1 = std::min(g.cols - 1, static_cast<int>(std::ceil((x1 - g.origin_x) / g.cell)));
  const int r0 = std::max(0, static_cast<int>(std::floor((z0 - g.origin_z) / g.cell)));
  const int r1 = std::min(g.rows - 1, static_cast<int>(std::ceil((z1 - g.origin_z) / g.cell)));
  for (int row = r0; row <= r1; ++row) {
    index.crossings(g.origin_z + row * g.cell, xs);
    for (int col = c0; col <= c1; ++col) {
      const double x = g.origin_x + col * g.cell, z = g.origin_z + row * g.cell;
      if (ShoreIndex::inside(xs, x)) ys.push_back(ground(level, x, z));
    }
  }
  if (ys.empty()) return ground(level, s[0].x, s[0].z);
  const auto at = ys.begin() + static_cast<long>(kSurface * static_cast<double>(ys.size() - 1));
  std::nth_element(ys.begin(), at, ys.end());
  return *at;
}

double smooth(double s) { return s * s * (3 - 2 * s); }

}  // namespace

std::vector<mg::MapLake> mapped_lakes(const mg::Level& level) {
  std::vector<mg::MapLake> out;
  if (!level.face) return out;
  const std::optional<mg::RealHints> hints = mg::real_hints(*level.face);
  if (!hints) return out;
  for (const mg::HintLake& h : hints->lakes) {
    // The centroid and the area of the shore's polygon (its winding either way).
    double a = 0, cx = 0, cz = 0;
    const std::vector<mg::PlanPoint>& s = h.shore;
    for (size_t i = 0, j = s.size() - 1; i < s.size(); j = i++) {
      const double c = s[j].x * s[i].z - s[i].x * s[j].z;
      a += c;
      cx += (s[j].x + s[i].x) * c;
      cz += (s[j].z + s[i].z) * c;
    }
    if (std::abs(a / 2) < kLeastLake) continue;
    mg::MapLake lake;
    lake.x = cx / (3 * a);
    lake.z = cz / (3 * a);
    lake.radius = std::sqrt(std::abs(a / 2) / kPi);
    lake.shore = s;
    lake.level = surface_of(level, s);
    out.push_back(std::move(lake));
  }
  return out;
}

double depth_of(const mg::MapLake& lake) {
  return std::clamp(0.1 * lake.radius, kMappedShallow, kMappedDeep);
}

void lay_mapped(mg::Level& level, mg::MapLake lake) {
  lake.ice = tarn_ice(level, lake);
  lake.thaw = lake.ice > 0 ? thaw_of(level) : 0;
  const bool flat = bears(lake);
  const double depth = depth_of(lake), shelf = std::min(kShelf, 0.5 * lake.radius);
  double x0 = 1e30, x1 = -1e30, z0 = 1e30, z1 = -1e30;
  for (const mg::PlanPoint& p : lake.shore) {
    x0 = std::min(x0, p.x);
    x1 = std::max(x1, p.x);
    z0 = std::min(z0, p.z);
    z1 = std::max(z1, p.z);
  }
  core::Heightfield& g = level.ground;
  const auto col_of = [&](double x) { return (x - g.origin_x) / g.cell; };
  const auto row_of = [&](double z) { return (z - g.origin_z) / g.cell; };
  const double reach = kLip + kBankReach;
  const int c0 = std::max(0, static_cast<int>(std::floor(col_of(x0 - reach))));
  const int c1 = std::min(g.cols - 1, static_cast<int>(std::ceil(col_of(x1 + reach))));
  const int r0 = std::max(0, static_cast<int>(std::floor(row_of(z0 - reach))));
  const int r1 = std::min(g.rows - 1, static_cast<int>(std::ceil(row_of(z1 + reach))));
  const double rimmed = lake.level + 0.3;
  double deepest = 0;
  const ShoreIndex index(lake.shore, reach);
  std::vector<double> xs;
  for (int row = r0; row <= r1; ++row) {
    index.crossings(g.origin_z + row * g.cell, xs);
    for (int col = c0; col <= c1; ++col) {
      const double x = g.origin_x + col * g.cell, z = g.origin_z + row * g.cell;
      if (x < x0 - reach || x > x1 + reach || z < z0 - reach || z > z1 + reach) continue;
      const bool in = ShoreIndex::inside(xs, x);
      if (in && flat) {
        g.data[static_cast<size_t>(row) * static_cast<size_t>(g.cols) + static_cast<size_t>(col)] =
            static_cast<float>(lake.level);
        continue;
      }
      const double d = index.distance(x, z, in ? shelf : reach);
      if (!in && d > reach) continue;
      float& y =
          g.data[static_cast<size_t>(row) * static_cast<size_t>(g.cols) + static_cast<size_t>(col)];
      if (in) {
        // Flat in the middle, shelving up to the shore.
        const double bowl = smooth(std::clamp(d / shelf, 0.0, 1.0));
        y = static_cast<float>(std::min<double>(y, rimmed - (0.3 + depth) * bowl));
        deepest = std::max(deepest, lake.level - y);
        continue;
      }
      // THE BANKS: just over the water out to `kLip` m, then no steeper than `kBankSlope` up to
      // the ground above or down to the ground below.
      const double off = std::max(0.0, d - kLip);
      y = static_cast<float>(
          std::clamp<double>(y, rimmed - kBankSlope * off, rimmed + kBankSlope * off));
    }
  }
  lake.depth = flat ? 0 : deepest;
  std::erase_if(level.trees, [&](const mg::TreeDef& tree) {
    if (tree.x < x0 - 4 || tree.x > x1 + 4 || tree.z < z0 - 4 || tree.z > z1 + 4) return false;
    return inside_shore(lake.shore, tree.x, tree.z) || index.distance(tree.x, tree.z, 4) < 4;
  });
  for (mg::TreeDef& tree : level.trees) {
    if (tree.x >= x0 - reach && tree.x <= x1 + reach && tree.z >= z0 - reach &&
        tree.z <= z1 + reach)
      tree.y = ground(level, tree.x, tree.z);
  }
  level.lakes.push_back(std::move(lake));
}

void lay_mapped_lakes(mg::Level& level) {
  for (mg::MapLake& lake : mapped_lakes(level)) lay_mapped(level, std::move(lake));
}

}  // namespace ge::mountain::tarn
