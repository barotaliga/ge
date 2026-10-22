// R27 — THE NETWORK'S INDEX: every run of a resort laid so far, hashed by its segments, and the
// one question every walk, lane and check asks of it: "which other run is nearest here?".
//
// A segment's geometry is kept as it was added (a run's line never moves once laid; only its
// heights and widths are read again, through the run's own points), in flat arrays. A question
// scans the cells ring by ring outward from the point and stops once nothing further could be
// nearer, so the cell order fixes which of two equally near segments is taken (the first),
// and the answer never hangs on the order the cells were filled in. A cell keeps its segments as
// SPANS of a run's consecutive ones with the box round them, so a span too far off to hold a
// nearer segment is passed over whole; the segments read are read in the same order, so no answer
// moves. `skip` is a template parameter so the scan inlines it.
//
// Held by tests/mountain_resort_build_test.cpp (the walk stage).
//
// ge:summary R27 the runs' spatial hash: the nearest other run to a point, and what covers it
// ge:keywords net-index nearest covers spatial-hash segments ring-scan ring-walk
// ge:skill mountain
// ge:test tests/mountain_resort_build_test.cpp
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <unordered_map>
#include <vector>

#include "core/math.hpp"
#include "core/num.hpp"
#include "mountain/mapgen/resort_rules.hpp"
#include "mountain/mapgen/rules.hpp"
#include "mountain/mapgen/types.hpp"

namespace ge::mountain::mapgen {

// The answer to "which other run is nearest here?".
struct NetHit {
  int run = -1;
  int index = 0;
  double s = 0;
  double distance = std::numeric_limits<double>::infinity();
  double lateral = 0;  // signed: positive to the right of the other run's travel, m
  double heading = 0;
  double width = 0;
  double x = 0;  // the nearest point on the other's centreline
  double z = 0;
};

class NetIndex {
 public:
  static constexpr double kCell = 24;
  static constexpr int kGridMargin = 24;
  // Half the widest a run is anywhere (R27's `piste.most`), m.
  static constexpr double kWidestHalf = resort_rules::piste::most / 2;
  static constexpr double kStopSlack = 1e-6;
  // The most segments a cell keeps in one span: a span this long is still short enough that its
  // box sits close round it.
  static constexpr int kSpan = 8;

  struct Run {
    const std::vector<TrackPoint>* points;  // the run's own, read live
    int rank;
    int into;
    double from;
    bool road;
  };
  std::vector<Run> runs;

  NetIndex();

  // Add a run's line (the vector must outlive the index and never reallocate); `rank` is how
  // hard it was built to, `into` the run it merges into from arc `from` on, `road` a lane.
  int add(const std::vector<TrackPoint>& points, int rank = 0, int into = -1, double from = 0,
          bool road = false);

  // The nearest segment of any run within `within` m of (x, z) that `skip(run, s)` lets through;
  // `out.distance` is infinity when none is.
  template <class Skip>
  NetHit& nearest(double x, double z, double within, Skip&& skip, NetHit& out) const {
    out.distance = std::numeric_limits<double>::infinity();
    const int rings = static_cast<int>(std::ceil(within / kCell)) + 1;
    const int qc = static_cast<int>(std::floor(x / kCell));
    const int qr = static_cast<int>(std::floor(z / kCell));
    const double left = x - qc * kCell;
    const double right = (qc + 1) * kCell - x;
    const double top = z - qr * kCell;
    const double bottom = (qr + 1) * kCell - z;
    const double inside =
        ge::core::num::min(ge::core::num::min(left, right), ge::core::num::min(top, bottom));
    // Cells outside the box every segment so far lies in are empty, and so are a row's cells
    // outside the columns it holds segments in (a column's outside its rows), so each ring is
    // walked only where it crosses them, in the same column-then-row order as the whole ring.
    const int bc0 = c0_ - qc;
    const int bc1 = c1_ - qc;
    const auto cell = [&](int dc, int dr) {
      if (const std::vector<Span>* list = this->list(qc + dc, qr + dr))
        scan(*list, x, z, within, skip, out);
    };
    const auto side = [&](int ring, int dc) {
      const Range rows = rows_of(qc + dc);
      const int dr_hi = std::min(ring, rows.hi - qr);
      for (int dr = std::max(-ring, rows.lo - qr); dr <= dr_hi; dr++) cell(dc, dr);
    };
    for (int ring = 0; ring <= rings; ring++) {
      const int lo = std::max(-ring, bc0);
      const int hi = std::min(ring, bc1);
      if (lo == -ring && lo <= hi) side(ring, -ring);
      if (ring > 0) {
        const Range near = cols_of(qr - ring);
        const Range far = cols_of(qr + ring);
        const Range both = near.lo > near.hi ? far
                           : far.lo > far.hi
                               ? near
                               : Range{std::min(near.lo, far.lo), std::max(near.hi, far.hi)};
        const int in_hi = std::min({hi, ring - 1, both.hi - qc});
        for (int dc = std::max({lo, 1 - ring, both.lo - qc}); dc <= in_hi; dc++) {
          if (qc + dc >= near.lo && qc + dc <= near.hi) cell(dc, -ring);
          if (qc + dc >= far.lo && qc + dc <= far.hi) cell(dc, ring);
        }
        if (hi == ring && lo <= hi) side(ring, ring);
      }
      const double beyond = ring * kCell + inside - kStopSlack;
      if (out.distance <= beyond || beyond > within) break;
    }
    return out;
  }

