// The run's create and its step (game.hpp has the order).
//
// ge:summary create_game, step and state_hash's bodies
// ge:keywords game create_game step state_hash log channel course skier field log_step wanted
// ge:skill sim
// ge:test tests/fall_line_sim_test.cpp
#include "fall_line/sim/game.hpp"

#include <cstring>
#include <string>

#include "core/num_text.hpp"
#include "core/output.hpp"

#include "fall_line/sim/course.hpp"
#include "fall_line/sim/field.hpp"
#include "fall_line/sim/free.hpp"
#include "fall_line/sim/lift_ride.hpp"
#include "fall_line/sim/wanted.hpp"
#include "mountain/mapgen/generate.hpp"
#include "prof/zones.hpp"
#include "skier/skier.hpp"

namespace fall_line::sim {

namespace mg = ge::mountain::mapgen;
namespace sk = ge::skier;
namespace out = ge::core::output;

namespace {

// THE RUN'S LOG (docs/debugging.md): this step's events on four channels (`course`: gates,
// misses, the finish, resets; `skier`: flights, landings, wipeouts, glances; `field`: racers
// meeting; `wanted`: the stars, seen or searching, the arrest) at debug, and every racer's state
// each step on `skier` at trace. Off by default: a dropped line costs a table read and formats
// nothing.
void log_step(const GameState& state) {
  static const out::Channel kCourse = out::channel("course");
  static const out::Channel kSkier = out::channel("skier");
  static const out::Channel kField = out::channel("field");
  static const out::Channel kWanted = out::channel("wanted");
  const auto f = [](double v, int d) { return ge::core::num::to_fixed(v, d); };
  for (const GameEvent& e : state.events) {
    const bool course = e.kind == EventKind::gate || e.kind == EventKind::missed ||
                        e.kind == EventKind::finish || e.kind == EventKind::reset ||
                        e.kind == EventKind::onto || e.kind == EventKind::arrived ||
                        e.kind == EventKind::boarded || e.kind == EventKind::unloaded;
    const out::Channel ch = e.kind == EventKind::shoulder ? kField
                            : e.kind == EventKind::wanted ? kWanted
                            : course                      ? kCourse
                                                          : kSkier;
    if (!out::enabled(ch, out::Level::debug)) continue;
    const sk::SkierState& s = state.racers[static_cast<size_t>(e.racer)].skier;
    std::string line = "racer " + std::to_string(e.racer) + " ";
    line += e.kind == EventKind::gate ? "took" : event_name(e.kind);
    switch (e.kind) {
      case EventKind::gate:
      case EventKind::missed:
        line += " gate " + std::to_string(e.gate);
        break;
      case EventKind::reset:
        line += ", owing gate " + std::to_string(e.gate);
        break;
      case EventKind::finish:
        line += " in " + f(e.value, 3) + " s";
        break;
      case EventKind::landing:
        line += " at " + f(e.value, 2) + " g, owing gate " + std::to_string(e.gate);
        break;
      case EventKind::wipeout:
        line += " (cause " + std::to_string(e.gate) + ") at " + f(e.value * 3.6, 1) + " km/h";
        break;
      case EventKind::glance:
        line += " tree " + std::to_string(e.gate);
        break;
      case EventKind::shoulder:
        line += " with racer " + std::to_string(e.gate) + " closing at " + f(e.value, 2) + " m/s";
        break;
      case EventKind::takeoff:
        line += " owing gate " + std::to_string(e.gate);
        break;
      case EventKind::wanted:
        line += " " + std::string(wanted_change_name(wanted_change(e))) + ", " +
                std::to_string(e.gate) + " stars";
        break;
      case EventKind::onto:
        line += " run " + std::to_string(e.gate);
        break;
      case EventKind::arrived:
        line += " at the foot of run " + std::to_string(e.gate) + " in " + f(e.value, 3) + " s";
        break;
      case EventKind::boarded:
        line += " lift " + std::to_string(e.gate);
        break;
      case EventKind::unloaded:
        line += " at the top of lift " + std::to_string(e.gate) + " after " + f(e.value, 1) + " s";
        break;
    }
    line += " at x " + f(s.x, 1) + " z " + f(s.z, 1) + ", " + f(s.speed * 3.6, 1) + " km/h";
    out::log_on(ch, out::Level::debug, line);
  }
  if (!out::enabled(kSkier, out::Level::trace)) return;
  for (size_t i = 0; i < state.racers.size(); i++) {
    const sk::SkierState& s = state.racers[i].skier;
    out::log_on(kSkier, out::Level::trace,
                "racer " + std::to_string(i) + " x " + f(s.x, 2) + " z " + f(s.z, 2) + " speed " +
                    f(s.speed * 3.6, 2) + " km/h edge " + f(s.edge, 3) + " tuck " + f(s.tuck, 2) +
                    " lean " + f(s.lean, 3) + " g_lat " + f(s.g_lat, 2) +
                    (s.on_snow ? "" : " air") + (s.down ? " down" : ""));
  }
}

// The reset the run's mode makes.
void reset(GameState& state, int32_t i) {
  if (state.mode == Mode::free) {
    reset_free(state, i);
  } else {
    reset_racer(state, i);
  }
}

}  // namespace

const char* event_name(EventKind kind) {
  switch (kind) {
    case EventKind::gate:
      return "gate";
    case EventKind::missed:
      return "missed";
    case EventKind::finish:
      return "finish";
    case EventKind::takeoff:
      return "takeoff";
    case EventKind::landing:
      return "landing";
    case EventKind::wipeout:
      return "wipeout";
    case EventKind::glance:
      return "glance";
    case EventKind::reset:
      return "reset";
    case EventKind::shoulder:
      return "shoulder";
    case EventKind::wanted:
      return "wanted";
    case EventKind::onto:
      return "onto";
    case EventKind::arrived:
      return "arrived";
    case EventKind::boarded:
      return "boarded";
    case EventKind::unloaded:
      return "unloaded";
  }
  return "?";
}

mg::Level race_map(double seed, std::optional<int> version) {
  mg::GenerateOptions opts;
  opts.version = version.value_or(1);
  return mg::generate_level(seed, opts);
}

GameState create_game(const GameOptions& opts) {
  return create_game_on(std::make_shared<const mg::Level>(race_map(opts.seed, opts.version)),
                        opts.racers);
}

GameState create_game_on(std::shared_ptr<const mg::Level> level, int32_t racers) {
  GameState state;
  state.level = std::move(level);
  state.trees = sk::build_tree_index(*state.level);
  state.spec = sk::kReferenceSkis;
  const auto slots = static_cast<int32_t>(state.level->grid.size());
  const int32_t most = slots < 1 ? 1 : slots;
  state.racers.resize(static_cast<size_t>(racers > most ? most : racers < 1 ? 1 : racers));
  state.events.reserve(kEventsReserve);
  state.wanted.pursuers.reserve(wanted::kMaxPursuers);
  const std::vector<Rider> field =
      deal_field(*state.level, static_cast<int32_t>(state.racers.size()));
  for (int32_t i = 0; i < static_cast<int32_t>(state.racers.size()); i++) {
    reset_racer(state, i);
    state.racers[static_cast<size_t>(i)].progress = Progress{};
    state.racers[static_cast<size_t>(i)].rider = field[static_cast<size_t>(i)];
  }
  state.events.clear();
  return state;
}

void step(GameState& state, std::span<const Controls> controls) {
  out::set_step(state.steps);
  state.events.clear();
  const sk::World world = state.world();
  for (int32_t i = 0; i < static_cast<int32_t>(state.racers.size()); i++) {
    Racer& r = state.racers[static_cast<size_t>(i)];
    Progress& p = r.progress;
    const Controls c =
        static_cast<size_t>(i) < controls.size() ? controls[static_cast<size_t>(i)] : Controls{};
    if (c.reset && !p.finished) reset(state, i);
    // A lift that has him carries him: his skis, his run and the run's resets wait.
    if (state.mode == Mode::free && on_lift(state, i)) {
      step_lift(state, i);
      continue;
    }
    sk::SkierInput in = c.ski;
    if (p.finished) in = sk::SkierInput{0, 0, 1, false};
    const double x0 = r.skier.x;
    const double z0 = r.skier.z;
    sk::SkierEvents ev;
    {
      GE_ZONE("skier");
      sk::step_skier(r.skier, in, world, state.spec, kDt, ev);
    }
    if (state.mode == Mode::free) {
      GE_ZONE("free");
      step_free(state, i, x0, z0);
      board_lift(state, i);
    } else {
      GE_ZONE("course");
      step_course(state, i, x0, z0);
    }
    if (ev.took_off) state.events.push_back(GameEvent{EventKind::takeoff, i, p.next, 0});
    if (ev.landed) state.events.push_back(GameEvent{EventKind::landing, i, p.next, ev.landing_g});
    if (ev.glanced) state.events.push_back(GameEvent{EventKind::glance, i, ev.tree, 0});
    if (ev.wiped_out) {
      p.wipeouts++;
      state.events.push_back(
          GameEvent{EventKind::wipeout, i, static_cast<int32_t>(ev.cause), r.skier.speed});
    }
    if (p.finished) continue;
    // Going nowhere while asking to go. A race's racer always is; on the free ride a skier
    // standing still to look at the view has his poles still, and is left there.
    const bool asks = state.mode != Mode::free || in.tuck > 0;
    p.stuck = !r.skier.down && asks && r.skier.speed < kStuckSpeed ? p.stuck + kDt : 0;
    if ((r.skier.down && r.skier.down_time > kDownReset) || p.stuck > kStuckReset) {
      reset(state, i);
    }
  }
  {
    GE_ZONE("meet");
    meet_racers(state);
  }
  {
    GE_ZONE("wanted");
    step_wanted(state);
  }
  log_step(state);
  state.steps++;
  state.t = static_cast<double>(state.steps) * kDt;
}

uint64_t state_hash(const GameState& state, uint64_t h) {
  const auto mix = [&h](double v) {
    uint64_t u;
    std::memcpy(&u, &v, sizeof u);
    for (int b = 0; b < 64; b += 8) {
      h ^= (u >> b) & 0xffu;
      h *= 0x100000001b3ull;
    }
  };
  mix(state.t);
  for (const Racer& r : state.racers) {
    h = sk::skier_hash(r.skier, h);
    const Progress& p = r.progress;
    mix(p.next);
    mix(p.missed ? 1 : 0);
    mix(p.finished ? 1 : 0);
    mix(p.finish_time);
    mix(p.resets);
    mix(p.wipeouts);
    mix(p.missed_gates);
    mix(p.stuck);
    if (state.mode != Mode::free) continue;
    const FreeRide& f = r.free;
    mix(f.run);
    mix(f.last);
    mix(f.last_s);
    mix(static_cast<double>(f.skied.size()));
    for (const int32_t k : f.skied) mix(k);
    mix(f.distance);
    mix(f.arrived ? 1 : 0);
    mix(f.arrived_at);
    mix(static_cast<double>(f.lift.phase));
    mix(f.lift.lift);
    mix(f.lift.carrier);
    mix(f.lift.since);
    mix(f.lift.ridden);
  }
  return wanted_hash(state.wanted, h);
}

}  // namespace fall_line::sim
