// The traffic's routes (traffic_route.hpp): the lanes' offset lines joined round the corners by
// cubics, laid every half metre with their headings and smoothed curvatures, and driven: the
// corner speeds, the pulls away and the braking, the time at every point.
//
// ge:summary The traffic's routes laid and driven: corners as cubics, curvature, speeds, times
// ge:keywords traffic route lay-path drive-path raw-line cubic corner handles
// ge:skill mountain
// ge:test tests/mountain_traffic_test.cpp
#include "mountain/village/traffic_route.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "core/num.hpp"

namespace ge::mountain::village {

namespace {

namespace mg = ge::mountain::mapgen;
namespace num = ge::core::num;
namespace libm = ge::core::libm;
using ge::core::hypot;
using mg::PlanPoint;

double wrap(double a) { return libm::atan2(libm::sin(a), libm::cos(a)); }

// A cubic from `a` going `ta` to `b` arriving along `tb`, its handles `ha` and `hb` long, every
// quarter metre or so (excluding `a`).
void cubic(PlanPoint a, PlanPoint ta, PlanPoint b, PlanPoint tb, double ha, double hb, int32_t lane,
           double top, std::vector<RawPoint>& out) {
  const PlanPoint p1{a.x + ta.x * ha, a.z + ta.z * ha};
  const PlanPoint p2{b.x - tb.x * hb, b.z - tb.z * hb};
  const int n = std::max(4, int(std::ceil(hypot(b.x - a.x, b.z - a.z) / 0.25)));
  for (int i = 1; i <= n; i++) {
    const double u = double(i) / n;
    const double w = 1 - u;
    out.push_back({w * w * w * a.x + 3 * w * w * u * p1.x + 3 * w * u * u * p2.x + u * u * u * b.x,
                   w * w * w * a.z + 3 * w * w * u * p1.z + 3 * w * u * u * p2.z + u * u * u * b.z,
                   lane, top});
  }
}

// The way a raw line runs at its end (`end`) or its start.
PlanPoint tangent(const std::vector<RawPoint>& pts, bool end) {
  const size_t n = pts.size();
  const RawPoint& a = end ? pts[n >= 3 ? n - 3 : 0] : pts[0];
  const RawPoint& b = end ? pts[n - 1] : pts[std::min(n - 1, size_t(2))];
  const double d = hypot(b.x - a.x, b.z - a.z);
  const double k = d != 0 ? d : 1;
  return {(b.x - a.x) / k, (b.z - a.z) / k};
}

// The handles of the curve round a corner from `a` (going `ta`) to `b` (leaving along `tb`): a
// quarter circle's for a square turn, a third of the way for a straight one.
std::pair<double, double> handles(PlanPoint a, PlanPoint ta, PlanPoint b, PlanPoint tb) {
  const double d = hypot(b.x - a.x, b.z - a.z);
  const double cos = ta.x * tb.x + ta.z * tb.z;
  if (cos > 0.97) return {d / 3, d / 3};
  // Where the two lines meet: a + u ta = b - w tb.
  const double det = ta.x * -tb.z - ta.z * -tb.x;
  if (std::fabs(det) > 1e-6) {
    const double rx = b.x - a.x;
    const double rz = b.z - a.z;
    const double u = (rx * -tb.z - rz * -tb.x) / det;
    const double w = (ta.x * rz - ta.z * rx) / det;
    if (u > 0 && w > 0 && u < 3 * d && w < 3 * d) return {u * 0.55, w * 0.55};
  }
  return {d * 0.5, d * 0.5};
}

// The corner from the end of `prev` round to the start of `next`, into `out` (its last point,
// `next`'s first, left off).
void corner(const std::vector<RawPoint>& prev, const std::vector<RawPoint>& next,
            std::vector<RawPoint>& out) {
  const RawPoint& a = prev.back();
  const RawPoint& b = next.front();
  const PlanPoint ta = tangent(prev, true);
  const PlanPoint tb = tangent(next, false);
  const auto [ha, hb] = handles({a.x, a.z}, ta, {b.x, b.z}, tb);
  cubic({a.x, a.z}, ta, {b.x, b.z}, tb, ha, hb, -1, num::min(a.top, b.top), out);
  out.pop_back();
}

}  // namespace

double lane_offset(const Street& st, const Lane& lane, Keep keep) {
  const double off =
      keep == Keep::middle ? st.section.lane / 2 : st.section.lane - traffic::kBikeEdge;
  return lane.dir == 1 ? -off : off;
}

double top_of(StreetKind kind, const Drive& drive) {
  return kind == StreetKind::aisle  ? drive.aisle
         : kind == StreetKind::road ? drive.road
                                    : drive.street;
}

double trim_of(const Streets& plan, const Street& st, int end) {
  const Junction& j = plan.junctions[size_t(end == 0 ? st.from : st.to)];
  if (j.exit) return 0;
  return trim_at(plan, st, end).road + traffic::kTrim;
}

Visit whole_visit(const Streets& plan, const Furniture& f, int32_t lane) {
  const Lane& ln = f.lanes[size_t(lane)];
  const Street& st = plan.streets[size_t(ln.street)];
  const double a = trim_of(plan, st, 0);
  const double b = st.length - trim_of(plan, st, 1);
  const double lo = num::min(a, (a + b) / 2);
  const double hi = num::max(b, (a + b) / 2);
  return ln.dir == 1 ? Visit{lane, lo, hi} : Visit{lane, hi, lo};
}

double arc_nearest(const Street& st, double x, double z) {
  double best = std::numeric_limits<double>::infinity();
  double at = 0;
  const int n = int(std::floor(st.length / 0.5));
  for (int i = 0; i <= n; i++) {
    const double s = i * 0.5;
    const StreetPoint p = street_at(st, s);
    const double d = hypot(p.x - x, p.z - z);
    if (d < best) {
      best = d;
      at = s;
    }
  }
  return at;
}

std::vector<RawPoint> raw_line(const mg::Level& level, const Streets& plan, const Furniture& f,
                               std::span<const Visit> visits, Keep keep, const Drive& drive,
                               bool closed) {
  std::vector<std::vector<RawPoint>> pieces;
  pieces.reserve(visits.size());
  for (const Visit& vi : visits) {
    const Lane& ln = f.lanes[size_t(vi.lane)];
    const Street& st = plan.streets[size_t(ln.street)];
    const double top = top_of(st.kind, drive);
    const std::vector<StreetPoint> pts =
        offset_line(level, st, lane_offset(st, ln, keep), vi.s0, vi.s1, 2);
    std::vector<RawPoint> piece;
    piece.reserve(pts.size());
    for (const StreetPoint& p : pts) piece.push_back({p.x, p.z, vi.lane, top});
    pieces.push_back(std::move(piece));
  }
  std::vector<RawPoint> out;
  for (size_t i = 0; i < pieces.size(); i++) {
    if (i > 0) corner(pieces[i - 1], pieces[i], out);
    out.insert(out.end(), pieces[i].begin(), pieces[i].end());
  }
  if (closed && pieces.size() > 1) corner(pieces.back(), pieces.front(), out);
  return out;
}

std::vector<RawPoint> bay_turn(PlanPoint a, PlanPoint ta, PlanPoint b, PlanPoint fb, double top) {
  std::vector<RawPoint> out{{a.x, a.z, -1, top}};
  const double d = hypot(b.x - a.x, b.z - a.z);
  cubic(a, ta, b, fb, d * 0.55, d * 0.45, -1, top, out);
  return out;
}

Path lay_path(std::span<const RawPoint> raw, bool closed) {
  namespace T = traffic;
  std::vector<double> cum{0};
  cum.reserve(raw.size());
  for (size_t i = 1; i < raw.size(); i++) {
    cum.push_back(cum[i - 1] + hypot(raw[i].x - raw[i - 1].x, raw[i].z - raw[i - 1].z));
  }
  double total = cum.back();
  if (closed) total += hypot(raw[0].x - raw.back().x, raw[0].z - raw.back().z);
  const int n = std::max(2, int(num::round(total / T::kDs)) + (closed ? 0 : 1));
  const double step = closed ? total / n : total / (n - 1);
  Path p;
  p.n = n;
  p.closed = closed;
  p.x.resize(size_t(n));
  p.z.resize(size_t(n));
  p.lane.resize(size_t(n));
  p.top.resize(size_t(n));
  size_t j = 0;
  for (int i = 0; i < n; i++) {
    const double s = i * step;
    while (j < raw.size() - 1 && cum[j + 1] < s) j++;
    const RawPoint& a = raw[j];
    const bool last = j == raw.size() - 1;
    const RawPoint& b = last ? raw[0] : raw[j + 1];
    const double len = last ? total - cum[j] : cum[j + 1] - cum[j];
    const double u = len > 1e-9 ? num::max(0.0, num::min(1.0, (s - cum[j]) / len)) : 0;
    p.x[size_t(i)] = a.x + (b.x - a.x) * u;
    p.z[size_t(i)] = a.z + (b.z - a.z) * u;
    p.lane[size_t(i)] = u < 0.5 ? a.lane : b.lane;
    p.top[size_t(i)] = num::min(a.top, b.top);
  }
  const auto at = [n, closed](int i) {
    return size_t(closed ? ((i % n) + n) % n : std::max(0, std::min(n - 1, i)));
  };
  p.h.resize(size_t(n));
  for (int i = 0; i < n; i++) {
    const size_t a = at(i - 2);
    const size_t b = at(i + 2);
    p.h[size_t(i)] = libm::atan2(p.x[b] - p.x[a], p.z[b] - p.z[a]);
  }
  // The curvature off the heading's change, smoothed over +-`smooth` points.
  std::vector<double> raw1(static_cast<size_t>(n));
  for (int i = 0; i < n; i++) {
    const auto a = int(at(i - 1));
    const auto b = int(at(i + 1));
    const double span0 = double(b - a + (closed && b < a ? n : 0)) * step;
    const double span = span0 != 0 ? span0 : step;
    raw1[size_t(i)] = wrap(p.h[size_t(b)] - p.h[size_t(a)]) / span;
  }
  constexpr int kSmooth = 4;
  p.k.resize(size_t(n));
  for (int i = 0; i < n; i++) {
    double sum = 0;
    int cnt = 0;
    for (int d = -kSmooth; d <= kSmooth; d++) {
      const int q = closed ? int(at(i + d)) : i + d;
      if (q < 0 || q >= n) continue;
      sum += raw1[size_t(q)];
      cnt++;
    }
    p.k[size_t(i)] = sum / cnt;
  }
  const int ahead = int(num::round(T::kSignalAhead / step));
  p.turn.resize(size_t(n));
  for (int i = 0; i < n; i++) {
    double most = 0;
    for (int d = 4; d <= ahead; d += 4) {
      const size_t q = closed ? at(i + d) : size_t(std::min(n - 1, i + d));
      const double dh = wrap(p.h[q] - p.h[size_t(i)]);
      if (std::fabs(dh) > std::fabs(most)) most = dh;
    }
    p.turn[size_t(i)] = most;
  }
  return p;
}

double step_of(const Path& p) {
  const auto n = size_t(p.n);
  double total = 0;
  for (size_t i = 1; i < n; i++) total += hypot(p.x[i] - p.x[i - 1], p.z[i] - p.z[i - 1]);
  if (p.closed) total += hypot(p.x[0] - p.x[n - 1], p.z[0] - p.z[n - 1]);
  return total / double(p.closed ? n : n - 1);
}

Driven drive_path(Path path, const Drive& drive, double v0, double v1, bool reverse, double scale) {
  const int n = path.n;
  const bool closed = path.closed;
  const double ds = step_of(path);
  std::vector<double> v(static_cast<size_t>(n));
  for (int i = 0; i < n; i++) {
    const double bend = std::fabs(path.k[size_t(i)]);
    const double corner =
        bend > 1e-6 ? std::sqrt(drive.lateral / bend) : std::numeric_limits<double>::infinity();
    v[size_t(i)] = num::min(reverse ? drive.reverse : path.top[size_t(i)], corner);
  }
  if (!closed) {
    v[0] = num::min(v[0], v0);
    v[size_t(n) - 1] = num::min(v[size_t(n) - 1], v1);
  }
  const int rounds = closed ? 3 : 1;
  const int wrap_n = closed ? n : 0;
  for (int r = 0; r < rounds; r++) {
    // Pulled away: never faster than the last point allows.
    for (int i = 1; i < n + wrap_n; i++) {
      const auto a = size_t((i - 1) % n);
      const auto b = size_t(i % n);
      v[b] = num::min(v[b], std::sqrt(v[a] * v[a] + 2 * drive.accel * ds));
    }
    // Slowed into: never faster than the next point can be braked to.
    for (int i = n - 2 + wrap_n; i >= 0; i--) {
      const auto a = size_t(i % n);
      const auto b = size_t((i + 1) % n);
      v[a] = num::min(v[a], std::sqrt(v[b] * v[b] + 2 * drive.brake * ds));
    }
  }
  if (scale != 1) {
    for (double& x : v) x /= scale;
  }
  std::vector<double> t(size_t(closed ? n + 1 : n), 0);
  for (size_t i = 1; i < t.size(); i++) {
    const double a = v[i - 1];
    const double b = v[i % size_t(n)];
    t[i] = t[i - 1] + (2 * ds) / num::max(0.05, a + b);
  }
  Driven d;
  d.duration = t.back();
  d.path = std::move(path);
  d.ds = ds;
  d.reverse = reverse;
  d.v = std::move(v);
  d.t = std::move(t);
  return d;
}

double roll_of(const Driven& d) { return double(d.path.closed ? d.path.n : d.path.n - 1) * d.ds; }

}  // namespace ge::mountain::village
