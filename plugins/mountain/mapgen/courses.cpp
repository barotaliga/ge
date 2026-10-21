// The definitions behind courses.hpp.
//
// ge:summary Composes a course down the network and reads a run's arc as the course's
// ge:keywords courses compose-course course-arc resample
// ge:skill mountain
// ge:test tests/mountain_resort_build_test.cpp
#include "mountain/mapgen/courses.hpp"

#include <cmath>
#include <limits>

#include "core/math.hpp"
#include "core/num.hpp"
#include "mountain/mapgen/resort_rules.hpp"
#include "mountain/mapgen/rules.hpp"
#include "mountain/mapgen/track.hpp"

namespace ge::mountain::mapgen {

namespace num = ge::core::num;
using ge::core::clamp;
using ge::core::hypot;
using ge::core::smoothstep;

namespace {

struct LinePoint {
  double x, z, width;
};

// A point of a run `s` m down it, interpolated.
LinePoint run_at(const WalkedRun& run, double s) {
  const auto& pts = run.track.points;
  const auto n = static_cast<long>(pts.size());
  const double u = clamp(s, 0, run.track.length) / run.track.length;
  const double f = u * double(n - 1);
  const auto i = static_cast<long>(num::min(double(n - 2), std::floor(f)));
  const double t = f - double(i);
  const TrackPoint& a = pts[size_t(i)];
  const TrackPoint& b = pts[size_t(i + 1)];
  return {a.x + (b.x - a.x) * t, a.z + (b.z - a.z) * t, a.width + (b.width - a.width) * t};
}

// The point of a run nearest (x, z) between arcs `from` and `to`.
LinePoint nearest_on(const WalkedRun& run, double x, double z, double from, double to) {
  const auto& pts = run.track.points;
  const auto n = static_cast<long>(pts.size());
  const double step = run.track.length / num::max(1.0, double(n - 1));
  const long i0 = long(clamp(std::floor(from / step), 0, double(n - 2)));
  const long i1 = long(clamp(std::ceil(to / step), double(i0 + 1), double(n - 1)));
  double best = std::numeric_limits<double>::infinity();
  LinePoint out{pts[size_t(i1)].x, pts[size_t(i1)].z, pts[size_t(i1)].width};
  for (long i = i0; i < i1; i++) {
    const TrackPoint& a = pts[size_t(i)];
    const TrackPoint& b = pts[size_t(i + 1)];
    const double dx = b.x - a.x;
    const double dz = b.z - a.z;
    const double len2 = dx * dx + dz * dz;
    const double t = clamp(((x - a.x) * dx + (z - a.z) * dz) / (len2 == 0 ? 1 : len2), 0, 1);
    const double px = a.x + dx * t;
    const double pz = a.z + dz * t;
    const double d = (px - x) * (px - x) + (pz - z) * (pz - z);
    if (d < best) {
      best = d;
      out = {px, pz, a.width + (b.width - a.width) * t};
    }
  }
  return out;
}

}  // namespace

std::variant<CoursePlan, std::string> compose_course(std::span<const WalkedRun* const> runs,
                                                     int start) {
  const double step = rules::track::step;
  const double merge = resort_rules::course::merge;
  struct Raw {
    double x, z, width;
    int piece;
  };
  std::vector<Raw> line;
  std::vector<CoursePiece> spans;
  int cur = start;
  double from = 0;
  for (size_t guard = 0; guard < runs.size() + 1; guard++) {
    if (cur < 0 || size_t(cur) >= runs.size()) {
      return std::string("a course runs into a run that was never laid");
    }
    const WalkedRun& run = *runs[size_t(cur)];
    const int piece = int(spans.size());
    spans.push_back({cur, from, run.track.length, 0, 0});
    const RunJoin* into = run.into ? &*run.into : nullptr;
    const WalkedRun* target = into && into->run >= 0 && size_t(into->run) < runs.size()
                                  ? runs[size_t(into->run)]
                                  : nullptr;
    const double length = run.track.length;
    double s = from;
    while (s <= length + 1e-6) {
      LinePoint p = run_at(run, s);
      if (into && target && s > length - merge) {
        // Across the junction: onto the other's line (the nearest point of it up to the
        // junction, so a run that came in from the side eases across rather than along), eased.
        const double w = smoothstep(length - merge, length, s);
        const LinePoint q = nearest_on(*target, p.x, p.z, into->s - 2 * merge, into->s);
        p.x += (q.x - p.x) * w;
        p.z += (q.z - p.z) * w;
        p.width += (q.width - p.width) * w;
      }
      line.push_back({p.x, p.z, p.width, piece});
      s += step;
    }
    if (!into) break;
    cur = into->run;
    from = into->s + step;
    for (const CoursePiece& sp : spans) {
      if (sp.run == cur) return std::string("a course loops back onto a run it skied");
    }
  }
  // Laid again every `step` m.
  const size_t n0 = line.size();
  if (n0 < 2) return std::string("a course of fewer than two points");
  std::vector<double> cum(n0, 0);
  for (size_t i = 1; i < n0; i++) {
    cum[i] = cum[i - 1] + hypot(line[i].x - line[i - 1].x, line[i].z - line[i - 1].z);
  }
  const double total = cum[n0 - 1];
  const double count = num::max(2.0, num::round(total / step));
  const double spacing = total / count;
  CoursePlan plan;
  plan.start = start;
  std::vector<int> piece_at;
  size_t j = 0;
  // `count` is whole, so a whole counter gives the same stations.
  for (long k = 0; k <= static_cast<long>(count); k++) {
    const double s = double(k) * spacing;
    while (j < n0 - 2 && cum[j + 1] < s) j++;
    const double t = cum[j + 1] > cum[j] ? (s - cum[j]) / (cum[j + 1] - cum[j]) : 0;
    const Raw& a = line[j];
    const Raw& b = line[j + 1];
    plan.points.push_back(
        {a.x + (b.x - a.x) * t, a.z + (b.z - a.z) * t, 0, s, 0, a.width + (b.width - a.width) * t});
    piece_at.push_back(t < 0.5 ? a.piece : b.piece);
  }
  set_headings(plan.points);
  // Where on the course each piece lies.
  for (size_t k = 0; k < spans.size(); k++) {
    CoursePiece sp = spans[k];
    sp.at = std::numeric_limits<double>::infinity();
    sp.until = -std::numeric_limits<double>::infinity();
    for (size_t i = 0; i < plan.points.size(); i++) {
      if (piece_at[i] != int(k)) continue;
      sp.at = num::min(sp.at, plan.points[i].s);
      sp.until = num::max(sp.until, plan.points[i].s);
    }
    plan.pieces.push_back(sp);
  }
  plan.length = count * spacing;
  return plan;
}

double course_arc(const CoursePlan& course, int run, double s) {
  for (const CoursePiece& p : course.pieces) {
    if (p.run != run || s < p.from - 1e-6 || s > p.to + 1e-6) continue;
    const double k = (p.until - p.at) / num::max(1e-9, p.to - p.from);
    return p.at + (s - p.from) * k;
  }
  return -1;
}

}  // namespace ge::mountain::mapgen
