// The skier's pilot's plan and its step (pilot.hpp).
//
// ge:summary lay_pilot_plan and skier_pilot: the speed plan, the pursuit, the brake and the tuck
// ge:keywords pilot skier_pilot lay_pilot_plan pursuit speed-plan
// ge:skill skier bots
// ge:test tests/skier_pilot_test.cpp
#include "skier/pilot.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "mountain/mapgen/query.hpp"
#include "skier/limits.hpp"
#include "skier/tuning.hpp"

namespace ge::skier {

namespace mg = ge::mountain::mapgen;
namespace core = ge::core;
namespace num = ge::core::num;

void lay_pilot_plan(const mg::Level& level, const SkiSpec& spec, std::span<const double> lips,
                    PilotPlan& plan, double slow_from, double slow) {
  const auto& pts = plan.track->points;
  const size_t n = pts.size();
  plan.curvature.assign(n, 0);
  plan.speed.assign(n, 0);
  if (n < 2) return;
  // The heading the pilot travels at each point: the track's, or the offset line's where it has
  // one.
  std::vector<double> heading(n);
  for (size_t i = 0; i < n; i++) heading[i] = pts[i].heading;
  if (!plan.line.empty()) {
    for (size_t i = 0; i < n; i++) {
      const size_t a = i == 0 ? 0 : i - 1;
      const size_t b = i + 1 >= n ? n - 1 : i + 1;
      const double ax = pts[a].x + core::libm::cos(pts[a].heading) * plan.line[a];
      const double az = pts[a].z - core::libm::sin(pts[a].heading) * plan.line[a];
      const double bx = pts[b].x + core::libm::cos(pts[b].heading) * plan.line[b];
      const double bz = pts[b].z - core::libm::sin(pts[b].heading) * plan.line[b];
      heading[i] = core::libm::atan2(bx - ax, bz - az);
    }
  }
  // The bend at each point, over four stations either side, then smoothed over as many again.
  std::vector<double> raw(n, 0);
  constexpr size_t kSpan = 4;
  for (size_t i = 0; i < n; i++) {
    const size_t a = i < kSpan ? 0 : i - kSpan;
    const size_t b = i + kSpan >= n ? n - 1 : i + kSpan;
    const double ds = pts[b].s - pts[a].s;
    raw[i] = ds > 0 ? core::angle_diff(heading[a], heading[b]) / ds : 0;
  }
  for (size_t i = 0; i < n; i++) {
    const size_t a = i < kSpan ? 0 : i - kSpan;
    const size_t b = i + kSpan >= n ? n - 1 : i + kSpan;
    double sum = 0;
    for (size_t k = a; k <= b; k++) sum += raw[k];
    plan.curvature[i] = sum / static_cast<double>(b - a + 1);
  }
  for (size_t i = 0; i < n; i++) {
    const double packed = core::clamp(level.packed_at(pts[i].x, pts[i].z), 0, 1);
    plan.speed[i] = pilot::kCornerShare * carve_speed(spec, plan.curvature[i], packed);
  }
  for (const double lip : lips) {
    for (size_t i = 0; i < n; i++) {
      if (pts[i].s > lip - pilot::kKickerReach && pts[i].s <= lip) {
        plan.speed[i] = num::min(plan.speed[i], pilot::kKickerSpeed);
      }
    }
  }
  for (size_t i = 0; i < n; i++) {
    if (pts[i].s >= slow_from) plan.speed[i] = num::min(plan.speed[i], slow);
  }
  // Carried back up the line so every slowing is braked for in time.
  const double g = tuning::kGravity;
  for (size_t i = n - 1; i-- > 0;) {
    const double ds = pts[i + 1].s - pts[i].s;
    const double drop = pts[i].y - pts[i + 1].y;
    const double sin_p = ds > 0 ? drop / core::hypot(ds, drop) : 0;
    const double cos_p = std::sqrt(1 - sin_p * sin_p);
    const double decel = num::max(pilot::kBrakeLeast, pilot::kBrakeDecel * cos_p - g * sin_p);
    const double reach = std::sqrt(plan.speed[i + 1] * plan.speed[i + 1] + 2 * decel * ds);
    plan.speed[i] = num::min(plan.speed[i], reach);
  }
}

SkierInput skier_pilot(const SkierState& s, const SkiSpec& spec, const PilotPlan& plan,
                       const PilotRider& rider, ge::ai::Why* why, double lead_s) {
  SkierInput c;
  const mg::Track& track = *plan.track;
  const mg::TrackHit hit = mg::nearest_track_point(track, s.x, s.z);
  const double v = s.speed;
  // The line: pursue his lane a look ahead down the line; further with late hands.
  const double look =
      core::clamp(pilot::kLookTime * v + pilot::kLookLeast, pilot::kLookLeast, pilot::kLookMost) +
      (lead_s > 0 ? pilot::kLeadLook * lead_s * num::max(v, pilot::kLeadLeast) : 0);
  const mg::TrackPoint aim = mg::track_point_at(track, hit.s + look);
  // His lane: held right of the track (or of the offset line, inside its room), never nearer the
  // edge of the way than `kLaneMargin`.
  const double room = num::max(0.0, aim.width / 2 - pilot::kLaneMargin);
  double lane = core::clamp(rider.lane, -room, room);
  if (!plan.line.empty()) {
    const auto& line = track.points;
    const auto past = std::upper_bound(line.begin(), line.end(), aim.s,
                                       [](double at, const mg::TrackPoint& p) { return at < p.s; });
    const auto k = static_cast<size_t>(std::max<std::ptrdiff_t>(0, past - line.begin() - 1));
    const double gap = plan.room[k];
    lane = core::clamp(plan.line[k] + core::clamp(rider.lane, -gap, gap), -room, room);
  }
  const double dx = aim.x + core::libm::cos(aim.heading) * lane - s.x;
  const double dz = aim.z - core::libm::sin(aim.heading) * lane - s.z;
  const double alpha = core::angle_diff(s.heading, core::libm::atan2(dx, dz));
  const double curvature = 2 * core::libm::sin(alpha) / num::max(core::hypot(dx, dz), 1.0);
  const double edge = edge_for_curvature(spec, curvature);
  const double most = edge_most(spec, v, s.tuck);
  if (s.on_snow) c.steer = num::sign(alpha) * num::min(1.0, most > 0 ? edge / most : 1.0);
  if (why) {
    why->decide("line");
    why->fact("s_m", hit.s);
    why->fact("off_line_m", hit.distance);
    why->fact("look_m", look);
    why->fact("lane_m", lane);
    why->fact("aim_x", s.x + dx);
    why->fact("aim_z", s.z + dz);
    why->fact("bearing_rad", alpha);
    why->fact("curvature", curvature);
    why->fact("edge", edge);
    why->fact("edge_most", most);
    why->fact("steer", c.steer);
    why->choose(!s.on_snow  ? "airborne"
                : alpha > 0 ? "right"
                : alpha < 0 ? "left"
                            : "straight",
                "pursuit");
  }
  // The speed: the plan's least over the stretch ahead, at his pace.
  const auto& pts = track.points;
  const size_t from = static_cast<size_t>(hit.index);
  const auto ahead = static_cast<size_t>((pilot::kReadTime * v + pilot::kReadLeast) / 2) + 1;
  double cap = plan.speed[from];
  for (size_t k = from + 1; k < pts.size() && k <= from + ahead; k++) {
    cap = num::min(cap, plan.speed[k]);
  }
  cap *= rider.pace;
  const double turning = c.steer < 0 ? -c.steer : c.steer;
  if (v > cap + 0.5) {
    c.brake = core::clamp((v - cap) / 4, 0.2, 1);
  } else if (v < cap - 1 && turning < 0.35) {
    c.tuck = 1;
  }
  if (why) {
    // Each option scored by how far his speed is past its threshold: the one over zero is the
    // one taken (the thresholds never overlap), the glide the zero they are measured from.
    why->decide("speed");
    why->fact("speed", v);
    why->fact("cap", cap);
    why->fact("plan_here", plan.speed[from] * rider.pace);
    why->fact("pace", rider.pace);
    why->fact("turning", turning);
    why->option("brake", v - cap - 0.5);
    why->term("over_plan", v - cap);
    why->term("deadband", -0.5);
    why->option("tuck", cap - 1 - v);
    why->term("under_plan", cap - v);
    why->term("deadband", -1);
    if (turning >= 0.35) why->bar("turning");
    why->option("glide", 0);
    why->choose(c.brake > 0  ? "brake"
                : c.tuck > 0 ? "tuck"
                             : "glide",
                c.brake > 0                      ? "over_plan"
                : c.tuck > 0                     ? "under_plan"
                : turning >= 0.35 && v < cap - 1 ? "turning"
                                                 : "near_plan");
  }
  return c;
}

}  // namespace ge::skier
