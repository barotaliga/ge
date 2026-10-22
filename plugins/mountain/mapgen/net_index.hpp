// R27 — THE NETWORK'S INDEX: every run of a resort laid so far, hashed by its segments, and the
// one question every walk, lane and check asks of it: "which other run is nearest here?".
//
// A segment's geometry is kept as it was added (a run's line never moves once laid; only its
// heights and widths are read again, through the run's own points), in flat arrays. A question
// scans the cells ring by ring outward from the point and stops once nothing further could be
// nearer, so the cell order fixes which of two equally near segments is taken (the first),
// and the answer never hangs on the order the cells were filled in. `skip` is a template parameter
// so the scan inlines it.
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
    // Cells outside the box every segment so far lies in are empty, so each ring is walked
    // only where it crosses the box, in the same column-then-row order as the whole ring.
    const int bc0 = c0_ - qc;
    const int bc1 = c1_ - qc;
    const int br0 = r0_ - qr;
    const int br1 = r1_ - qr;
    const auto cell = [&](int dc, int dr) {
      if (const std::vector<int>* list = this->list(qc + dc, qr + dr))
        scan(*list, x, z, within, skip, out);
    };
    for (int ring = 0; ring <= rings; ring++) {
      const int dc_hi = std::min(ring, bc1);
      for (int dc = std::max(-ring, bc0); dc <= dc_hi; dc++) {
        if (dc == -ring || dc == ring) {
          const int dr_hi = std::min(ring, br1);
          for (int dr = std::max(-ring, br0); dr <= dr_hi; dr++) cell(dc, dr);
        } else {
          if (-ring >= br0 && -ring <= br1) cell(dc, -ring);
          if (ring >= br0 && ring <= br1) cell(dc, ring);
        }
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
  std::vector<std::vector<int>> grid_;  // empty list = no cell
  std::unordered_map<int64_t, std::vector<int>> cells_;
  std::vector<int> seg_run_;
  std::vector<int> seg_index_;
  std::vector<double> seg_geo_;  // per segment: a.x, a.z, dx, dz, len2, a.s

  // The segments in cell (c, r), or null for an empty one.
  const std::vector<int>* list(int c, int r) const {
    const int gc = c + kGridMargin;
    const int gr = r + kGridMargin;
    if (gc >= 0 && gc < side_ && gr >= 0 && gr < side_) {
      const std::vector<int>& l = grid_[size_t(gr) * size_t(side_) + size_t(gc)];
      return l.empty() ? nullptr : &l;
    }
    const auto it = cells_.find(ge::core::cell_key(c, r));
    return it == cells_.end() ? nullptr : &it->second;
  }
  std::vector<int>& slot(int c, int r);

  template <class Skip>
  void scan(const std::vector<int>& list, double x, double z, double within, Skip& skip,
            NetHit& out) const {
    const double* geo = seg_geo_.data();
    const double most = within * within;
    for (const int g : list) {
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
      if (d2 >= out.distance * out.distance || d2 > most) continue;
      const int run = seg_run_[size_t(g)];
      if (skip(run, o[5])) continue;
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
  }
};

}  // namespace ge::mountain::mapgen
