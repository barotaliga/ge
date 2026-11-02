// A tarn in a cirque: the site's search, the basin dug or filled, the winter's ice.
//
// ge:summary A tarn laid on a map: the site search, the basin, the winter so far and its ice
// ge:keywords tarn find_tarn dig_basin lay_tarn winter_days tarn_ice clearance flattest
// ge:skill mountain
// ge:test tests/mountain_tarn_test.cpp
#include "mountain/tarn/tarn.hpp"

#include <algorithm>
#include <cmath>

#include "core/heightfield.hpp"
#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "core/num.hpp"
#include "mountain/air/marks.hpp"
#include "wind/cold.hpp"

namespace ge::mountain::tarn {

namespace {

namespace mg = mapgen;
namespace lm = core::libm;

constexpr double kTau = 6.283185307179586;
constexpr double kGrid = 16;          // m between the sites tried
constexpr double kFreezeNorth = 320;  // the day of the year the north's lakes freeze
constexpr double kFreezeSouth = 136;  // the south's
constexpr double kWinter = 182;       // days a winter lasts, at most

double ground(const mg::Level& level, double x, double z) {
  return core::sample_field(level.ground, x, z);
}

// The squared distance in plan from (x, z) to the segment a-b.
double to_segment2(double x, double z, double ax, double az, double bx, double bz) {
  const double dx = bx - ax, dz = bz - az;
  const double len2 = dx * dx + dz * dz;
  double t = len2 > 0 ? ((x - ax) * dx + (z - az) * dz) / len2 : 0;
  t = std::clamp(t, 0.0, 1.0);
  const double px = ax + t * dx - x, pz = az + t * dz - z;
  return px * px + pz * pz;
}

bool clear_of_points(const std::vector<mg::TrackPoint>& points, double x, double z, double reach) {
  for (const mg::TrackPoint& p : points) {
    const double dx = p.x - x, dz = p.z - z;
    if (dx * dx + dz * dz < reach * reach) return false;
  }
  return true;
}

// Every point of the ground a tarn `radius` round at (x, z) reshapes clear of the pistes and the
// lifts by `kTarnClear`.
bool clear(const mg::Level& level, double x, double z, double radius) {
  const double reach = 2 * kTarnBulge * radius + kTarnClear;
  if (!clear_of_points(level.track.points, x, z, reach)) return false;
  if (!level.resort) return true;
  const mg::ResortMap& area = *level.resort;
  for (const mg::Run& run : area.runs) {
    if (!clear_of_points(run.points, x, z, reach)) return false;
  }
  for (const mg::SkiRoute& route : area.routes) {
    if (!clear_of_points(route.points, x, z, reach)) return false;
  }
  for (const mg::Lift& lift : area.lifts) {
    if (to_segment2(x, z, lift.bottom.x, lift.bottom.z, lift.top.x, lift.top.z) < reach * reach)
      return false;
  }
  return true;
}

// The lowest ground on `lake`'s shore.
double rim_of(const mg::Level& level, const mg::MapLake& lake) {
  double rim = 1e30;
  for (const water::Point& p : shore_of(lake)) rim = std::min(rim, ground(level, p.x, p.z));
  return rim;
}

}  // namespace

double reach_of(const mg::MapLake& lake, double angle) {
  if (!lake.shore.empty()) {
    // A mapped shore: the nearest of its edges met along the ray out from the middle.
    const double dx = lm::cos(angle), dz = lm::sin(angle);
    double best = 1e30;
    const std::vector<mg::PlanPoint>& s = lake.shore;
    for (size_t i = 0, j = s.size() - 1; i < s.size(); j = i++) {
      const double ex = s[i].x - s[j].x, ez = s[i].z - s[j].z;
      const double den = dx * ez - dz * ex;
      if (std::abs(den) < 1e-12) continue;
      const double ax = s[j].x - lake.x, az = s[j].z - lake.z;
      const double t = (ax * ez - az * ex) / den;
      const double u = (ax * dz - az * dx) / den;
      if (t > 0 && u >= 0 && u <= 1) best = std::min(best, t);
    }
    return best < 1e30 ? best : 0;
  }
  // The phases off the site, so every tarn has its own shape and the same site the same one.
  const double p2 = kTau * core::num::mod(lake.x * 0.0137 + lake.z * 0.0291, 1.0);
  const double p3 = kTau * core::num::mod(lake.x * 0.0219 - lake.z * 0.0173, 1.0);
  return lake.radius *
         (1 + kTarnLobe2 * lm::cos(2 * angle + p2) + kTarnLobe3 * lm::cos(3 * angle + p3));
}

std::optional<mg::MapLake> find_tarn(const mg::Level& level, double radius) {
  const double low = level.mountain.base.y, high = level.mountain.summit.y;
  const double y_low = low + kTarnLow * (high - low), y_high = low + kTarnHigh * (high - low);
  const double edge = 2.2 * radius, reach = 1.5 * radius;
  const int steps = static_cast<int>((level.size - 2 * edge) / kGrid);
  double best = 1e30;
  mg::MapLake out;
  for (int row = 0; row <= steps; ++row) {
    const double z = edge + kGrid * row;
    for (int col = 0; col <= steps; ++col) {
      const double x = edge + kGrid * col;
      const double y0 = ground(level, x, z);
      if (y0 < y_low || y0 > y_high) continue;
      double lo = 1e30, hi = -1e30;
      for (int j = -4; j <= 4; ++j) {
        for (int i = -4; i <= 4; ++i) {
          const double y = ground(level, x + reach * i / 4.0, z + reach * j / 4.0);
          lo = std::min(lo, y);
          hi = std::max(hi, y);
        }
      }
      if (hi - lo >= best || !clear(level, x, z, radius)) continue;
      best = hi - lo;
      out.x = x;
      out.z = z;
    }
  }
  if (best > kTarnRelief * radius) return std::nullopt;
  out.radius = radius;
  out.level = rim_of(level, out);
  return out;
}

double winter_days(const mg::Level& level) {
  const double freeze = level.sun.latitude < 0 ? kFreezeSouth : kFreezeNorth;
  const double days = core::num::mod(level.sun.day_of_year - freeze + 365, 365.0);
  return days < kWinter ? days : 0;
}

double thaw_of(const mg::Level& level) {
  const double days = winter_days(level);
  return std::clamp((days - (kWinter - kThawDays)) / kThawDays, 0.0, 1.0);
}

double tarn_ice(const mg::Level& level, const mg::MapLake& lake) {
  const double days = winter_days(level);
  return air::lake_ice(level, lake.level, std::min(days, kWinter - kThawDays)) *
         (1 - thaw_of(level));
}

bool bears(double ice, double kg) { return wind::ice_bears(ice) >= kg; }

bool bears(const mg::MapLake& lake, double kg) {
  // What ice carries goes as its thickness squared (Gold): rotten ice is thinner sound ice.
  return bears(lake.ice * std::sqrt(1 - (1 - kRotten) * lake.thaw), kg);
}

double moat_of(const mg::MapLake& lake) { return kMoat * lake.thaw; }

bool inside_shore(const std::vector<mg::PlanPoint>& shore, double x, double z) {
  bool odd = false;
  for (size_t i = 0, j = shore.size() - 1; i < shore.size(); j = i++) {
    const mg::PlanPoint a = shore[i], b = shore[j];
    if ((a.z > z) != (b.z > z) && x < (b.x - a.x) * (z - a.z) / (b.z - a.z) + a.x) odd = !odd;
  }
  return odd;
}

void dig_basin(mg::Level& level, mg::MapLake& lake, double depth, bool flat) {
  lake.level = rim_of(level, lake);
  const double most = kTarnBulge * lake.radius;
  core::Heightfield& g = level.ground;
  double deepest = 0;
  for (int row = 0; row < g.rows; ++row) {
    for (int col = 0; col < g.cols; ++col) {
      const double x = g.origin_x + col * g.cell, z = g.origin_z + row * g.cell;
      const double dx = x - lake.x, dz = z - lake.z;
      const double d = std::sqrt(dx * dx + dz * dz);
      if (d > 2 * most) continue;
      const double reach = reach_of(lake, lm::atan2(dz, dx));
      const double r = d / reach;
      if (r > 2) continue;
      float& y =
          g.data[static_cast<size_t>(row) * static_cast<size_t>(g.cols) + static_cast<size_t>(col)];
      const double rimmed = lake.level + 0.3 + 0.02 * reach * std::max(r - 1, 0.0);
      if (r < 1) {
        if (flat) {
          y = static_cast<float>(lake.level);
          continue;
        }
        // A smooth floor: flat in the middle, easing up to the rim, so the shore shelves.
        const double s = std::clamp((1 - r) / 0.7, 0.0, 1.0);
        const double bowl = s * s * (3 - 2 * s);
        y = static_cast<float>(std::min<double>(y, rimmed - (0.3 + depth) * bowl));
        deepest = std::max(deepest, lake.level - y);
      } else if (y < rimmed) {
        // Wholly raised to the box's corners (r = 1.42), blended back by r = 2.
        const double s = std::clamp((2 - r) / (2 - 1.45), 0.0, 1.0);
        const double w = s * s * (3 - 2 * s);
        y = static_cast<float>(y + (rimmed - y) * w);
      } else {
        // The slope above cut back into a headwall: down to the shore at r = 1, untouched by r = 2.
        const double s = std::clamp(2 - r, 0.0, 1.0);
        const double w = s * s * (3 - 2 * s);
        y = static_cast<float>(y + (rimmed - y) * w);
      }
    }
  }
  lake.depth = deepest;
  std::erase_if(level.trees, [&](const mg::TreeDef& tree) {
    const double dx = tree.x - lake.x, dz = tree.z - lake.z;
    const double reach = reach_of(lake, lm::atan2(dz, dx)) + 4;
    return dx * dx + dz * dz < reach * reach;
  });
  for (mg::TreeDef& tree : level.trees) tree.y = ground(level, tree.x, tree.z);
}

void lay_tarn(mg::Level& level, mg::MapLake lake) {
  lake.ice = tarn_ice(level, lake);
  lake.thaw = lake.ice > 0 ? thaw_of(level) : 0;
  dig_basin(level, lake, kTarnDepth, bears(lake));
  level.lakes.push_back(lake);
}

std::vector<water::Point> shore_of(const mg::MapLake& lake, int points) {
  std::vector<water::Point> out;
  if (!lake.shore.empty()) {
    for (const mg::PlanPoint& p : lake.shore) out.push_back({p.x, p.z});
    return out;
  }
  out.reserve(static_cast<size_t>(points));
  for (int i = 0; i < points; ++i) {
    const double a = kTau * i / points;
    const double reach = reach_of(lake, a);
    out.push_back({lake.x + reach * lm::cos(a), lake.z + reach * lm::sin(a)});
  }
  return out;
}

bool on_lake(const mg::MapLake& lake, double x, double z) {
  if (!lake.shore.empty()) return inside_shore(lake.shore, x, z);
  const double dx = x - lake.x, dz = z - lake.z;
  const double most = kTarnBulge * lake.radius;
  if (dx * dx + dz * dz >= most * most) return false;
  const double reach = reach_of(lake, lm::atan2(dz, dx));
  return dx * dx + dz * dz < reach * reach;
}

}  // namespace ge::mountain::tarn
