// The definitions behind straight_venue.hpp.
//
// ge:summary R37 the straight venue's line search and grading
// ge:keywords straight-venue find-line grade-venue
// ge:skill mountain
// ge:test tests/mountain_ski_jump_test.cpp
#include "mountain/mapgen/straight_venue.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "core/num.hpp"
#include "mountain/mapgen/rules.hpp"

namespace ge::mountain::mapgen {

namespace {

namespace num = ge::core::num;
namespace libm = ge::core::libm;
using ge::core::clamp;
using ge::core::kPi;
using ge::core::smoothstep;

constexpr double kRad = kPi / 180;

// Whether a lift station or the village stands on or beside a venue `length` m long from (x, z)
// at `heading`.
bool near_stations(const ResortMap& resort, double x, double z, double heading, double length,
                   const VenueRule& r) {
  const double fx = libm::sin(heading);
  const double fz = libm::cos(heading);
  const auto at = [&](const Vec3& q, double reach) {
    const double along = (q.x - x) * fx + (q.z - z) * fz;
    const double across = (q.x - x) * fz - (q.z - z) * fx;
    return along > -reach && along < length + reach &&
           std::fabs(across) < r.width / 2 + r.ease + reach;
  };
  for (const Lift& l : resort.lifts) {
    if (at(l.bottom, r.fit.stations) || at(l.top, r.fit.stations)) return true;
  }
  return at(resort.village, r.fit.village);
}

}  // namespace

double profile_height_at(const VenueProfile& p, double x) {
  const double last = double(p.y.size() - 1);
  const double f = clamp(x / p.dx, 0, last);
  const size_t i = std::min(p.y.size() - 2, size_t(f));
  return p.y[i] + (p.y[i + 1] - p.y[i]) * (f - double(i));
}

std::optional<VenueLine> find_line(const Level& level, const VenueProfile& p, const VenueRule& r) {
  const venue_rules::Search& S = r.search;
  const auto n = size_t(std::ceil(p.end / S.step)) + 1;
  std::vector<double> prof(n);
  for (size_t k = 0; k < n; k++) prof[k] = profile_height_at(p, double(k) * S.step);
  std::vector<double> ground(n);
  std::optional<VenueLine> best;
  const int columns = int(std::floor((level.size - 2 * S.edge) / S.stride));
  for (int ci = 0; ci <= columns; ci++) {
    const double cx = S.edge + ci * S.stride;
    for (const double deg : S.bearings) {
      const double heading = deg * kRad;
      const double fx = libm::sin(heading);
      const double fz = libm::cos(heading);
      for (int di = 0;; di++) {
        const double d = di * S.starts;
        const double x0 = cx + fx * d;
        const double z0 = S.top + fz * d;
        const double x1 = x0 + fx * p.end;
        const double z1 = z0 + fz * p.end;
        if (z1 > level.size - S.edge || x1 < S.edge || x1 > level.size - S.edge) break;
        if (x0 < S.edge || x0 > level.size - S.edge) continue;
        double mean = 0;
        for (size_t k = 0; k < n; k++) {
          const double along = double(k) * S.step;
          ground[k] = level.ground_at(x0 + fx * along, z0 + fz * along);
          mean += ground[k] - prof[k];
        }
        mean /= double(n);
        double sq = 0;
        double deepest = 0;
        for (size_t k = 0; k < n; k++) {
          const double e = prof[k] + mean - ground[k];
          sq += e * e;
          deepest = num::max(deepest, std::fabs(e));
        }
        const double cost = std::sqrt(sq / double(n)) + r.fit.deepest * deepest;
        if (best && cost >= best->cost) continue;
        if (level.resort && near_stations(*level.resort, x0, z0, heading, p.end, r)) continue;
        best = VenueLine{x0, z0, heading, mean, cost};
      }
    }
  }
  return best;
}

std::shared_ptr<const Level> original_of(const Level& level) {
  if (level.race) return level.race->base;
  if (level.ski_jump) return level.ski_jump->base;
  return std::make_shared<const Level>(level);
}

GradedVenue grade_venue(const Level& original, const VenueProfile& p, const VenueRule& r) {
  const VenueLine line =
      find_line(original, p, r)
          .value_or(VenueLine{original.size / 2, r.search.top, 0,
                              original.ground_at(original.size / 2, r.search.top), 0});
  const double fx = libm::sin(line.heading);
  const double fz = libm::cos(line.heading);
  const double rx = libm::cos(line.heading);
  const double rz = -libm::sin(line.heading);
  const auto y_at = [&](double d) { return profile_height_at(p, d) + line.shift; };
  Level level = original;
  // The track, every 2 m of plan: the arc IS the plan distance along the line.
  level.track = Track{};
  const int stations = int(std::floor(p.end / 2 + 1e-6));
  for (int i = 0; i <= stations; i++) {
    const double s = 2.0 * i;
    level.track.points.push_back(
        {line.x + fx * s, line.z + fz * s, y_at(s), s, line.heading, r.width});
  }
  const double length = level.track.points.back().s;
  level.track.length = length;
  // THE GROUND graded to the profile across the venue, eased out past it, and under a raised
  // stretch LOWERED to the ground under the structure (never raised to it), the cut banked out
  // the wider the deeper it is.
  const double half = r.width / 2;
  ge::core::Heightfield& ground = level.ground;
  ge::core::Heightfield& packed = level.packed;
  const double fade = rules::track::shoulder_packed;
  const VenueProfile::Under* under = p.under ? &*p.under : nullptr;
  const auto gap_at = [&](double d) {
    if (!under) return 0.0;
    const double last = double(under->y.size() - 1);
    const double q = clamp(d / p.dx, 0, last);
    const size_t i = std::min(under->y.size() - 2, size_t(q));
    const double u = under->y[i] + (under->y[i + 1] - under->y[i]) * (q - double(i));
    return num::max(0.0, profile_height_at(p, d) - u);
  };
  double deepest = 0;
  if (under) {
    const int metres = int(std::floor(p.end));
    for (int d = 0; d <= metres; d++) deepest = num::max(deepest, gap_at(d));
  }
  const double bank = under ? under->bank : 0;
  const double top = r.ease + bank * gap_at(0);
  const double reach = half + num::max(r.ease + bank * deepest, fade) + 2;
  const std::array<std::array<double, 2>, 4> corners{
      {{-top, -reach}, {-top, reach}, {length + r.ease, -reach}, {length + r.ease, reach}}};
  double x_lo = std::numeric_limits<double>::infinity();
  double x_hi = -x_lo;
  double z_lo = x_lo;
  double z_hi = -x_lo;
  for (const auto& [a, c] : corners) {
    const double x = line.x + fx * a + rx * c;
    const double z = line.z + fz * a + rz * c;
    x_lo = num::min(x_lo, x);
    x_hi = num::max(x_hi, x);
    z_lo = num::min(z_lo, z);
    z_hi = num::max(z_hi, z);
  }
  const double cell = ground.cell;
  const int c0 = int(clamp(std::floor((x_lo - ground.origin_x) / cell), 0, ground.cols - 1));
  const int c1 = int(clamp(std::ceil((x_hi - ground.origin_x) / cell), 0, ground.cols - 1));
  const int r0 = int(clamp(std::floor((z_lo - ground.origin_z) / cell), 0, ground.rows - 1));
  const int r1 = int(clamp(std::ceil((z_hi - ground.origin_z) / cell), 0, ground.rows - 1));
  const bool grooms = packed.cols == ground.cols && packed.rows == ground.rows;
  for (int row = r0; row <= r1; row++) {
    for (int col = c0; col <= c1; col++) {
      const double x = ground.origin_x + col * cell;
      const double z = ground.origin_z + row * cell;
      const double dx = x - line.x;
      const double dz = z - line.z;
      const double along = dx * fx + dz * fz;
      const double across = std::fabs(dx * rx + dz * rz);
      const double ends =
          smoothstep(-top, 0, along) * (1 - smoothstep(length, length + r.ease, along));
      if (ends <= 0) continue;
      const size_t i = size_t(row) * size_t(ground.cols) + size_t(col);
      const double at = clamp(along, 0, length);
      const double gap = gap_at(at);
      const double ease = r.ease + bank * gap;
      const double w = (1 - smoothstep(half, half + ease, across)) * ends;
      const double want = y_at(at) - gap;
      const double was = ground.data[i];
      const double to = gap > 0 ? num::min(was, want) : want;
      if (w > 0) ground.data[i] = static_cast<float>(was + w * (to - was));
      if (grooms) {
        // Under a structure the lowered ground is no piste; its deck is.
        const double deck = under ? 1 - smoothstep(under->half, under->half + 1, across) : 1;
        const double kept = num::max(deck, 1 - smoothstep(0.5, 2, gap));
        const double firm = (1 - smoothstep(half, half + fade, across)) * ends * kept;
        if (firm > packed.data[i]) packed.data[i] = static_cast<float>(firm);
      }
    }
  }
  // THE WOODS and the finish arena cleared; the piste's kickers and drops in the cut taken out
  // with the ground they stood on.
  const venue_rules::Arena& arena = r.arena;
  const auto off = [&](double x, double z, double margin) {
    const double dx = x - line.x;
    const double dz = z - line.z;
    const double along = dx * fx + dz * fz;
    const double across = std::fabs(dx * rx + dz * rz);
    if (along > -20 && along < length + half && across < half + margin) return true;
    return along > p.finish - arena.before && along < p.finish + arena.past && across < arena.half;
  };
  std::erase_if(level.trees, [&](const TreeDef& t) { return off(t.x, t.z, r.margin); });
  std::erase_if(level.kickers, [&](const Kicker& k) { return off(k.x, k.z, r.ease + k.ramp); });
  for (Kicker& k : level.kickers) {
    k.on_track = false;
    k.s = std::nullopt;
  }
  std::erase_if(level.cliffs, [&](const Cliff& c) { return off(c.x, c.z, r.ease + c.width / 2); });
  for (Cliff& c : level.cliffs) {
    c.on_track = false;
    c.s = std::nullopt;
  }
  level.drifts.clear();
  level.race.reset();
  level.ski_jump.reset();
  level.venue.reset();
  return GradedVenue{line, fx, fz, rx, rz, std::move(level)};
}

}  // namespace ge::mountain::mapgen
