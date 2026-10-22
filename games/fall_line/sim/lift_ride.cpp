// The lift ride: boarding, waiting, carried, let go (lift_ride.hpp).
//
// ge:summary board_lift, wait_for_lift and step_lift
// ge:keywords lift-ride board_lift step_lift wait_for_lift carried let-go
// ge:skill sim
// ge:test tests/fall_line_lift_ride_test.cpp
#include "fall_line/sim/lift_ride.hpp"

#include "mountain/mapgen/lift_line.hpp"
#include "skier/skier.hpp"

namespace fall_line::sim {

namespace ml = ge::mountain::lifts;
namespace mg = ge::mountain::mapgen;
namespace sk = ge::skier;

namespace {

void let_go(GameState& state, int32_t i, const ml::LiftPlan& plan) {
  Racer& r = state.racers[static_cast<size_t>(i)];
  OnLift& l = r.free.lift;
  const mg::LetGo at = mg::let_go_of(plan.kind, plan.bottom, plan.top);
  r.skier = sk::stand_skier(state.world(), at.x, at.z, at.heading, 0);
  const double t = state.t + kDt;
  state.events.push_back(GameEvent{EventKind::unloaded, i, l.lift, t - l.since});
  l.phase = LiftPhase::none;
  l.carrier = -1;
  l.ridden++;
  r.free.run = -1;
  r.free.arrived = false;
  r.progress.stuck = 0;
}

}  // namespace

void wait_for_lift(GameState& state, int32_t i, int32_t lift) {
  Racer& r = state.racers[static_cast<size_t>(i)];
  const ml::Place at = ml::load_spot((*state.lifts)[static_cast<size_t>(lift)]);
  r.skier = sk::stand_skier(state.world(), at.x, at.z, at.heading, 0);
  r.free.lift.phase = LiftPhase::waiting;
  r.free.lift.lift = lift;
  r.free.lift.carrier = -1;
  r.progress.stuck = 0;
  state.events.push_back(GameEvent{EventKind::boarded, i, lift, 0});
}

void board_lift(GameState& state, int32_t i) {
  if (!state.lifts) return;
  const sk::SkierState& s = state.racers[static_cast<size_t>(i)].skier;
  if (!s.on_snow || s.down) return;
  const auto& lifts = *state.lifts;
  for (size_t k = 0; k < lifts.size(); k++) {
    if (ml::in_load_zone(lifts[k], s.x, s.z, s.heading, s.speed)) {
      wait_for_lift(state, i, static_cast<int32_t>(k));
      return;
    }
  }
}

void step_lift(GameState& state, int32_t i) {
  Racer& r = state.racers[static_cast<size_t>(i)];
  OnLift& l = r.free.lift;
  const ml::LiftPlan& plan = (*state.lifts)[static_cast<size_t>(l.lift)];
  const double t = state.t + kDt;
  if (l.phase == LiftPhase::waiting) {
    const int32_t k = ml::carrier_passing(plan, ml::load_at(plan), state.t, kDt);
    if (k < 0) return;
    l.phase = LiftPhase::carried;
    l.carrier = k;
    l.since = t;
  }
  const ml::LoopSpot spot = ml::carrier_at(plan, l.carrier, t);
  if (spot.side != 0 || spot.turn >= 0 || spot.u >= ml::off_at(plan)) {
    let_go(state, i, plan);
    return;
  }
  const ml::Place at = ml::carrier_place(plan, spot);
  r.skier = sk::stand_skier(state.world(), at.x, at.z, at.heading,
                            ml::carrier_speed_at(plan, spot.u, spot.side));
}

}  // namespace fall_line::sim
