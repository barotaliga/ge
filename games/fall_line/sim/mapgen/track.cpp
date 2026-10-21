// Port of game5 `engine/mapgen/track.ts`.
//
// ge:summary The piste's walk, grading and corridor stamp
// ge:keywords track drawPiste gradePiste stampCorridor bendRoom
// ge:oracle game5/engine/mapgen/track.ts
// ge:skill sim
// ge:test tests/mapgen_generate_test.cpp
#include "mapgen/track.hpp"

#include <cmath>
#include <limits>
#include <unordered_map>

#include "core/js.hpp"
#include "core/js_text.hpp"
#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "core/noise.hpp"
#include "mapgen/berm.hpp"
#include "mapgen/spawn.hpp"

namespace fall_line::mapgen {

namespace js = ge::core::js;
namespace libm = ge::core::libm;
namespace R = rules;
using ge::core::angle_diff;
using ge::core::clamp;
using ge::core::hypot;
using ge::core::kTau;
using ge::core::sample_field;
using ge::core::smoothstep;

namespace {

constexpr double kInf = std::numeric_limits<double>::infinity();

// A sweep: a traverse held `angle` rad off the fall line for `length` m from arc `at` — or a
// schuss, held at 0.
struct Sweep {
  double at;
  double length;
  double angle;
};

// How far either side of a sweep the heading eases into and out of it, m.
constexpr double kSweepEase = 40;

// The wave's shape: |sin| to this power, signed — under one, so the walk lingers on the
// traverses and turns through the middle, as a piste does.
constexpr double kWaveShape = 0.6;

// How far ahead the walk reads the ground along a wanted heading, m.
constexpr double kLook = 30;

// R6 — how far either side of a station the fall is read for its bend's floor, m: the walk's
// own look, so the walk and the check judge a bend on one pitch.
constexpr double kBendPitch = kLook;

// Half-width of the window the walk's headings are smoothed over, in stations.
constexpr int kSmooth = 5;

// Half-width of the running mean the camber is carried down the piste with, m.
constexpr double kCamberWindow = 40;

// The wave's mean cosine at amplitude A, read off a quarter of it.
double wave_fall(double a) {
  double sum = 0;
  constexpr int n = 32;
  for (int i = 0; i < n; i++) {
    const double phase = ((i + 0.5) / n) * (kTau / 4);
    sum += libm::cos(a * libm::pow(libm::sin(phase), kWaveShape));
  }
  return sum / n;
}

// The amplitude whose wave spends `fall` of its length going down the map, up to the swing.
double amplitude_for(double fall) {
  double lo = 0;
  double hi = R::track::swing;
  if (wave_fall(hi) >= fall) return hi;
  for (int i = 0; i < 12; i++) {
    const double mid = (lo + hi) / 2;
    if (wave_fall(mid) > fall)
      lo = mid;
    else
      hi = mid;
  }
  return (lo + hi) / 2;
}

// The heading the ground steers a wanted heading to at a point (R5).
double steer(const ge::core::Heightfield& ground, double x, double z, double want, double bend,
             double steepest) {
  const double w = R::track::grade_window;
  const Fall fall = fall_along(ground, x, z, want, kLook);
  const double mean =
      (sample_field(ground, x, z) -
       sample_field(ground, x + libm::sin(want) * kLook, z + libm::cos(want) * kLook)) /
      kLook;
  if (mean >= R::track::min_fall * 1.5 && fall.steepest <= steepest) return want;
  // The mountain's own fall line here, read over the window.
  const double gx = (sample_field(ground, x + w, z) - sample_field(ground, x - w, z)) / (2 * w);
  const double gz = (sample_field(ground, x, z + w) - sample_field(ground, x, z - w)) / (2 * w);
  const double grade = hypot(gx, gz);
  const double down = libm::atan2(-gx, -gz);
  if (fall.steepest > steepest && grade > steepest) {
    // A traverse: at least this far off the fall line to hold the grade.
    const double theta = libm::acos(steepest / grade);
    const double off = angle_diff(down, want);
    const double side = std::abs(off) < 1e-6 ? (bend >= 0 ? 1 : -1) : js::sign(off);
    if (std::abs(off) < theta) return down + side * theta;
    return want;
  }
  // Toward the fall line, a little at a time.
  return want + clamp(angle_diff(want, down), -0.6, 0.6);
}

// The walk smoothed: its headings through a triangular window and the line laid again from
// the start along them (R6 still holds: a mean never turns tighter than the walk did).
void smooth_walk(std::vector<TrackPoint>& points, const Spawn& start) {
  const size_t n = points.size();
  std::vector<double> raw(n);
  for (size_t i = 0; i < n; i++) raw[i] = points[i].heading;
  const double step = R::track::step;
  double x = start.x;
  double z = start.z;
  for (size_t i = 1; i < n; i++) {
    double sum = 0;
    double norm = 0;
    for (int j = -kSmooth; j <= kSmooth; j++) {
      const double w = kSmooth + 1 - std::abs(j);
      const double k = clamp(double(i) + j, 1, double(n - 1));
      sum += raw[static_cast<size_t>(k)] * w;
      norm += w;
    }
    const double h = sum / norm;
    points[i].heading = h;
    x += libm::sin(h) * step;
    z += libm::cos(h) * step;
    points[i].x = x;
    points[i].z = z;
  }
}

// The bend radius at station i: the circumradius of it and the stations k either side, m.
double radius_at(const std::vector<TrackPoint>& pts, size_t i, size_t k) {
  const TrackPoint& a = pts[i - k];
  const TrackPoint& b = pts[i];
  const TrackPoint& c = pts[i + k];
  const double ab = hypot(b.x - a.x, b.z - a.z);
  const double bc = hypot(c.x - b.x, c.z - b.z);
  const double ca = hypot(a.x - c.x, a.z - c.z);
  const double area2 = std::abs((b.x - a.x) * (c.z - a.z) - (b.z - a.z) * (c.x - a.x));
  if (area2 < 1e-9) return kInf;
  return (ab * bc * ca) / (2 * area2);
}

size_t turn_k() {
  return static_cast<size_t>(js::max(1.0, js::round(R::track::turn_window / 2 / R::track::step)));
}

// One box-blur pass down an open profile, half-window k: carried on at its own grade above the
// start, held past the end.
std::vector<double> blur(const std::vector<double>& y, int k) {
  const int n = static_cast<int>(y.size());
  std::vector<double> out(y.size());
  const auto at = [&](int i) {
    return i < 0 ? y[0] + (y[0] - y[1]) * -i : i >= n ? y[size_t(n - 1)] : y[size_t(i)];
  };
  double sum = 0;
  for (int j = -k; j <= k; j++) sum += at(j);
  for (int i = 0; i < n; i++) {
    out[size_t(i)] = sum / (2 * k + 1);
    sum += at(i + k + 1) - at(i - k);
  }
  return out;
}

// How far inside a grade's colour ceiling the grading holds every colour window (R23).
constexpr double kColourMargin = 0.9;

}  // namespace

std::variant<Piste, std::string> draw_piste(ge::core::Rng& rng, const TerrainPlan& plan,
                                            const ge::core::Heightfield& ground,
                                            const Spawn& start) {
  namespace T = R::track;
  const GradeRow& G = *plan.grade;
  const double size = R::world::size;
  const double step = T::step;
  // Everything drawn is drawn here, in this order; the walk itself draws nothing.
  const double aim = in_band(rng, T::aim);
  const double bend_seed = ge::core::int_in(rng, 1, 1 << 30);
  const double width_seed = ge::core::int_in(rng, 1, 1 << 30);
  const double period = in_band(rng, T::wander_scale);
  double phase = ge::core::range(rng, 0, kTau);
  std::vector<Sweep> sweeps;
  const int n_sweeps =
      ge::core::int_in(rng, int32_t(G.track.sweeps.min), int32_t(G.track.sweeps.max));
  for (int i = 0; i < n_sweeps; i++) {
    const double at = ge::core::range(rng, 0.15, 0.8) * aim;
    const double length = in_band(rng, T::sweeps_length);
    const double angle = in_band(rng, T::sweeps_angle);
    sweeps.push_back({at, length, angle * (ge::core::chance(rng, 0.5) ? 1 : -1)});
  }
  // THE SCHUSSES: the same hold, down the fall line.
  const int n_schuss =
      ge::core::int_in(rng, int32_t(T::schuss_count.min), int32_t(T::schuss_count.max));
  for (int i = 0; i < n_schuss; i++) {
    const double at = ge::core::range(rng, 0.15, 0.8) * aim;
    const double length = in_band(rng, T::schuss_length);
    sweeps.push_back({at, length, 0});
  }
  const double z_end = size * T::finish_z;
  const double edge = R::mountain::flank::inner - 120;
  const auto most = static_cast<size_t>(std::ceil((T::length.max + 100) / step));
  const auto straight = static_cast<size_t>(js::round(T::finish / step));
  std::vector<double> cross(most + straight + 2, 0.0);
  // R7 — the width at a station: the noise's wander inside the grade's band, narrowed where
  // the untouched face falls steeply across the line; and the cross-slope, kept for the camber.
  const Band W = G.track.width;
  const auto width_at = [&](const TrackPoint& p, size_t i) {
    const double v = ge::core::value_noise(p.s, 0, T::width_scale, width_seed);
    const double wide = W.min + (W.max - W.min) * smoothstep(0.15, 0.85, v);
    const double rx = libm::cos(p.heading);
    const double rz = -libm::sin(p.heading);
    const double reach = wide / 2;
    const double fall = (sample_field(ground, p.x + rx * reach, p.z + rz * reach) -
                         sample_field(ground, p.x - rx * reach, p.z - rz * reach)) /
                        wide;
    cross[i] = clamp(fall, -T::camber, T::camber);
    return wide - (wide - W.min) * smoothstep(T::narrow.min, T::narrow.max, std::abs(fall));
  };
  // R6 — the turn a step may make, rad: the least radius the pitch allows, or the bench
  // radius of a wide piste, whichever is wider.
  const auto turn_for = [&](double width, double grade) {
    return (step / js::max(bend_floor(grade), width / 2 + T::shoulder_flat + R::berm::width + 4)) *
           0.85;
  };
  // The pitch under the walk: the raw ground's fall along the heading, ahead and behind.
  const auto pitch_at = [&](double px, double pz, double heading) {
    const double dx = libm::sin(heading) * kBendPitch;
    const double dz = libm::cos(heading) * kBendPitch;
    const double here = sample_field(ground, px, pz);
    const double ahead = (here - sample_field(ground, px + dx, pz + dz)) / kBendPitch;
    const double behind = (sample_field(ground, px - dx, pz - dz) - here) / kBendPitch;
    return js::max(ahead, behind);
  };
  double x = start.x;
  double z = start.z;
  double h = start.heading;
  double s = 0;
  std::vector<TrackPoint> points;
  points.reserve(most + straight + 2);
  points.push_back({x, z, 0, s, h, 0});
  points[0].width = width_at(points[0], 0);
  while (z < z_end) {
    if (points.size() > most)
      return std::string("the piste runs out of length before the valley floor");
    double want = h;
    if (s >= T::hold) {
      // THE WAVE, its wavelength wandering, its amplitude the controller's.
      const double vary =
          1 + T::wander_vary * (ge::core::value_noise(s, 0, 500, bend_seed) * 2 - 1);
      phase += (kTau * step) / (period * vary);
      const double need = clamp((z_end - z) / js::max(step, aim - s), 0.25, 1);
      const double a = amplitude_for(need);
      const double wave = libm::sin(phase);
      double bend = a * js::sign(wave) * libm::pow(std::abs(wave), kWaveShape);
      // THE SWEEPS, eased into and out of.
      for (const Sweep& sw : sweeps) {
        const double into = s - sw.at;
        if (into <= -kSweepEase || into >= sw.length + kSweepEase) continue;
        const double w = smoothstep(-kSweepEase, 0, into) *
                         (1 - smoothstep(sw.length, sw.length + kSweepEase, into));
        bend = bend * (1 - w) + sw.angle * w;
      }
      // THE FINISH: turned to the fall line for the arena.
      bend *= 1 - smoothstep(z_end - T::finish_turn, z_end - 20, z);
      // THE FACE: turned back from the side ridges.
      const double across = x - size / 2;
      const double away = smoothstep(edge, edge + 120, std::abs(across));
      bend = bend * (1 - away) + -js::sign(across) * 0.8 * away;
      want = clamp(steer(ground, x, z, bend, bend, G.track.steepest), -T::swing, T::swing);
    }
    const double max_turn = turn_for(points.back().width, pitch_at(x, z, h) * G.track.pitch);
    h += clamp(angle_diff(h, want), -max_turn, max_turn);
    x += libm::sin(h) * step;
    z += libm::cos(h) * step;
    s += step;
    TrackPoint p{x, z, 0, s, h, 0};
    p.width = width_at(p, points.size());
    points.push_back(p);
  }
  // THE FINISH STRAIGHT, opening out into the arena.
  const double arena = points.back().width;
  for (size_t k = 1; k <= straight; k++) {
    x += libm::sin(h) * step;
    z += libm::cos(h) * step;
    s += step;
    const double open = smoothstep(0, 0.5, double(k) / double(straight));
    TrackPoint p{x, z, 0, s, h, 0};
    const size_t at = points.size();
    cross[at] = clamp(cross[at - 1] * (1 - open), -T::camber, T::camber);
    p.width = arena + (T::width.max - arena) * open;
    points.push_back(p);
  }
  const double length = s;
  if (length < G.track.length.min || length > G.track.length.max) {
    return "the piste comes out " + js::to_fixed(length, 0) + " m long";
  }
  smooth_walk(points, start);
  const size_t walked = points.size() - straight - 1;
  for (size_t i = 0; i <= walked; i++) points[i].width = width_at(points[i], i);
  for (size_t k = 1; k <= straight; k++) {
    const double open = smoothstep(0, 0.5, double(k) / double(straight));
    points[walked + k].width = points[walked].width + (T::width.max - points[walked].width) * open;
    cross[walked + k] = cross[walked] * (1 - open);
  }
  // R8 — the camber varies slowly down the piste: a running mean of the cross-fall over the
  // camber window, so the corridor's edges do not corrugate.
  const auto cw = static_cast<long>(js::round(kCamberWindow / step));
  const size_t n = points.size();
  const std::vector<double> raw_cross(cross.begin(), cross.begin() + long(n));
  for (long i = 0; i < long(n); i++) {
    double sum = 0;
    double count = 0;
    for (long j = std::max(0L, i - cw); j <= std::min(long(n) - 1, i + cw); j++) {
      sum += raw_cross[size_t(j)];
      count++;
    }
    cross[size_t(i)] = sum / count;
  }
  set_headings(points);

  Piste piste{Track{std::move(points), length, nullptr}, std::vector<double>(n),
              std::vector<double>(cross.begin(), cross.begin() + long(n))};
  const auto& pts = piste.track.points;
  const BendReading bend = tightest_bend(
      piste.track, [&](size_t i) { return sample_field(ground, pts[i].x, pts[i].z); });
  if (bend.radius < bend.floor) {
    return "a bend tightens to " + js::to_fixed(bend.radius, 0) + " m on a " +
           js::to_fixed(bend.grade * 100, 0) + " % pitch";
  }
  const double room = bend_room(piste.track);
  if (room < 0) return "a bend folds the inside bench by " + js::to_fixed(-room, 0) + " m";
  const double gap = min_separation(piste.track);
  if (gap < T::separation_plan) {
    return "two stretches of the piste pass " + js::to_fixed(gap, 0) + " m apart";
  }
  // R1, R2 — the whole corridor, bank and all, stays on the map and on the face.
  const double reach = T::width.max / 2 + T::shoulder_flat + R::berm::width + T::bank_max + 10;
  for (const TrackPoint& p : pts) {
    if (p.x < reach || p.x > size - reach || p.z < reach || p.z > size - reach) {
      return std::string("the piste runs off the map");
    }
    const double rx = libm::cos(p.heading) * reach;
    const double rz = -libm::sin(p.heading) * reach;
    if (flank_at(plan, p.x + rx, p.z + rz) > 0.02 || flank_at(plan, p.x - rx, p.z - rz) > 0.02) {
      return std::string("the piste runs up a side ridge");
    }
  }
  return piste;
}

void set_headings(std::vector<TrackPoint>& points) {
  const size_t n = points.size();
  for (size_t i = 0; i < n; i++) {
    const TrackPoint& a = points[i == 0 ? 0 : i - 1];
    const TrackPoint& b = points[std::min(n - 1, i + 1)];
    points[i].heading = libm::atan2(b.x - a.x, b.z - a.z);
  }
}

double min_radius(const Track& track) {
  const auto& pts = track.points;
  const size_t n = pts.size();
  const size_t k = turn_k();
  double best = kInf;
  for (size_t i = k; i + k < n; i++) {
    const double r = radius_at(pts, i, k);
    if (r < best) best = r;
  }
  return best;
}

BendReading tightest_bend(const Track& track, const std::function<double(size_t)>& y_at) {
  const auto& pts = track.points;
  const size_t n = pts.size();
  const double step = R::track::step;
  const size_t k = turn_k();
  const auto m = static_cast<size_t>(js::max(1.0, js::round(kBendPitch / step)));
  BendReading worst{kInf, R::track::min_radius, 0, 0};
  double ratio = kInf;
  for (size_t i = k; i + k < n; i++) {
    const double r = radius_at(pts, i, k);
    if (r == kInf) continue;
    const size_t a = i >= m ? i - m : 0;
    const size_t b = std::min(n - 1, i + m);
    const double grade = (y_at(a) - y_at(b)) / js::max(step, pts[b].s - pts[a].s);
    const double floor = bend_floor(grade);
    if (r / floor < ratio) {
      ratio = r / floor;
      worst = {r, floor, grade, pts[i].s};
    }
  }
  return worst;
}

double bend_room(const Track& track) {
  const auto& pts = track.points;
  const size_t n = pts.size();
  const size_t k = turn_k();
  const double bench = R::track::shoulder_flat + R::berm::width;
  double least = kInf;
  for (size_t i = k; i + k < n; i++) {
    const TrackPoint& a = pts[i - k];
    const TrackPoint& b = pts[i];
    const TrackPoint& c = pts[i + k];
    const double ab = hypot(b.x - a.x, b.z - a.z);
    const double bc = hypot(c.x - b.x, c.z - b.z);
    const double ca = hypot(a.x - c.x, a.z - c.z);
    const double area2 = std::abs((b.x - a.x) * (c.z - a.z) - (b.z - a.z) * (c.x - a.x));
    if (area2 < 1e-9) continue;
    const double room = (ab * bc * ca) / (2 * area2) - (b.width / 2 + bench);
    if (room < least) least = room;
  }
  return least;
}

double min_separation(const Track& track) {
  const auto& pts = track.points;
  const size_t n = pts.size();
  const double cell = R::track::separation_plan;
  std::unordered_map<int64_t, std::vector<int32_t>> cells;
  const auto key = [](double c, double r) { return int64_t(c * 8192 + r); };
  for (size_t i = 0; i < n; i++) {
    cells[key(std::floor(pts[i].x / cell), std::floor(pts[i].z / cell))].push_back(int32_t(i));
  }
  const double along = R::track::separation_along;
  double best = kInf;
  for (size_t i = 0; i < n; i++) {
    const TrackPoint& p = pts[i];
    const double c = std::floor(p.x / cell);
    const double r = std::floor(p.z / cell);
    for (int dc = -1; dc <= 1; dc++) {
      for (int dr = -1; dr <= 1; dr++) {
        const auto it = cells.find(key(c + dc, r + dr));
        if (it == cells.end()) continue;
        for (const int32_t j : it->second) {
          if (size_t(j) <= i) continue;
          if (std::abs(pts[size_t(j)].s - p.s) <= along) continue;
          const double d = hypot(pts[size_t(j)].x - p.x, pts[size_t(j)].z - p.z);
          if (d < best) best = d;
        }
      }
    }
  }
  return best;
}

WindowGrades window_grades(const std::vector<double>& y, double step,
                           const std::vector<uint8_t>& skip) {
  const size_t n = y.size();
  const auto k = static_cast<size_t>(js::max(1.0, js::round(R::track::grade_window / step)));
  double steepest = -kInf;
  double gentlest = kInf;
  for (size_t i = 0; i + k < n; i++) {
    const size_t j = i + k;
    if (!skip.empty() && (skip[i] || skip[j])) continue;
    const double g = (y[i] - y[j]) / (double(k) * step);
    if (g > steepest) steepest = g;
    if (g < gentlest) gentlest = g;
  }
  return {steepest, gentlest};
}

std::optional<std::string> grade_piste(Piste& piste, const ge::core::Heightfield& country,
                                       double max_grade, std::optional<double> colour) {
  namespace T = R::track;
  auto& pts = piste.track.points;
  const size_t n = pts.size();
  const double step = piste.track.length / double(n - 1);
  auto& raw = piste.raw;
  for (size_t i = 0; i < n; i++) raw[i] = sample_field(country, pts[i].x, pts[i].z);
  // Aim a little inside both bounds, so the blur and the window cannot tip a stretch over.
  const double least = T::min_grade * 1.1 * step;
  const double most = max_grade * 0.95 * step;
  // NEVER CLIMBS: the cut from above and the fill from below, and their mean.
  std::vector<double> cut(raw);
  for (size_t i = 1; i < n; i++) cut[i] = js::min(raw[i], cut[i - 1] - least);
  std::vector<double> fill(raw);
  for (size_t i = n - 1; i-- > 0;) fill[i] = js::max(raw[i], fill[i + 1] + least);
  std::vector<double> y(n);
  for (size_t i = 0; i < n; i++) y[i] = (cut[i] + fill[i]) / 2;
  // NEVER STEEPER THAN THE RULE: the fill that holds every step from above and the cut that
  // holds it from below, and their mean — and on a graded map every colour window likewise.
  const size_t K =
      colour ? static_cast<size_t>(js::max(1.0, js::round(T::colour_window / step))) : 0;
  const double most_k = colour ? *colour * kColourMargin * double(K) * step : kInf;
  std::vector<double> held(y);
  for (size_t i = 1; i < n; i++) {
    held[i] = js::max(held[i], held[i - 1] - most);
    if (K > 0 && i >= K) held[i] = js::max(held[i], held[i - K] - most_k);
  }
  std::vector<double> shaved(y);
  for (size_t i = n - 1; i-- > 0;) {
    shaved[i] = js::min(shaved[i], shaved[i + 1] + most);
    if (K > 0 && i + K < n) shaved[i] = js::min(shaved[i], shaved[i + K] + most_k);
  }
  for (size_t i = 0; i < n; i++) y[i] = (held[i] + shaved[i]) / 2;
  // THE FINISH STRAIGHT, flat, and the run-out eased into it.
  const double flat_d = js::max(0.0, double(n) - 1 - js::round(T::finish / step));
  const auto flat = static_cast<size_t>(flat_d);
  for (size_t i = flat; i < n; i++) y[i] = y[flat];
  const int k = static_cast<int>(js::round(12 / step));
  y = blur(blur(y, k), k);
  const int ease = static_cast<int>(js::round(T::runout / (2 * step)));
  const std::vector<double> eased = blur(y, ease);
  const double from = js::max(0.0, flat_d - js::round((T::runout + 12) / step));
  for (size_t i = static_cast<size_t>(from); i < n; i++) {
    const double w = smoothstep(from, js::min(double(n) - 1, from + ease), double(i));
    y[i] = y[i] + (eased[i] - y[i]) * w;
  }
  double deepest = 0;
  for (size_t i = 0; i < n; i++) {
    pts[i].y = y[i];
    deepest = js::max(deepest, std::abs(y[i] - raw[i]));
  }
  if (deepest > T::max_cut) {
    return "the piste has to be cut " + js::to_fixed(deepest, 1) + " m into the mountain";
  }
  return std::nullopt;
}

Corridor stamp_corridor(const Piste& piste, ge::core::Heightfield& ground) {
  namespace T = R::track;
  const auto& pts = piste.track.points;
  const size_t n = pts.size();
  const int cols = ground.cols;
  const int rows = ground.rows;
  const double cell = ground.cell;
  const size_t cells = size_t(cols) * size_t(rows);
  Corridor out{ge::core::create_heightfield(0, 0, cell, cols, rows),
               std::vector<int32_t>(cells, -1), std::vector<float>(cells, 0.0f),
               std::vector<float>(cells, std::numeric_limits<float>::infinity())};
  auto& dist = out.dist;
  auto& near = out.near;
  auto& along = out.along;
  std::vector<float> target(cells, 0.0f);
  std::vector<float> half(cells, 0.0f);
  const double reach_max = T::width.max / 2 + T::shoulder_flat + R::berm::width + T::bank_max;
  // THE HEIGHT A CELL IS LEVELLED TO is read off the line as a smooth curve: each cell takes
  // its arc from the LOCAL CIRCLE of the nearest station (its heading and curvature), which is
  // the station's own perpendicular at the centreline and a smooth fan out at the bench — no
  // staircase on the outside of a bend.
  std::vector<double> fx(n), fz(n), kappa(n);
  for (size_t i = 0; i < n; i++) {
    fx[i] = libm::sin(pts[i].heading);
    fz[i] = libm::cos(pts[i].heading);
    const TrackPoint& a = pts[i == 0 ? 0 : i - 1];
    const TrackPoint& b = pts[std::min(n - 1, i + 1)];
    kappa[i] = angle_diff(a.heading, b.heading) / js::max(1e-9, b.s - a.s);
  }
  const double length = piste.track.length;
  // The graded line's height s m down it, carried on at its own grade off either end.
  const auto y_at = [&](double s) {
    const double u = (s / length) * double(n - 1);
    const auto i = static_cast<size_t>(clamp(std::floor(u), 0, double(n) - 2));
    return pts[i].y + (pts[i + 1].y - pts[i].y) * (u - double(i));
  };
  const auto level_at = [&](size_t k, double x, double z, double hw) {
    const TrackPoint& P = pts[k];
    const double vx = x - P.x;
    const double vz = z - P.z;
    const double a = vx * fx[k] + vz * fz[k];
    const double lateral = vx * fz[k] - vz * fx[k];
    const double c = kappa[k];
    double ds = a;
    if (std::abs(c) > 1e-4) {
      const double sgn = c > 0 ? 1 : -1;
      const double radius = 1 / std::abs(c);
      ds = libm::atan2(a, radius - sgn * lateral) * radius;
    }
    return y_at(P.s + ds) + piste.cross[k] * clamp(lateral, -hw, hw);
  };
  for (size_t i = 0; i + 1 < n; i++) {
    const TrackPoint& a = pts[i];
    const TrackPoint& b = pts[i + 1];
    const double dx = b.x - a.x;
    const double dz = b.z - a.z;
    double len2 = dx * dx + dz * dz;
    if (len2 == 0) len2 = 1;  // `|| 1`
    const int c0 = int(js::max(0.0, std::floor((js::min(a.x, b.x) - reach_max) / cell)));
    const int c1 =
        int(js::min(double(cols - 1), std::ceil((js::max(a.x, b.x) + reach_max) / cell)));
    const int r0 = int(js::max(0.0, std::floor((js::min(a.z, b.z) - reach_max) / cell)));
    const int r1 =
        int(js::min(double(rows - 1), std::ceil((js::max(a.z, b.z) + reach_max) / cell)));
    for (int r = r0; r <= r1; r++) {
      const double z = r * cell;
      for (int c = c0; c <= c1; c++) {
        const double x = c * cell;
        double t = ((x - a.x) * dx + (z - a.z) * dz) / len2;
        t = t < 0 ? 0 : t > 1 ? 1 : t;
        const double ex = x - (a.x + dx * t);
        const double ez = z - (a.z + dz * t);
        const size_t o = size_t(r) * size_t(cols) + size_t(c);
        // Most cells already hold a nearer segment: the square, with a margin far wider than
        // hypot's rounding and the float store's, turns those away before the dear hypot.
        const double held = dist[o];
        if (ex * ex + ez * ez > held * held * (1 + 1e-5)) continue;
        const double d = hypot(ex, ez);
        if (d < held) {
          dist[o] = static_cast<float>(d);
          near[o] = int32_t(i);
          along[o] = static_cast<float>(t);
          const double hw = (a.width + (b.width - a.width) * t) / 2;
          target[o] = static_cast<float>(level_at(t < 0.5 ? i : i + 1, x, z, hw));
          half[o] = static_cast<float>(hw);
        }
      }
    }
  }
  float* g = ground.data.data();
  float* p = out.packed.data.data();
  for (size_t o = 0; o < cells; o++) {
    const double d = dist[o];
    if (d == kInf) continue;
    const double hw = half[o];
    // R18 — the bench the windrow stands on is level too; the bank starts behind it.
    const double toe = hw + T::shoulder_flat;
    const double flat = toe + R::berm::width;
    const double delta = double(target[o]) - double(g[o]);
    const double bank = js::min(T::bank_max, js::max(T::bank_min, std::abs(delta) / T::bank_slope));
    const double w = d <= flat ? 1 : 1 - smoothstep(flat, flat + bank, d);
    g[o] = static_cast<float>(g[o] + w * delta);
    if (d > toe && d < flat) {
      g[o] = static_cast<float>(
          g[o] + berm_profile(berm_crest((near[o] + double(along[o])) * T::step), d - toe));
    }
    p[o] = static_cast<float>(d <= hw ? 1 : 1 - smoothstep(hw, hw + T::shoulder_packed, d));
  }
  return out;
}

}  // namespace fall_line::mapgen
