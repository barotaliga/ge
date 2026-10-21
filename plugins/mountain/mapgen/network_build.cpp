// The grading of a walked run: the envelopes, the start, the finish, the junction's plane.
//
// ge:summary Grading a walked run: envelopes, start hold, finish, the junction's plane and pins
// ge:keywords network-build grade-run plane-of pin-of nearer-ahead funnel-into cross-fall
// ge:skill mountain
// ge:test tests/mountain_resort_build_test.cpp
#include "mountain/mapgen/network_build.hpp"

#include <cmath>
#include <limits>
#include <vector>

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "core/num.hpp"
#include "core/num_text.hpp"
#include "mountain/mapgen/spawn.hpp"
#include "mountain/mapgen/track.hpp"

namespace ge::mountain::mapgen {

namespace num = ge::core::num;
namespace libm = ge::core::libm;
namespace R = rules;
using ge::core::angle_diff;
using ge::core::clamp;
using ge::core::sample_field;
using ge::core::smoothstep;

namespace {

constexpr double kInf = std::numeric_limits<double>::infinity();
const double kNaN = std::numeric_limits<double>::quiet_NaN();

// How far inside its colour's ceiling the grading holds every colour window.
constexpr double kColourMargin = 0.9;
// How far inside its colour's start (R12) a run's first stretch is held, and how far past the
// run out of the gate the hold reaches, m.
constexpr double kStartMargin = 0.9;
constexpr double kStartBlur = 24;
// How far past where their corridors' benches overlap a merging run's line starts easing onto
// the other's surface, m across.
constexpr double kPinReach = 14;
// How far past its ceiling a run's fall may be spread between two runs' surfaces.
constexpr double kCeilingGive = 1.18;
// How far either side of a stretch on another run's core the line eases onto that surface, m.
constexpr double kPinEase = 24;
// The flat shoulder and the windrow's bench past a run's width, m.
constexpr double kBenchW = R::track::shoulder_flat + R::berm::width;

struct Plane {
  std::vector<double> y, cross, w;
};

// How much of the ground as it lies each station takes: all of it on a core `pinned` names,
// easing to none `kPinEase` m off one; empty where no station does.
std::vector<double> pin_of(const WalkedRun& run, const PinnedAt& pinned) {
  const auto& pts = run.track.points;
  const size_t n = pts.size();
  // Never in the finish (R5): it is laid flat into the village beside the others.
  const double finish =
      run.into ? kInf : run.track.length - R::track::finish - R::track::runout - kPinEase;
  std::vector<double> on;
  for (size_t i = 0; i < n; i++) {
    if (pts[i].s < finish && pinned(pts[i].x, pts[i].z)) on.push_back(pts[i].s);
  }
  if (on.empty()) return {};
  std::vector<double> w(n);
  size_t k = 0;
  for (size_t i = 0; i < n; i++) {
    const double s = pts[i].s;
    while (k + 1 < on.size() && on[k + 1] <= s) k++;
    const double d =
        num::min(std::abs(s - on[k]), k + 1 < on.size() ? std::abs(on[k + 1] - s) : kInf);
    w[i] = pts[i].s < finish ? 1 - smoothstep(0, kPinEase, d) : 0;
  }
  return w;
}

// Whether the nearest station on another's core to station `i` lies down the run from it.
bool nearer_ahead(const std::vector<TrackPoint>& pts, const std::vector<double>& pin, size_t i) {
  double up = kInf;
  double down = kInf;
  for (size_t j = i; j-- > 0;) {
    if (pin[j] >= 1) {
      up = pts[i].s - pts[j].s;
      break;
    }
  }
  for (size_t j = i + 1; j < pts.size(); j++) {
    if (pin[j] >= 1) {
      down = pts[j].s - pts[i].s;
      break;
    }
  }
  return down < up;
}

// The other run's surface under each station of a merging run: its line's height at the nearest
// arc with its camber carried out to the station, that camber in this run's frame, and how much
// of it the station takes. The nearest arc only moves on down the other run, never past the
// junction.
Plane plane_of(const WalkedRun& run, const WalkedRun& onto) {
  const auto& pts = run.track.points;
  const size_t n = pts.size();
  const auto& tp = onto.track.points;
  Plane out{std::vector<double>(n), std::vector<double>(n), std::vector<double>(n)};
  const double from = run.merge_start - 20;
  const double step = onto.track.length / num::max(1.0, double(tp.size()) - 1);
  const double at = run.into ? run.into->s : onto.track.length;
  const size_t last =
      static_cast<size_t>(num::min(double(tp.size() - 1), std::ceil((at + 20) / step)));
  size_t j = 0;
  for (size_t i = 0; i < n; i++) {
    const TrackPoint& p = pts[i];
    if (p.s < from) continue;
    const auto d2 = [&](size_t k) {
      const double dx = tp[k].x - p.x;
      const double dz = tp[k].z - p.z;
      return dx * dx + dz * dz;
    };
    double best = d2(j);
    for (size_t k = j + 1; k <= last; k++) {
      const double d = d2(k);
      if (d < best) {
        best = d;
        j = k;
      }
    }
    const TrackPoint& t = tp[j];
    const double lateral = (p.x - t.x) * libm::cos(t.heading) - (p.z - t.z) * libm::sin(t.heading);
    const double hw = t.width / 2;
    out.y[i] = t.y + onto.cross[j] * clamp(lateral, -hw, hw);
    out.cross[i] = onto.cross[j] * libm::cos(angle_diff(t.heading, p.heading));
    const double touch = hw + kBenchW + p.width / 2;
    out.w[i] = 1 - smoothstep(touch, touch + kPinReach, std::abs(lateral));
  }
  return out;
}

}  // namespace

std::optional<std::string> grade_run(WalkedRun& run, const ge::core::Heightfield& ground,
                                     double max_grade, std::optional<double> colour,
                                     const WalkedRun* onto, const PinnedAt* pinned, bool stepped,
                                     const GradeFace& face) {
  namespace T = R::track;
  auto& pts = run.track.points;
  const size_t n = pts.size();
  const double step = run.track.length / double(n - 1);
  std::vector<double>& raw = run.raw;
  for (size_t i = 0; i < n; i++) raw[i] = sample_field(ground, pts[i].x, pts[i].z);
  const double least = face.fall * 1.1 * step;
  const double most = max_grade * 0.95 * step;
  std::vector<double> cut = raw;
  for (size_t i = 1; i < n; i++) cut[i] = num::min(raw[i], cut[i - 1] - least);
  std::vector<double> fill = raw;
  for (size_t i = n - 1; i-- > 0;) fill[i] = num::max(raw[i], fill[i + 1] + least);
  std::vector<double> y(n);
  for (size_t i = 0; i < n; i++) y[i] = (cut[i] + fill[i]) / 2;
  const size_t K =
      colour ? static_cast<size_t>(num::max(1.0, num::round(T::colour_window / step))) : 0;
  const double most_k = colour ? *colour * kColourMargin * double(K) * step : kInf;
  // THE START (R12): off its top station a run opens no steeper than its colour's start.
  const size_t start_n = static_cast<size_t>(
      num::round((start_gate_arc() + R::spawn::run + T::grade_window + kStartBlur) / step));
  if (face.head) {
    for (size_t i = 0; i <= std::min(start_n, n - 1); i++) {
      const double w = start_n > 0 ? double(i) / double(start_n) : 1;
      y[i] = cut[i] + (y[i] - cut[i]) * w;
    }
  }
  const double start_most =
      num::min(most, run.start_slope.value_or(run.spec.row->spawn.max_slope) * kStartMargin * step);
  const auto cap = [&](size_t i) { return i <= start_n ? start_most : most; };
  std::vector<double> held = y;
  for (size_t i = 1; i < n; i++) {
    held[i] = num::max(held[i], held[i - 1] - cap(i));
    if (K > 0 && i >= K) held[i] = num::max(held[i], held[i - K] - most_k);
  }
  std::vector<double> shaved = y;
  for (size_t i = n - 1; i-- > 0;) {
    shaved[i] = num::min(shaved[i], shaved[i + 1] + cap(i + 1));
    if (K > 0 && i + K < n) shaved[i] = num::min(shaved[i], shaved[i + K] + most_k);
  }
  for (size_t i = 0; i < n; i++) y[i] = (held[i] + shaved[i]) / 2;
  const int k = static_cast<int>(num::round(12 / step));
  if (!run.into) {
    const size_t flat =
        static_cast<size_t>(num::max(0.0, double(n - 1) - num::round(T::finish / step)));
    for (size_t i = flat; i < n; i++) y[i] = y[flat];
    y = blur(blur(y, k), k);
    const int ease = static_cast<int>(num::round(T::runout / (2 * step)));
    const std::vector<double> eased = blur(y, ease);
    const size_t from =
        static_cast<size_t>(num::max(0.0, double(flat) - num::round((T::runout + 12) / step)));
    for (size_t i = from; i < n; i++) {
      const double w =
          smoothstep(double(from), num::min(double(n - 1), double(from) + ease), double(i));
      y[i] = y[i] + (eased[i] - y[i]) * w;
    }
  } else {
    y = blur(blur(y, k), k);
  }
  // THE JUNCTION: from where the corridors touch, onto the other's surface, eased in over the
  // stretch before.
  std::optional<Plane> plane;
  if (onto) plane = plane_of(run, *onto);
  // ON ANOTHER'S SURFACE: where this run's line runs over the core of a run pressed before it,
  // the line is drawn onto that surface, eased in and out.
  std::vector<double> pin;
  if (pinned) pin = pin_of(run, *pinned);
  const std::vector<double>* pin_w = plane ? &plane->w : !pin.empty() ? &pin : nullptr;
  if (plane && !pin.empty()) {
    // On the other's core the ground as it lies IS its surface; off it the plane is moved by the
    // gap at the nearest core station, as much as the station is pinned.
    std::vector<double> gap(n);
    if (!stepped) {
      double last = kNaN;
      for (size_t i = 0; i < n; i++) {
        if (pin[i] >= 1) last = raw[i] - plane->y[i];
        gap[i] = last;
      }
      last = kNaN;
      for (size_t i = n; i-- > 0;) {
        if (pin[i] >= 1) {
          last = raw[i] - plane->y[i];
        } else if (std::isnan(gap[i]) || (!std::isnan(last) && nearer_ahead(pts, pin, i))) {
          gap[i] = last;
        }
      }
    }
    for (size_t i = 0; i < n; i++) {
      if (pin[i] > plane->w[i] || (!stepped && pin[i] >= 1)) {
        plane->w[i] = pin[i];
        plane->y[i] = raw[i];
      } else if (!stepped && pin[i] > 0 && !std::isnan(gap[i])) {
        plane->y[i] += gap[i] * pin[i];
      }
    }
  }
  if (pin_w) {
    const std::vector<double>& pw = *pin_w;
    for (size_t i = 0; i < n; i++) {
      const double w = pw[i];
      if (w > 0) y[i] = y[i] * (1 - w) + (plane ? plane->y[i] : raw[i]) * w;
    }
    // A station wholly on the other's surface stays on it.
    const auto fixed = [&](size_t i) { return !stepped && !pin.empty() && pin[i] >= 1; };
    const auto target = [&](size_t i) { return plane ? plane->y[i] : raw[i]; };
    if (stepped) {
      for (size_t i = 1; i < n; i++) {
        if (pw[i] < 1) y[i] = num::max(y[i], y[i - 1] - most);
      }
    }
    // From the junction back up the run, the line is held between never climbing onto the other
    // and never falling onto it steeper than its own ceiling or its colour's.
    const double least_back = face.fall * step;
    const auto back = [&](double ceiling) {
      if (fixed(n - 1)) y[n - 1] = target(n - 1);
      for (size_t i = n - 1; i-- > 0;) {
        if (fixed(i)) {
          y[i] = target(i);
          continue;
        }
        const double floor = y[i + 1] + least_back * (1 - pw[i]);
        double top = y[i + 1] + ceiling;
        if (K > 0 && i + K < n && pw[i] < 1) top = num::min(top, y[i + K] + most_k);
        y[i] = num::max(floor, num::min(y[i], top));
      }
    };
    if (stepped) {
      back(most);
    } else {
      // Off another's surface down the run the line neither falls away from it steeper than its
      // ceiling nor climbs off it, then is held back up from the junction again until the two
      // agree; where that cannot be done within the ceiling, the fall is spread a little past
      // it, and past `kCeilingGive` of it the run cannot be graded.
      const std::vector<double> shaped = y;
      double ceiling = most;
      for (;;) {
        y = shaped;
        back(ceiling);
        double worst = 0;
        for (int pass = 0; pass < 4; pass++) {
          worst = 0;
          for (size_t i = 1; i < n; i++) {
            if (fixed(i)) continue;
            const double lo = y[i - 1] - ceiling;
            const double hi = y[i - 1] - least_back * (1 - pw[i]);
            y[i] = num::min(num::max(y[i], lo), hi);
          }
          back(ceiling);
          for (size_t i = 1; i < n; i++) {
            if (fixed(i) && fixed(i - 1)) continue;
            const double fall = y[i - 1] - y[i];
            worst = num::max(num::max(worst, fall - ceiling), least_back * (1 - pw[i]) - fall);
          }
          if (worst < 1e-3) break;
        }
        if (worst < 1e-3) break;
        ceiling *= 1.04;
        if (ceiling > most * kCeilingGive) {
          return std::string("a run cannot be graded between the runs it meets");
        }
      }
    }
  }
  double deepest = 0;
  for (size_t i = 0; i < n; i++) {
    pts[i].y = y[i];
    deepest = num::max(deepest, std::abs(y[i] - raw[i]));
  }
  if (deepest > face.cut) {
    return "a run has to be cut " + num::to_fixed(deepest, 1) + " m into the mountain";
  }
  // R6 on the graded line.
  const BendReading bend = tightest_bend(run.track, [&](size_t i) { return pts[i].y; });
  if (bend.radius < bend.floor - 0.5) {
    return "a bend tightens to " + num::to_fixed(bend.radius, 0) + " m on a " +
           num::to_fixed(bend.grade * 100, 0) + " % pitch";
  }
  // R8 — the cross-fall the corridor keeps: read across the width, carried as a running mean.
  std::vector<double> raw_cross(n);
  for (size_t i = 0; i < n; i++) {
    const TrackPoint& p = pts[i];
    const double reach = p.width / 2;
    const double rx = libm::cos(p.heading);
    const double rz = -libm::sin(p.heading);
    const double fall = (sample_field(ground, p.x + rx * reach, p.z + rz * reach) -
                         sample_field(ground, p.x - rx * reach, p.z - rz * reach)) /
                        p.width;
    raw_cross[i] = clamp(fall, -T::camber, T::camber);
  }
  const size_t cw = static_cast<size_t>(num::round(40 / step));
  for (size_t i = 0; i < n; i++) {
    double sum = 0;
    double count = 0;
    for (size_t j = i > cw ? i - cw : 0; j <= std::min(n - 1, i + cw); j++) {
      sum += raw_cross[j];
      count++;
    }
    run.cross[i] = sum / count;
    if (plane) run.cross[i] = run.cross[i] * (1 - plane->w[i]) + plane->cross[i] * plane->w[i];
  }
  if (!run.into) {
    const size_t straight = static_cast<size_t>(num::round(T::finish / step));
    const size_t walked = n - straight - 1;
    for (size_t j = 1; j <= straight; j++) {
      run.cross[walked + j] =
          run.cross[walked] * (1 - smoothstep(0, 0.5, double(j) / double(straight)));
    }
  }
  return std::nullopt;
}

void funnel_into(WalkedRun& run, const WalkedRun& onto) {
  if (!run.into) return;
  const auto& tp = onto.track.points;
  const double j = clamp(num::round((run.into->s / onto.track.length) * double(tp.size() - 1)), 0,
                         double(tp.size() - 1));
  const double theirs = tp[size_t(j)].width;
  const double from = num::min(run.merge_start, run.core_from - 40);
  const double to = num::max(from + 20, run.core_from);
  for (TrackPoint& p : run.track.points) {
    if (p.width <= theirs) continue;
    p.width += (theirs - p.width) * smoothstep(from, to, p.s);
  }
}

}  // namespace ge::mountain::mapgen
