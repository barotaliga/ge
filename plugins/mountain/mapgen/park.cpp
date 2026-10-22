// The terrain park behind park.hpp: each size where it first fits down the line, the turn kept
// round the sizes, the built kickers stamped into the ground by arc, and the map read back.
//
// ge:summary R20 the terrain park: the plan down the line, the built profile, the stamp
// ge:keywords terrain-park plan-park stamp-park park-altitude landing-ease run-out-ease r20
// ge:skill mountain
// ge:test tests/mountain_park_test.cpp
#include "mountain/mapgen/park.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "core/heightfield.hpp"
#include "core/math.hpp"
#include "core/num.hpp"
#include "mountain/mapgen/query.hpp"
#include "mountain/mapgen/rules.hpp"
#include "mountain/mapgen/spawn.hpp"

namespace ge::mountain::mapgen {

namespace num = ge::core::num;
namespace T = rules::trick;
using ge::core::clamp;
using ge::core::smoothstep;

namespace {

// The sizes in the order they are laid, round and round.
constexpr std::array<TrickSize, 3> kOrder{TrickSize::low, TrickSize::medium, TrickSize::high};
// How much of a built landing is rounded over at its knuckle and out at its foot, and of its
// run-out at either end.
constexpr double kLandingRound = 0.25;
constexpr double kRunoutRound = 0.5;

const T::Size& size_of(TrickSize s) {
  return s == TrickSize::low ? T::low : s == TrickSize::medium ? T::medium : T::high;
}

// 0 … 1 over 0 … 1: a quadratic round-over for the first `round` of it, a straight slope through
// the middle and a quadratic round-out at the end, so the knuckle is rounded, the slope a skier
// lands on is one grade and the foot blends into the flat.
double ease(double x, double round) {
  if (x <= 0) return 0;
  if (x >= 1) return 1;
  const double m = 1 / (1 - round);
  if (x < round) return m * x * x / (2 * round);
  if (x < 1 - round) return m * (x - round / 2);
  return 1 - m * (1 - x) * (1 - x) / (2 * round);
}

// The arcs a kicker covers, its ramp's foot to its run-out's end, m.
std::pair<double, double> footprint(double s, double ramp, double landing) {
  return {s - ramp, s + landing};
}

// The line's height at arc `s`, off its stations.
double line_y(const Track& line, double s) { return track_point_at(line, s).y; }

// The kicker of `size` with its lip at station `p`, or nothing where it will not stand: inside
// the lead, on busy piste, too near a gate or without the run-in from the gate before, on a bend
// or on a pitch.
std::optional<Kicker> fits(const Track& line, const TrackPoint& p, TrickSize size, double free,
                           const std::vector<std::pair<double, double>>& busy, const ParkAsk& ask,
                           std::optional<double> last_end) {
  const T::Size& z = size_of(size);
  const double ramp = z.height * z.ramp;
  const double landing = z.deck + z.fall + z.runout;
  const double from = p.s - ramp;
  const double to = p.s + landing;
  if (from < free || to > line.length - T::lead) return std::nullopt;
  if (last_end && ask.slot > 0) {
    const double gap = from - *last_end;
    const bool ok = (gap >= ask.slot && gap <= ask.slot + ask.spread) || gap >= ask.far;
    if (!ok) return std::nullopt;
  }
  for (const auto& [a, b] : busy) {
    if (from < b + T::gap && to > a - T::gap) return std::nullopt;
  }
  double gate = -std::numeric_limits<double>::infinity();
  for (const double g : ask.gates) {
    if (g > from - T::gate_before && g < to + T::gate_after) return std::nullopt;
    if (g <= from - T::gate_before && g > gate) gate = g;
  }
  // The run-in from the gate before: enough fall to reach the lip from it.
  if (std::isfinite(gate) && line_y(line, gate) - line_y(line, from) < T::run_in * z.height) {
    return std::nullopt;
  }
  // Straight from the ramp's foot to the landing slope's, where the skier is on the ramp or in
  // the air, and on a moderate pitch.
  const double flight = p.s + z.deck + z.fall;
  const auto& pts = line.points;
  const double step = rules::track::step;
  const auto i0 = static_cast<size_t>(num::max(0.0, std::floor(from / step)));
  const auto i1 = static_cast<size_t>(num::min(double(pts.size() - 1), std::ceil(flight / step)));
  for (size_t j = i0; j <= i1; j++) {
    if (std::abs(ge::core::angle_diff(pts[j].heading, p.heading)) > T::straight) {
      return std::nullopt;
    }
  }
  if ((pts[i0].y - pts[i1].y) / (double(i1 - i0) * step) > T::max_grade) return std::nullopt;
  Kicker k;
  k.x = p.x;
  k.z = p.z;
  k.y = p.y;
  k.heading = p.heading;
  k.height = z.height;
  k.ramp = ramp;
  k.landing = landing;
  k.width = p.width + 2 * (rules::track::shoulder_flat + rules::berm::width);
  k.on_track = true;
  k.s = p.s;
  k.trick = size;
  k.shape = KickerShape{z.deck, z.fall, z.dig};
  return k;
}

}  // namespace

ParkPlan plan_park(const Track& line, std::span<const Kicker> taken, std::span<const Cliff> drops,
                   const ParkAsk& ask) {
  std::vector<std::pair<double, double>> busy;
  for (const Kicker& k : taken) {
    if (k.on_track) busy.push_back(footprint(k.s.value_or(0), k.ramp, k.landing));
  }
  for (const Cliff& d : drops) {
    if (d.on_track) busy.push_back(footprint(d.s.value_or(0), d.shelf, d.face + d.landing));
  }
  busy.insert(busy.end(), ask.avoid.begin(), ask.avoid.end());
  ParkPlan out;
  double free = start_gate_arc() + T::lead;
  size_t i = 0;
  size_t turn = 0;
  const auto& pts = line.points;
  while (i < pts.size() && double(out.field.size()) < T::count.max) {
    // The size whose turn it is, where it first fits, unless a smaller one fits more than
    // `wait` sooner: a stretch too short, bent or steep for the big lip still carries a small one,
    // and the turn goes on from whatever was laid.
    struct Pick {
      Kicker k;
      size_t at;
      size_t index;
    };
    std::optional<Pick> pick;
    const std::optional<double> last_end =
        out.field.empty()
            ? std::nullopt
            : std::optional<double>(out.field.back().s.value_or(0) + out.field.back().landing);
    for (size_t t = 0; t < kOrder.size(); t++) {
      const size_t at = (turn + kOrder.size() - t) % kOrder.size();
      if (t > 0 && size_of(kOrder[at]).height > size_of(kOrder[turn]).height) continue;
      for (size_t j = i; j < pts.size(); j++) {
        auto k = fits(line, pts[j], kOrder[at], free, busy, ask, last_end);
        if (!k) continue;
        if (!pick || k->s.value_or(0) + T::wait < pick->k.s.value_or(0)) {
          pick = Pick{std::move(*k), at, j};
        }
        break;
      }
    }
    if (!pick) break;
    pick->k.id = "T" + std::to_string(out.field.size() + 1);
    free = pick->k.s.value_or(0) + pick->k.landing + T::gap;
    turn = (pick->at + 1) % kOrder.size();
    i = pick->index + 1;
    out.field.push_back(std::move(pick->k));
  }
  if (double(out.field.size()) < T::count.min) {
    out.why = "only " + std::to_string(out.field.size()) + " park kicker(s) fit the piste (R20)";
    out.field.clear();
  }
  return out;
}

double park_altitude(const Kicker& k, double u, double foot, double floor, double line) {
  const KickerShape z = k.shape.value_or(KickerShape{0, 0, 0});
  if (u <= 0) {
    const double t = 1 + u / k.ramp;
    return foot + k.height * t * t;
  }
  const double top = foot + k.height;
  if (u <= z.deck) return top;
  const double v = u - z.deck;
  if (v < z.fall) return top - (top - floor) * ease(v / z.fall, kLandingRound);
  const double out = k.landing - z.deck - z.fall;
  return floor + (line - floor) * ease((v - z.fall) / out, kRunoutRound);
}

void stamp_park(Level& level, std::vector<Kicker> field) {
  const Track& line = level.track;
  // Each kicker's two altitudes: its ramp's foot on the line, and its landing's floor `dig`
  // under the line at the slope's foot.
  struct Frame {
    const Kicker* k;
    double s, foot, floor;
  };
  std::vector<Frame> frames;
  frames.reserve(field.size());
  for (const Kicker& k : field) {
    const KickerShape z = k.shape.value_or(KickerShape{0, 0, 0});
    const double s = k.s.value_or(0);
    frames.push_back({&k, s, line_y(line, s - k.ramp), line_y(line, s + z.deck + z.fall) - z.dig});
  }
  const double toe = rules::track::shoulder_flat + rules::berm::width;
  double widest = 0;
  for (const TrackPoint& p : line.points) widest = num::max(widest, p.width);
  const double reach = widest / 2 + toe + T::edge;
  ge::core::Heightfield& g = level.ground;
  // Every cell near a kicker, read once: its nearest point down the line, and the kicker there.
  std::vector<uint8_t> seen(g.data.size(), 0);
  std::vector<std::pair<size_t, float>> lifts;
  TrackHit hit;
  for (const Frame& f : frames) {
    for (int k = 0; f.s - f.k->ramp + 2.0 * k <= f.s + f.k->landing; k++) {
      const TrackPoint p = track_point_at(line, f.s - f.k->ramp + 2.0 * k);
      const auto col = [&](double v) { return (v - g.origin_x) / g.cell; };
      const auto row = [&](double v) { return (v - g.origin_z) / g.cell; };
      const int c0 = static_cast<int>(clamp(std::floor(col(p.x - reach)), 0, g.cols - 1));
      const int c1 = static_cast<int>(clamp(std::ceil(col(p.x + reach)), 0, g.cols - 1));
      const int r0 = static_cast<int>(clamp(std::floor(row(p.z - reach)), 0, g.rows - 1));
      const int r1 = static_cast<int>(clamp(std::ceil(row(p.z + reach)), 0, g.rows - 1));
      for (int r = r0; r <= r1; r++) {
        for (int c = c0; c <= c1; c++) {
          const size_t o =
              static_cast<size_t>(r) * static_cast<size_t>(g.cols) + static_cast<size_t>(c);
          if (seen[o] != 0) continue;
          seen[o] = 1;
          const double x = g.origin_x + c * g.cell;
          const double z = g.origin_z + r * g.cell;
          nearest_within(line, x, z, reach, hit);
          if (hit.distance > reach) continue;
          double lift = 0;
          for (const Frame& e : frames) {
            const double u = hit.s - e.s;
            if (u <= -e.k->ramp || u >= e.k->landing) continue;
            const double y = line_y(line, hit.s);
            lift = park_altitude(*e.k, u, e.foot, e.floor, y) - y;
            break;
          }
          if (lift == 0) continue;
          const double half = track_point_at(line, hit.s).width / 2 + toe;
          const double w = 1 - smoothstep(half, half + T::edge, hit.distance);
          lifts.emplace_back(o, static_cast<float>(w * lift));
        }
      }
    }
  }
  for (const auto& [o, lift] : lifts) g.data[o] += lift;
  // The map read back off the stamped ground: the line, the gates, the lips; a trunk the park
  // moved taken off.
  for (TrackPoint& p : level.track.points) p.y = level.ground_at(p.x, p.z);
  for (Checkpoint& c : level.checkpoints) c.y = track_point_at(level.track, c.s).y;
  std::vector<TreeDef> trees;
  trees.reserve(level.trees.size());
  for (TreeDef t : level.trees) {
    const double y = level.ground_at(t.x, t.z);
    if (std::abs(y - t.y) > T::tree_shift) continue;
    t.y = y;
    trees.push_back(t);
  }
  level.trees = std::move(trees);
  for (Kicker& k : field) k.y = level.ground_at(k.x, k.z);
  level.kickers.insert(level.kickers.end(), field.begin(), field.end());
}

}  // namespace ge::mountain::mapgen