  // Whether (x, z) stands within some run's half-width and `pad` m of its centreline: ANY
  // run's, not only the nearest's.
  bool covers(double x, double z, double pad, NetHit& out) const;

 private:
  int side_;
  // The box, in cells, every segment added so far lies in; empty (c0_ > c1_) before the first.
  int c0_ = 1, c1_ = 0, r0_ = 1, r1_ = 0;
  // A grid row's occupied columns and a grid column's occupied rows, in cells (empty: lo > hi);
  // once a segment reaches past the grid, every row and column answers the whole box instead.
  struct Range {
    int lo = 1, hi = 0;
  };
  std::vector<Range> row_cols_;
  std::vector<Range> col_rows_;
  bool outside_ = false;
  Range cols_of(int r) const {
    const int gr = r + kGridMargin;
    if (outside_) return {c0_, c1_};
    return gr >= 0 && gr < side_ ? row_cols_[size_t(gr)] : Range{};
  }
  Range rows_of(int c) const {
    const int gc = c + kGridMargin;
    if (outside_) return {r0_, r1_};
    return gc >= 0 && gc < side_ ? col_rows_[size_t(gc)] : Range{};
  }
  // A cell's run of consecutive segments [g0, g1) and the box round them.
  struct Span {
    int g0, g1;
    double x0, z0, x1, z1;
  };
  std::vector<std::vector<Span>> grid_;  // empty list = no cell
  std::unordered_map<int64_t, std::vector<Span>> cells_;
  std::vector<int> seg_run_;
  std::vector<int> seg_index_;
  std::vector<double> seg_geo_;  // per segment: a.x, a.z, dx, dz, len2, a.s

  // The segments in cell (c, r), or null for an empty one.
  const std::vector<Span>* list(int c, int r) const {
    const int gc = c + kGridMargin;
    const int gr = r + kGridMargin;
    if (gc >= 0 && gc < side_ && gr >= 0 && gr < side_) {
      const std::vector<Span>& l = grid_[size_t(gr) * size_t(side_) + size_t(gc)];
      return l.empty() ? nullptr : &l;
    }
    const auto it = cells_.find(ge::core::cell_key(c, r));
    return it == cells_.end() ? nullptr : &it->second;
  }
  std::vector<Span>& slot(int c, int r);

  template <class Skip>
  void scan(const std::vector<Span>& list, double x, double z, double within, Skip& skip,
            NetHit& out) const {
    const double most = within * within;
    for (const Span& span : list) {
      // Every segment of the span lies at least this far (squared) off: past both bounds, by a
      // margin far wider than the rounding of either side, none of them could be taken.
      const double ox = x < span.x0 ? span.x0 - x : x > span.x1 ? x - span.x1 : 0;
      const double oz = z < span.z0 ? span.z0 - z : z > span.z1 ? z - span.z1 : 0;
      const double bound = ge::core::num::min(out.distance * out.distance, most);
      if (ox * ox + oz * oz > bound * (1 + 1e-9) + 1e-9) continue;
      for (int g = span.g0; g < span.g1; g++) segment(g, x, z, most, skip, out);
    }
  }

  // One segment against the nearest so far: taken when it is nearer, within reach and let through.
  template <class Skip>
  void segment(int g, double x, double z, double most, Skip& skip, NetHit& out) const {
    const double* geo = seg_geo_.data();
    const double* o = geo + static_cast<size_t>(g) * 6;
    const double ax = o[0];
    const double az = o[1];
    const double dx = o[2];
    const double dz = o[3];
    const double len2 = o[4];
    double t = ((x - ax) * dx + (z - az) * dz) / len2;
    t = t < 0 ? 0 : t > 1 ? 1 : t;
    const double ex = x - (ax + dx * t);
    const double ez = z - (az + dz * t);
    const double d2 = ex * ex + ez * ez;
    if (d2 >= out.distance * out.distance || d2 > most) return;
    const int run = seg_run_[size_t(g)];
    if (skip(run, o[5])) return;
    const int index = seg_index_[size_t(g)];
    const std::vector<TrackPoint>& pts = *runs[size_t(run)].points;
    const TrackPoint& a = pts[size_t(index)];
    const TrackPoint& b = pts[size_t(index) + 1];
    const double d = std::sqrt(d2);
    const double len = std::sqrt(len2);
    out.run = run;
    out.index = index;
    out.s = a.s + (b.s - a.s) * t;
    out.distance = d;
    out.lateral = (ex * dz - ez * dx) / len;
    out.heading = a.heading + ge::core::angle_diff(a.heading, b.heading) * t;
    out.width = a.width + (b.width - a.width) * t;
    out.x = ax + dx * t;
    out.z = az + dz * t;
  }
};

}  // namespace ge::mountain::mapgen
