// R35's checks behind race_checks.hpp: the ski-cross course as its rule states it. The stretch
// and its bands (warnings: a short or steep piste builds the best course it has), the line (its
// width, no turn tighter than the rule's tightest), the profile (never rising off its features,
// its crests and knees rounded past the start ramp, the ground under it the profile), the
// features (spaced, clear of the turns, every jump the course's own kicker), the gates (a turning
// gate in every berm on its inside, a corridor gate before every feature, none in a landing, red
// and blue in turn), the start's four lanes, the finish and its area, the fence, groomed hard and
// cleared of trees, and no kicker or drop of the piste left on it.
//
// ge:summary R35 checks: the ski-cross stretch, line, profile, features, gates, lanes and finish
// ge:keywords analysis ski-cross berm corridor-gate turning-gate landing fence lanes r35
// ge:skill mountain
// ge:test tests/mountain_ski_cross_test.cpp
#include <cmath>
#include <string>
#include <vector>

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "core/num.hpp"
#include "mountain/analysis/race_checks.hpp"
#include "mountain/mapgen/discipline_rules.hpp"
#include "mountain/mapgen/query.hpp"

namespace ge::mountain::analysis {

namespace num = ge::core::num;
namespace libm = ge::core::libm;
namespace K = mapgen::discipline_rules::ski_cross;
using mapgen::Checkpoint;
using mapgen::CrossFeature;
using mapgen::TrackPoint;
using Feature = mapgen::RaceCourse::Feature;

namespace {

// How far a crest or a knee read off the built line may pass its radius, a share: read over 8 m
// of stations 2 m apart.
constexpr double kBendSlack = 1.15;
// How far a turn read off the line's headings may pass its tightest radius, a share.
constexpr double kTurnSlack = 1.05;
// How far the ground under the line may stand off its profile, m (the grid's bilinear read).
constexpr double kGround = 0.3;
// How packed the course must read (R10's groomer).
constexpr double kGroomed = 0.98;
// How far either side of a feature its own rise and bends are its own, m.
constexpr double kAround = 2;

bool on_feature(const std::vector<Feature>& features, double s) {
  for (const Feature& f : features) {
    if (f.kind != CrossFeature::berm && s > f.from - kAround && s < f.to + kAround) return true;
  }
  return false;
}

}  // namespace

void check_ski_cross(const mapgen::Level& level, const mapgen::RaceCourse& race, const Add& add) {
  const std::vector<TrackPoint>& pts = level.track.points;
  if (pts.size() < 3 || !race.stretch) {
    add("R35", Severity::error, "the ski cross names no stretch it was built on");
    return;
  }
  // The stretch and its bands.
  const double length = race.to - race.from;
  const double grade = race.vertical / num::max(1.0, length);
  if (race.vertical < K::vertical.min || race.vertical > K::vertical.max) {
    add("R35", Severity::warn,
        "the course drops " + fmt(race.vertical) + " m, outside " + str(K::vertical.min) + "–" +
            str(K::vertical.max));
  }
  if (length < K::length.min || length > K::length.max) {
    add("R35", Severity::warn,
        "the course runs " + fmt(length) + " m, outside " + str(K::length.min) + "–" +
            str(K::length.max));
  }
  if (grade < K::grade.min || grade > K::grade.max) {
    add("R35", Severity::warn,
        "the course's mean gradient is " + fmt(grade * 100) + " %, outside " +
            str(K::grade.min * 100) + "–" + str(K::grade.max * 100) + " %");
  }
  // The line: its width, its turns.
  const double ramp_end = K::start.platform + K::start.ramp.length;
  for (const TrackPoint& p : pts) {
    const bool start = p.s < ramp_end + 10;
    if (start ? p.width < K::start.width - 1e-9 || p.width > K::width + 1e-9
              : p.width != K::width) {
      add("R35", Severity::error,
          "the course is " + fmt(p.width) + " m wide at s " + fmt(p.s) + " m, not " +
              str(K::width));
      break;
    }
  }
  for (size_t i = 1; i < pts.size(); i++) {
    const double ds = pts[i].s - pts[i - 1].s;
    const double bend = std::abs(ge::core::angle_diff(pts[i - 1].heading, pts[i].heading)) / ds;
    if (bend > kTurnSlack / K::turn.tightest) {
      add("R35", Severity::error,
          "the course turns at " + fmt(1 / bend) + " m of radius at s " + fmt(pts[i].s) +
              " m, under " + str(K::turn.tightest));
      break;
    }
  }
  // The start straight: no direction change within it.
  for (const TrackPoint& p : pts) {
    if (std::abs(ge::core::angle_diff(pts[0].heading, p.heading)) > 1e-6) {
      if (p.s - race.from < K::start.straight - 1e-6) {
        add("R35", Severity::error,
            "the course changes direction " + fmt(p.s - race.from) +
                " m below its doors, inside its " + str(K::start.straight) + " m start straight");
      }
      break;
    }
  }
  // The profile: never rising off its features, its crests and knees rounded past the start.
  for (size_t i = 1; i < pts.size(); i++) {
    if (pts[i].y > pts[i - 1].y + 1e-6 && !on_feature(race.features, pts[i].s)) {
      add("R35", Severity::error, "the course rises at s " + fmt(pts[i].s) + " m off its features");
      break;
    }
  }
  const double h = pts[1].s - pts[0].s;
  const auto w = static_cast<size_t>(num::round(8 / h));
  const double settled = ramp_end + K::start.ramp.ease + 8;
  for (size_t i = w; i + w < pts.size(); i++) {
    if (pts[i].s < settled) continue;
    if (on_feature(race.features, pts[i - w].s) || on_feature(race.features, pts[i].s) ||
        on_feature(race.features, pts[i + w].s)) {
      continue;
    }
    const double span = double(w) * h;
    const double bend = (pts[i - w].y - 2 * pts[i].y + pts[i + w].y) / (span * span);
    if (bend <= -kBendSlack / K::crest || bend >= kBendSlack / K::knee) {
      add("R35", Severity::error,
          std::string(bend < 0 ? "a crest" : "a knee") + " at s " + fmt(pts[i].s) + " m bends at " +
              fmt(1 / std::abs(bend)) + " m of radius, under " +
              str(bend < 0 ? K::crest : K::knee) + " m");
      break;
    }
  }
  for (const TrackPoint& p : pts) {
    const double off = std::abs(level.ground_at(p.x, p.z) - p.y);
    if (off >= kGround) {
      add("R35", Severity::error,
          "the ground stands " + fmt(off, 2) + " m off the course's profile at s " + fmt(p.s) +
              " m");
      break;
    }
    const double firm = level.packed_at(p.x, p.z);
    if (firm < kGroomed) {
      add("R35", Severity::error,
          "the course is only " + fmt(firm, 2) + " packed at s " + fmt(p.s) + " m");
      break;
    }
  }
  // The features: spaced, clear of the turns, every jump the course's own kicker.
  std::vector<const Feature*> berms;
  std::vector<const Feature*> built;
  for (const Feature& f : race.features)
    (f.kind == CrossFeature::berm ? berms : built).push_back(&f);
  for (size_t i = 0; i < built.size(); i++) {
    const Feature& f = *built[i];
    if (i > 0 && f.from < built[i - 1]->to + K::between - 1e-6) {
      add("R35", Severity::error,
          std::string(mapgen::cross_feature_name(f.kind)) + " at s " + fmt(f.from) +
              " m starts under " + str(K::between) + " m past the feature before it");
    }
    for (const Feature* b : berms) {
      if (f.to > b->from - K::clear + 1e-6 && f.from < b->to + K::exit - 1e-6) {
        add("R35", Severity::error,
            std::string(mapgen::cross_feature_name(f.kind)) + " at s " + fmt(f.from) +
                " m stands within " + str(K::clear) + " m before or " + str(K::exit) +
                " m after the berm at s " + fmt(b->from) + " m");
      }
    }
    if (f.kind == CrossFeature::rollers) continue;
    bool kicked = false;
    for (const mapgen::Kicker& k : level.kickers) {
      kicked = kicked || (k.on_track && k.s && std::abs(*k.s - f.lip) < 1e-6);
    }
    if (!kicked) {
      add("R35", Severity::error,
          std::string(mapgen::cross_feature_name(f.kind)) + " at s " + fmt(f.lip) +
              " m is no kicker on the course");
    }
  }
  for (const mapgen::Kicker& k : level.kickers) {
    if (k.on_track && k.id.empty()) continue;
    if (k.on_track && k.id[0] != 'J') {
      add("R35", Severity::error, "the piste's kicker " + k.id + " stands on the course");
    }
  }
  for (const mapgen::Cliff& c : level.cliffs) {
    if (c.on_track) add("R35", Severity::error, "drop " + c.id + " crosses the course");
  }
  // The gates: the start gate first, the finish line last, flags between.
  const auto& cps = level.checkpoints;
  if (cps.size() < 2 || std::abs(cps.front().s - race.from - 0.3) > 1e-6 ||
      std::abs(cps.back().s - race.to) > 1e-6) {
    add("R35", Severity::error, "the course's first gate is not its start nor its last its finish");
    return;
  }
  if (cps.back().width < K::finish.width - 1e-9) {
    add("R35", Severity::error,
        "the finish line is " + fmt(cps.back().width) + " m wide, under " + str(K::finish.width));
  }
  if (level.track.length - race.to < K::finish.area - 1e-6) {
    add("R35", Severity::error,
        "the finish area runs " + fmt(level.track.length - race.to) + " m past the line, under " +
            str(K::finish.area));
  }
  for (size_t i = 1; i + 1 < cps.size(); i++) {
    const Checkpoint& g = cps[i];
    if (!g.flags) add("R35", Severity::error, gate_name(i) + " flies no flags");
    const auto want = (i - 1) % 2 == 0 ? mapgen::GateColour::red : mapgen::GateColour::blue;
    if (g.colour != want)
      add("R35", Severity::error, gate_name(i) + " breaks red and blue in turn");
    for (const Feature* f : built) {
      if (f->kind != CrossFeature::rollers && g.s > f->lip && g.s < f->to) {
        add("R35", Severity::error,
            gate_name(i) + " at s " + fmt(g.s) + " m stands in the landing of the " +
                mapgen::cross_feature_name(f->kind) + " at s " + fmt(f->lip) + " m");
      }
    }
  }
  for (const Feature* b : berms) {
    bool turning = false;
    for (size_t i = 1; i + 1 < cps.size(); i++) {
      const Checkpoint& g = cps[i];
      turning = turning || (g.pole == mapgen::PoleGate::open && g.turn == b->side &&
                            g.s >= b->from && g.s <= b->to);
    }
    if (!turning) {
      add("R35", Severity::error,
          "the berm at s " + fmt(b->from) + " m has no turning gate on its inside");
    }
  }
  for (const Feature* f : built) {
    bool corridor = false;
    for (size_t i = 1; i + 1 < cps.size(); i++) {
      corridor =
          corridor || (!cps[i].pole && std::abs(cps[i].s - (f->from - K::gate_before)) < 1e-6);
    }
    if (!corridor) {
      add("R35", Severity::error,
          std::string("the ") + mapgen::cross_feature_name(f->kind) + " at s " + fmt(f->from) +
              " m has no corridor gate before it");
    }
  }
  // The start's lanes, the fence.
  if (level.grid.size() != static_cast<size_t>(K::start.lanes)) {
    add("R35", Severity::error,
        "the start gate has " + str(double(level.grid.size())) + " lanes, not " +
            str(K::start.lanes));
  }
  if (!race.nets || race.nets->gap != K::fence.gap) {
    add("R35", Severity::error, "the course is not fenced along both edges");
  }
  // The fence, the spill zones and the finish area cleared of trees.
  const TrackPoint finish = mapgen::track_point_at(level.track, race.to);
  const TrackPoint& end = pts.back();
  const double fx = libm::sin(end.heading);
  const double fz = libm::cos(end.heading);
  for (const mapgen::TreeDef& t : level.trees) {
    mapgen::TrackHit hit;
    mapgen::nearest_within(level.track, t.x, t.z, K::width / 2 + K::clear_trees, hit);
    if (hit.distance <= K::width / 2 + K::clear_trees) {
      add("R35", Severity::error,
          "a tree stands " + fmt(hit.distance) + " m off the course's middle at s " + fmt(hit.s) +
              " m, inside its spill zone");
      break;
    }
    const double dx = t.x - finish.x;
    const double dz = t.z - finish.z;
    const double along = dx * fx + dz * fz;
    const double side = dx * fz - dz * fx;
    if (along > -K::finish.before && along < K::finish.area + 10 &&
        std::abs(side) < K::finish.half) {
      add("R35", Severity::error, "a tree stands in the finish area");
      break;
    }
  }
}

}  // namespace ge::mountain::analysis
