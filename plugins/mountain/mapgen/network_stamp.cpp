// The runs pressed into the ground: each run's corridor indexed over its own stretch of the
// grid, then levelled, banked, windrowed and packed off that index.
//
// ge:summary Pressing a graded run: its corridor's nearest-segment index, then the level and pack
// ge:keywords network-stamp stamp-run past-junction level-at corridor-index core bench
// ge:skill mountain
// ge:test tests/mountain_resort_build_test.cpp
#include "mountain/mapgen/network_stamp.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "core/num.hpp"
#include "core/parallel.hpp"
#include "mountain/mapgen/berm.hpp"
#include "mountain/mapgen/drift.hpp"
#include "mountain/mapgen/rules.hpp"

namespace ge::mountain::mapgen {

namespace libm = ge::core::libm;
namespace num = ge::core::num;
namespace T = rules::track;
using ge::core::angle_diff;
using ge::core::clamp;
using ge::core::hypot;
using ge::core::smoothstep;

namespace {

constexpr float kFar = std::numeric_limits<float>::infinity();
constexpr uint8_t kCore = 2;
constexpr uint8_t kBenchCell = 1;
// The furthest along its line a cell by a station is levelled to, m: a station's stride and a
// half, so the inside of a tight bend never reaches across the bend's centre.
constexpr double kArcMost = 1.5 * T::step;
// How far down the run it merges into past the junction a merging run's corridor may still
// reach, m; how far up and down that run its stations are searched, and the stride of the
// first, coarse pass.
constexpr double kOpenPast = 24;
constexpr long kOpenSearch = 150;
constexpr long kOpenStride = 4;

double sq(double v) { return v * v; }

// THE JUNCTION'S END: whether a cell by station `i` of a merging run lies along the run it merges
// into further down than the junction (the other's own bench and bank, where it bends back
// under this run's corridor). Never for a station before the two corridors touch.
struct PastJunction {
  const WalkedRun& run;
  const WalkedRun* onto;
  long lo = 0, hi = 0;
  double at = 0;

  PastJunction(const WalkedRun& r, const WalkedRun* o) : run(r), onto(o) {
    if (!onto || !run.into) return;
    const auto& tp = onto->track.points;
    const double step = onto->track.length / std::max(1.0, double(tp.size()) - 1);
    at = run.into->s;
    const long j = long(num::round(at / step));
    lo = std::max(0L, j - kOpenSearch);
    hi = std::min(long(tp.size()) - 1, j + kOpenSearch);
  }

