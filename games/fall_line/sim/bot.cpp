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

#include "core/libm/libm.hpp"
#include "core/math.hpp"
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

}  // namespace

BotPlan plan_bot(const mg::Level& level, const sk::SkiSpec& spec) {
  const auto& pts = level.track.points;
  const size_t n = pts.size();
  BotPlan plan;
  plan.curvature.assign(n, 0);
  plan.speed.assign(n, 0);
  if (n < 2) return plan;
  weave_through_gates(level, plan);
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
    if (!k.on_track || !k.s.has_value()) continue;
    const double lip = k.s.value();
    for (size_t i = 0; i < n; i++) {
      if (pts[i].s > lip - bot::kKickerReach && pts[i].s <= lip) {
        plan.speed[i] = num::min(plan.speed[i], bot::kKickerSpeed);
      }
    }
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
  return plan;
}

Controls bot_controls(const GameState& state, int32_t i, const BotPlan& plan) {
  const Racer& r = state.racers[static_cast<size_t>(i)];
  const sk::SkierState& s = r.skier;
  Controls c;
  if (r.progress.finished || s.down) return c;
  if (r.progress.missed) {
    c.reset = true;
    return c;
  }
  const mg::Level& level = *state.level;
  const mg::TrackHit hit = mg::nearest_track_point(level.track, s.x, s.z);
  const double v = s.speed;
  // The line: pursue his lane a look ahead down the piste.
  const double look =
      core::clamp(bot::kLookTime * v + bot::kLookLeast, bot::kLookLeast, bot::kLookMost);
  const mg::TrackPoint aim = mg::track_point_at(level.track, hit.s + look);
  // His lane: held right of the centreline (or of the weave through the gates, inside their
  // panels), never nearer the piste's edge than `kLaneMargin`.
  const double room = num::max(0.0, aim.width / 2 - bot::kLaneMargin);
  double lane = core::clamp(r.rider.lane, -room, room);
  if (!plan.line.empty()) {
    const auto& line = level.track.points;
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
  // The speed: the plan's least over the stretch ahead, at his pace.
  const auto& pts = level.track.points;
  const size_t from = static_cast<size_t>(hit.index);
  const auto ahead = static_cast<size_t>((bot::kReadTime * v + bot::kReadLeast) / 2) + 1;
  double cap = plan.speed[from];
  for (size_t k = from + 1; k < pts.size() && k <= from + ahead; k++) {
    cap = num::min(cap, plan.speed[k]);
  }
  cap *= r.rider.pace;
  if (v > cap + 0.5) {
    c.ski.brake = core::clamp((v - cap) / 4, 0.2, 1);
  } else if (v < cap - 1 && (c.ski.steer < 0 ? -c.ski.steer : c.ski.steer) < 0.35) {
    c.ski.tuck = 1;
  }
  return c;
}

}  // namespace fall_line::sim
