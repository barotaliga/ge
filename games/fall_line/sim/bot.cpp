// The bot's plan and its step (bot.hpp).
//
// ge:summary plan_bot and bot_controls: the weave, the lips, the mode, over the skier's pilot
// ge:keywords bot plan_bot bot_controls weave lips mode
// ge:skill sim
// ge:test tests/fall_line_sim_test.cpp
#include "fall_line/sim/bot.hpp"

#include <cmath>
#include <utility>

#include "core/math.hpp"
#include "fall_line/sim/free.hpp"
#include "mountain/mapgen/query.hpp"

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

// THE PLAN along `plan.track`: the course's gates woven through in a race, the kickers' lips on
// the course's piste by their arc in a race and by their nearest point on the free ride's line;
// the rest is the pilot's (`ge::skier::lay_pilot_plan`).
void lay_plan(const mg::Level& level, const sk::SkiSpec& spec, bool race, BotPlan& plan,
              double slow_from = INFINITY, double slow = 0) {
  const auto& pts = plan.track->points;
  std::vector<double> lips;
  if (pts.size() < 2) return sk::lay_pilot_plan(level, spec, lips, plan, slow_from, slow);
  if (race) weave_through_gates(level, plan);
  for (const mg::Kicker& k : level.kickers) {
    if (race) {
      if (!k.on_track || !k.s.has_value()) continue;
      lips.push_back(k.s.value());
    } else {
      mg::TrackHit hit;
      hit.distance = INFINITY;
      mg::nearest_within(*plan.track, k.x, k.z, kRunLook, hit);
      if (!(hit.distance <= pts[static_cast<size_t>(hit.index)].width / 2)) continue;
      lips.push_back(hit.s);
    }
  }
  sk::lay_pilot_plan(level, spec, lips, plan, slow_from, slow);
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
  c.ski = sk::skier_pilot(s, state.spec, plan, {r.rider.lane, r.rider.pace}, why, lead_s);
  return c;
}

}  // namespace fall_line::sim
