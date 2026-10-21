// The speed events' shared course behind speed_course.hpp: the bend, the trap, the relaxed
// racing line and its reader, the downhill's gate arcs.
//
// ge:summary The speed events' racing line, its reader, the trap and the downhill's gate arcs
// ge:keywords speed-course racing-line speed-line-at speed-trap trap-arc gate-arcs
// ge:skill mountain
// ge:test tests/mountain_speed_events_test.cpp
#include "mountain/mapgen/speed_course.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "core/num.hpp"
#include "mountain/mapgen/query.hpp"

namespace ge::mountain::mapgen {

namespace num = ge::core::num;
namespace libm = ge::core::libm;
using ge::core::angle_diff;
using ge::core::clamp;

namespace {

// How far either side a line's bend is read over, m.
constexpr double kBendSpan = 8;

}  // namespace

double piste_bend_at(const Track& track, double s, double span) {
  const TrackPoint a = track_point_at(track, s - span);
  const TrackPoint b = track_point_at(track, s + span);
  return angle_diff(a.heading, b.heading) / (2 * span);
}

double trap_arc(const Track& track, double from, double to, const TrapRule& T) {
  const double lo = num::max(from + (to - from) * T.late, from + T.run);
  const double hi = to - T.end;
  double best = hi;
  double most = -std::numeric_limits<double>::infinity();
  for (const bool straight : {true, false}) {
    for (int k = 0; lo + 2.0 * k <= hi; k++) {
      const double s = lo + 2.0 * k;
      if (straight) {
        bool bent = false;
        for (int j = 0; s - T.span + 10.0 * j <= s + T.span; j++) {
          if (std::abs(piste_bend_at(track, s - T.span + 10.0 * j, 10)) > 1 / T.straight) {
            bent = true;
            break;
          }
        }
        if (bent) continue;
      }
      const double fall = track_point_at(track, s - T.run).y - track_point_at(track, s).y;
      if (fall > most) {
        most = fall;
        best = s;
      }
    }
    if (std::isfinite(most)) break;
  }
  return best;
}

std::vector<double> racing_line(const Track& track, double from, double to, const LineRule& L) {
  const auto n = static_cast<size_t>(num::max(2.0, std::floor((to - from) / L.step) + 1));
  std::vector<double> cx(n), cz(n), rx(n), rz(n), room(n);
  for (size_t i = 0; i < n; i++) {
    const TrackPoint p = track_point_at(track, from + double(i) * L.step);
    cx[i] = p.x;
    cz[i] = p.z;
    rx[i] = libm::cos(p.heading);
    rz[i] = -libm::sin(p.heading);
    room[i] = num::max(0.0, p.width / 2 - L.margin);
    if (L.most > 0) room[i] = num::min(room[i], L.most);
  }
  // The room opens and closes gradually, up the piste and down it.
  const double ease = L.ease * L.step;
  for (size_t i = 1; i < n; i++) room[i] = num::min(room[i], room[i - 1] + ease);
  for (size_t i = n - 1; i-- > 0;) room[i] = num::min(room[i], room[i + 1] + ease);
  std::vector<double> o(n, 0.0);
  for (const int stride : L.strides) {
    const auto k = static_cast<size_t>(stride);
    if (k >= n - 1) continue;
    for (int pass = 0; pass < L.passes; pass++) {
      for (size_t i = 1; i + 1 < n; i++) {
        const size_t a = i > k ? i - k : 0;
        const size_t b = std::min(n - 1, i + k);
        const double mx = (cx[a] + rx[a] * o[a] + cx[b] + rx[b] * o[b]) / 2;
        const double mz = (cz[a] + rz[a] * o[a] + cz[b] + rz[b] * o[b]) / 2;
        const double t = (mx - cx[i]) * rx[i] + (mz - cz[i]) * rz[i];
        o[i] = clamp(t, -room[i], room[i]);
      }
    }
  }
  return o;
}

double line_offset(const std::vector<double>& line, double from, double at, double step) {
  const double last = double(line.size() - 1);
  const double f = clamp((at - from) / step, 0, last);
  const auto i = static_cast<size_t>(num::min(last - 1, std::floor(f)));
  return line[i] + (line[i + 1] - line[i]) * (f - double(i));
}

std::vector<double> speed_gate_arcs(double from, double to, const std::vector<double>& jumps,
                                    const SpacingRule& G, double clear) {
  std::vector<double> arcs;
  for (double prev = from;;) {
    const double left = to - prev;
    if (left <= G.max) break;
    double at = prev + left / num::max(1.0, num::round(left / G.target));
    for (const double j : jumps) {
      if (at <= j - clear || at >= j + clear) continue;
      // Stepped off the jump to whichever side fits the band, the nearer first.
      double pick = at;
      double off = std::numeric_limits<double>::infinity();
      for (const double a : {j - clear, j + clear}) {
        if (a < prev + G.min || a > prev + G.max || to - a < G.min) continue;
        if (std::abs(a - at) < off) {
          off = std::abs(a - at);
          pick = a;
        }
      }
      at = pick;
      break;
    }
    arcs.push_back(at);
    prev = at;
  }
  return arcs;
}

std::optional<SpeedLineAt> speed_line_at(const Level& level, double s) {
  if (!level.race || level.race->discipline == Discipline::slalom) return std::nullopt;
  const std::vector<RaceCourse::LinePoint>& line = level.race->line;
  const size_t n = line.size();
  if (n < 3) return std::nullopt;
  const double step = (line[n - 1].s - line[0].s) / double(n - 1);
  const auto point = [&](size_t i) {
    const TrackPoint p = track_point_at(level.track, line[i].s);
    return PlanPoint{p.x + libm::cos(p.heading) * line[i].x,
                     p.z - libm::sin(p.heading) * line[i].x};
  };
  // The line's heading change over 8 m either side of station i, per metre.
  const auto k = static_cast<size_t>(num::max(1.0, num::round(kBendSpan / step)));
  const auto bend = [&](size_t i) {
    const size_t a = i > k ? i - k : 0;
    const size_t b = std::min(n - 1, i + k);
    if (a == i || b == i) return 0.0;
    const PlanPoint pa = point(a), pi = point(i), pb = point(b);
    const double h0 = libm::atan2(pi.x - pa.x, pi.z - pa.z);
    const double h1 = libm::atan2(pb.x - pi.x, pb.z - pi.z);
    return angle_diff(h0, h1) / ((double(b - a) * step) / 2);
  };
  const double f = clamp((s - line[0].s) / step, 0, double(n - 1));
  const auto i = static_cast<size_t>(num::min(double(n - 2), std::floor(f)));
  const double u = f - double(i);
  const double b0 = bend(i);
  const double b1 = bend(i + 1);
  return SpeedLineAt{line[i].x + (line[i + 1].x - line[i].x) * u, b0 + (b1 - b0) * u};
}

}  // namespace ge::mountain::mapgen
