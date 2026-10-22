// The race course's checks behind race.hpp: R31, the slalom, as its rule states it: the stretch
// (its drop, its length, its width, clear of the piste's drops), the start and the finish, the
// pole gates (their kinds, colours, widths, every pole inside the piste, turning pole to turning
// pole), the combinations and where they stand, the count off the vertical, the racing line
// outside every turning pole, the hill prepared (no kicker left on it, groomed, the woods cut).
//
// ge:summary R31's checks: the stretch, the pole gates, the combinations, the line, the hill
// ge:keywords analysis analyze-race slalom pole-gate turning-pole hairpin r31
// ge:skill mountain
// ge:test tests/mountain_slalom_test.cpp
#include "mountain/analysis/race.hpp"

#include "mountain/analysis/race_checks.hpp"

#include <cmath>
#include <string>
#include <vector>

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "core/num.hpp"
#include "mountain/mapgen/discipline_rules.hpp"
#include "mountain/mapgen/query.hpp"
#include "mountain/mapgen/slalom.hpp"

namespace ge::mountain::analysis {

namespace num = ge::core::num;
namespace libm = ge::core::libm;
namespace S = mapgen::discipline_rules::slalom;
using mapgen::Checkpoint;
using mapgen::PlanPoint;
using mapgen::PoleGate;
using mapgen::TrackHit;

namespace {

// How far a measured length may stray from its band, m: a gate's arc is the piste's, read off
// stations 2 m apart.
constexpr double kSlack = 0.5;
// How far outside a turning pole the racing line must pass, m.
constexpr double kLineClear = 0.3;
// How packed the course must read from the hut to the end of the run-out (R10's groomer).
constexpr double kGroomed = 0.98;

void check_slalom(const mapgen::Level& level, const mapgen::RaceCourse& race, const Add& add) {
  const mapgen::Track& track = level.track;
  const auto& cps = level.checkpoints;
  const double length = race.to - race.from;
  // The stretch.
  if (race.vertical < S::drops.back() - 1) {
    add("R31", Severity::error,
        "the slalom drops " + fmt(race.vertical) + " m, under the least slalom drop of " +
            str(S::drops.back()) + " m");
  }
  if (length > S::max_length + kSlack) {
    add("R31", Severity::error,
        "the slalom runs " + fmt(length) + " m down the piste, over the " + str(S::max_length) +
            " m a slalom hill runs");
  }
  for (const mapgen::TrackPoint& p : track.points) {
    if (p.s < race.from || p.s > race.to) continue;
    if (p.width < S::min_width - kSlack) {
      add("R31", Severity::error,
          "the piste is " + fmt(p.width) + " m wide at s " + fmt(p.s) + " m, under the slalom's " +
              str(S::min_width) + " m");
      break;
    }
  }
  for (const mapgen::Cliff& c : level.cliffs) {
    if (!c.on_track || !c.s) continue;
    if (*c.s + c.face + c.landing > race.from && *c.s - c.shelf < race.to) {
      add("R31", Severity::error, "drop " + c.id + " crosses the slalom at s " + fmt(*c.s) + " m");
    }
  }
  // The start and the finish.
  if (cps.size() < 3 + 2 * static_cast<size_t>(S::clean)) {
    add("R31", Severity::error, "the slalom sets only " + std::to_string(cps.size()) + " gates");
    return;
  }
  if (std::abs(cps.front().s - race.from) > 1e-6 || std::abs(cps.back().s - race.to) > 1e-6) {
    add("R31", Severity::error, "the start wand or the finish line is off the slalom's stretch");
  }
  if (cps.back().width < S::finish_width - 1e-9) {
    add("R31", Severity::error,
        "the finish line is " + fmt(cps.back().width) + " m wide, under " + str(S::finish_width) +
            " m");
  }
  if (level.grid.size() != 1 ||
      mapgen::nearest_track_point(track, level.spawn.x, level.spawn.z).s >= race.from) {
    add("R31", Severity::error, "the start hut is not one racer stood above the wand");
  }
  // The gates.
  int open_count = 0;
  for (size_t i = 1; i + 1 < cps.size(); i++) {
    const Checkpoint& g = cps[i];
    const std::string at = gate_name(i);
    if (!g.pole) {
      add("R31", Severity::error, at + " is not a pole gate");
      continue;
    }
    const PoleGate kind = g.pole.value_or(PoleGate::open);
    const mapgen::GateColour want = i % 2 == 0 ? mapgen::GateColour::red : mapgen::GateColour::blue;
    if (g.colour != want) add("R31", Severity::error, at + " breaks the red and blue in turn");
    const mapgen::Band band = kind == PoleGate::open ? S::width : S::closed;
    if (g.width < band.min - 1e-9 || g.width > band.max + 1e-9) {
      add("R31", Severity::error,
          at + " is " + fmt(g.width, 2) + " m between its poles, outside " + band_text(band, " m"));
    }
    if (kind == PoleGate::open) open_count++;
    const GatePoles p = poles_of(g);
    for (const PlanPoint& pole : {p.a, p.b}) {
      const TrackHit hit = mapgen::nearest_track_point(track, pole.x, pole.z);
      const double half = track.points[static_cast<size_t>(hit.index)].width / 2;
      if (hit.distance > half + 0.05) {
        add("R31", Severity::error,
            at + "'s pole stands " + fmt(hit.distance - half, 2) + " m outside the piste");
      }
    }
    // The racing line outside its turning pole.
    if (kind == PoleGate::open) {
      const auto line = mapgen::slalom_line_at(level, g.s);
      const TrackHit hit = mapgen::nearest_track_point(track, p.a.x, p.a.z);
      const double inward = -double(g.turn);
      if (!line || (line->offset - hit.lateral) * inward <= kLineClear) {
        add("R31", Severity::error,
            "the racing line does not pass outside " + at + "'s turning pole");
      }
    }
  }
  // Turning pole to turning pole, two open gates in a row (a delay's the longest).
  for (size_t i = 2; i + 1 < cps.size(); i++) {
    const Checkpoint& a = cps[i - 1];
    const Checkpoint& b = cps[i];
    if (a.pole != PoleGate::open || b.pole != PoleGate::open) continue;
    const PlanPoint pa = poles_of(a).a;
    const PlanPoint pb = poles_of(b).a;
    const double d = ge::core::hypot(pb.x - pa.x, pb.z - pa.z);
    if (d > S::delayed.max + kSlack || d < S::turn.min - kSlack) {
      add("R31", Severity::error,
          gate_name(i) + "'s turning pole stands " + fmt(d) + " m from the one before, outside " +
              str(S::turn.min) + "–" + str(S::delayed.max) + " m");
    }
  }
  // The combinations: as many as the rule asks, none at the start or the finish.
  if (race.hairpins < S::hairpins[0] || race.verticals < S::verticals[0] ||
      race.delays < S::delays[0]) {
    add("R31", Severity::error,
        "the slalom carries " + std::to_string(race.hairpins) + " hairpins, " +
            std::to_string(race.verticals) + " verticals and " + std::to_string(race.delays) +
            " delays, under the rule's " + std::to_string(S::hairpins[0]) + ", " +
            std::to_string(S::verticals[0]) + " and " + std::to_string(S::delays[0]));
  }
  const size_t inner = cps.size() - 2;
  for (size_t k = 0; k < static_cast<size_t>(S::clean); k++) {
    for (const size_t i : {1 + k, inner - k}) {
      if (cps[i].pole != PoleGate::open) {
        add("R31", Severity::error,
            gate_name(i) + " is a combination within " + std::to_string(S::clean) +
                " gates of the start or the finish");
      }
    }
  }
  if (race.to - cps[inner].s < S::last - 4) {
    add("R31", Severity::error,
        "the last gate stands " + fmt(race.to - cps[inner].s) + " m above the line, under " +
            str(S::last - 4) + " m");
  }
  // The direction changes off the vertical, where the spacing allows.
  const double most = num::max(num::round(S::changes::max * race.vertical) + S::changes::slack,
                               length / S::spacing.min);
  if (double(inner) > most + 1) {
    add("R31", Severity::error,
        "the slalom sets " + std::to_string(inner) + " gates, over the " + fmt(most + 1, 0) +
            " its vertical and spacing allow");
  }
  if (open_count == 0) add("R31", Severity::error, "the slalom sets no open gate");
  // The hill prepared: no kicker left on it, groomed hard, the woods cut.
  check_prepared(level, race, S::prep, "R31", add);
}

}  // namespace

GatePoles poles_of(const Checkpoint& g) {
  const double rx = libm::cos(g.heading);
  const double rz = -libm::sin(g.heading);
  const double t = g.turn != 0 ? double(g.turn) : 1.0;
  const double h = g.width / 2;
  return {{g.x + rx * t * h, g.z + rz * t * h}, {g.x - rx * t * h, g.z - rz * t * h}};
}

void check_prepared(const mapgen::Level& level, const mapgen::RaceCourse& race,
                    const mapgen::CoursePrep& P, const char* rule, const Add& add) {
  const mapgen::Track& track = level.track;
  for (const mapgen::Kicker& k : level.kickers) {
    if (!k.on_track || !k.s) continue;
    if (*k.s + k.landing >= race.from - P.clearance && *k.s - k.ramp <= race.to + P.clearance) {
      add(rule, Severity::error,
          "kicker " + k.id + " stands on the course at s " + fmt(*k.s) + " m");
    }
  }
  // (The groomer's edge down the run-out reads a cell soft either side of it.)
  const double groomed = num::min(track.length, race.to + P.outrun_length - 2 * level.cell);
  for (int k = 0; race.from + 2.0 * k <= groomed; k++) {
    const double s = race.from + 2.0 * k;
    const mapgen::TrackPoint p = mapgen::track_point_at(track, s);
    const double firm = level.packed_at(p.x, p.z);
    if (firm < kGroomed) {
      add(rule, Severity::error,
          "the course is only " + fmt(firm, 2) + " packed at s " + fmt(s) + " m");
      break;
    }
  }
  double widest = 0;
  for (const mapgen::TrackPoint& p : track.points) widest = num::max(widest, p.width);
  for (const mapgen::TreeDef& t : level.trees) {
    TrackHit hit;
    mapgen::nearest_within(track, t.x, t.z, widest / 2 + P.clear, hit);
    if (!std::isfinite(hit.distance) || hit.s < race.from || hit.s > race.to) continue;
    const double keep = track.points[static_cast<size_t>(hit.index)].width / 2 + P.clear - 1;
    if (hit.distance <= keep) {
      add(rule, Severity::error,
          "a tree stands " + fmt(hit.distance) + " m off the course's line at s " + fmt(hit.s) +
              " m, inside the " + fmt(keep) + " m cleared");
      break;
    }
  }
}

LevelAnalysis analyze_race(const mapgen::Level& level) {
  LevelAnalysis out;
  const Add add = [&](const char* rule, Severity severity, std::string message) {
    if (severity == Severity::error) out.ok = false;
    out.findings.push_back({rule, severity, std::move(message)});
  };
  if (!level.race) {
    add("R31", Severity::error, "the map names no race course");
    return out;
  }
  switch (level.race->discipline) {
    case mapgen::Discipline::slalom:
      check_slalom(level, *level.race, add);
      break;
    case mapgen::Discipline::downhill:
      check_downhill(level, *level.race, add);
      break;
    case mapgen::Discipline::super_g:
    case mapgen::Discipline::giant_slalom:
      check_turns(level, *level.race, add);
      break;
    case mapgen::Discipline::speed_ski:
      check_speed_ski(level, *level.race, add);
      break;
  }
  return out;
}

}  // namespace ge::mountain::analysis
