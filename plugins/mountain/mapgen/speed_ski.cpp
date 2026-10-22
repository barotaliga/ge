// The speed-skiing track behind speed_ski.hpp: the search (each line read and graded, each start
// skied by the point mass, each launch timed through the zone and stopped on the run-out), the
// grading of the winner into the map, and the two runs set over it.
//
// ge:summary R34 the speed track: the line searched, graded into the map, its two runs
// ge:keywords speed-skiing set-speed-ski speed-track timing-zone point-mass run-out hull r34
// ge:skill mountain
// ge:test tests/mountain_speed_ski_test.cpp
#include "mountain/mapgen/speed_ski.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <utility>
#include <vector>

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "core/num.hpp"
#include "core/prng.hpp"
#include "mountain/mapgen/discipline_rules.hpp"
#include "mountain/mapgen/rules.hpp"

namespace ge::mountain::mapgen {

namespace num = ge::core::num;
namespace libm = ge::core::libm;
namespace K = discipline_rules::speed_ski;
using ge::core::clamp;
using ge::core::hypot;
using ge::core::kPi;
using ge::core::smoothstep;

namespace {

constexpr double kG = 9.81;
// What a track's speed is dealt off beside the map's seed.
constexpr uint32_t kSpeedSkiSalt = 0x5bd1;

// A line down the face: its head, its bearing, and the graded profile and the ground under its
// middle every `search::step` m of plan along it.
struct Line {
  double x = 0;
  double z = 0;
  double heading = 0;
  std::vector<double> profile;
  std::vector<double> ground;
};

// Where on a line a track stands: the final's start (the platform's lip), the zone's two lines and
// the stop, as plan distances down the line, m; the final's speed, km/h; the launch, m.
struct Fit {
  double start;
  double entry;
  double exit;
  double stop;
  double speed;
  double launch;
};

// NEVER RISING down the track, every crest ROUNDED to no tighter than `crest` (cut) and every
// knee to no tighter than `knee` (filled). In place.
void round_profile(std::vector<double>& y, double step) {
  for (size_t i = 1; i < y.size(); i++) y[i] = num::min(y[i], y[i - 1]);
  hull_of(y, step, 1 / K::crest, true);
  hull_of(y, step, 1 / K::knee, false);
  for (size_t i = 1; i < y.size(); i++) y[i] = num::min(y[i], y[i - 1]);
}

// THE GROUND ACROSS A LINE, combed and graded, read every `step` m down it from its head until it
// leaves the map; nothing for a line too short to hold a track.
std::optional<Line> read_line(const Level& level, double x, double z, double heading) {
  const double step = K::search::step;
  const double fx = libm::sin(heading);
  const double fz = libm::cos(heading);
  const double rx = libm::cos(heading);
  const double rz = -libm::sin(heading);
  const double half = K::width / 2;
  const double lo = K::search::edge;
  const double hi = level.size - K::search::edge;
  std::vector<double> raw;
  Line line;
  for (int k = 0;; k++) {
    const double d = step * k;
    const double px = x + fx * d;
    const double pz = z + fz * d;
    if (px < lo || px > hi || pz > level.size - 40) break;
    double sum = 0;
    for (int j = -2; j <= 2; j++) {
      sum += level.ground_at(px + (rx * j * half) / 2, pz + (rz * j * half) / 2);
    }
    raw.push_back(sum / 5);
    line.ground.push_back(level.ground_at(px, pz));
  }
  const size_t n = raw.size();
  if (n < 80) return std::nullopt;
  // COMBED: the mean along it over `smooth` either side.
  const auto w = static_cast<size_t>(num::max(1.0, num::round(K::smooth / step)));
  line.profile.assign(n, 0);
  for (size_t i = 0; i < n; i++) {
    double s = 0;
    double c = 0;
    for (size_t j = i > w ? i - w : 0; j <= std::min(n - 1, i + w); j++) {
      s += raw[j];
      c += 1;
    }
    line.profile[i] = s / c;
  }
  round_profile(line.profile, step);
  line.x = x;
  line.z = z;
  line.heading = heading;
  return line;
}

// A point mass of the top class launched from sample `start` of `line` at a push's speed, tucked
// all the way down: the slope distance, the clock and the speed at every sample from it on; the
// count of samples written.
size_t tuck_down(const Line& line, size_t start, std::vector<double>& along,
                 std::vector<double>& clock, std::vector<double>& speed) {
  const K::Stop& S = K::stop;
  const double step = K::search::step;
  const std::vector<double>& y = line.profile;
  const double tuck = (0.5 * S.air * S.tuck) / S.mass;
  double v = 2;
  along[0] = 0;
  clock[0] = 0;
  speed[0] = v;
  size_t k = 1;
  for (size_t i = start; i + 1 < y.size(); i++, k++) {
    const double dy = y[i] - y[i + 1];
    const double ds = hypot(step, dy);
    const double fall = kG * ((dy - S.friction * step) / ds);
    const double v1 = std::sqrt(num::max(0.01, v * v + 2 * ds * (fall - tuck * v * v)));
    along[k] = along[k - 1] + ds;
    clock[k] = clock[k - 1] + (2 * ds) / (v + v1);
    speed[k] = v1;
    v = v1;
  }
  return k;
}

// Where the run-out stops a racer at `v` m/s at sample `from` of `line` (stood up into the wind,
// skidding once slow past the braking line) as a plan distance, m; nothing where the line ends
// first.
std::optional<double> stop_on(const Line& line, size_t from, double v) {
  const K::Stop& S = K::stop;
  const double step = K::search::step;
  const double exit_at = double(from) * step;
  const std::vector<double>& y = line.profile;
  const double stood = (0.5 * S.air * S.stood) / S.mass;
  for (size_t i = from; i + 1 < y.size(); i++) {
    const double dy = y[i] - y[i + 1];
    const double ds = hypot(step, dy);
    const double fall = kG * ((dy - S.friction * step) / ds);
    const bool braking = double(i + 1) * step - exit_at >= K::run_out::brake;
    const double skid = braking && v < S.below ? (S.skid * kG * step) / ds : 0;
    v = std::sqrt(num::max(0.0, v * v + 2 * ds * (fall - stood * v * v - skid)));
    if (v <= S.rest) return double(i + 1) * step;
  }
  return std::nullopt;
}

// The sample at which `along` (rising, `count` long) passes `length`, as a fraction; −1 where it
// never does.
double sample_at(const std::vector<double>& along, size_t count, double length) {
  size_t lo = 0;
  size_t hi = count - 1;
  if (along[hi] < length) return -1;
  while (hi - lo > 1) {
    const size_t mid = (lo + hi) >> 1U;
    if (along[mid] < length) {
      lo = mid;
    } else {
      hi = mid;
    }
  }
  return double(lo) + (length - along[lo]) / num::max(1e-9, along[hi] - along[lo]);
}

double lerp_at(const std::vector<double>& a, double f) {
  const auto i = static_cast<size_t>(std::floor(f));
  if (i + 1 >= a.size()) return a.back();
  return a[i] + (a[i + 1] - a[i]) * (f - double(i));
}

// How deep the stretch of `line` from `a` to `b` (plan distances, m) grades the ground under its
// middle, where it keeps clear of every lift station and the village and grades no deeper than
// `cut`; nothing where it does not.
std::optional<double> clear_of(const Level& level, const Line& line, double a, double b) {
  const double step = K::search::step;
  const double fx = libm::sin(line.heading);
  const double fz = libm::cos(line.heading);
  const auto near = [&](const Vec3& p, double r) {
    const double dx = p.x - line.x;
    const double dz = p.z - line.z;
    const double along = dx * fx + dz * fz;
    const double across = dx * fz - dz * fx;
    return along > a - r && along < b + r && std::abs(across) < K::width / 2 + r;
  };
  if (level.resort) {
    for (const Lift& l : level.resort->lifts) {
      if (near(l.bottom, K::stations) || near(l.top, K::stations)) return std::nullopt;
    }
    if (near(level.resort->village, 150)) return std::nullopt;
  }
  double deepest = 0;
  const double last = num::min(double(line.profile.size() - 1), b / step);
  for (auto i = static_cast<size_t>(num::max(0.0, num::round(a / step))); double(i) <= last; i++) {
    deepest = num::max(deepest, std::abs(line.profile[i] - line.ground[i]));
  }
  if (deepest > K::cut) return std::nullopt;
  return deepest;
}

// THE TRACK'S LINE on `level` (R34): the line and the fit that win (the final nearest the speed
// aimed at inside the band, a gentle trap, a run-out of a real track's length, then the least
// graded), or nothing on a face where no line fits.
std::optional<std::pair<Line, Fit>> find_track(const Level& level) {
  const double step = K::search::step;
  // THE SPEED THIS TRACK IS BUILT FOR, dealt off the map's seed inside the band.
  ge::core::Rng rng = ge::core::make_rng(num::to_uint32(level.seed) ^ kSpeedSkiSalt);
  const double aim =
      K::speed::aim.min + (K::speed::aim.max - K::speed::aim.min) * ge::core::next(rng);
  const K::Fit& F = K::fit;
  std::optional<std::pair<Line, Fit>> best;
  double best_score = 0;
  const auto size = static_cast<size_t>(std::ceil(level.size / step)) + 2;
  std::vector<double> along(size), clock(size), speed(size);
  for (int col = 0; K::search::edge + K::search::stride * col <= level.size - K::search::edge;
       col++) {
    const double x = K::search::edge + K::search::stride * col;
    for (const double deg : K::search::bearings) {
      const std::optional<Line> read = read_line(level, x, K::search::top, (deg * kPi) / 180);
      if (!read) continue;
      const Line& line = *read;
      const std::vector<double>& y = line.profile;
      const size_t n = y.size();
      for (size_t i = 0; i + 40 < n; i += static_cast<size_t>(K::search::starts)) {
        const size_t count = tuck_down(line, i, along, clock, speed);
        for (const double launch : K::launch) {
          const double e = sample_at(along, count, launch);
          const double x2 = e < 0 ? -1 : sample_at(along, count, launch + K::trap);
          if (x2 < 0) break;
          const double v = (K::trap / (lerp_at(clock, x2) - lerp_at(clock, e))) * 3.6;
          const double miss =
              std::abs(v - aim) + (v < K::speed::min || v > K::speed::max ? 1000 : 0);
          if (best && miss > best_score) continue;
          const double entry = (double(i) + e) * step;
          const double exit = (double(i) + x2) * step;
          const double trap_grade =
              (lerp_at(y, double(i) + e) - lerp_at(y, double(i) + x2)) / (exit - entry);
          double score = miss + F.trap * num::max(0.0, trap_grade - F.trap_grade) +
                         F.launch * std::abs(launch - F.launch_aim);
          if (best && score > best_score) continue;
          const std::optional<double> stop =
              stop_on(line, static_cast<size_t>(std::ceil(double(i) + x2)), lerp_at(speed, x2));
          if (!stop || *stop + K::past > double(n - 1) * step) continue;
          score += F.outrun * num::max(0.0, *stop - exit - F.outrun_most);
          if (best && score > best_score) continue;
          const std::optional<double> cut =
              clear_of(level, line, double(i) * step - K::platform, *stop + K::past);
          if (!cut) continue;
          score += F.cut * *cut;
          if (!best || score < best_score) {
            best = std::make_pair(line, Fit{double(i) * step, entry, exit, *stop, v, launch});
            best_score = score;
          }
        }
      }
    }
  }
  return best;
}

// The arc, m, `length` m along the snow down `points` from arc `from`.
double arc_along(const std::vector<TrackPoint>& points, double from, double length) {
  double along = 0;
  for (size_t i = 1; i < points.size(); i++) {
    const TrackPoint& a = points[i - 1];
    const TrackPoint& b = points[i];
    if (b.s <= from) continue;
    const double s0 = num::max(a.s, from);
    const double share = (b.s - s0) / (b.s - a.s);
    const double seg = hypot(b.s - a.s, b.y - a.y) * share;
    if (along + seg >= length) return s0 + ((length - along) / seg) * (b.s - s0);
    along += seg;
  }
  return points.back().s;
}

// The height of the track at arc `s`, m.
double height_at(const std::vector<TrackPoint>& points, double s) {
  const double step = points[1].s - points[0].s;
  const double f = clamp(s / step, 0, double(points.size() - 1));
  const auto i = static_cast<size_t>(num::min(double(points.size() - 2), std::floor(f)));
  return points[i].y + (points[i + 1].y - points[i].y) * (f - double(i));
}

// The speed through the zone, km/h, of the rule's point mass launched from arc `from` on the
// built track.
double zone_speed(const std::vector<TrackPoint>& points, double from, double entry, double exit) {
  const K::Stop& S = K::stop;
  const double tuck = (0.5 * S.air * S.tuck) / S.mass;
  double v = 2;
  double t = 0;
  for (int k = 0; from + k < exit; k++) {
    const double s = from + k;
    const double dy = height_at(points, s) - height_at(points, s + 1);
    const double len = hypot(1, dy);
    const double sin = dy / len;
    const double cos = 1 / len;
    const double v1 =
        std::sqrt(num::max(0.01, v * v + 2 * len * (kG * (sin - S.friction * cos) - tuck * v * v)));
    if (s >= entry) t += (2 * len) / (v + v1);
    v = v1;
  }
  return t > 0 ? (K::trap / t) * 3.6 : 0;
}

// A line across the track at arc `s`.
Checkpoint across(const Level& level, double s, double width) {
  const std::vector<TrackPoint>& points = level.track.points;
  const double step = points[1].s - points[0].s;
  const auto i = static_cast<size_t>(clamp(num::round(s / step), 0, double(points.size() - 1)));
  const TrackPoint& p = points[i];
  const double u = s - p.s;
  const double x = p.x + libm::sin(p.heading) * u;
  const double z = p.z + libm::cos(p.heading) * u;
  return {x, z, height_at(points, s), p.heading, width, s, GateColour::red};
}

// The graded map with the track on it, and the arcs both runs stand on.
struct Graded {
  Level level;
  double top;
  double entry;
  double exit;
  double stop;
  double qualify;
};

// A line straight down the middle of a face too small for any track, its profile the ground.
Line fallback_line(const Level& original) {
  const double step = K::search::step;
  if (auto line = read_line(original, original.size / 2, K::search::top, 0)) return *line;
  Line line;
  line.x = original.size / 2;
  line.z = K::search::top;
  for (int i = 0; i < 400; i++) {
    line.profile.push_back(original.ground_at(original.size / 2, K::search::top + i * step));
  }
  line.ground.assign(400, 0);
  return line;
}

// THE TRACK CUT INTO `original` (R34): its own `track`, the ground graded to its profile across
// its width, the snow groomed, the margin and the finish enclosure cleared, the piste's kickers
// and drops off it.
Graded grade_track(const Level& original) {
  const double step = K::search::step;
  const std::optional<std::pair<Line, Fit>> found = find_track(original);
  const Line line = found ? found->first : fallback_line(original);
  const Fit fit = found
                      ? found->second
                      : Fit{20, 300, 380, double(line.profile.size() - 30) * step, 0, K::launch[0]};
  const double fx = libm::sin(line.heading);
  const double fz = libm::cos(line.heading);
  const double rx = libm::cos(line.heading);
  const double rz = -libm::sin(line.heading);
  // The track: from the platform's top to past the stop, every 2 m.
  const double d0 = num::max(0.0, fit.start - K::platform);
  const double d_end = num::min(double(line.profile.size() - 1) * step, fit.stop + K::past);
  // Read between the samples on a cubic (Catmull-Rom): a straight line between them would leave
  // a kink in the snow every few metres, and a racer at 55 m/s rides every one of them.
  const std::vector<double>& P = line.profile;
  const double last = double(P.size() - 1);
  const auto y_at = [&](double d) {
    const double f = clamp(d / step, 0, last);
    const auto i = static_cast<size_t>(num::min(last - 1, std::floor(f)));
    const double u = f - double(i);
    const double p0 = P[i > 0 ? i - 1 : 0];
    const double p1 = P[i];
    const double p2 = P[i + 1];
    const double p3 = P[std::min(P.size() - 1, i + 2)];
    const double y =
        p1 +
        0.5 * u * (p2 - p0 + u * (2 * p0 - 5 * p1 + 4 * p2 - p3 + u * (3 * (p1 - p2) + p3 - p0)));
    // Held between its two samples: the cubic may overshoot them by a hair where the pitch
    // changes, and the track never rises.
    return clamp(y, num::min(p1, p2), num::max(p1, p2));
  };
  Level level = original;
  std::vector<TrackPoint>& points = level.track.points;
  points.clear();
  for (int k = 0; 2.0 * k <= d_end - d0 + 1e-6; k++) {
    const double s = 2.0 * k;
    points.push_back(
        {line.x + fx * (d0 + s), line.z + fz * (d0 + s), y_at(d0 + s), s, line.heading, K::width});
  }
  level.track.length = points.back().s;
  level.track.index.reset();
  // THE GROUND graded to the profile across the track, eased out past it; the snow groomed.
  const double half = K::width / 2;
  ge::core::Heightfield& ground = level.ground;
  ge::core::Heightfield& packed = level.packed;
  const double fade = rules::track::shoulder_packed;
  const double reach = half + num::max(K::ease, fade) + 2;
  double x0 = std::numeric_limits<double>::infinity();
  double x1 = -x0;
  double z0 = x0;
  double z1 = -x0;
  // Graded whole two cells past either end, so the grid's bilinear read at the track's first and
  // last points is the profile: past the line's own samples the profile runs on at its end's
  // pitch.
  const double lo = d0 - 2 * ground.cell;
  const double hi = d_end + 2 * ground.cell;
  const double first = 0;
  const double end = last * step;
  const auto graded_at = [&](double d) {
    if (d < first) return y_at(first) + (y_at(first) - y_at(first + step)) / step * (first - d);
    if (d > end) return y_at(end) - (y_at(end - step) - y_at(end)) / step * (d - end);
    return y_at(d);
  };
  for (const double a : {lo - K::ease, hi + K::ease}) {
    for (const double c : {-reach, reach}) {
      const double px = line.x + fx * a + rx * c;
      const double pz = line.z + fz * a + rz * c;
      x0 = num::min(x0, px);
      x1 = num::max(x1, px);
      z0 = num::min(z0, pz);
      z1 = num::max(z1, pz);
    }
  }
  const auto col_of = [&](double v, bool up) {
    const double f = (v - ground.origin_x) / ground.cell;
    return static_cast<int>(clamp(up ? std::ceil(f) : std::floor(f), 0, ground.cols - 1));
  };
  const auto row_of = [&](double v, bool up) {
    const double f = (v - ground.origin_z) / ground.cell;
    return static_cast<int>(clamp(up ? std::ceil(f) : std::floor(f), 0, ground.rows - 1));
  };
  const int c0 = col_of(x0, false), c1 = col_of(x1, true);
  const int r0 = row_of(z0, false), r1 = row_of(z1, true);
  for (int r = r0; r <= r1; r++) {
    for (int c = c0; c <= c1; c++) {
      const double x = ground.origin_x + c * ground.cell;
      const double z = ground.origin_z + r * ground.cell;
      const double dx = x - line.x;
      const double dz = z - line.z;
      const double along = dx * fx + dz * fz;
      const double off = std::abs(dx * rx + dz * rz);
      const double ends =
          smoothstep(lo - K::ease, lo, along) * (1 - smoothstep(hi, hi + K::ease, along));
      if (ends <= 0) continue;
      const size_t i =
          static_cast<size_t>(r) * static_cast<size_t>(ground.cols) + static_cast<size_t>(c);
      const double w = (1 - smoothstep(half, half + K::ease, off)) * ends;
      if (w > 0) {
        float& h = ground.data[i];
        h = static_cast<float>(h + w * (graded_at(along) - h));
      }
      const double firm = (1 - smoothstep(half, half + fade, off)) * ends;
      if (i < packed.data.size() && firm > packed.data[i]) {
        packed.data[i] = static_cast<float>(firm);
      }
    }
  }
  // THE MARGIN and the finish enclosure cleared; the piste's kickers and drops in the cut taken
  // out with the ground they stood on, and the rest on no track a racer skis.
  const K::Arena A = K::arena;
  const auto in_cut = [&](double x, double z, double r) {
    const double dx = x - line.x;
    const double dz = z - line.z;
    const double along = dx * fx + dz * fz;
    const double off = std::abs(dx * rx + dz * rz);
    if (along > d0 - 20 && along < d_end + 10 && off < half + r) return true;
    return along > fit.stop - A.before && along < fit.stop + A.past && off < A.half;
  };
  std::vector<TreeDef> trees;
  trees.reserve(original.trees.size());
  for (const TreeDef& t : original.trees) {
    if (!in_cut(t.x, t.z, K::margin)) trees.push_back(t);
  }
  level.trees = std::move(trees);
  std::vector<Kicker> kickers;
  for (const Kicker& k : original.kickers) {
    if (in_cut(k.x, k.z, K::ease + k.ramp)) continue;
    kickers.push_back(k);
    kickers.back().on_track = false;
    kickers.back().s = std::nullopt;
  }
  level.kickers = std::move(kickers);
  std::vector<Cliff> cliffs;
  for (const Cliff& k : original.cliffs) {
    if (in_cut(k.x, k.z, K::ease + k.width / 2)) continue;
    cliffs.push_back(k);
    cliffs.back().on_track = false;
    cliffs.back().s = std::nullopt;
  }
  level.cliffs = std::move(cliffs);
  level.drifts.clear();
  level.race.reset();
  // The arcs on the built track: the final's wand at the platform's lip, the zone along the snow
  // from it.
  const double top = fit.start - d0;
  const double entry = arc_along(points, top, fit.launch);
  const double exit = arc_along(points, entry, K::trap);
  const double stop = fit.stop - d0;
  const double final_speed = zone_speed(points, top, entry, exit);
  // THE QUALIFICATION'S START, lowered until the launch gives `qualify` km/h less.
  double qualify = top;
  for (int k = 0; top + k < entry - 60; k++) {
    qualify = top + k;
    if (zone_speed(points, qualify, entry, exit) <= final_speed - K::qualify) break;
  }
  return {std::move(level), top, entry, exit, stop, qualify};
}

// Run `run`'s track over the graded map `g`, set over `original`.
Level run_over(const Graded& g, const std::shared_ptr<const Level>& original, int run) {
  Level out = g.level;
  const double from = run == 2 ? g.top : g.qualify;
  const Checkpoint start = across(out, from, 2);
  const Checkpoint exit = across(out, g.exit, K::width + 2 * K::margin);
  out.checkpoints = {start, across(out, g.entry, K::width + 2 * K::margin), exit};
  // The zone's length along the snow, read every half metre.
  const std::vector<TrackPoint>& pts = out.track.points;
  double zone = 0;
  for (int k = 0; g.entry + 0.5 * k < g.exit - 1e-9; k++) {
    const double s = g.entry + 0.5 * k;
    zone += hypot(num::min(0.5, g.exit - s),
                  height_at(pts, s) - height_at(pts, num::min(g.exit, s + 0.5)));
  }
  const Checkpoint house = across(out, from - K::stand, 2);
  out.spawn = {house.x, house.z, start.heading};
  out.grid = {out.spawn};
  RaceCourse race;
  race.discipline = Discipline::speed_ski;
  race.run = run;
  race.base = original;
  race.from = from;
  race.to = g.exit;
  race.vertical = start.y - exit.y;
  race.zone = RaceCourse::Zone{g.entry, g.exit, zone};
  race.top = g.top;
  race.qualify = g.qualify;
  race.stop = g.stop;
  race.width = K::width;
  out.race = std::move(race);
  return out;
}

}  // namespace

void hull_of(std::vector<double>& y, double step, double c, bool below) {
  const size_t n = y.size();
  const double k = below ? c / 2 : -c / 2;
  const auto lift = [&](size_t i) {
    const double x = double(i) * step;
    return y[i] + k * x * x;
  };
  const double sign = below ? 1 : -1;
  std::vector<size_t> hull;
  for (size_t i = 0; i < n; i++) {
    while (hull.size() >= 2) {
      const size_t a = hull[hull.size() - 2];
      const size_t b = hull.back();
      // b on the wrong side of the chord a–i: not on the hull.
      const double cross =
          (lift(b) - lift(a)) * double(i - a) - (lift(i) - lift(a)) * double(b - a);
      if (sign * cross >= 0) {
        hull.pop_back();
      } else {
        break;
      }
    }
    hull.push_back(i);
  }
  std::vector<double> out(n, 0);
  for (size_t h = 0; h + 1 < hull.size(); h++) {
    const size_t a = hull[h];
    const size_t b = hull[h + 1];
    for (size_t i = a; i <= b; i++) {
      const double at =
          lift(a) + ((lift(b) - lift(a)) * double(i - a)) / num::max(1.0, double(b - a));
      const double x = double(i) * step;
      out[i] = at - k * x * x;
    }
  }
  if (n == 1) out[0] = y[0];
  y = std::move(out);
}

Level set_speed_ski(const Level& level, int run) {
  if (level.race && level.race->discipline == Discipline::speed_ski) {
    if (level.race->run == run) return level;
    // The other run's track: the same graded map, its start moved.
    const RaceCourse& r = *level.race;
    Graded g{level, r.top, r.zone ? r.zone->from : 0, r.to, r.stop, r.qualify};
    g.level.race.reset();
    Level out = run_over(g, r.base, run);
    out.sun = level.sun;
    out.weather = level.weather;
    return out;
  }
  const std::shared_ptr<const Level> original =
      level.race ? level.race->base : std::make_shared<const Level>(level);
  Level out = run_over(grade_track(*original), original, run);
  out.sun = level.sun;
  out.weather = level.weather;
  return out;
}

double speed_ski_aim(const Level& level) {
  if (!level.race || level.race->discipline != Discipline::speed_ski || !level.race->zone) {
    return 0;
  }
  const RaceCourse& r = *level.race;
  return zone_speed(level.track.points, r.from, r.zone->from, r.zone->to);
}

std::optional<SpeedSkiLines> speed_ski_lines(const Level& level) {
  if (!level.race || level.race->discipline != Discipline::speed_ski) return std::nullopt;
  const RaceCourse& r = *level.race;
  Checkpoint braking = across(level, r.to + K::run_out::brake, r.width);
  braking.colour = GateColour::blue;
  return SpeedSkiLines{braking, across(level, r.stop, r.width + 6)};
}

}  // namespace ge::mountain::mapgen
