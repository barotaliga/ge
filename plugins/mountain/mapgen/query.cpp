// The definitions behind query.hpp.
//
// ge:summary The piste's lazily built spatial hash and the nearest-point searches
// ge:keywords query spatial-hash
// ge:skill mountain
// ge:test tests/mountain_generate_test.cpp
#include "mountain/mapgen/query.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>

#include "core/math.hpp"
#include "core/num.hpp"

namespace ge::mountain::mapgen {

namespace num = ge::core::num;

namespace {

constexpr double kInf = std::numeric_limits<double>::infinity();

// Hash cell, m: several segments a cell and a handful of cells a query.
constexpr double kCell = 24;

// How many rings a query walks before it gives up on the hash and walks the whole piste.
constexpr int kMaxRings = 90;

// A segment's cells: its bounding box's columns and rows.
struct SegCells {
  int32_t c0, c1, r0, r1;
};

SegCells seg_cells(const TrackPoint& a, const TrackPoint& b) {
  return {static_cast<int32_t>(std::floor(num::min(a.x, b.x) / kCell)),
          static_cast<int32_t>(std::floor(num::max(a.x, b.x) / kCell)),
          static_cast<int32_t>(std::floor(num::min(a.z, b.z) / kCell)),
          static_cast<int32_t>(std::floor(num::max(a.z, b.z) / kCell))};
}

// Two passes over the segments: count each cell's, then place them, in index order.
const TrackIndex& index_of(const Track& track) {
  if (track.index) return *track.index;
  auto index = std::make_shared<TrackIndex>();
  const auto& points = track.points;
  const size_t n = points.size();
  if (n >= 2) {
    SegCells box = seg_cells(points[0], points[1]);
    for (size_t i = 1; i + 1 < n; i++) {
      const SegCells s = seg_cells(points[i], points[i + 1]);
      box = {std::min(box.c0, s.c0), std::max(box.c1, s.c1), std::min(box.r0, s.r0),
             std::max(box.r1, s.r1)};
    }
    index->c0 = box.c0;
    index->r0 = box.r0;
    index->cols = box.c1 - box.c0 + 1;
    index->rows = box.r1 - box.r0 + 1;
    const auto cols = size_t(index->cols);
    std::vector<int32_t>& start = index->start;
    start.assign(cols * size_t(index->rows) + 1, 0);
    const auto each_cell = [&](size_t i, auto&& fn) {
      const SegCells s = seg_cells(points[i], points[i + 1]);
      for (int32_t c = s.c0; c <= s.c1; c++) {
        for (int32_t r = s.r0; r <= s.r1; r++) {
          fn(size_t(r - box.r0) * cols + size_t(c - box.c0));
        }
      }
    };
    for (size_t i = 0; i + 1 < n; i++) each_cell(i, [&](size_t k) { start[k + 1]++; });
    for (size_t k = 1; k < start.size(); k++) start[k] += start[k - 1];
    index->segs.resize(size_t(start.back()));
    std::vector<int32_t> fill(start.begin(), start.end() - 1);
    for (size_t i = 0; i + 1 < n; i++) {
      each_cell(i, [&](size_t k) { index->segs[size_t(fill[k]++)] = int32_t(i); });
    }
  }
  track.index = std::move(index);
  return *track.index;
}

// Project (x, z) onto segment i → i+1, writing into `hit` if nearer.
void try_segment(const std::vector<TrackPoint>& points, size_t i, double x, double z,
                 TrackHit& hit) {
  const TrackPoint& a = points[i];
  const TrackPoint& b = points[i + 1];
  const double dx = b.x - a.x;
  const double dz = b.z - a.z;
  const double len2 = dx * dx + dz * dz;
  double t = len2 > 0 ? ((x - a.x) * dx + (z - a.z) * dz) / len2 : 0;
  t = t < 0 ? 0 : t > 1 ? 1 : t;
  const double px = a.x + dx * t;
  const double pz = a.z + dz * t;
  const double ex = x - px;
  const double ez = z - pz;
  // Plainly further segments are turned away on the square before the dear hypot; the margin
  // is far wider than hypot's rounding, so what passes is decided exactly as before.
  const double held = hit.distance;
  if (ex * ex + ez * ez > held * held * (1 + 1e-9)) return;
  const double d = ge::core::hypot(ex, ez);
  if (d >= held) return;
  double len = std::sqrt(len2);
  if (len == 0) len = 1;  // a zero-length segment divides by 1
  hit.index = static_cast<int>(i);
  hit.s = a.s + (b.s - a.s) * t;
  hit.distance = d;
  hit.lateral = (ex * dz - ez * dx) / len;
  hit.x = px;
  hit.z = pz;
}

}  // namespace

void nearest_within(const Track& track, double x, double z, double within, TrackHit& hit) {
  const auto& points = track.points;
  hit.distance = kInf;
  const TrackIndex& index = index_of(track);
  const auto qc = static_cast<int32_t>(std::floor(x / kCell));
  const auto qr = static_cast<int32_t>(std::floor(z / kCell));
  const int rings = within == kInf
                        ? kMaxRings
                        : static_cast<int>(num::min(kMaxRings, std::ceil(within / kCell) + 1));
  // The box in cells relative to the query's cell; cells outside it are empty, so each ring is
  // walked only where it crosses the box, in the same column-then-row order as the whole ring.
  const int bc0 = index.c0 - qc;
  const int bc1 = index.c0 + index.cols - 1 - qc;
  const int br0 = index.r0 - qr;
  const int br1 = index.r0 + index.rows - 1 - qr;
  const auto cell = [&](int dc, int dr) {
    const size_t k = size_t(dr - br0) * size_t(index.cols) + size_t(dc - bc0);
    for (int32_t j = index.start[k]; j < index.start[k + 1]; j++) {
      try_segment(points, size_t(index.segs[size_t(j)]), x, z, hit);
    }
  };
  for (int ring = 0; ring <= rings; ring++) {
    if (index.cols > 0) {
      const int dc_lo = std::max(-ring, bc0);
      const int dc_hi = std::min(ring, bc1);
      for (int dc = dc_lo; dc <= dc_hi; dc++) {
        if (dc == -ring || dc == ring) {
          const int dr_hi = std::min(ring, br1);
          for (int dr = std::max(-ring, br0); dr <= dr_hi; dr++) cell(dc, dr);
        } else {
          if (-ring >= br0 && -ring <= br1) cell(dc, -ring);
          if (ring >= br0 && ring <= br1) cell(dc, ring);
        }
      }
    }
    // Every segment in the next ring out is at least `ring` whole cells away.
    if (hit.distance <= ring * kCell) return;
  }
  if (within != kInf) {
    if (hit.distance > within) hit.distance = kInf;
    return;
  }
  for (size_t i = 0; i + 1 < points.size(); i++) try_segment(points, i, x, z, hit);
}

TrackHit nearest_track_point(const Track& track, double x, double z) {
  TrackHit hit;
  nearest_within(track, x, z, kInf, hit);
  return hit;
}

TrackPoint track_point_at(const Track& track, double s) {
  const auto& points = track.points;
  const double length = track.length;
  const auto n = static_cast<double>(points.size());
  const double u = s <= 0 ? 0 : s >= length ? length : s;
  // Stations are evenly spaced, so the guess is nearly always the answer.
  size_t i =
      static_cast<size_t>(num::min(n - 2, num::max(0.0, std::floor((u / length) * (n - 1)))));
  while (i > 0 && points[i].s > u) i--;
  while (double(i) < n - 2 && points[i + 1].s <= u) i++;
  const TrackPoint& a = points[i];
  const TrackPoint& b = points[i + 1];
  const double t = b.s > a.s ? (u - a.s) / (b.s - a.s) : 0;
  TrackPoint p;
  p.x = a.x + (b.x - a.x) * t;
  p.z = a.z + (b.z - a.z) * t;
  p.y = a.y + (b.y - a.y) * t;
  p.s = u;
  p.heading = a.heading + ge::core::angle_diff(a.heading, b.heading) * t;
  p.width = a.width + (b.width - a.width) * t;
  return p;
}

}  // namespace ge::mountain::mapgen
