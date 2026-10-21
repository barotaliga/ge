// The definitions behind query.hpp.
//
// ge:summary The piste's lazily built spatial hash and the nearest-point searches
// ge:keywords query spatial-hash
// ge:skill mountain
// ge:test tests/mountain_generate_test.cpp
#include "mountain/mapgen/query.hpp"

#include <cmath>
#include <limits>
#include <memory>

#include "core/math.hpp"
#include "core/num.hpp"

namespace ge::mountain::mapgen {

namespace num = ge::core::num;
using ge::core::cell_key;

namespace {

constexpr double kInf = std::numeric_limits<double>::infinity();

// Hash cell, m: several segments a cell and a handful of cells a query.
constexpr double kCell = 24;

// How many rings a query walks before it gives up on the hash and walks the whole piste.
constexpr int kMaxRings = 90;

const TrackIndex& index_of(const Track& track) {
  if (track.index) return *track.index;
  auto index = std::make_shared<TrackIndex>();
  const auto& points = track.points;
  const size_t n = points.size();
  for (size_t i = 0; i + 1 < n; i++) {
    const TrackPoint& a = points[i];
    const TrackPoint& b = points[i + 1];
    const auto c0 = static_cast<int32_t>(std::floor(num::min(a.x, b.x) / kCell));
    const auto c1 = static_cast<int32_t>(std::floor(num::max(a.x, b.x) / kCell));
    const auto r0 = static_cast<int32_t>(std::floor(num::min(a.z, b.z) / kCell));
    const auto r1 = static_cast<int32_t>(std::floor(num::max(a.z, b.z) / kCell));
    for (int32_t c = c0; c <= c1; c++) {
      for (int32_t r = r0; r <= r1; r++) index->cells[cell_key(c, r)].push_back(int32_t(i));
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
  const auto& cells = index_of(track).cells;
  const auto qc = static_cast<int32_t>(std::floor(x / kCell));
  const auto qr = static_cast<int32_t>(std::floor(z / kCell));
  const int rings = within == kInf
                        ? kMaxRings
                        : static_cast<int>(num::min(kMaxRings, std::ceil(within / kCell) + 1));
  for (int ring = 0; ring <= rings; ring++) {
    for (int dc = -ring; dc <= ring; dc++) {
      const bool edge = dc == -ring || dc == ring;
      for (int dr = -ring; dr <= ring; dr += edge ? 1 : 2 * ring) {
        const auto it = cells.find(cell_key(qc + dc, qr + dr));
        if (it != cells.end()) {
          for (const int32_t i : it->second) try_segment(points, size_t(i), x, z, hit);
        }
        if (ring == 0) break;
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
