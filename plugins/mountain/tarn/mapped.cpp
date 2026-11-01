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

// The plan distance from (x, z) to the shore's nearest edge, m.
double to_shore(const std::vector<mg::PlanPoint>& s, double x, double z) {
  double best = 1e30;
  for (size_t i = 0, j = s.size() - 1; i < s.size(); j = i++) {
    const double ex = s[i].x - s[j].x, ez = s[i].z - s[j].z;
    const double len2 = ex * ex + ez * ez;
    double t = len2 > 0 ? ((x - s[j].x) * ex + (z - s[j].z) * ez) / len2 : 0;
    t = std::clamp(t, 0.0, 1.0);
    const double dx = x - (s[j].x + t * ex), dz = z - (s[j].z + t * ez);
    best = std::min(best, dx * dx + dz * dz);
  }
  return std::sqrt(best);
}

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
  std::vector<double> ys;
  const int c0 = std::max(0, static_cast<int>(std::floor((x0 - g.origin_x) / g.cell)));
  const int c1 = std::min(g.cols - 1, static_cast<int>(std::ceil((x1 - g.origin_x) / g.cell)));
  const int r0 = std::max(0, static_cast<int>(std::floor((z0 - g.origin_z) / g.cell)));
  const int r1 = std::min(g.rows - 1, static_cast<int>(std::ceil((z1 - g.origin_z) / g.cell)));
  for (int row = r0; row <= r1; ++row) {
    for (int col = c0; col <= c1; ++col) {
      const double x = g.origin_x + col * g.cell, z = g.origin_z + row * g.cell;
      if (inside_shore(s, x, z)) ys.push_back(ground(level, x, z));
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
  for (int row = r0; row <= r1; ++row) {
    for (int col = c0; col <= c1; ++col) {
      const double x = g.origin_x + col * g.cell, z = g.origin_z + row * g.cell;
      if (x < x0 - reach || x > x1 + reach || z < z0 - reach || z > z1 + reach) continue;
      const bool in = inside_shore(lake.shore, x, z);
      const double d = to_shore(lake.shore, x, z);
      if (!in && d > reach) continue;
      float& y =
          g.data[static_cast<size_t>(row) * static_cast<size_t>(g.cols) + static_cast<size_t>(col)];
      if (in) {
        if (flat) {
          y = static_cast<float>(lake.level);
          continue;
        }
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
    return inside_shore(lake.shore, tree.x, tree.z) || to_shore(lake.shore, tree.x, tree.z) < 4;
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
