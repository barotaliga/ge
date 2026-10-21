// The run's create and its step (game.hpp has the order).
//
// ge:summary create_game, step and state_hash's bodies
// ge:keywords game create_game step state_hash
// ge:skill sim
// ge:test tests/fall_line_sim_test.cpp
#include "fall_line/sim/game.hpp"

#include <cstring>

#include "fall_line/sim/course.hpp"
#include "fall_line/sim/field.hpp"
#include "mountain/mapgen/generate.hpp"
#include "skier/skier.hpp"

namespace fall_line::sim {

namespace mg = ge::mountain::mapgen;
namespace sk = ge::skier;

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
  state.events.clear();
  const sk::World world = state.world();
  for (int32_t i = 0; i < static_cast<int32_t>(state.racers.size()); i++) {
    Racer& r = state.racers[static_cast<size_t>(i)];
    Progress& p = r.progress;
    const Controls c =
        static_cast<size_t>(i) < controls.size() ? controls[static_cast<size_t>(i)] : Controls{};
    if (c.reset && !p.finished) reset_racer(state, i);
    sk::SkierInput in = c.ski;
    if (p.finished) in = sk::SkierInput{0, 0, 1, false};
    const double x0 = r.skier.x;
    const double z0 = r.skier.z;
    sk::SkierEvents ev;
    sk::step_skier(r.skier, in, world, state.spec, kDt, ev);
    step_course(state, i, x0, z0);
    if (ev.took_off) state.events.push_back(GameEvent{EventKind::takeoff, i, p.next, 0});
    if (ev.landed) state.events.push_back(GameEvent{EventKind::landing, i, p.next, ev.landing_g});
    if (ev.glanced) state.events.push_back(GameEvent{EventKind::glance, i, ev.tree, 0});
    if (ev.wiped_out) {
      p.wipeouts++;
      state.events.push_back(
          GameEvent{EventKind::wipeout, i, static_cast<int32_t>(ev.cause), r.skier.speed});
    }
    if (p.finished) continue;
    p.stuck = !r.skier.down && r.skier.speed < kStuckSpeed ? p.stuck + kDt : 0;
    if ((r.skier.down && r.skier.down_time > kDownReset) || p.stuck > kStuckReset) {
      reset_racer(state, i);
    }
  }
  meet_racers(state);
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
  }
  return h;
}

}  // namespace fall_line::sim
