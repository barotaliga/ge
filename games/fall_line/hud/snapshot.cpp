// Fall Line's HUD snapshot defined: the state read into figures, the events into news.
//
// ge:summary The HUD snapshot defined: the figures read off the state, the news off the events
// ge:keywords hud snapshot note_step news events wanted-tile best-air prompt machine
// ge:skill ui sim
// ge:test tests/fall_line_hud_test.cpp
#include "fall_line/hud/snapshot.hpp"

#include <algorithm>

#include "core/math.hpp"
#include "fall_line/hud/words.hpp"
#include "fall_line/sim/heli_ride.hpp"
#include "fall_line/sim/lift_ride.hpp"
#include "fall_line/sim/sled_ride.hpp"
#include "fall_line/sim/wanted.hpp"
#include "mountain/air/air.hpp"
#include "wind/field.hpp"

namespace fall_line::hud {

namespace {

namespace mg = ge::mountain::mapgen;

constexpr double kKmh = 3.6;
// Where the wind meter asks for the air he feels: his body's middle over the snow, m.
constexpr double kBodyHeight = 1.2;

const mg::Run* run_at(const sim::GameState& state, int32_t index) {
  if (index < 0 || !state.level) return nullptr;
  const auto& resort = state.level->resort;
  if (!resort) return nullptr;
  const auto& runs = resort->runs;
  return static_cast<size_t>(index) < runs.size() ? &runs[static_cast<size_t>(index)] : nullptr;
}

bool hunted(const sim::WantedState& w) {
  return w.active && (w.stars > 0 || w.ended != sim::WantedEnd::running);
}

// How long a hunt's tile stays up after it ended, s: long enough to read how.
constexpr double kEndedHold = 5;

}  // namespace

void note_step(HudRun& run, const sim::GameState& state, const NewsInk& ink) {
  const double t = state.t;
  for (const sim::GameEvent& e : state.events) {
    if (e.racer != 0) continue;
    switch (e.kind) {
      case sim::EventKind::landing:
        // Only a flight the air clock showed is news; it may set the run's best.
        if (run.flight >= kAirShown) {
          const bool best = run.flight > run.best_air;
          run.news.push(words::landed(run.flight, e.value), best ? ink.good : ink.plain, t);
          run.best_air = std::max(run.best_air, run.flight);
        }
        break;
      case sim::EventKind::wipeout:
        run.news.push(words::wipeout(static_cast<ge::skier::Wipeout>(e.gate)), ink.bad, t);
        break;
      case sim::EventKind::glance:
        run.news.push(words::kTree, ink.bad, t);
        break;
      case sim::EventKind::onto:
        if (const mg::Run* r = run_at(state, e.gate)) run.news.push(words::onto(*r), ink.plain, t);
        break;
      case sim::EventKind::arrived:
        if (const mg::Run* r = run_at(state, e.gate))
          run.news.push(words::arrived(*r), ink.good, t);
        break;
      case sim::EventKind::unloaded:
        run.news.push(words::lift_top(e.value), ink.plain, t);
        break;
      case sim::EventKind::wanted: {
        const sim::WantedChange c = sim::wanted_change(e);
        const bool good = c == sim::WantedChange::lost || c == sim::WantedChange::lowered;
        if (c == sim::WantedChange::seen || c == sim::WantedChange::searching) break;
        run.news.push(words::wanted(c, e.gate), good ? ink.good : ink.bad, t);
        break;
      }
      default:
        break;
    }
  }
  const ge::skier::SkierState& sk = state.racers[0].skier;
  run.flight = sk.on_snow ? 0 : sk.air_time;
  run.news.expire(t);
}

HudSnapshot snapshot(const sim::GameState& state, const HudRun& run) {
  HudSnapshot s;
  const sim::Racer& me = state.racers.front();
  const ge::skier::SkierState& sk = me.skier;
  s.t = state.t;
  s.seed = state.level ? state.level->seed : 0;
  s.x = sk.x, s.z = sk.z;
  s.speed_kmh = sk.speed * kKmh;
  s.edge = state.spec.edge_max > 0 ? ge::core::clamp(sk.edge / state.spec.edge_max, -1, 1) : 0;
  s.tuck = sk.tuck;
  s.heading = sk.heading;
  if (state.level) {
    const ge::mountain::mapgen::Level& l = *state.level;
    const ge::mountain::air::Wind open = ge::mountain::air::wind_at(l, state.t);
    s.weather_x = open.x, s.weather_z = open.z;
    const ge::wind::Air here =
        ge::wind::wind_at(ge::mountain::air::field_of(l), {sk.x, sk.y, sk.z, kBodyHeight}, state.t);
    s.felt_x = here.x - sk.vx, s.felt_z = here.z - sk.vz;
  } else {
    s.felt_x = -sk.vx, s.felt_z = -sk.vz;
  }
  s.altitude_m = sk.y;
  s.skied_m = me.free.distance;
  s.best_air_s = run.best_air;
  if (!sk.on_snow && sk.air_time >= kAirShown) {
    s.air_s = sk.air_time;
    s.air_best = sk.air_time > run.best_air;
  }
  if (const mg::Run* r = run_at(state, me.free.run)) s.run = words::run_name(*r);

  if (sim::on_lift(state, 0)) {
    s.machine = Machine::lift;
  } else if (sim::on_sled(state, 0)) {
    s.machine = Machine::sled;
    s.machine_kmh = state.sled.body.speed * kKmh;
  } else if (sim::on_heli(state, 0)) {
    const auto& b = state.heli.body;
    s.machine = Machine::heli;
    s.machine_kmh = ge::core::hypot(b.vx, b.vz) * kKmh;
    s.machine_height = b.agl;
  } else if (state.para.present && state.para.body.flying) {
    s.machine = Machine::para;
    s.machine_height = state.para.body.agl;
  }

  const sim::WantedState& w = state.wanted;
  if (hunted(w) && !(w.ended != sim::WantedEnd::running && state.t - w.stopped > kEndedHold)) {
    WantedTile tile;
    tile.stars = w.stars;
    tile.of = sim::wanted::kMaxStars;
    tile.flashing = w.ended == sim::WantedEnd::running && w.stars > 0 && !w.seen;
    const double lose = sim::wanted::kLoseS[static_cast<size_t>(std::clamp(w.stars, 0, 5))];
    if (tile.flashing && lose > 0) tile.search = std::max(0.0, 1 - w.unseen / lose);
    tile.hidden = tile.flashing && sim::under_cover(state);
    tile.survived = sim::survived_of(w, state.t);
    tile.ended = w.ended;
    s.wanted = tile;
  }

  if (me.body.dead >= 0 || w.ended == sim::WantedEnd::killed) {
    s.prompt = Prompt::died;
  } else if (w.ended == sim::WantedEnd::arrested) {
    s.prompt = Prompt::arrested;
  } else if (s.machine == Machine::skis && sk.down) {
    s.prompt = Prompt::down;
  } else if (s.machine == Machine::skis && me.progress.stuck >= kStuckShown) {
    s.prompt = Prompt::stuck;
  } else if (me.free.lift.phase == sim::LiftPhase::waiting) {
    s.prompt = Prompt::waiting;
  }
  return s;
}

}  // namespace fall_line::hud
