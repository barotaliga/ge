// The headless run and the replay (simulate.hpp).
//
// ge:summary The Driver (one taped, hashed step), simulate, simulate_on and replay_tape
// ge:keywords simulate replay_tape loop pilot tape driver rivals
// ge:skill sim
// ge:test tests/fall_line_sim_test.cpp
#include "fall_line/sim/simulate.hpp"

#include <array>

#include "core/math.hpp"
#include "fall_line/sim/controls.hpp"
#include "fall_line/sim/field.hpp"
#include "fall_line/sim/free.hpp"
#include "fall_line/sim/game.hpp"
#include "fall_line/sim/outing.hpp"
#include "prof/zones.hpp"

namespace fall_line::sim {

namespace mg = ge::mountain::mapgen;
namespace num = ge::core::num;

Driver::Driver(std::shared_ptr<const mg::Level> level, int32_t rivals)
    : Driver(create_game_on(std::move(level), 1 + (rivals < 0 ? 0 : rivals))) {}

Driver::Driver(GameState state, int32_t lifts)
    : state_(std::move(state)),
      plan_(state_.mode == Mode::free
                ? plan_free(*state_.level, state_.racers[0].free.start, state_.spec)
                : plan_bot(*state_.level, state_.spec)),
      recorder_(kTapeSchema),
      controls_(state_.racers.size()),
      shoulders_(state_.racers.size(), 0),
      lifts_(lifts) {}

void Driver::step(Controls player) {
  std::array<double, 4> axes{};
  controls_to_axes(player, axes);
  {
    GE_ZONE("tape");
    recorder_.record(axes);
  }
  controls_[0] = controls_from_axes(axes);
  {
    GE_ZONE("bot");
    for (size_t i = 1; i < controls_.size(); i++) {
      const auto at = static_cast<int32_t>(i);
      controls_[i] =
          bot_controls(state_, at, plan_, why_ ? why_->follow(at, state_.steps) : nullptr);
    }
  }
  fall_line::sim::step(state_, controls_);
  {
    GE_ZONE("hash");
    hash_ = state_hash(state_, hash_);
  }
  top_speed_ = num::max(top_speed_, state_.racers[0].skier.speed);
  for (const GameEvent& e : state_.events) {
    if (e.kind == EventKind::shoulder) {
      shoulders_[static_cast<size_t>(e.racer)]++;
      shoulders_[static_cast<size_t>(e.gate)]++;
    }
    if (e.racer != 0) continue;
    if (e.kind == EventKind::boarded) boarded_at_ = state_.t;
    if (e.kind == EventKind::unloaded) {
      lift_ride_ += e.value;
      lift_wait_ += state_.t - e.value - boarded_at_;
    }
    if (e.kind == EventKind::landing) {
      flights_++;
      max_landing_g_ = num::max(max_landing_g_, e.value);
    }
  }
}

bool Driver::over() const {
  if (state_.mode == Mode::free) {
    const FreeRide& f = state_.racers[0].free;
    return f.arrived && f.lift.ridden >= lifts_;
  }
  for (const Racer& r : state_.racers) {
    if (!r.progress.finished) return false;
  }
  return true;
}

RunReport Driver::report(double seed) const {
  RunReport rep;
  const Racer& me = state_.racers[0];
  rep.seed = seed;
  rep.gate_count = static_cast<int32_t>(state_.level->checkpoints.size());
  rep.finished = me.progress.finished;
  rep.time = rep.finished ? me.progress.finish_time : state_.t;
  rep.gates = me.progress.next;
  rep.missed = me.progress.missed_gates;
  rep.resets = me.progress.resets;
  rep.wipeouts = me.progress.wipeouts;
  rep.flights = flights_;
  rep.top_speed = top_speed_;
  rep.max_landing_g = max_landing_g_;
  rep.mean_speed = rep.finished && rep.time > 0 ? state_.level->track.length / rep.time : 0;
  rep.place = place_of(state_, 0);
  rep.steps = state_.steps;
  rep.digest = hash_;
  rep.tape = recorder_.seal();
  if (state_.mode == Mode::free) {
    const FreeRide& f = me.free;
    rep.finished = over();
    rep.time = rep.finished ? f.arrived_at : state_.t;
    rep.gates = 0;
    rep.distance = f.distance;
    rep.runs = static_cast<int32_t>(f.skied.size());
    rep.lifts = f.lift.ridden;
    rep.lift_wait = lift_wait_;
    rep.lift_ride = lift_ride_;
    rep.mean_speed = rep.time > 0 ? f.distance / rep.time : 0;
  }
  for (size_t i = 0; i < state_.racers.size(); i++) {
    const Racer& r = state_.racers[i];
    const Progress& p = r.progress;
    rep.field.push_back(FieldRow{r.rider.pace, r.rider.lane, p.finished,
                                 p.finished ? p.finish_time : state_.t,
                                 place_of(state_, static_cast<int32_t>(i)), p.next, p.missed_gates,
                                 p.resets, p.wipeouts, shoulders_[i]});
  }
  return rep;
}

namespace {

// THE ONE LOOP every headless pilot drives: the player's controls through the `Driver`, until
// the whole field is home or the limit; then the report.
template <class Pilot>
RunReport drive(Driver d, double seed, double max_seconds, Pilot&& pilot,
                std::vector<std::vector<LinePoint>>* lines) {
  const auto limit = static_cast<int64_t>(num::round(max_seconds * kHz));
  const auto every = static_cast<int64_t>(num::round(kHz / 10));
  if (lines) lines->assign(d.state().racers.size(), {});
  while (d.state().steps < limit && !d.over()) {
    if (lines && d.state().steps % every == 0) {
      for (size_t i = 0; i < d.state().racers.size(); i++) {
        const Racer& r = d.state().racers[i];
        if (r.progress.finished) continue;
        (*lines)[i].push_back(LinePoint{r.skier.x, r.skier.z, r.skier.speed, r.skier.on_snow,
                                        r.skier.down, r.free.lift.phase != LiftPhase::none});
      }
    }
    d.step(pilot(d));
  }
  return d.report(seed);
}

}  // namespace

RunReport simulate(double seed, const SimOptions& opts) {
  return simulate_on(std::make_shared<const mg::Level>(race_map(seed, opts.version)), seed, opts);
}

RunReport simulate_on(std::shared_ptr<const mg::Level> level, double seed, const SimOptions& opts,
                      std::vector<std::vector<LinePoint>>* lines) {
  return drive(
      Driver(std::move(level), opts.rivals), seed, opts.max_seconds,
      [](const Driver& d) { return bot_controls(d.state(), 0, d.plan()); }, lines);
}

RunReport simulate_free(std::shared_ptr<const mg::Level> level, double seed, int32_t run,
                        const SimOptions& opts, std::vector<std::vector<LinePoint>>* lines) {
  return drive(
      Driver(create_free_ride(std::move(level), run)), seed, opts.max_seconds,
      [](const Driver& d) { return bot_controls(d.state(), 0, d.plan()); }, lines);
}

RunReport simulate_outing(std::shared_ptr<const mg::Level> level, double seed, int32_t run,
                          const SimOptions& opts, std::vector<std::vector<LinePoint>>* lines) {
  GameState state = create_free_ride(std::move(level), run);
  const Outing o = outing_from(*state.level, *state.lifts, run);
  if (o.lift < 0) return Driver(std::move(state), 1).report(seed);
  const OutingPilot pilot(state, o);
  return drive(
      Driver(std::move(state), 1), seed, opts.max_seconds,
      [&pilot](const Driver& d) { return pilot(d.state(), 0); }, lines);
}

RunReport replay_tape(std::shared_ptr<const mg::Level> level, double seed,
                      const ge::racing::ControlTape& tape, int32_t rivals) {
  const ge::racing::TapeReader reader(tape, kTapeSchema);
  std::array<double, 4> axes{};
  return drive(
      Driver(std::move(level), rivals), seed, static_cast<double>(tape.steps) / kHz,
      [&](const Driver& d) {
        if (!reader.at(d.state().steps, axes)) return Controls{};
        return controls_from_axes(axes);
      },
      nullptr);
}

}  // namespace fall_line::sim
