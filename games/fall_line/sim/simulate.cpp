// The headless run and the replay (simulate.hpp).
//
// ge:summary simulate, simulate_on and replay_tape: one loop, two pilots
// ge:keywords simulate replay_tape loop pilot tape
// ge:skill sim
// ge:test tests/fall_line_sim_test.cpp
#include "fall_line/sim/simulate.hpp"

#include <array>

#include "core/math.hpp"
#include "fall_line/sim/bot.hpp"
#include "fall_line/sim/controls.hpp"
#include "fall_line/sim/game.hpp"

namespace fall_line::sim {

namespace mg = ge::mountain::mapgen;
namespace num = ge::core::num;

namespace {

// THE ONE LOOP every pilot drives: controls -> snapped -> taped -> step, until the player
// finishes or the limit; then the report.
template <class Pilot>
RunReport drive(std::shared_ptr<const mg::Level> level, double seed, double max_seconds,
                Pilot&& pilot, std::vector<LinePoint>* line) {
  GameState state = create_game_on(std::move(level), 1);
  ge::racing::TapeRecorder recorder(kTapeSchema);
  RunReport rep;
  rep.seed = seed;
  rep.gate_count = static_cast<int32_t>(state.level->checkpoints.size());
  uint64_t h = 0xcbf29ce484222325ull;
  const auto limit = static_cast<int64_t>(num::round(max_seconds * kHz));
  const auto every = static_cast<int64_t>(num::round(kHz / 10));
  std::array<double, 4> axes{};
  while (state.steps < limit && !state.racers[0].progress.finished) {
    const Racer& me = state.racers[0];
    if (line && state.steps % every == 0) {
      line->push_back(
          LinePoint{me.skier.x, me.skier.z, me.skier.speed, me.skier.on_snow, me.skier.down});
    }
    Controls c = pilot(state);
    controls_to_axes(c, axes);
    recorder.record(axes);
    c = controls_from_axes(axes);
    step(state, std::span<const Controls>(&c, 1));
    h = state_hash(state, h);
    rep.top_speed = num::max(rep.top_speed, state.racers[0].skier.speed);
    for (const GameEvent& e : state.events) {
      if (e.racer != 0) continue;
      if (e.kind == EventKind::landing) {
        rep.flights++;
        rep.max_landing_g = num::max(rep.max_landing_g, e.value);
      }
    }
  }
  const Racer& me = state.racers[0];
  rep.finished = me.progress.finished;
  rep.time = rep.finished ? me.progress.finish_time : state.t;
  rep.gates = me.progress.next;
  rep.missed = me.progress.missed_gates;
  rep.resets = me.progress.resets;
  rep.wipeouts = me.progress.wipeouts;
  rep.mean_speed = rep.finished && rep.time > 0 ? state.level->track.length / rep.time : 0;
  rep.steps = state.steps;
  rep.digest = h;
  rep.tape = recorder.seal();
  return rep;
}

}  // namespace

RunReport simulate(double seed, const SimOptions& opts) {
  return simulate_on(std::make_shared<const mg::Level>(race_map(seed, opts.version)), seed, opts);
}

RunReport simulate_on(std::shared_ptr<const mg::Level> level, double seed, const SimOptions& opts,
                      std::vector<LinePoint>* line) {
  const BotPlan plan = plan_bot(*level, ge::skier::kReferenceSkis);
  return drive(
      std::move(level), seed, opts.max_seconds,
      [&plan](const GameState& s) { return bot_controls(s, 0, plan); }, line);
}

RunReport replay_tape(std::shared_ptr<const mg::Level> level, double seed,
                      const ge::racing::ControlTape& tape) {
  const ge::racing::TapeReader reader(tape, kTapeSchema);
  std::array<double, 4> axes{};
  return drive(
      std::move(level), seed, static_cast<double>(tape.steps) / kHz,
      [&](const GameState& s) {
        if (!reader.at(s.steps, axes)) return Controls{};
        return controls_from_axes(axes);
      },
      nullptr);
}

}  // namespace fall_line::sim
