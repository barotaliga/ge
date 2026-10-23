// The free ride's paramotor (para_ride.hpp).
//
// ge:summary para_start, create_para_ride, the press, the held skis, the rig's step, the restart
// ge:keywords para paramotor para_start create_para_ride start_para para_press para_held
// ge:keywords step_para_ride restart_para events
// ge:skill sim paramotor
// ge:test tests/fall_line_para_test.cpp
#include "fall_line/sim/para_ride.hpp"

#include "core/math.hpp"
#include "fall_line/sim/free.hpp"
#include "paramotor/air.hpp"

namespace fall_line::sim {

namespace mg = ge::mountain::mapgen;
namespace pm = ge::paramotor;
namespace sk = ge::skier;

namespace {

void say(GameState& state, int32_t i, ParaPhase phase) {
  state.events.push_back(GameEvent{EventKind::para, i, static_cast<int32_t>(phase),
                                   state.racers[static_cast<size_t>(i)].skier.speed});
}

ParaPhase cut_phase(pm::ParaCut cut) {
  switch (cut) {
    case pm::ParaCut::ground:
      return ParaPhase::ground;
    case pm::ParaCut::crown:
      return ParaPhase::crown;
    case pm::ParaCut::rider:
      return ParaPhase::rider;
    case pm::ParaCut::drop:
    case pm::ParaCut::none:
      break;
  }
  return ParaPhase::drop;
}

void say_all(GameState& state, int32_t i, const pm::ParaEvents& ev) {
  if (ev.launched) say(state, i, ParaPhase::launch);
  if (ev.took_off) say(state, i, ParaPhase::take_off);
  if (ev.touched) say(state, i, ParaPhase::touch);
  if (ev.folded) say(state, i, ParaPhase::fold);
  if (ev.looped) say(state, i, ParaPhase::loop);
  if (ev.tangled) say(state, i, ParaPhase::tangle);
  if (ev.cut != pm::ParaCut::none) say(state, i, cut_phase(ev.cut));
}

}  // namespace

ParaStart para_start(const std::vector<mg::Track>& runs) {
  ParaStart best;
  double best_y = 0;
  for (size_t k = 0; k < runs.size(); k++) {
    if (runs[k].points.empty()) continue;
    const mg::TrackPoint& head = runs[k].points.front();
    if (k == 0 || head.y > best_y) {
      best = ParaStart{static_cast<int32_t>(k), head.x, head.z, head.heading};
      best_y = head.y;
    }
  }
  return best;
}

GameState create_para_ride(std::shared_ptr<const mg::Level> level) {
  const std::shared_ptr<const std::vector<mg::Track>> runs = run_tracks(*level);
  GameState state = create_free_ride(std::move(level), para_start(*runs).run);
  FreePara& m = state.para;
  const ParaStart at = para_start(*state.runs);
  m.present = true;
  m.field = pm::make_air_field(state.level->seed);
  m.start_x = at.x;
  m.start_z = at.z;
  m.start_heading = at.heading;
  start_para(state);
  state.events.clear();
  return state;
}

void start_para(GameState& state) {
  FreePara& m = state.para;
  Racer& r = state.racers[0];
  r.skier = sk::stand_skier(state.world(), m.start_x, m.start_z, m.start_heading, 0);
  r.progress.stuck = 0;
  m.body = pm::ready_para(r.skier);
  m.starts++;
}

pm::ParaControls para_controls(const Controls& c) {
  return pm::ParaControls{
      ge::core::clamp(c.ski.tuck, 0.0, 1.0), ge::core::clamp(c.ski.brake, 0.0, 1.0),
      ge::core::clamp(c.ski.steer, -1.0, 1.0), ge::core::clamp(-c.lean, -1.0, 1.0)};
}

pm::ParaWorld para_world(const GameState& state) {
  return pm::ParaWorld{state.level.get(), &state.trees, &state.para.field,
                       pm::level_wind(*state.level), state.t};
}

void para_press(GameState& state, int32_t i, Controls& c) {
  if (!under_para(state, i) || !c.machine) return;
  c.machine = false;
  pm::ParaEvents ev;
  pm::drop_para(state.para.body, state.racers[static_cast<size_t>(i)].skier, pm::ParaCut::drop, ev);
  say_all(state, i, ev);
}

sk::SkierInput para_held(const GameState& state, int32_t i, const sk::SkierInput& in) {
  return hung_in_para(state, i) ? sk::SkierInput{} : in;
}

void step_para_ride(GameState& state, int32_t i, const Controls& c) {
  if (i != 0 || !state.para.present) return;
  pm::ParaEvents ev;
  pm::step_para(state.para.body, state.racers[0].skier, para_controls(c), para_world(state),
                state.spec, kDt, ev);
  say_all(state, i, ev);
}

bool restart_para(GameState& state, int32_t i) {
  if (!under_para(state, i)) return false;
  start_para(state);
  say(state, i, ParaPhase::restart);
  return true;
}

const char* para_phase_name(ParaPhase p) {
  switch (p) {
    case ParaPhase::launch:
      return "launch";
    case ParaPhase::take_off:
      return "take-off";
    case ParaPhase::touch:
      return "touch";
    case ParaPhase::fold:
      return "fold";
    case ParaPhase::drop:
      return "drop";
    case ParaPhase::ground:
      return "ground";
    case ParaPhase::crown:
      return "crown";
    case ParaPhase::rider:
      return "rider";
    case ParaPhase::restart:
      return "restart";
    case ParaPhase::loop:
      return "loop";
    case ParaPhase::tangle:
      return "tangle";
  }
  return "?";
}

}  // namespace fall_line::sim
