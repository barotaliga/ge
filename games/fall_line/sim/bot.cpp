// The bot's plan and its step (bot.hpp).
//
// ge:summary plan_bot and bot_controls: the speed plan, the pursuit, the brake and the tuck
// ge:keywords bot plan_bot bot_controls pursuit speed-plan
// ge:skill sim
// ge:test tests/fall_line_sim_test.cpp
#include "fall_line/sim/bot.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <utility>

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "fall_line/sim/free.hpp"
#include "mountain/mapgen/query.hpp"
#include "skier/limits.hpp"
#include "skier/tuning.hpp"

namespace fall_line::sim {

namespace mg = ge::mountain::mapgen;
namespace sk = ge::skier;
namespace core = ge::core;
namespace num = ge::core::num;

namespace {

// THE WEAVE: on a course whose gates stand off the line, the bot's line through them (from each
// gate's offset to the next's on a smoothstep) and the half-room a lane has about it (inside the
// panels of the gate it is nearest by `kLaneMargin`). Left empty where every gate is on the line.
void weave_through_gates(const mg::Level& level, BotPlan& plan) {
  const auto& gates = level.checkpoints;
  bool offset = false;
  for (const mg::Checkpoint& g : gates) offset = offset || g.offset.has_value();
  if (!offset || gates.size() < 2) return;
  const auto& pts = level.track.points;
  const size_t n = pts.size();
  plan.line.assign(n, 0);
  plan.room.assign(n, 0);
  size_t k = 0;
  for (size_t i = 0; i < n; i++) {
    const double s = pts[i].s;
    while (k + 2 < gates.size() && gates[k + 1].s <= s) k++;
    const mg::Checkpoint& a = gates[k];
    const mg::Checkpoint& b = gates[k + 1];
    const double t = core::clamp((s - a.s) / num::max(1e-9, b.s - a.s), 0, 1);
    const double oa = a.offset.value_or(0);
    const double ob = b.offset.value_or(0);
    plan.line[i] = oa + (ob - oa) * core::smoothstep(0, 1, t);
    const mg::Checkpoint& near = t < 0.5 ? a : b;
    plan.room[i] = num::max(0.0, near.width / 2 - bot::kLaneMargin);
  }
}

// THE PLAN along `plan.track`: the course's gates woven through in a race, the kickers on the
// course's piste by their arc in a race and by their nearest point on the free ride's line.
void lay_plan(const mg::Level& level, const sk::SkiSpec& spec, bool race, BotPlan& plan,
              double slow_from = INFINITY, double slow = 0) {
  const auto& pts = plan.track->points;
  const size_t n = pts.size();
  plan.curvature.assign(n, 0);
  plan.speed.assign(n, 0);
  if (n < 2) return;
  if (race) weave_through_gates(level, plan);
  // The heading the bot travels at each point: the piste's, or the weave's where it has one.
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
    plan.speed[i] = bot::kCornerShare * sk::carve_speed(spec, plan.curvature[i], packed);
  }
  for (const mg::Kicker& k : level.kickers) {
    double lip = 0;
    if (race) {
      if (!k.on_track || !k.s.has_value()) continue;
      lip = k.s.value();
    } else {
      mg::TrackHit hit;
      hit.distance = INFINITY;
      mg::nearest_within(*plan.track, k.x, k.z, kRunLook, hit);
      if (!(hit.distance <= pts[static_cast<size_t>(hit.index)].width / 2)) continue;
      lip = hit.s;
    }
    for (size_t i = 0; i < n; i++) {
      if (pts[i].s > lip - bot::kKickerReach && pts[i].s <= lip) {
        plan.speed[i] = num::min(plan.speed[i], bot::kKickerSpeed);
      }
    }
  }
  for (size_t i = 0; i < n; i++) {
    if (pts[i].s >= slow_from) plan.speed[i] = num::min(plan.speed[i], slow);
  }
  // Carried back up the piste so every slowing is braked for in time.
  const double g = sk::tuning::kGravity;
  for (size_t i = n - 1; i-- > 0;) {
    const double ds = pts[i + 1].s - pts[i].s;
    const double drop = pts[i].y - pts[i + 1].y;
    const double sin_p = ds > 0 ? drop / core::hypot(ds, drop) : 0;
    const double cos_p = std::sqrt(1 - sin_p * sin_p);
    const double decel = num::max(bot::kBrakeLeast, bot::kBrakeDecel * cos_p - g * sin_p);
    const double reach = std::sqrt(plan.speed[i + 1] * plan.speed[i + 1] + 2 * decel * ds);
    plan.speed[i] = num::min(plan.speed[i], reach);
  }
}

}  // namespace

BotPlan plan_bot(const mg::Level& level, const sk::SkiSpec& spec) {
  BotPlan plan;
  // The map's own piste, held without owning it: the plan lives no longer than the run's map.
  plan.track = std::shared_ptr<const mg::Track>(std::shared_ptr<const mg::Track>{}, &level.track);
  lay_plan(level, spec, true, plan);
  return plan;
}

BotPlan plan_free(const mg::Level& level, int32_t run, const sk::SkiSpec& spec) {
  return plan_line(level, run_chain(level, run), spec);
}

BotPlan plan_line(const mg::Level& level, mg::Track line, const sk::SkiSpec& spec, double slow_from,
                  double slow) {
  BotPlan plan;
  plan.track = std::make_shared<const mg::Track>(std::move(line));
  lay_plan(level, spec, false, plan, slow_from, slow);
  return plan;
}

