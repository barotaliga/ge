// The ski-cross course behind ski_cross.hpp: the stretch picked, the line's corners dealt and
// rounded into turns, the profile combed and its start cut, the features dealt and built onto it,
// the ground graded and banked, the trees, kickers and drops cleared, the gates and the lanes.
//
// ge:summary R35 the ski-cross course: stretch, line, profile, features, grading, gates, lanes
// ge:keywords ski-cross set-ski-cross berm bank rollers jump step-down start-ramp weave r35
// ge:skill mountain
// ge:test tests/mountain_ski_cross_test.cpp
#include "mountain/mapgen/ski_cross.hpp"

#include <algorithm>
#include <array>
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
#include "mountain/mapgen/query.hpp"
#include "mountain/mapgen/rules.hpp"
#include "mountain/mapgen/speed_ski.hpp"

namespace ge::mountain::mapgen {

namespace num = ge::core::num;
namespace libm = ge::core::libm;
namespace K = discipline_rules::ski_cross;
using ge::core::clamp;
using ge::core::hypot;
using ge::core::kPi;
using ge::core::smoothstep;
using Feature = RaceCourse::Feature;

namespace {

// What the course's set is dealt off beside the map's seed.
constexpr uint32_t kSkiCrossSalt = 0x5c1c05;
constexpr double kStep = K::station;
// How far inside the length band a stretch's estimated course is aimed, a share.
constexpr double kLengthMargin = 0.04;

// A plan point and the way down the course there.
struct Pose {
  double x = 0;
  double z = 0;
  double heading = 0;
};

// A piece of the line: straight from (x, z), or a turn of `r` m round (cx, cz), `sign` +1 to the
// right, entered at `heading`.
struct Piece {
  bool arc = false;
  double x = 0, z = 0, heading = 0, length = 0;
  double cx = 0, cz = 0, r = 0;
  int sign = 0;
};

// A turn of the line: its arcs, which way it turns and how far, degrees.
struct Turn {
  double from, to;
  int sign;
  double angle;
};

double right_x(double h) { return libm::cos(h); }
double right_z(double h) { return -libm::sin(h); }

// How far a line through the piste's line from `from` to `to`, a point every leg's mean length
// along it, runs in plan, m: what a course weaving down the stretch is laid round.
double plan_length(const Level& level, double from, double to) {
  const double leg = (K::leg.min + K::leg.max) / 2;
  const int legs = std::max(1, static_cast<int>(num::round((to - from) / leg)));
  double out = 0;
  TrackPoint a = track_point_at(level.track, from);
  for (int k = 1; k <= legs; k++) {
    const TrackPoint b = track_point_at(level.track, from + (to - from) * k / legs);
    out += hypot(b.x - a.x, b.z - a.z);
    a = b;
  }
  return out;
}

// THE STRETCH OF THE PISTE: of every stretch `axis` long, the one whose course comes nearest the
// aimed mean gradient (inside the rule's bands where one does), crossing the fewest drops, and
// then the lowest.
RaceCourse::Stretch pick_stretch(const Level& level) {
  const double L = level.track.length;
  const K::Axis A = K::axis;
  std::optional<RaceCourse::Stretch> best;
  double best_score = 0;
  for (int a = 0; A.min + a * A.len_step <= num::min(A.max, L); a++) {
    const double len = A.min + a * A.len_step;
    for (int b = 0; L - b * A.step - len >= 0; b++) {
      const double to = L - b * A.step;
      const double from = to - len;
      const double drop = track_point_at(level.track, from).y - track_point_at(level.track, to).y;
      const double course = plan_length(level, from, to) - K::start.platform - K::finish.area;
      const double grade = drop / course;
      double score = std::abs(grade - K::grade.aim);
      if (grade < K::grade.min || grade > K::grade.max) score += 1;
      if (drop < K::vertical.min || drop > K::vertical.max) score += 1;
      // The length band aimed inside by a margin: the estimate is a few per cent either way.
      if (course < K::length.min * (1 + kLengthMargin) ||
          course > K::length.max * (1 - kLengthMargin)) {
        score += 1;
      }
      for (const Cliff& d : level.cliffs) {
        const double s = d.s.value_or(-1);
        if (d.on_track && s > from && s < to) score += K::drop;
      }
      // ...and then the lowest: the finish on the valley floor.
      score += (L - to) * 1e-5;
      if (!best || score < best_score) {
        best = RaceCourse::Stretch{from, to};
        best_score = score;
      }
    }
  }
  return best.value_or(RaceCourse::Stretch{0, L});
}

// THE LINE'S CORNERS down the stretch: its top, the end of the start straight, the legs swung
// either side of the piste's line in turn, the start of the finish straight and its end.
std::vector<Pose> corners(const Level& level, ge::core::Rng& rng, double from, double to) {
  const auto at = [&](double s, double off) {
    const TrackPoint p = track_point_at(level.track, s);
    return Pose{p.x + right_x(p.heading) * off, p.z + right_z(p.heading) * off, p.heading};
  };
  std::vector<Pose> out{at(from, 0)};
  // The start straight's end: where the piste's line stands the platform, the straight and the
  // first turn's room clear of the top in plan.
  double first = from;
  for (int k = 1; first < to; k++) {
    first = from + k * kStep;
    const Pose p = at(first, 0);
    const double clear = K::start.platform + K::start.straight + K::start.room;
    if (hypot(p.x - out[0].x, p.z - out[0].z) >= clear) break;
  }
  out.push_back(at(first, 0));
  // The finish straight: the finish area and a finish jump's room above it.
  const double last = to - (K::finish.area + K::finish.straight);
  int side = ge::core::chance(rng, 0.5) ? 1 : -1;
  double s = first;
  while (true) {
    const double leg = ge::core::range(rng, K::leg.min, K::leg.max);
    if (s + leg > last - K::leg.min * 0.5) break;
    s += leg;
    out.push_back(at(s, side * ge::core::range(rng, K::swing.min, K::swing.max)));
    side = -side;
  }
  out.push_back(at(last, 0));
  out.push_back(at(to, 0));
  return out;
}

struct Laid {
  std::vector<Piece> pieces;
  std::vector<Turn> turns;
  double length = 0;
};

// THE LINE through the corners, each rounded to a radius dealt off `rng`: tighter only where the
// legs either side leave no room for it.
Laid lay(ge::core::Rng& rng, const std::vector<Pose>& pts) {
  const size_t n = pts.size();
  std::vector<double> heads(n - 1);
  std::vector<double> lens(n - 1);
  for (size_t i = 0; i + 1 < n; i++) {
    heads[i] = libm::atan2(pts[i + 1].x - pts[i].x, pts[i + 1].z - pts[i].z);
    lens[i] = hypot(pts[i + 1].x - pts[i].x, pts[i + 1].z - pts[i].z);
  }
  std::vector<double> cut(n, 0);
  std::vector<double> radius(n, 0);
  std::vector<double> turn(n, 0);
  for (size_t i = 1; i + 1 < n; i++) {
    const double th = ge::core::angle_diff(heads[i - 1], heads[i]);
    turn[i] = th;
    const double half = libm::tan(std::abs(th) / 2);
    if (half < 1e-4) continue;
    double r = ge::core::range(rng, K::turn.radius.min, K::turn.radius.max);
    double room = K::turn.room * num::min(lens[i - 1], lens[i]);
    // The start straight keeps its length to the first direction change.
    if (i == 1) room = num::min(room, lens[0] - K::start.platform - K::start.straight);
    if (r * half > room) r = num::max(K::turn.tightest, room / half);
    radius[i] = r;
    cut[i] = r * half;
  }
  Laid out;
  double s = 0;
  for (size_t i = 0; i + 1 < n; i++) {
    const double h = heads[i];
    const double fx = libm::sin(h);
    const double fz = libm::cos(h);
    const double len = num::max(0.0, lens[i] - cut[i] - cut[i + 1]);
    Piece line;
    line.x = pts[i].x + fx * cut[i];
    line.z = pts[i].z + fz * cut[i];
    line.heading = h;
    line.length = len;
    out.pieces.push_back(line);
    s += len;
    const size_t j = i + 1;
    if (j + 1 < n && radius[j] > 0) {
      const int sign = turn[j] >= 0 ? 1 : -1;
      const double tx = pts[j].x - fx * cut[j];
      const double tz = pts[j].z - fz * cut[j];
      Piece arc;
      arc.arc = true;
      arc.r = radius[j];
      arc.cx = tx + right_x(h) * arc.r * sign;
      arc.cz = tz + right_z(h) * arc.r * sign;
      arc.heading = h;
      arc.sign = sign;
      arc.length = arc.r * std::abs(turn[j]);
      out.pieces.push_back(arc);
      out.turns.push_back({s, s + arc.length, sign, std::abs(turn[j]) * 180 / kPi});
      s += arc.length;
    }
  }
  out.length = s;
  return out;
}

// Where the line is `s` m down it.
Pose pose_at(const std::vector<Piece>& pieces, double s) {
  double u = num::max(0.0, s);
  for (size_t i = 0; i < pieces.size(); i++) {
    const Piece& p = pieces[i];
    if (u > p.length && i + 1 < pieces.size()) {
      u -= p.length;
      continue;
    }
    if (!p.arc) {
      return {p.x + libm::sin(p.heading) * u, p.z + libm::cos(p.heading) * u, p.heading};
    }
    const double h = p.heading + (p.sign * u) / p.r;
    return {p.cx - right_x(h) * p.r * p.sign, p.cz - right_z(h) * p.r * p.sign, h};
  }
  return {};
}

// THE PROFILE down the line, a sample every station: the ground across the course's width taken
// to its mean along it, never rising, its crests and knees rounded.
std::vector<double> base_profile(const Level& level, const Laid& laid) {
  const auto n = static_cast<size_t>(std::floor(laid.length / kStep)) + 1;
  std::vector<double> raw(n);
  const double half = K::width / 2;
  for (size_t i = 0; i < n; i++) {
    const Pose p = pose_at(laid.pieces, double(i) * kStep);
    double sum = 0;
    for (int k = -2; k <= 2; k++) {
      const double off = k * half / 2;
      sum += level.ground_at(p.x + right_x(p.heading) * off, p.z + right_z(p.heading) * off);
    }
    raw[i] = sum / 5;
  }
  const auto w = static_cast<size_t>(num::max(1.0, num::round(K::smooth / kStep)));
  std::vector<double> y(n);
  for (size_t i = 0; i < n; i++) {
    double sum = 0;
    double count = 0;
    for (size_t j = i > w ? i - w : 0; j <= std::min(n - 1, i + w); j++) {
      sum += raw[j];
      count += 1;
    }
    y[i] = sum / count;
  }
  for (size_t i = 1; i < n; i++) y[i] = num::min(y[i], y[i - 1]);
  hull_of(y, kStep, 1 / K::crest, true);
  hull_of(y, kStep, 1 / K::knee, false);
  for (size_t i = 1; i < n; i++) y[i] = num::min(y[i], y[i - 1]);
  return y;
}

// THE START: the platform level under the doors, and the START RAMP cut below them to the rule's
// grade, eased back onto the line. In place.
void cut_start(std::vector<double>& y) {
  const double P = K::start.platform;
  const K::Ramp R = K::start.ramp;
  const auto at = [&](double s) {
    return y[std::min(y.size() - 1, static_cast<size_t>(num::round(s / kStep)))];
  };
  const double level = at(P);
  const double natural = (at(P) - at(P + R.length)) / R.length;
  // A smoothstep's steepest is 1.5 times its mean; the ease back onto the line never rises, so
  // it is no steeper than the line falls under it.
  const double under = (at(P + R.length) - at(P + R.length + R.ease)) / R.ease;
  const double deep = num::min(num::min(R.most, under * R.ease / 1.5),
                               num::max(0.0, R.grade - natural) * R.length / 1.5);
  const double knee = P + R.length;
  for (size_t i = 0; i < y.size(); i++) {
    const double s = double(i) * kStep;
    if (s <= P) {
      y[i] = level;
    } else if (s < knee) {
      y[i] -= deep * smoothstep(P, knee, s);
    } else {
      y[i] -= deep * (1 - smoothstep(knee, knee + R.ease, s));
    }
  }
  // Where the line under the ease falls unevenly, the ease is held level rather than rising.
  for (size_t i = 1; i < y.size(); i++) y[i] = num::min(y[i], y[i - 1]);
}

// A feature built on a straight: what it is and its shape, its offset over the line at `s`.
struct Built {
  Feature f{};
  double spacing = 0;  // a roller's
  double ramp = 0, deck = 0, fall = 0, dig = 0, out = 0, grade = 0, landing = 0;

