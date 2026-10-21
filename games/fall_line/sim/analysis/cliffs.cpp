// Port of game5 `engine/analysis/cliffs.ts` (single piste).
//
// ge:summary The cliff and drop checks' readings of the ground
// ge:keywords analysis checkCliffs checkDrops
// ge:oracle game5/engine/analysis/cliffs.ts
// ge:skill sim
// ge:test tests/mapgen_generate_test.cpp
#include "analysis/cliffs.hpp"

#include <cmath>

#include "core/js.hpp"
#include "core/libm/libm.hpp"
#include "mapgen/cliffs.hpp"
#include "mapgen/grades.hpp"
#include "mapgen/query.hpp"
#include "mapgen/regions.hpp"

namespace fall_line::analysis {

namespace js = ge::core::js;
namespace libm = ge::core::libm;
namespace R = mapgen::rules;
using mapgen::Cliff;
using mapgen::Kicker;
using mapgen::Level;
using mapgen::TrackPoint;

namespace {

// R22, R24 — the least share of its own drop a face must still show on the ground.
constexpr double kDropShown = 0.75;

}  // namespace

void check_cliffs(const Level& level, const Add& add) {
  const auto& G = mapgen::grade_row(level.grade).cliffs;
  const double clear = R::track::width.max / 2 + G.clearance;
  int count = 0;
  for (const Cliff& c : level.cliffs) {
    if (c.on_track) continue;
    count++;
    const double fx = libm::sin(c.heading);
    const double fz = libm::cos(c.heading);
    const double top = level.ground_at(c.x - fx, c.z - fz);
    const double foot = level.ground_at(c.x + fx * (c.face + 1), c.z + fz * (c.face + 1));
    if (top - foot < c.drop * kDropShown) {
      add("R22", Severity::warn,
          c.id + " drops only " + fmt(top - foot) + " m of its " + fmt(c.drop));
    }
    double nearest = INFINITY;
    for (const mapgen::PlanPoint& p : mapgen::cliff_footprint(c)) {
      nearest = js::min(nearest, mapgen::nearest_track_point(level.track, p.x, p.z).distance);
    }
    if (nearest < clear - 1) {
      add("R22", Severity::error,
          c.id + " comes " + fmt(nearest, 0) + " m from the piste's centreline");
    }
  }
  const auto& region = mapgen::region_row(level.region);
  if (count < mapgen::scale_count(R::cliff::count, region.kickers * G.count).min) {
    add("R22", Severity::warn, "only " + num(count) + " cliff(s)");
  }
}

void check_drops(const Level& level, std::span<const Kicker* const> kickers, const Add& add) {
  namespace D = R::drop;
  const mapgen::Band band = mapgen::grade_row(level.grade).drops;
  std::vector<const Cliff*> drops;
  for (const Cliff& c : level.cliffs) {
    if (c.on_track) drops.push_back(&c);
  }
  if (!mapgen::within_band(double(drops.size()), band)) {
    add("R24", Severity::error,
        num(double(drops.size())) + " drop(s) across the piste (band " + num(band.min) + "–" +
            num(band.max) + ")");
  }
  for (size_t a = 0; a < drops.size(); a++) {
    const Cliff& d = *drops[a];
    const double s0 = d.s.value_or(0);
    const TrackPoint above = mapgen::track_point_at(level.track, s0 - 1);
    const TrackPoint below = mapgen::track_point_at(level.track, s0 + d.face + 1);
    const double shown = level.ground_at(above.x, above.z) - level.ground_at(below.x, below.z);
    if (shown < d.drop * kDropShown) {
      add("R24", Severity::error, d.id + " drops only " + fmt(shown) + " m of its " + fmt(d.drop));
    }
    if (!mapgen::within_band(d.drop, D::drop) || !mapgen::within_band(d.shelf, D::shelf)) {
      add("R24", Severity::error,
          d.id + ": a " + fmt(d.drop) + " m drop off a " + fmt(d.shelf, 0) + " m shelf");
    }
    const double from = s0 - d.shelf;
    const double to = s0 + d.face + d.landing;
    for (const mapgen::Checkpoint& c : level.checkpoints) {
      if (c.s > from - D::gate_clear + 1 && c.s < to + D::gate_clear - 1) {
        add("R24", Severity::error,
            d.id + " at s " + fmt(s0, 0) + " m stands on the gate at " + fmt(c.s, 0) + " m");
        break;
      }
    }
    for (const Kicker* k : kickers) {
      const double ks = k->s.value_or(0);
      if (from < ks + k->landing + D::kicker_clear - 1 && to > ks - k->ramp - D::kicker_clear + 1) {
        add("R24", Severity::error, d.id + " stands on " + k->id + "'s ramp or landing");
      }
    }
    for (size_t b = a + 1; b < drops.size(); b++) {
      const double ds = std::abs(drops[b]->s.value_or(0) - s0);
      if (ds < D::spacing - 1) {
        add("R24", Severity::error,
            d.id + " and " + drops[b]->id + " stand " + fmt(ds, 0) + " m apart");
      }
    }
  }
}

}  // namespace fall_line::analysis