  bool operator()(size_t i, double x, double z) const {
    if (!onto || !run.into || run.track.points[i].s < run.merge_from) return false;
    const auto& tp = onto->track.points;
    const auto d2 = [&](long q) { return sq(tp[size_t(q)].x - x) + sq(tp[size_t(q)].z - z); };
    double best = std::numeric_limits<double>::infinity();
    long k = lo;
    for (long q = lo; q <= hi; q += kOpenStride) {
      const double d = d2(q);
      if (d < best) {
        best = d;
        k = q;
      }
    }
    const long k1 = std::min(hi, k + kOpenStride);
    for (long q = std::max(lo, k - kOpenStride); q <= k1; q++) {
      const double d = d2(q);
      if (d < best) {
        best = d;
        k = q;
      }
    }
    return tp[size_t(k)].s > at + kOpenPast;
  }
};

}  // namespace

NetworkStamp network_stamp(const ge::core::Heightfield& ground) {
  const size_t cells = size_t(ground.cols) * size_t(ground.rows);
  return {std::vector<uint8_t>(cells, 0), std::vector<float>(cells, kFar)};
}

bool on_core(const NetworkStamp& shared, const ge::core::Heightfield& ground, double x, double z) {
  const double c = num::round(x / ground.cell);
  const double r = num::round(z / ground.cell);
  if (c < 0 || r < 0 || c >= ground.cols || r >= ground.rows) return false;
  return shared.core[size_t(r) * size_t(ground.cols) + size_t(c)] == kCore;
}

void stamp_run(const WalkedRun& run, ge::core::Heightfield& ground, ge::core::Heightfield& packed,
               NetworkStamp& shared, std::span<const Drift> drifts, const WalkedRun* onto) {
  const auto& pts = run.track.points;
  const size_t n = pts.size();
  const double cell = ground.cell;
  const long cols = ground.cols;
  const long rows = ground.rows;
  const double reach_max = kWidest / 2 + T::shoulder_flat + rules::berm::width + T::bank_max;
  // The run's own stretch of the grid.
  double x0 = std::numeric_limits<double>::infinity();
  double x1 = -x0;
  double z0 = x0;
  double z1 = -x0;
  for (const TrackPoint& p : pts) {
    x0 = std::min(x0, p.x);
    x1 = std::max(x1, p.x);
    z0 = std::min(z0, p.z);
    z1 = std::max(z1, p.z);
  }
  const long c0 = std::max(0L, long(std::floor((x0 - reach_max) / cell)));
  const long c1 = std::min(cols - 1, long(std::ceil((x1 + reach_max) / cell)));
  const long r0 = std::max(0L, long(std::floor((z0 - reach_max) / cell)));
  const long r1 = std::min(rows - 1, long(std::ceil((z1 + reach_max) / cell)));
  if (c1 < c0 || r1 < r0) return;
  const long bw = c1 - c0 + 1;
  const size_t local = size_t(bw) * size_t(r1 - r0 + 1);
  std::vector<float> dist(local, kFar);
  std::vector<int32_t> near(local, -1);
  std::vector<float> along(local, 0);
  const bool ends_inside = run.into.has_value();
  std::vector<double> fx(n), fz(n), kappa(n);
  for (size_t i = 0; i < n; i++) {
    fx[i] = libm::sin(pts[i].heading);
    fz[i] = libm::cos(pts[i].heading);
    const TrackPoint& a = pts[i == 0 ? 0 : i - 1];
    const TrackPoint& b = pts[std::min(n - 1, i + 1)];
    kappa[i] = angle_diff(a.heading, b.heading) / std::max(1e-9, b.s - a.s);
  }
  const TrackPoint& last = pts[n - 1];
  const auto past_end = [&](double x, double z) {
    return (x - last.x) * fx[n - 1] + (z - last.z) * fz[n - 1] > 0;
  };
  const auto y_at = [&](double s) {
    const double u = (s / run.track.length) * double(n - 1);
    const size_t i = size_t(clamp(std::floor(u), 0, double(n - 2)));
    return pts[i].y + (pts[i + 1].y - pts[i].y) * (u - double(i));
  };
  const auto level_at = [&](size_t k, double x, double z, double hw) {
    const TrackPoint& p = pts[k];
    const double vx = x - p.x;
    const double vz = z - p.z;
    const double a = vx * fx[k] + vz * fz[k];
    const double lateral = vx * fz[k] - vz * fx[k];
    const double c = kappa[k];
    double ds = a;
    if (std::abs(c) > 1e-4) {
      const double sgn = c > 0 ? 1 : -1;
      const double radius = 1 / std::abs(c);
      ds = clamp(libm::atan2(a, radius - sgn * lateral) * radius, -kArcMost, kArcMost);
    }
    return y_at(p.s + ds) + run.cross[k] * clamp(lateral, -hw, hw);
  };
  // The nearest segment to every cell of the stretch, its distance and how far along it. A cell's
  // answer hangs only on the segments read in order, so bands of rows are read side by side, each
  // reading every segment in order over its own rows.
  ge::core::parallel_bands(size_t(r1 - r0 + 1), [&](size_t lo, size_t hi) {
    const long band0 = r0 + long(lo);
    const long band1 = r0 + long(hi) - 1;
    for (size_t i = 0; i + 1 < n; i++) {
      const TrackPoint& a = pts[i];
      const TrackPoint& b = pts[i + 1];
      const double dx = b.x - a.x;
      const double dz = b.z - a.z;
      const double len2 = dx * dx + dz * dz > 0 ? dx * dx + dz * dz : 1;
      const bool end_stretch = ends_inside && b.s > run.track.length - 2 * reach_max;
      const double reach =
          std::max(a.width, b.width) / 2 + T::shoulder_flat + rules::berm::width + T::bank_max;
      const long sc0 = std::max(c0, long(std::floor((std::min(a.x, b.x) - reach) / cell)));
      const long sc1 = std::min(c1, long(std::ceil((std::max(a.x, b.x) + reach) / cell)));
      const long sr0 = std::max(band0, long(std::floor((std::min(a.z, b.z) - reach) / cell)));
      const long sr1 = std::min(band1, long(std::ceil((std::max(a.z, b.z) + reach) / cell)));
      for (long r = sr0; r <= sr1; r++) {
        const double z = double(r) * cell;
        for (long c = sc0; c <= sc1; c++) {
          const double x = double(c) * cell;
          // A run that merges ends inside the other: nothing of its last stretch past its last
          // station, where the other's surface runs on. (Only its last stretch: a run that turns
          // back across the fall line has earlier stretches beyond that plane too.)
          if (end_stretch && past_end(x, z)) continue;
          double t = ((x - a.x) * dx + (z - a.z) * dz) / len2;
          t = t < 0 ? 0 : t > 1 ? 1 : t;
          const double ex = x - (a.x + dx * t);
          const double ez = z - (a.z + dz * t);
          const size_t o = size_t(r - r0) * size_t(bw) + size_t(c - c0);
          const double held = dist[o];
          if (ex * ex + ez * ez > held * held * (1 + 1e-5)) continue;
          const double d = hypot(ex, ez);
          if (d < held) {
            dist[o] = float(d);
            near[o] = int32_t(i);
            along[o] = float(t);
          }
        }
      }
    }
  });
  float* g = ground.data.data();
  float* p = packed.data.data();
  const PastJunction past(run, onto);
  // Every cell is levelled off its own answer and its own heights alone, so the rows are
  // levelled side by side too.
  ge::core::parallel_bands(size_t(r1 - r0 + 1), [&](size_t lo, size_t hi) {
    for (long r = r0 + long(lo); r < r0 + long(hi); r++) {
      for (long c = c0; c <= c1; c++) {
        const size_t o = size_t(r - r0) * size_t(bw) + size_t(c - c0);
        const double d = dist[o];
        if (!(d <= reach_max)) continue;
        const size_t go = size_t(r) * size_t(cols) + size_t(c);
        const size_t i = size_t(near[o]);
        const double t = along[o];
        const double hw = (pts[i].width + (pts[i + 1].width - pts[i].width) * t) / 2;
        // The crust keeps off a run by its edge (R21), read as a race piste's distance from its
        // line whatever this run's width.
        const double edge = d - hw + T::width.max / 2;
        if (edge < shared.dist[go]) shared.dist[go] = float(edge);
        const double x = double(c) * cell;
        const double z = double(r) * cell;
        if (past(i, x, z)) {
          near[o] = -1;
          continue;
        }
        const double pk0 = d <= hw ? 1 : 1 - smoothstep(hw, hw + T::shoulder_packed, d);
        if (shared.core[go] == kCore) {
          // Another run's surface: its levelling and windrow are its own, but the groomer runs
          // on across the seam between the two.
          if (pk0 > p[go]) p[go] = float(pk0);
          continue;
        }
        const double toe = hw + T::shoulder_flat;
        const double flat = toe + rules::berm::width;
        // Another run's windrow is levelled only by this one's own corridor (a junction opens
        // it), never by the bank behind it.
        if (shared.core[go] == kBenchCell && d > flat) continue;
        const double delta = level_at(t < 0.5 ? i : i + 1, x, z, hw) - g[go];
        const double bank =
            std::min(T::bank_max, std::max(T::bank_min, std::abs(delta) / T::bank_slope));
        const double w = d <= flat ? 1 : 1 - smoothstep(flat, flat + bank, d);
        double y = g[go] + w * delta;
        const double s = pts[i].s + (pts[i + 1].s - pts[i].s) * t;
        if (d > toe && d < flat) y += berm_profile(berm_crest(s), d - toe);
        g[go] = float(y);
        double pk = pk0;
        if (pk > 0 && !drifts.empty()) {
          const double wd = drift_at(drifts, s);
          if (wd > 0) pk *= 1 - (1 - rules::drift::packed) * wd;
        }
        if (pk > p[go]) p[go] = float(pk);
      }
    }
  });
  if (run.spec.kind == RunKind::road) return;
  for (long r = r0; r <= r1; r++) {
    for (long c = c0; c <= c1; c++) {
      const size_t o = size_t(r - r0) * size_t(bw) + size_t(c - c0);
      if (near[o] < 0) continue;
      const size_t go = size_t(r) * size_t(cols) + size_t(c);
      const size_t i = size_t(near[o]);
      const double hw = (pts[i].width + (pts[i + 1].width - pts[i].width) * along[o]) / 2;
      if (dist[o] <= hw + T::shoulder_flat) {
        shared.core[go] = kCore;
      } else if (dist[o] <= hw + T::shoulder_flat + rules::berm::width && shared.core[go] == 0) {
        shared.core[go] = kBenchCell;
      }
    }
  }
}

}  // namespace ge::mountain::mapgen
