// The speed events behind speed_events.hpp: the downhill (its line, then gates marking it), and
// the turning course a super-G and a giant slalom share (the lowered start, the jumps, the gates
// dealt down the piste, the line swung round them, the gates stood outward of its apexes).
//
// ge:summary The downhill and the turning course (super-G, giant slalom) set over a map
// ge:keywords downhill super-g giant-slalom turn-course lowered-start crests-of swing gate-arcs
// ge:skill mountain
// ge:test tests/mountain_speed_events_test.cpp
#include "mountain/mapgen/speed_events.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <utility>
#include <vector>

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "core/num.hpp"
#include "core/prng.hpp"
#include "mountain/mapgen/course_prep.hpp"
#include "mountain/mapgen/discipline_rules.hpp"
#include "mountain/mapgen/query.hpp"
#include "mountain/mapgen/speed_course.hpp"

namespace ge::mountain::mapgen {

namespace num = ge::core::num;
namespace libm = ge::core::libm;
namespace D = discipline_rules::downhill;
using ge::core::clamp;
using ge::core::kPi;

namespace {

// What a super-G's gates are dealt off beside the map's seed, and a giant slalom's (with its run).
constexpr uint32_t kSuperGSalt = 0x5e9a;
constexpr uint32_t kGiantSlalomSalt = 0x6151;

// The map a course is set over: the one under any course `level` carries, else `level` itself.
std::shared_ptr<const Level> original_of(const Level& level) {
  return level.race ? level.race->base : std::make_shared<const Level>(level);
}

// The course keeps the day and the sky of the map it was handed.
Level under_the_same_sky(Level course, const Level& level) {
  course.sun = level.sun;
  course.weather = level.weather;
  return course;
}

Checkpoint line_across(const TrackPoint& p, double width) {
  return {p.x, p.z, p.y, p.heading, width, p.s, GateColour::red};
}

// The drops on the piste between `from` and `to`, by arc, in order.
std::vector<double> drops_between(const Level& level, double from, double to) {
  std::vector<double> out;
  for (const Cliff& c : level.cliffs) {
    if (c.on_track && c.s && *c.s > from && *c.s < to) out.push_back(*c.s);
  }
  std::sort(out.begin(), out.end());
  return out;
}

// What every course stands up the same way: the start wand, the finish line, the start house
// `stand` m above the wand, the woods cleared, the course named.
void stand_up(Level& base, std::vector<Checkpoint> gates, double finish_width, const CoursePrep& P,
              const Stretch& stretch, RaceCourse course) {
  const TrackPoint start = track_point_at(base.track, stretch.from);
  const TrackPoint finish = track_point_at(base.track, stretch.to);
  gates.insert(gates.begin(), line_across(start, 2));
  gates.push_back(line_across(finish, num::max(finish_width, finish.width + 6)));
  const TrackPoint house = track_point_at(base.track, stretch.from - P.stand);
  base.spawn = {house.x, house.z, start.heading};
  base.grid = {base.spawn};
  base.checkpoints = std::move(gates);
  base.trees = cleared_trees(base, stretch, P);
  base.race = std::move(course);
}

RaceCourse course_named(Discipline d, int run, const std::shared_ptr<const Level>& original,
                        const Track& track, double from, double to) {
  RaceCourse c;
  c.discipline = d;
  c.run = run;
  c.base = original;
  c.from = from;
  c.to = to;
  c.vertical = track_point_at(track, from).y - track_point_at(track, to).y;
  return c;
}

RaceCourse::Trap trap_of(const Track& track, double s, double gap) {
  const TrackPoint p = track_point_at(track, s);
  return {s, p.x, p.z, p.heading, p.width + 2 * gap};
}

std::vector<RaceCourse::LinePoint> line_points(const std::vector<double>& line, double from,
                                               double step) {
  std::vector<RaceCourse::LinePoint> out;
  out.reserve(line.size());
  for (size_t i = 0; i < line.size(); i++) out.push_back({from + double(i) * step, line[i]});
  return out;
}

// R32 — the downhill set over `original`, a map with no course on it.
Level downhill_over(const std::shared_ptr<const Level>& original) {
  const double from = original->checkpoints.empty() ? 0 : original->checkpoints.front().s;
  const double to = original->track.length;
  const Stretch stretch{from, to};
  Level base = prepare_course(*original, stretch, D::prep);
  RaceCourse course = course_named(Discipline::downhill, 1, original, base.track, from, to);
  // The jumps the course keeps: its drops (R24), by arc.
  course.jumps = drops_between(base, from, to);
  const std::vector<double> line = racing_line(base.track, from, to, D::line);
  std::vector<Checkpoint> gates;
  for (const double at : speed_gate_arcs(from, to, course.jumps, D::spacing, D::jump)) {
    const TrackPoint p = track_point_at(base.track, at);
    const double offset = line_offset(line, from, at, D::line.step);
    const double x = p.x + libm::cos(p.heading) * offset;
    const double z = p.z - libm::sin(p.heading) * offset;
    Checkpoint g{x, z, base.ground_at(x, z), p.heading, D::width, at, GateColour::red};
    g.offset = offset;
    g.span = p.width;
    g.panels = true;
    gates.push_back(g);
  }
  course.trap = trap_of(base.track, trap_arc(base.track, from, to, D::trap), D::nets.gap);
  course.nets = RaceCourse::Nets{D::nets.gap, D::nets.height, from - D::prep.stand - 6, to - 10};
  course.line = line_points(line, from, D::line.step);
  stand_up(base, std::move(gates), D::finish_width, D::prep, stretch, std::move(course));
  return base;
}

// The arcs of a turning course's gates from the start gate at `from` to the finish at `to`: as
// many as the vertical's direction changes ask and the spacing aimed at gives, whichever is more,
// each strayed off the even spacing by the course's rhythm and stepped off the jumps (clear of
// every one of them) inside the spacing's band.
std::vector<double> turn_gate_arcs(double from, double to, double vertical,
                                   const std::vector<double>& jumps, double seed, const TurnRule& G,
                                   uint32_t salt) {
  const SpacingRule& S = G.spacing;
  ge::core::Rng rng = ge::core::make_rng(num::to_uint32(seed) ^ salt);
  const double length = to - from;
  // The last gate stands a spacing above the finish.
  const double room = length - S.target * 0.6;
  const double want = num::max(std::ceil(G.changes * vertical), num::round(room / S.target));
  const int n = static_cast<int>(num::max(1.0, num::min(want, std::floor(room / S.min))));
  const double even = room / n;
  const auto clear_of = [&](double a) {
    for (const double j : jumps) {
      if (a > j - G.jump && a < j + G.landing) return false;
    }
    return true;
  };
  std::vector<double> arcs;
  double prev = from;
  for (int i = 0; i < n; i++) {
    double at = from + even * (i + 1) + even * G.rhythm * (ge::core::next(rng) * 2 - 1);
    at = clamp(at, prev + S.min, num::min(prev + S.max, to - S.min));
    // Stepped off a jump to whichever side clear of every jump fits the band, the nearest first;
    // else past the jumps, the spacing's most stretched over them.
    if (!clear_of(at)) {
      double pick = std::numeric_limits<double>::infinity();
      double off = pick;
      for (const double j : jumps) {
        for (const double a : {j - G.jump, j + G.landing}) {
          if (!clear_of(a) || a < prev + S.min || a > prev + S.max || to - a < S.min) continue;
          if (std::abs(a - at) < off) {
            off = std::abs(a - at);
            pick = a;
          }
        }
      }
      if (!std::isfinite(pick)) {
        for (const double j : jumps) {
          if (j + G.landing > at && j + G.landing < pick && clear_of(j + G.landing)) {
            pick = j + G.landing;
          }
        }
      }
      if (!std::isfinite(pick)) continue;
      at = pick;
    }
    if (at <= prev || to - at < S.min) continue;
    arcs.push_back(at);
    prev = at;
  }
  return arcs;
}

// THE CRESTS A RACER FLIES OFF between `from` and `to`, by arc: where the piste's profile, shaved
// round, still bends over tighter than `G.takeoff` m of radius (read over `G.crest_span` m either
// side, past the start drop), the tightest of each run of them, one a jump's landing apart.
std::vector<double> crests_of(const Level& level, double from, double to, const TurnRule& G) {
  const std::vector<TrackPoint>& pts = level.track.points;
  std::vector<double> out;
  if (pts.size() < 3) return out;
  double best = 0;
  double at = -1;
  const auto w = static_cast<size_t>(
      num::max(1.0, num::round(G.crest_span / num::max(1e-6, pts[1].s - pts[0].s))));
  const CoursePrep::Drop& drop = G.prep.drop;
  const double head = from + drop.lip + drop.length + drop.ease;
  for (size_t k = w; k + w < pts.size(); k++) {
    const TrackPoint& p = pts[k];
    if (p.s <= head || p.s >= to) continue;
    const double h = (pts[k + w].s - pts[k - w].s) / 2;
    const double convex = -(pts[k - w].y - 2 * p.y + pts[k + w].y) / (h * h);
    if (convex > 1 / G.takeoff) {
      if (convex > best) {
        best = convex;
        at = p.s;
      }
    } else if (at >= 0) {
      if (out.empty() || at - out.back() > G.landing) out.push_back(at);
      best = 0;
      at = -1;
    }
  }
  return out;
}

double lowered_start(const Level& level, const TurnRule& G) {
  const double gate = level.checkpoints.empty() ? 0 : level.checkpoints.front().s;
  const double L = level.track.length;
  const double foot = track_point_at(level.track, L).y;
  double from = gate;
  for (int k = 0; gate + 2.0 * k < L - 400; k++) {
    const double s = gate + 2.0 * k;
    if (track_point_at(level.track, s).y - foot <= G.target) {
      from = s;
      break;
    }
  }
  // Off any drop on the piste, by a jump's clearance, downhill.
  for (const Cliff& c : level.cliffs) {
    if (!c.on_track || !c.s) continue;
    if (from > *c.s - G.jump && from < *c.s + G.jump) from = *c.s + G.jump;
  }
  return from;
}

// R33, R36 — A TURNING COURSE set over `original`, a map with no course on it: from `from` down its
// piste to the finish, by the rule `G`, its gates and swing dealt off `salt`, and a speed trap
// where `trap` names one.
Level turn_course_over(const std::shared_ptr<const Level>& original, Discipline discipline, int run,
                       double from, const TurnRule& G, uint32_t salt, const TrapRule* trap) {
  const double to = original->track.length;
  const Stretch stretch{from, to};
  Level base = prepare_course(*original, stretch, G.prep);
  RaceCourse course = course_named(discipline, run, original, base.track, from, to);
  // The jumps the course keeps: its drops (R24) and the crests a racer flies off, by arc; a drop's
  // edge is a crest too, one jump.
  std::vector<double> jumps = drops_between(base, from, to);
  const std::vector<double> crests = crests_of(base, from, to, G);
  jumps.insert(jumps.end(), crests.begin(), crests.end());
  std::sort(jumps.begin(), jumps.end());
  for (const double j : jumps) {
    if (course.jumps.empty() || j - course.jumps.back() > G.jump) course.jumps.push_back(j);
  }
  const double step = G.line.step;
  const std::vector<double> centre = racing_line(base.track, from, to, G.line);
  const std::vector<double> arcs =
      turn_gate_arcs(from, to, course.vertical, course.jumps, original->seed, G, salt);
  // THE SWING at each gate: its side in turn, its size dealt, held to the snow the gate needs
  // beside the line and to the tightest bend the spacing either side allows; straighter before a
  // jump, at the first gates and on a gentle stretch.
  ge::core::Rng rng = ge::core::make_rng(num::to_uint32(original->seed) ^ salt ^ 0x7au);
  std::vector<double> swing;
  std::vector<int> side;
  const int first = ge::core::next(rng) < 0.5 ? 1 : -1;
  for (size_t i = 0; i < arcs.size(); i++) {
    const double at = arcs[i];
    const int sigma = i % 2 == 0 ? first : -first;
    const double before = i == 0 ? at - from : at - arcs[i - 1];
    const double after = (i + 1 == arcs.size() ? to : arcs[i + 1]) - at;
    const double span = num::min(before, after);
    // The swing a half cosine over `span` bends at most (a·π²/span²).
    const double most = (span * span) / (kPi * kPi * G.bend);
    double a = G.swing.min + (G.swing.max - G.swing.min) * ge::core::next(rng);
    a = num::min(a, most);
    for (const double j : course.jumps) {
      if (j > at && j - at < G.approach) {
        a *= G.straight;
        break;
      }
    }
    if (i < G.opening.size()) a *= G.opening[i];
    const double fall =
        (track_point_at(base.track, at - G.flat.over).y - track_point_at(base.track, at).y) /
        G.flat.over;
    a *= clamp((fall - G.flat.from) / (G.flat.to - G.flat.from), G.flat.least, 1);
    const TrackPoint p = track_point_at(base.track, at);
    const double o = line_offset(centre, from, at, step);
    // The line's apex no further toward the gate's side than leaves its outside pole on the snow,
    // nor further the other way than its turning pole.
    const double reach = num::max(0.0, p.width / 2 - G.inside - G.width + G.pass);
    const double apex = clamp(o + sigma * a, -reach, reach);
    swing.push_back(apex - o);
    side.push_back(sigma);
  }
  // The line: the centre swung, the swing eased from apex to apex.
  std::vector<double> knots{from};
  knots.insert(knots.end(), arcs.begin(), arcs.end());
  knots.push_back(to);
  std::vector<double> values{0};
  values.insert(values.end(), swing.begin(), swing.end());
  values.push_back(0);
  std::vector<double> line(centre.size());
  size_t k = 0;
  for (size_t i = 0; i < centre.size(); i++) {
    const double s = from + double(i) * step;
    while (k + 2 < knots.size() && s > knots[k + 1]) k++;
    const double u = clamp((s - knots[k]) / num::max(1e-6, knots[k + 1] - knots[k]), 0, 1);
    const double w = values[k] + ((values[k + 1] - values[k]) * (1 - libm::cos(kPi * u))) / 2;
    const double half = track_point_at(base.track, s).width / 2 - G.inside;
    line[i] = clamp(centre[i] + w, -half, half);
  }
  std::vector<Checkpoint> gates;
  for (size_t i = 0; i < arcs.size(); i++) {
    const double at = arcs[i];
    const TrackPoint p = track_point_at(base.track, at);
    // The gate stands outward of the line's apex: its turning pole `pass` inside it, its outside
    // pole `width` beyond that.
    const double offset = line_offset(line, from, at, step) + side[i] * (G.width / 2 - G.pass);
    const double x = p.x + libm::cos(p.heading) * offset;
    const double z = p.z - libm::sin(p.heading) * offset;
    Checkpoint g{x,
                 z,
                 base.ground_at(x, z),
                 p.heading,
                 G.width,
                 at,
                 i % 2 == 0 ? GateColour::red : GateColour::blue};
    g.offset = offset;
    g.span = p.width;
    g.panels = true;
    g.turn = side[i] == 1 ? -1 : 1;
    gates.push_back(g);
  }
  course.nets = RaceCourse::Nets{G.nets.gap, G.nets.height, from - G.prep.stand - 6, to - 10};
  if (trap != nullptr)
    course.trap = trap_of(base.track, trap_arc(base.track, from, to, *trap), G.nets.gap);
  course.line = line_points(line, from, step);
  course.turns = static_cast<int>(arcs.size());
  stand_up(base, std::move(gates), G.finish_width, G.prep, stretch, std::move(course));
  return base;
}

}  // namespace

std::optional<std::string> downhill_course_of(const Level& level) {
  if (!level.resort) return std::nullopt;
  const Course* best = nullptr;
  bool best_fits = false;
  for (const Course& c : level.resort->courses) {
    const bool fits = c.drop >= D::vertical.min && c.drop <= D::vertical.max;
    if (!best || (fits && !best_fits) ||
        (fits == best_fits && (c.drop > best->drop || (c.drop == best->drop && c.id < best->id)))) {
      best = &c;
      best_fits = fits;
    }
  }
  if (!best) return std::nullopt;
  return best->id;
}

std::optional<std::string> most_vertical_course_of(const Level& level) {
  if (!level.resort) return std::nullopt;
  const Course* best = nullptr;
  for (const Course& c : level.resort->courses) {
    if (!best || c.drop > best->drop || (c.drop == best->drop && c.id < best->id)) best = &c;
  }
  if (!best) return std::nullopt;
  return best->id;
}

Level set_downhill(const Level& level) {
  if (level.race && level.race->discipline == Discipline::downhill) return level;
  return under_the_same_sky(downhill_over(original_of(level)), level);
}

double super_g_start(const Level& level) {
  return lowered_start(level, discipline_rules::super_g::rule);
}

Level set_super_g(const Level& level) {
  if (level.race && level.race->discipline == Discipline::super_g) return level;
  namespace G = discipline_rules::super_g;
  const std::shared_ptr<const Level> original = original_of(level);
  // A super-G times its racers through a speed trap; a giant slalom does not.
  return under_the_same_sky(
      turn_course_over(original, Discipline::super_g, 1, super_g_start(*original), G::rule,
                       kSuperGSalt, &G::trap),
      level);
}

double giant_slalom_start(const Level& level) {
  return lowered_start(level, discipline_rules::giant_slalom::rule);
}

Level set_giant_slalom(const Level& level, int run) {
  if (level.race && level.race->discipline == Discipline::giant_slalom && level.race->run == run) {
    return level;
  }
  const std::shared_ptr<const Level> original = original_of(level);
  const uint32_t salt =
      kGiantSlalomSalt ^ static_cast<uint32_t>(num::imul(static_cast<uint32_t>(run), 0x9e3779b1u));
  Level course =
      turn_course_over(original, Discipline::giant_slalom, run, giant_slalom_start(*original),
                       discipline_rules::giant_slalom::rule, salt, nullptr);
  return under_the_same_sky(std::move(course), level);
}

}  // namespace ge::mountain::mapgen
