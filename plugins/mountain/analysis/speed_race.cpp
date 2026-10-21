// The speed events' checks behind race_checks.hpp: R32, the downhill (its speed gates centred on
// a racing line that bends less than the piste, spaced as a downhill's, the nets, the speed trap
// late on a straight), and R33 and R36, the super-G and the giant slalom (the start lowered into
// the band, the gates side to side in turn as many as the direction changes ask, each turning
// pole inside the line's apex, every pole on the snow, none on a jump's lip or landing); for all
// three, the crests shaved round and the hill prepared.
//
// ge:summary R32 R33 R36 checks: the speed gates, the line, the crests, the nets and the trap
// ge:keywords analysis downhill super-g giant-slalom speed-gate turning-pole speed-trap a-nets
// ge:keywords crests r32 r33 r36
// ge:skill mountain
// ge:test tests/mountain_speed_events_test.cpp
#include <cmath>
#include <string>
#include <vector>

#include "core/math.hpp"
#include "core/num.hpp"
#include "mountain/analysis/race_checks.hpp"
#include "mountain/mapgen/discipline_rules.hpp"
#include "mountain/mapgen/query.hpp"
#include "mountain/mapgen/speed_course.hpp"

namespace ge::mountain::analysis {

namespace num = ge::core::num;
namespace D = mapgen::discipline_rules::downhill;
using mapgen::Checkpoint;
using mapgen::Discipline;
using mapgen::TrackHit;
using mapgen::TrackPoint;

namespace {

// How far a gate may stand off its band's edge, m: a gate's arc is the piste's, read off stations
// 2 m apart.
constexpr double kSlack = 1e-6;
// How far the line may stray past its room between its stations, m: it is held at stations 4 m
// apart, and the piste's edge between two of them is its own curve, not the straight between.
constexpr double kBetween = 0.15;

// Whether `s` is on a drop's footprint (its shelf, its face, its landing): a drop is kept, never
// shaved, and is the one place the profile may break.
bool on_a_drop(const mapgen::Level& level, double s) {
  for (const mapgen::Cliff& c : level.cliffs) {
    if (c.on_track && c.s && s > *c.s - c.shelf - 4 && s < *c.s + c.face + c.landing + 4) {
      return true;
    }
  }
  return false;
}

// The course's stretch, the start wand and the finish line on it, the house above the wand.
void check_stretch(const mapgen::Level& level, const mapgen::RaceCourse& race, double finish_width,
                   const char* rule, const Add& add) {
  const auto& cps = level.checkpoints;
  if (std::abs(race.to - level.track.length) > 1e-6) {
    add(rule, Severity::error, "the course does not run to the piste's finish");
  }
  if (std::abs(cps.front().s - race.from) > 1e-6 || std::abs(cps.back().s - race.to) > 1e-6) {
    add(rule, Severity::error, "the start wand or the finish line is off the course's stretch");
  }
  if (cps.back().width < finish_width - 1e-9) {
    add(rule, Severity::error,
        "the finish line is " + fmt(cps.back().width) + " m wide, under " + str(finish_width) +
            " m");
  }
  if (level.grid.size() != 1 ||
      mapgen::nearest_track_point(level.track, level.spawn.x, level.spawn.z).s >= race.from) {
    add(rule, Severity::error, "the start house is not one racer stood above the wand");
  }
}

// No crest down the course sharper than `radius` m (off the drops, which are kept).
void check_crests(const mapgen::Level& level, const mapgen::RaceCourse& race, double radius,
                  const char* rule, const Add& add) {
  const auto& pts = level.track.points;
  for (size_t i = 1; i + 1 < pts.size(); i++) {
    const TrackPoint& p = pts[i];
    if (p.s <= race.from + 4 || p.s >= race.to - 4 || on_a_drop(level, p.s)) continue;
    const double h = (pts[i + 1].s - pts[i - 1].s) / 2;
    const double bend = (pts[i - 1].y - 2 * p.y + pts[i + 1].y) / (h * h);
    if (bend < -1 / radius - 1e-3) {
      add(rule, Severity::error,
          "a crest at s " + fmt(p.s) + " m bends at " + fmt(-1 / bend) + " m of radius, under " +
              str(radius) + " m");
      return;
    }
  }
}

// The nets along both edges, from above the house to the finish.
void check_nets(const mapgen::RaceCourse& race, const mapgen::NetRule& N, const char* rule,
                const Add& add) {
  if (!race.nets || race.nets->gap != N.gap || race.nets->height != N.height ||
      race.nets->from >= race.from || race.nets->to <= race.from) {
    add(rule, Severity::error, "the course is not netted along both edges from above its house");
  }
}

// The speed trap late on the course, short of the finish.
void check_trap(const mapgen::RaceCourse& race, const mapgen::TrapRule& T, const char* rule,
                const Add& add) {
  if (!race.trap) {
    add(rule, Severity::error, "the course has no speed trap");
    return;
  }
  const double s = race.trap->s;
  if (s < race.from + (race.to - race.from) * T.late - kSlack || s > race.to - T.end + kSlack) {
    add(rule, Severity::error,
        "the speed trap stands at s " + fmt(s) + " m, outside the course's last " +
            fmt(T.late * 100, 0) + " % short of " + str(T.end) + " m from the line");
  }
}

}  // namespace

void check_downhill(const mapgen::Level& level, const mapgen::RaceCourse& race, const Add& add) {
  const mapgen::Track& track = level.track;
  const auto& cps = level.checkpoints;
  if (cps.size() < 3) {
    add("R32", Severity::error, "the downhill sets no speed gate");
    return;
  }
  check_stretch(level, race, D::finish_width, "R32", add);
  // The speed gates: red, panelled, on the line, inside the piste.
  for (size_t i = 1; i + 1 < cps.size(); i++) {
    const Checkpoint& g = cps[i];
    const std::string at = gate_name(i);
    if (!g.panels || g.pole || g.colour != mapgen::GateColour::red || g.width != D::width) {
      add("R32", Severity::error, at + " is not a red speed gate " + str(D::width) + " m wide");
    }
    const auto line = mapgen::speed_line_at(level, g.s);
    if (!line || !g.offset || std::abs(*g.offset - line->offset) > 1e-6) {
      add("R32", Severity::error, at + " is not centred on the racing line");
    }
    const TrackPoint p = mapgen::track_point_at(track, g.s);
    const double across = mapgen::nearest_track_point(track, g.x, g.z).lateral;
    const double room = num::max(p.width / 2 - D::inside, g.width / 2);
    if (std::abs(across) + g.width / 2 > room + kBetween) {
      add("R32", Severity::error,
          at + "'s pole stands " + fmt(std::abs(across) + g.width / 2 - room, 2) +
              " m past its room inside the piste");
    }
    for (const double j : race.jumps) {
      if (std::abs(g.s - j) < D::jump - kSlack) {
        add("R32", Severity::error,
            at + " stands " + fmt(std::abs(g.s - j)) + " m from a jump, inside " + str(D::jump) +
                " m");
      }
    }
    if (i > 1) {
      const double d = g.s - cps[i - 1].s;
      if (d < D::spacing.min - kSlack || d > D::spacing.max + kSlack) {
        add("R32", Severity::error,
            at + " stands " + fmt(d) + " m below the one before, outside " + str(D::spacing.min) +
                "–" + str(D::spacing.max) + " m");
      }
    }
  }
  // The line: inside its margin, bending less than the piste. (Where the room closes to nothing
  // it closes at the line's ease, so between two stations it may read half a station's ease
  // more.)
  const double eased = D::line.ease * D::line.step / 2 + kBetween;
  double piste_bend = 0;
  double line_bend = 0;
  for (int k = 0; race.from + 10 + 4.0 * k < race.to - 10; k++) {
    const double s = race.from + 10 + 4.0 * k;
    const TrackPoint a = mapgen::track_point_at(track, s - 8);
    const TrackPoint b = mapgen::track_point_at(track, s + 8);
    piste_bend += std::abs(b.heading - a.heading) / 16;
    const auto line = mapgen::speed_line_at(level, s);
    if (!line) continue;
    line_bend += std::abs(line->bend);
    const double half = mapgen::track_point_at(track, s).width / 2;
    if (std::abs(line->offset) > num::max(0.0, half - D::line.margin) + eased) {
      add("R32", Severity::error,
          "the racing line runs " + fmt(std::abs(line->offset)) + " m off the middle at s " +
              fmt(s) + " m, past its " + fmt(num::max(0.0, half - D::line.margin)) + " m");
      break;
    }
  }
  if (line_bend >= piste_bend) {
    add("R32", Severity::error, "the racing line bends no less than the piste");
  }
  check_crests(level, race, D::prep.crest, "R32", add);
  check_nets(race, D::nets, "R32", add);
  check_trap(race, D::trap, "R32", add);
  check_prepared(level, race, D::prep, "R32", add);
}

void check_turns(const mapgen::Level& level, const mapgen::RaceCourse& race, const Add& add) {
  const bool sg = race.discipline == Discipline::super_g;
  const mapgen::TurnRule& G =
      sg ? mapgen::discipline_rules::super_g::rule : mapgen::discipline_rules::giant_slalom::rule;
  const double apart =
      sg ? mapgen::discipline_rules::super_g::apart : mapgen::discipline_rules::giant_slalom::apart;
  const char* rule = sg ? "R33" : "R36";
  const mapgen::Track& track = level.track;
  const auto& cps = level.checkpoints;
  if (cps.size() < 3) {
    add(rule, Severity::error, "the course sets no gate");
    return;
  }
  check_stretch(level, race, G.finish_width, rule, add);
  // The start lowered into the band: no more than the aim, and no less than the band's floor
  // where the piste drops that far.
  if (race.vertical > G.target + 1) {
    add(rule, Severity::error,
        "the course drops " + fmt(race.vertical) + " m, over the " + str(G.target) +
            " m its start is lowered to");
  }
  if (race.base && !race.base->checkpoints.empty()) {
    const mapgen::Track& under = race.base->track;
    const double whole = mapgen::track_point_at(under, race.base->checkpoints.front().s).y -
                         mapgen::track_point_at(under, under.length).y;
    if (whole > G.vertical.min && race.vertical <= G.vertical.min) {
      add(rule, Severity::error,
          "the course drops " + fmt(race.vertical) + " m, under the band's " + str(G.vertical.min) +
              " m on a piste that drops " + fmt(whole) + " m");
    }
  }
  // The gates: as many as the direction changes ask, spaced, side to side in turn.
  const size_t inner = cps.size() - 2;
  if (static_cast<int>(inner) != race.turns ||
      double(inner) < std::ceil(G.changes * race.vertical)) {
    add(rule, Severity::error,
        "the course sets " + std::to_string(inner) + " gates, under the " +
            fmt(std::ceil(G.changes * race.vertical), 0) + " its vertical's direction changes ask");
  }
  for (size_t i = 1; i + 1 < cps.size(); i++) {
    const Checkpoint& g = cps[i];
    const std::string at = gate_name(i);
    const mapgen::GateColour want = i % 2 == 1 ? mapgen::GateColour::red : mapgen::GateColour::blue;
    if (!g.panels || g.pole || g.width != G.width || g.colour != want || g.turn == 0) {
      add(rule, Severity::error,
          at + " is not a panelled gate " + str(G.width) + " m wide, red and blue in turn");
    }
    if (i > 1) {
      const Checkpoint& b = cps[i - 1];
      if (g.turn != -b.turn) add(rule, Severity::error, at + " turns the racer the same way again");
      const GatePoles pa = poles_of(b);
      const GatePoles pb = poles_of(g);
      const double d = ge::core::hypot(pb.a.x - pa.a.x, pb.a.z - pa.a.z);
      if (d <= apart) {
        add(rule, Severity::error,
            at + "'s turning pole stands " + fmt(d) + " m from the one before, under " +
                str(apart) + " m");
      }
      if (g.s - b.s < G.spacing.min - kSlack) {
        add(rule, Severity::error,
            at + " stands " + fmt(g.s - b.s) + " m below the one before, under " +
                str(G.spacing.min) + " m");
      }
    }
    // The turning pole inside the line's apex, the outside pole within a gate of it, both on the
    // snow.
    const double centre = mapgen::nearest_track_point(track, g.x, g.z).lateral;
    const double turning = centre + g.turn * (g.width / 2);
    const double outside = centre - g.turn * (g.width / 2);
    const auto line = mapgen::speed_line_at(level, g.s);
    if (!line || std::abs(std::abs(line->offset - turning) - G.pass) > 0.5 ||
        std::abs(line->offset - outside) > g.width) {
      add(rule, Severity::error,
          "the racing line does not pass " + str(G.pass) + " m outside " + at + "'s turning pole");
    }
    const double half = mapgen::track_point_at(track, g.s).width / 2;
    if (num::max(std::abs(turning), std::abs(outside)) > half - G.inside + 0.5) {
      add(rule, Severity::error, at + "'s pole stands past its room inside the piste");
    }
    for (const double j : race.jumps) {
      if (g.s > j - G.jump + kSlack && g.s < j + G.landing - kSlack) {
        add(rule, Severity::error, at + " stands on a jump's lip or landing at s " + fmt(j) + " m");
      }
    }
  }
  check_crests(level, race, G.prep.crest, rule, add);
  check_nets(race, G.nets, rule, add);
  if (sg) {
    check_trap(race, mapgen::discipline_rules::super_g::trap, rule, add);
  } else if (race.trap) {
    add(rule, Severity::error, "the giant slalom has a speed trap");
  }
  check_prepared(level, race, G.prep, rule, add);
}

}  // namespace ge::mountain::analysis
