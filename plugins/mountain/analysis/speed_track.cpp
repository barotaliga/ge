// R34's checks behind race_checks.hpp: the speed-skiing track straight and the map's own, its
// profile graded (never rising, its crests and knees rounded, the ground under it the profile),
// the launch one of the rule's, the zone 100 m along the snow, the run-out past the braking line
// and short of the track's end, the final in the top class's band and the qualification under it,
// the start gate and the zone's two lines, the safety margin cleared of trunks, groomed hard,
// clear of every lift station, and no kicker or drop on it.
//
// ge:summary R34 checks: the speed track straight, graded, timed, run out, cleared and groomed
// ge:keywords analysis speed-skiing speed-track timing-zone run-out crest knee margin r34
// ge:skill mountain
// ge:test tests/mountain_speed_ski_test.cpp
#include <cmath>
#include <string>
#include <vector>

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "core/num.hpp"
#include "mountain/analysis/race_checks.hpp"
#include "mountain/mapgen/discipline_rules.hpp"
#include "mountain/mapgen/speed_ski.hpp"

namespace ge::mountain::analysis {

namespace num = ge::core::num;
namespace libm = ge::core::libm;
namespace K = mapgen::discipline_rules::speed_ski;
using mapgen::TrackPoint;

namespace {

// How far a crest or a knee read off the built track may pass its radius, a share: the profile is
// a cubic through samples 4 m apart, read here over 8 m.
constexpr double kBendSlack = 1.15;
// How far the ground under the track may stand off its profile, m (the grid's bilinear read).
constexpr double kGround = 0.3;
// How packed the track must read (R10's groomer).
constexpr double kGroomed = 0.98;

// The track's length along the snow between arcs `a` and `b`, m.
double along_snow(const std::vector<TrackPoint>& pts, double a, double b) {
  double along = 0;
  for (size_t i = 1; i < pts.size(); i++) {
    if (pts[i].s <= a || pts[i].s > b) continue;
    along += ge::core::hypot(pts[i].s - pts[i - 1].s, pts[i].y - pts[i - 1].y);
  }
  return along;
}

}  // namespace

void check_speed_ski(const mapgen::Level& level, const mapgen::RaceCourse& race, const Add& add) {
  const std::vector<TrackPoint>& pts = level.track.points;
  if (pts.size() < 3 || !race.zone) {
    add("R34", Severity::error, "the speed track has no timing zone");
    return;
  }
  // Straight, the rule's width, near the fall line; the piste's kickers and drops on no track.
  for (const TrackPoint& p : pts) {
    if (p.heading != pts[0].heading || p.width != K::width) {
      add("R34", Severity::error,
          "the speed track bends or narrows at s " + fmt(p.s) + " m; it is cut straight, " +
              str(K::width) + " m wide");
      break;
    }
  }
  const double most = K::search::bearings.back() * ge::core::kPi / 180;
  if (std::abs(pts[0].heading) > most + 1e-9) {
    add("R34", Severity::error,
        "the speed track runs " + fmt(pts[0].heading * 180 / ge::core::kPi) +
            "° off the fall line, past " + str(K::search::bearings.back()) + "°");
  }
  for (const mapgen::Kicker& k : level.kickers) {
    if (k.on_track) add("R34", Severity::error, "kicker " + k.id + " stands on the speed track");
  }
  for (const mapgen::Cliff& c : level.cliffs) {
    if (c.on_track) add("R34", Severity::error, "drop " + c.id + " crosses the speed track");
  }
  // Graded: never rising, its crests and knees rounded, the ground under it its profile.
  const double h = pts[1].s - pts[0].s;
  for (size_t i = 1; i < pts.size(); i++) {
    if (pts[i].y > pts[i - 1].y + 1e-6) {
      add("R34", Severity::error, "the speed track rises at s " + fmt(pts[i].s) + " m");
      break;
    }
  }
  const auto w = static_cast<size_t>(num::round(8 / h));
  for (size_t i = w; i + w < pts.size(); i++) {
    const double span = double(w) * h;
    const double bend = (pts[i - w].y - 2 * pts[i].y + pts[i + w].y) / (span * span);
    if (bend <= -kBendSlack / K::crest || bend >= kBendSlack / K::knee) {
      add("R34", Severity::error,
          std::string(bend < 0 ? "a crest" : "a knee") + " at s " + fmt(pts[i].s) + " m bends at " +
              fmt(1 / std::abs(bend)) + " m of radius, under " +
              str(bend < 0 ? K::crest : K::knee) + " m");
      break;
    }
  }
  for (const TrackPoint& p : pts) {
    const double off = std::abs(level.ground_at(p.x, p.z) - p.y);
    if (off >= kGround) {
      add("R34", Severity::error,
          "the ground stands " + fmt(off, 2) + " m off the track's profile at s " + fmt(p.s) +
              " m");
      break;
    }
    const double firm = level.packed_at(p.x, p.z);
    if (firm < kGroomed) {
      add("R34", Severity::error,
          "the speed track is only " + fmt(firm, 2) + " packed at s " + fmt(p.s) + " m");
      break;
    }
  }
  // The launch, the zone, the run-out.
  const mapgen::RaceCourse::Zone& zone = *race.zone;
  if (std::abs(zone.length - K::trap) > 0.05) {
    add("R34", Severity::error,
        "the timing zone runs " + fmt(zone.length, 2) + " m along the snow, not " + str(K::trap));
  }
  const double launch = num::round(along_snow(pts, race.top, zone.from) / 100) * 100;
  bool known = false;
  for (const double l : K::launch) known = known || l == launch;
  if (!known) {
    add("R34", Severity::error, "the launch runs " + fmt(launch, 0) + " m, not one of the rule's");
  }
  if (race.stop <= zone.to + K::run_out::brake || race.stop >= level.track.length) {
    add("R34", Severity::error,
        "the run-out stops the racer at s " + fmt(race.stop) +
            " m, not past the braking line and short of the track's end");
  }
  // The final in the band, the qualification some 12 km/h under it.
  if (race.run == 2) {
    const double final_speed = mapgen::speed_ski_aim(level);
    if (final_speed < K::speed::min || final_speed > K::speed::max) {
      add("R34", Severity::error,
          "the final comes through the zone at " + fmt(final_speed) + " km/h, outside " +
              str(K::speed::min) + "–" + str(K::speed::max));
    }
  }
  if (race.qualify <= race.top) {
    add("R34", Severity::error, "the qualification does not start below the final");
  }
  // The start gate and the zone's two lines.
  const auto& cps = level.checkpoints;
  const double wide = K::width + 2 * K::margin;
  if (cps.size() != 3 || std::abs(cps[0].s - race.from) > 1e-6 ||
      std::abs(cps[1].s - zone.from) > 1e-6 || std::abs(cps[2].s - zone.to) > 1e-6 ||
      cps[1].width != wide || cps[2].width != wide) {
    add("R34", Severity::error,
        "the speed track's gates are not its start gate and the zone's two lines, " + str(wide) +
            " m across");
  }
  if (level.grid.size() != 1) add("R34", Severity::error, "the start house is not one racer");
  // The safety margin cleared of every trunk.
  const double fx = libm::sin(pts[0].heading);
  const double fz = libm::cos(pts[0].heading);
  for (const mapgen::TreeDef& t : level.trees) {
    const double along = (t.x - pts[0].x) * fx + (t.z - pts[0].z) * fz;
    const double off = std::abs((t.x - pts[0].x) * fz - (t.z - pts[0].z) * fx);
    if (along > 0 && along < level.track.length && off <= K::width / 2 + K::margin) {
      add("R34", Severity::error,
          "a tree stands " + fmt(off) + " m off the speed track's middle " + fmt(along) +
              " m down it, inside its margin");
      break;
    }
  }
  // No lift station within its distance of the track.
  if (level.resort) {
    for (const mapgen::Lift& l : level.resort->lifts) {
      for (const mapgen::Vec3& p : {l.bottom, l.top}) {
        const double along = (p.x - pts[0].x) * fx + (p.z - pts[0].z) * fz;
        const double off = std::abs((p.x - pts[0].x) * fz - (p.z - pts[0].z) * fx);
        if (along > -K::stations && along < level.track.length + K::stations &&
            off < K::width / 2 + K::stations - 1e-6) {
          add("R34", Severity::error, "lift " + l.id + "'s station stands by the speed track");
        }
      }
    }
  }
}

}  // namespace ge::mountain::analysis
