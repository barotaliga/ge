// The bot's plan and its step (bot.hpp).
//
// ge:summary plan_bot and bot_controls: the speed plan, the pursuit, the brake and the tuck
// ge:keywords bot plan_bot bot_controls pursuit speed-plan
// ge:skill sim
// ge:test tests/fall_line_sim_test.cpp
#include "fall_line/sim/bot.hpp"

#include <cmath>

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

BotPlan plan_bot(const mg::Level& level, const sk::SkiSpec& spec) {
  const auto& pts = level.track.points;
  const size_t n = pts.size();
  BotPlan plan;
  plan.curvature.assign(n, 0);
  plan.speed.assign(n, 0);
  if (n < 2) return plan;
  // The bend at each point, over four stations either side, then smoothed over as many again.
  std::vector<double> raw(n, 0);
  constexpr size_t kSpan = 4;
  for (size_t i = 0; i < n; i++) {
    const size_t a = i < kSpan ? 0 : i - kSpan;
    const size_t b = i + kSpan >= n ? n - 1 : i + kSpan;
    const double ds = pts[b].s - pts[a].s;
    raw[i] = ds > 0 ? core::angle_diff(pts[a].heading, pts[b].heading) / ds : 0;
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
  // His lane: held right of the centreline, never nearer the piste's edge than `kLaneMargin`.
  const double room = num::max(0.0, aim.width / 2 - bot::kLaneMargin);
  const double lane = core::clamp(r.rider.lane, -room, room);
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