Controls bot_controls(const GameState& state, int32_t i, const BotPlan& plan, ge::ai::Why* why,
                      double lead_s) {
  const Racer& r = state.racers[static_cast<size_t>(i)];
  const sk::SkierState& s = r.skier;
  Controls c;
  if (why) {
    why->decide("mode");
    why->fact("speed", s.speed);
  }
  if (r.free.lift.phase != LiftPhase::none) {
    if (why) why->choose("lift", "on_lift");
    return c;
  }
  if (r.free.arrived && !plan.to_lift) {
    if (why) why->choose("stop", "arrived");
    c.ski.brake = 1;
    return c;
  }
  if (r.progress.finished || s.down) {
    if (why) why->choose("coast", r.progress.finished ? "finished" : "down");
    return c;
  }
  if (r.progress.missed) {
    if (why) why->choose("reset", "missed_gate");
    c.reset = true;
    return c;
  }
  if (why) why->choose("ride", "on_course");
  const mg::Track& track = *plan.track;
  const mg::TrackHit hit = mg::nearest_track_point(track, s.x, s.z);
  const double v = s.speed;
  // The line: pursue his lane a look ahead down the piste; further with late hands.
  const double look =
      core::clamp(bot::kLookTime * v + bot::kLookLeast, bot::kLookLeast, bot::kLookMost) +
      (lead_s > 0 ? bot::kLeadLook * lead_s * num::max(v, bot::kLeadLeast) : 0);
  const mg::TrackPoint aim = mg::track_point_at(track, hit.s + look);
  // His lane: held right of the centreline (or of the weave through the gates, inside their
  // panels), never nearer the piste's edge than `kLaneMargin`.
  const double room = num::max(0.0, aim.width / 2 - bot::kLaneMargin);
  double lane = core::clamp(r.rider.lane, -room, room);
  if (!plan.line.empty()) {
    const auto& line = track.points;
    const auto past = std::upper_bound(line.begin(), line.end(), aim.s,
                                       [](double at, const mg::TrackPoint& p) { return at < p.s; });
    const auto k = static_cast<size_t>(std::max<std::ptrdiff_t>(0, past - line.begin() - 1));
    const double gap = plan.room[k];
    lane = core::clamp(plan.line[k] + core::clamp(r.rider.lane, -gap, gap), -room, room);
  }
  const double dx = aim.x + core::libm::cos(aim.heading) * lane - s.x;
  const double dz = aim.z - core::libm::sin(aim.heading) * lane - s.z;
  const double alpha = core::angle_diff(s.heading, core::libm::atan2(dx, dz));
  const double curvature = 2 * core::libm::sin(alpha) / num::max(core::hypot(dx, dz), 1.0);
  const double edge = sk::edge_for_curvature(state.spec, curvature);
  const double most = sk::edge_most(state.spec, v, s.tuck);
  if (s.on_snow) c.ski.steer = num::sign(alpha) * num::min(1.0, most > 0 ? edge / most : 1.0);
  if (why) {
    why->decide("line");
    why->fact("s_m", hit.s);
    why->fact("off_line_m", hit.distance);
    why->fact("look_m", look);
    why->fact("lane_m", lane);
    why->fact("bearing_rad", alpha);
    why->fact("curvature", curvature);
    why->fact("edge", edge);
    why->fact("edge_most", most);
    why->fact("steer", c.ski.steer);
    why->choose(!s.on_snow  ? "airborne"
                : alpha > 0 ? "right"
                : alpha < 0 ? "left"
                            : "straight",
                "pursuit");
  }
  // The speed: the plan's least over the stretch ahead, at his pace.
  const auto& pts = track.points;
  const size_t from = static_cast<size_t>(hit.index);
  const auto ahead = static_cast<size_t>((bot::kReadTime * v + bot::kReadLeast) / 2) + 1;
  double cap = plan.speed[from];
  for (size_t k = from + 1; k < pts.size() && k <= from + ahead; k++) {
    cap = num::min(cap, plan.speed[k]);
  }
  cap *= r.rider.pace;
  const double turning = c.ski.steer < 0 ? -c.ski.steer : c.ski.steer;
  if (v > cap + 0.5) {
    c.ski.brake = core::clamp((v - cap) / 4, 0.2, 1);
  } else if (v < cap - 1 && turning < 0.35) {
    c.ski.tuck = 1;
  }
  if (why) {
    // Each option scored by how far his speed is past its threshold: the one over zero is the
    // one taken (the thresholds never overlap), the glide the zero they are measured from.
    why->decide("speed");
    why->fact("speed", v);
    why->fact("cap", cap);
    why->fact("plan_here", plan.speed[from] * r.rider.pace);
    why->fact("pace", r.rider.pace);
    why->fact("turning", turning);
    why->option("brake", v - cap - 0.5);
    why->term("over_plan", v - cap);
    why->term("deadband", -0.5);
    why->option("tuck", cap - 1 - v);
    why->term("under_plan", cap - v);
    why->term("deadband", -1);
    if (turning >= 0.35) why->bar("turning");
    why->option("glide", 0);
    why->choose(c.ski.brake > 0  ? "brake"
                : c.ski.tuck > 0 ? "tuck"
                                 : "glide",
                c.ski.brake > 0                  ? "over_plan"
                : c.ski.tuck > 0                 ? "under_plan"
                : turning >= 0.35 && v < cap - 1 ? "turning"
                                                 : "near_plan");
  }
  return c;
}

}  // namespace fall_line::sim