  double offset(double s) const {
    if (f.kind == CrossFeature::rollers) {
      if (s <= f.from || s >= f.to) return 0;
      return f.height * (1 - libm::cos(2 * kPi * (s - f.from) / spacing)) / 2;
    }
    const double u = s - f.lip;
    if (u <= -ramp || u >= landing) return 0;
    const double top = f.height + grade * deck;
    if (f.kind == CrossFeature::jump) {
      // Up its ramp to the lip, across its level table, down its long landing dug under the
      // line and back up onto it.
      if (u <= 0) {
        const double k = 1 + u / ramp;
        return f.height * k * k;
      }
      if (u <= deck) return f.height + grade * u;
      const double v = u - deck;
      if (v < fall) return top - (top + dig) * smoothstep(0, 1, v / fall);
      return -dig * (1 - smoothstep(0, 1, (v - fall) / out));
    }
    // A step-down: raised over its long approach, a level lip, the landing back onto the line.
    if (u <= 0) return f.height * smoothstep(-ramp, 0, u);
    if (u <= deck) return f.height + grade * u;
    const double v = u - deck;
    if (v < fall) return top * (1 - smoothstep(0, 1, v / fall));
    return 0;
  }
};

// A feature dealt for a straight, before it is placed: its whole length along the line.
struct Plan {
  Built b;
  double length;
};

std::optional<Plan> rollers_in(ge::core::Rng& rng, double room) {
  const K::Rollers R = K::rollers;
  const double spacing = ge::core::range(rng, R.spacing.min, R.spacing.max);
  const int count = std::min(ge::core::int_in(rng, R.least, R.most),
                             static_cast<int>(std::floor(room / spacing)));
  if (count < R.least) return std::nullopt;
  Plan p{};
  p.b.f.kind = CrossFeature::rollers;
  p.b.f.height = ge::core::range(rng, R.height.min, R.height.max);
  p.b.spacing = spacing;
  p.length = count * spacing;
  return p;
}

// A JUMP; a small one (the start straight's) at the least height and table.
std::optional<Plan> jump_in(ge::core::Rng& rng, double room, bool small) {
  const K::Jump J = K::jump;
  Plan p{};
  p.b.f.kind = CrossFeature::jump;
  p.b.ramp = ge::core::range(rng, J.ramp.min, J.ramp.max);
  p.b.deck = small ? J.deck.min : ge::core::range(rng, J.deck.min, J.deck.max);
  p.b.fall = ge::core::range(rng, J.fall.min, J.fall.max);
  p.b.f.height = small ? J.height.min : ge::core::range(rng, J.height.min, J.height.max);
  p.b.dig = J.dig;
  p.b.out = J.out;
  p.b.landing = p.b.deck + p.b.fall + J.out;
  p.length = p.b.ramp + p.b.landing;
  if (p.length > room) return std::nullopt;
  return p;
}

std::optional<Plan> step_in(ge::core::Rng& rng, double room) {
  const K::Step S = K::step;
  Plan p{};
  p.b.f.kind = CrossFeature::step;
  p.b.f.height = ge::core::range(rng, S.drop.min, S.drop.max);
  p.b.fall = ge::core::range(rng, S.fall.min, S.fall.max);
  p.b.ramp = S.approach;
  p.b.deck = S.deck;
  p.b.out = S.out;
  p.b.landing = S.deck + p.b.fall + S.out;
  p.length = p.b.ramp + p.b.landing;
  if (p.length > room) return std::nullopt;
  return p;
}

std::optional<Plan> deal(ge::core::Rng& rng, int kind, double room) {
  if (kind == 0) return rollers_in(rng, room);
  if (kind == 1) return jump_in(rng, room, false);
  return step_in(rng, room);
}

// THE FEATURES on the course's straights: a small jump on the start straight, as many features a
// straight between the turns as fit (dealt by the rule's weights, `between` apart) and a finish
// jump on the last, each straight's set centred on it. `grade_over(a, b)` is the profile's fall
// between two arcs.
template <class GradeOver>
std::vector<Built> lay_features(ge::core::Rng& rng, const std::vector<Turn>& turns, double ramp_end,
                                double finish, const GradeOver& grade_over) {
  struct Straight {
    double from, to;
    bool jump_only, small;
  };
  std::vector<Straight> straights;
  double s = ramp_end;
  for (size_t i = 0; i < turns.size(); i++) {
    straights.push_back({s, turns[i].from - K::clear, i == 0, i == 0});
    s = turns[i].to + K::exit;
  }
  straights.push_back({s, finish - K::finish.last, true, false});
  const K::Weights W = K::weights;
  std::vector<Built> out;
  for (const Straight& st : straights) {
    const double room = st.to - st.from;
    std::vector<Plan> plans;
    double used = 0;
    if (st.jump_only) {
      if (auto plan = jump_in(rng, room, st.small)) {
        used = plan->length;
        plans.push_back(*plan);
      }
    } else {
      while (true) {
        const double gap = plans.empty() ? 0 : K::between;
        const double left = room - used - gap;
        if (left < K::room) break;
        const double pick = ge::core::next(rng);
        const int first = pick < W.rollers ? 0 : pick < W.rollers + W.jump ? 1 : 2;
        // The pick first, then the others in a fixed order.
        const std::array<int, 3> order = first == 0   ? std::array<int, 3>{0, 1, 2}
                                         : first == 1 ? std::array<int, 3>{1, 0, 2}
                                                      : std::array<int, 3>{2, 1, 0};
        std::optional<Plan> plan;
        for (const int kind : order) {
          plan = deal(rng, kind, left);
          if (plan) break;
        }
        if (!plan) break;
        used += plan->length + gap;
        plans.push_back(*plan);
      }
    }
    double at = st.from + (room - used) / 2;
    for (Plan& plan : plans) {
      Built b = plan.b;
      b.f.from = at;
      b.f.to = at + plan.length;
      b.f.lip = b.f.kind == CrossFeature::rollers ? at : at + b.ramp;
      b.grade = grade_over(at, at + plan.length);
      out.push_back(b);
      at += plan.length + K::between;
    }
  }
  return out;
}

// The bank of the berm at arc `s` (the tangent of its angle) and the side of its inside; none off
// every berm.
struct Bank {
  double tan = 0;
  int side = 0;
};
Bank bank_at(const std::vector<Turn>& berms, double s) {
  for (const Turn& t : berms) {
    if (s < t.from - K::turn.ease || s > t.to + K::turn.ease) continue;
    const double w = s < t.from ? smoothstep(t.from - K::turn.ease, t.from, s)
                     : s > t.to ? 1 - smoothstep(t.to, t.to + K::turn.ease, s)
                                : 1;
    // A long sweeping corner is banked less than a hairpin.
    const double deg = K::turn.bank * clamp(t.angle / K::turn.full, 0.5, 1);
    return {libm::tan(deg * kPi / 180) * w, t.sign};
  }
  return {};
}

Checkpoint across(const Track& track, double s, double width) {
  const TrackPoint p = track_point_at(track, s);
  return {p.x, p.z, p.y, p.heading, width, s, GateColour::red};
}

Level course_over(const std::shared_ptr<const Level>& original) {
  const Level& map = *original;
  ge::core::Rng rng = ge::core::make_rng(num::to_uint32(map.seed) ^ kSkiCrossSalt);
  const RaceCourse::Stretch axis = pick_stretch(map);
  const Laid laid = lay(rng, corners(map, rng, axis.from, axis.to));
  std::vector<double> y = base_profile(map, laid);
  cut_start(y);
  const size_t n = y.size();
  const double finish_s = double(n - 1) * kStep - K::finish.area;
  const double ramp_end = K::start.platform + K::start.ramp.length;
  const auto y_at = [&](double s) {
    return y[static_cast<size_t>(clamp(num::round(s / kStep), 0, double(n - 1)))];
  };
  const std::vector<Built> built =
      lay_features(rng, laid.turns, ramp_end, finish_s, [&](double a, double b) {
        return num::max(0.0, (y_at(a) - y_at(b)) / num::max(1.0, b - a));
      });
  for (const Built& b : built) {
    for (size_t i = 0; i < n; i++) {
      const double s = double(i) * kStep;
      if (s > b.f.from - 1 && s < b.f.to + 1) y[i] += b.offset(s);
    }
  }
  std::vector<Turn> berms;
  for (const Turn& t : laid.turns) {
    if (t.angle >= K::turn.least) berms.push_back(t);
  }
  // THE STATIONS, every `station` m down the line.
  Level level = map;
  level.race.reset();
  std::vector<TrackPoint>& points = level.track.points;
  points.clear();
  for (size_t i = 0; i < n; i++) {
    const double s = double(i) * kStep;
    const Pose p = pose_at(laid.pieces, s);
    const double width =
        s < ramp_end + 10
            ? K::start.width + (K::width - K::start.width) * smoothstep(ramp_end, ramp_end + 10, s)
            : K::width;
    points.push_back({p.x, p.z, y[i], s, p.heading, width});
  }
  level.track.length = points.back().s;
  level.track.index.reset();
  const Track& track = level.track;
  const auto height_at = [&](double s) {
    const double f = clamp(s / kStep, 0, double(n - 1));
    const auto i = static_cast<size_t>(num::min(double(n - 2), std::floor(f)));
    return y[i] + (y[i + 1] - y[i]) * (f - double(i));
  };
  // THE GROUND graded to the course across its width, every berm's outside banked up into its
  // wall, eased out beyond; the snow groomed.
  const double half = K::width / 2;
  const double reach = half + K::turn.wall + K::ease + 2;
  ge::core::Heightfield& ground = level.ground;
  ge::core::Heightfield& packed = level.packed;
  const double fade = rules::track::shoulder_packed;
  std::vector<uint8_t> seen(ground.data.size(), 0);
  constexpr double kBox = 16;
  const auto col_of = [&](double v, bool up) {
    const double f = (v - ground.origin_x) / ground.cell;
    return static_cast<int>(clamp(up ? std::ceil(f) : std::floor(f), 0, ground.cols - 1));
  };
  const auto row_of = [&](double v, bool up) {
    const double f = (v - ground.origin_z) / ground.cell;
    return static_cast<int>(clamp(up ? std::ceil(f) : std::floor(f), 0, ground.rows - 1));
  };
  TrackHit hit;
  for (int b = 0; b * kBox < track.length; b++) {
    const double s0 = b * kBox;
    double x0 = std::numeric_limits<double>::infinity();
    double x1 = -x0;
    double z0 = x0;
    double z1 = -x0;
    for (int k = 0; s0 + k * kStep <= num::min(track.length, s0 + kBox); k++) {
      const Pose p = pose_at(laid.pieces, s0 + k * kStep);
      x0 = num::min(x0, p.x - reach);
      x1 = num::max(x1, p.x + reach);
      z0 = num::min(z0, p.z - reach);
      z1 = num::max(z1, p.z + reach);
    }
    const int c0 = col_of(x0, false), c1 = col_of(x1, true);
    const int r0 = row_of(z0, false), r1 = row_of(z1, true);
    for (int r = r0; r <= r1; r++) {
      for (int c = c0; c <= c1; c++) {
        const size_t i =
            static_cast<size_t>(r) * static_cast<size_t>(ground.cols) + static_cast<size_t>(c);
        if (seen[i] != 0) continue;
        seen[i] = 1;
        const double x = ground.origin_x + c * ground.cell;
        const double z = ground.origin_z + r * ground.cell;
        nearest_within(track, x, z, reach, hit);
        if (hit.distance > reach) continue;
        const Bank bank = bank_at(berms, hit.s);
        // Outward of the berm, positive: away from its inside.
        const double out = -hit.lateral * bank.side;
        const bool walled = bank.tan > 0 && out > 0;
        const double edge = walled ? half + K::turn.wall : half;
        const double w = 1 - smoothstep(edge, edge + K::ease, hit.distance);
        if (w > 0) {
          const double lift = bank.tan * clamp(out, -0.4 * half, half + K::turn.wall);
          float& h = ground.data[i];
          h = static_cast<float>(h + w * (height_at(hit.s) + lift - h));
        }
        const double firm = 1 - smoothstep(half, half + fade, hit.distance);
        if (i < packed.data.size() && firm > packed.data[i]) {
          packed.data[i] = static_cast<float>(firm);
        }
      }
    }
  }
  // THE FENCE, the spill zones and the finish area cleared of trees; the piste's kickers and drops
  // under the course taken out with the ground they stood on, and the rest on no course a racer
  // skis.
  const TrackPoint finish_at = track_point_at(track, finish_s);
  const double fx = libm::sin(points.back().heading);
  const double fz = libm::cos(points.back().heading);
  const auto in_course = [&](double x, double z, double r) {
    TrackHit near;
    nearest_within(track, x, z, half + r, near);
    if (near.distance <= half + r) return true;
    const double dx = x - finish_at.x;
    const double dz = z - finish_at.z;
    const double along = dx * fx + dz * fz;
    const double side = dx * fz - dz * fx;
    return along > -K::finish.before && along < K::finish.area + 10 &&
           std::abs(side) < K::finish.half;
  };
  std::vector<TreeDef> trees;
  trees.reserve(map.trees.size());
  for (const TreeDef& t : map.trees) {
    if (!in_course(t.x, t.z, K::clear_trees)) trees.push_back(t);
  }
  level.trees = std::move(trees);
  std::vector<Kicker> kickers;
  for (const Kicker& k : map.kickers) {
    if (in_course(k.x, k.z, K::ease + k.ramp)) continue;
    kickers.push_back(k);
    kickers.back().on_track = false;
    kickers.back().s = std::nullopt;
  }
  std::vector<Cliff> cliffs;
  for (const Cliff& k : map.cliffs) {
    if (in_course(k.x, k.z, K::ease + k.width / 2)) continue;
    cliffs.push_back(k);
    cliffs.back().on_track = false;
    cliffs.back().s = std::nullopt;
  }
  level.cliffs = std::move(cliffs);
  level.drifts.clear();
  // THE COURSE'S OWN JUMPS, on its line, `J1…` in the order skied.
  RaceCourse race;
  int jumps = 0;
  for (const Built& b : built) {
    if (b.f.kind == CrossFeature::rollers) continue;
    const TrackPoint p = track_point_at(track, b.f.lip);
    kickers.push_back({"J" + std::to_string(++jumps), p.x, p.z, height_at(b.f.lip), p.heading,
                       b.f.height, b.ramp, b.landing, K::width, true, b.f.lip});
    race.jumps.push_back(b.f.lip);
  }
  level.kickers = std::move(kickers);
  // THE GATES: a turning gate on the inside of every berm, a corridor gate before every feature,
  // red and blue in turn down the course.
  std::vector<Checkpoint> gates;
  for (const Turn& t : berms) {
    Checkpoint g = across(track, (t.from + t.to) / 2, K::width);
    g.pole = PoleGate::open;
    g.turn = t.sign;
    g.flags = true;
    gates.push_back(g);
  }
  for (const Built& b : built) {
    Checkpoint g = across(track, b.f.from - K::gate_before, K::width);
    g.flags = true;
    gates.push_back(g);
  }
  std::stable_sort(gates.begin(), gates.end(),
                   [](const Checkpoint& a, const Checkpoint& b) { return a.s < b.s; });
  const double doors = K::start.platform;
  level.checkpoints.clear();
  level.checkpoints.push_back(across(track, doors + 0.3, K::start.width));
  for (size_t i = 0; i < gates.size(); i++) {
    gates[i].colour = i % 2 == 0 ? GateColour::red : GateColour::blue;
    level.checkpoints.push_back(gates[i]);
  }
  level.checkpoints.push_back(across(track, finish_s, num::max(K::finish.width, K::width + 1)));
  // THE START GATE'S LANES, the inside of the first berm first.
  const TrackPoint stand = track_point_at(track, doors - K::start.stand);
  const int inside = berms.empty() ? 1 : berms.front().sign;
  level.grid.clear();
  for (int k = 0; k < K::start.lanes; k++) {
    const double off = (k - (K::start.lanes - 1) / 2.0) * K::start.lane;
    level.grid.push_back({stand.x + right_x(stand.heading) * off,
                          stand.z + right_z(stand.heading) * off, stand.heading});
  }
  const auto lane_off = [&](const Spawn& l) {
    return (l.x - stand.x) * right_x(stand.heading) + (l.z - stand.z) * right_z(stand.heading);
  };
  std::stable_sort(level.grid.begin(), level.grid.end(), [&](const Spawn& a, const Spawn& b) {
    return lane_off(a) * inside > lane_off(b) * inside;
  });
  level.spawn = {stand.x, stand.z, stand.heading};
  for (const Turn& t : berms) {
    race.features.push_back(
        {CrossFeature::berm, t.from, t.to, (t.from + t.to) / 2, 0, t.sign, t.angle});
  }
  for (const Built& b : built) race.features.push_back(b.f);
  std::stable_sort(race.features.begin(), race.features.end(),
                   [](const Feature& a, const Feature& b) { return a.from < b.from; });
  race.discipline = Discipline::ski_cross;
  race.run = 1;
  race.base = original;
  race.from = doors;
  race.to = finish_s;
  race.vertical = height_at(doors) - height_at(finish_s);
  race.nets = RaceCourse::Nets{K::fence.gap, K::fence.height, doors + 2, finish_s};
  race.width = K::width;
  race.stretch = axis;
  race.turns = static_cast<int>(berms.size());
  level.race = std::move(race);
  return level;
}

}  // namespace

std::optional<std::string> ski_cross_course_of(const Level& level) {
  if (!level.resort) return std::nullopt;
  const K::CourseAim C = K::course;
  const Course* best = nullptr;
  double best_score = 0;
  for (const Course& c : level.resort->courses) {
    const double score = std::abs(c.drop / num::max(1.0, c.length) - C.grade) +
                         (c.length < C.length ? 1 : 0) + (c.drop < K::vertical.min * 1.5 ? 1 : 0);
    if (!best || score < best_score || (score == best_score && c.id < best->id)) {
      best = &c;
      best_score = score;
    }
  }
  if (!best) return std::nullopt;
  return best->id;
}

Level set_ski_cross(const Level& level) {
  if (level.race && level.race->discipline == Discipline::ski_cross) return level;
  const std::shared_ptr<const Level> original =
      level.race ? level.race->base : std::make_shared<const Level>(level);
  Level out = course_over(original);
  out.sun = level.sun;
  out.weather = level.weather;
  return out;
}

}  // namespace ge::mountain::mapgen
