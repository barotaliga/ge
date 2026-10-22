// THE STATE OF A RUN: the map, the field of racers on it (each a skier and his progress down the
// course), the run's clock and this step's events. `create_game` builds it (game.hpp) and
// `step` is the only thing that writes it during a run; a pilot, a lab or a renderer reads it.
//
// Every buffer a step writes is sized at create time: `racers` never grows, and `events` is
// reserved for `kEventsReserve` a step, so a step allocates nothing.
//
// ge:summary The state of a run: GameState, Racer, Progress, Controls, the events
// ge:keywords game-state state racer progress controls events event-kind gate missed finish
// ge:keywords reset wipeout landing takeoff field clock wanted
// ge:skill sim
// ge:test tests/fall_line_sim_test.cpp
#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "mountain/mapgen/types.hpp"
#include "skier/skier.hpp"
#include "skier/skis.hpp"
#include "skier/trees.hpp"

#include "fall_line/sim/wanted_state.hpp"

namespace fall_line::sim {

// THE CLOCK every run is stepped on: 120 steps a second (the edge and the landing's contact
// want it, and every pilot decides on every step).
inline constexpr double kHz = 120;
inline constexpr double kDt = 1 / kHz;

// What a racer is asked this step: the skis' controls, and the reset (stand me back on the
// piste past the last gate I took).
struct Controls {
  ge::skier::SkierInput ski;
  bool reset = false;
};

enum class EventKind : uint8_t {
  gate,      // a gate taken (`gate` its index; the start gate is 0)
  missed,    // the owed gate skied past outside its width
  finish,    // the finish line crossed (`value` the run's time, s)
  takeoff,   // a flight began (it passed the skier's least flight)
  landing,   // back on the snow (`value` the landing's load, g)
  wipeout,   // went down (`gate` the skier's Wipeout as a number)
  glance,    // brushed a trunk and kept going
  reset,     // stood back on the piste (`gate` the gate owed)
  shoulder,  // two racers met (`racer` and `gate` the two, `value` the closing speed)
  wanted,    // the wanted level moved (`gate` the stars after, `value` its WantedChange)
};

const char* event_name(EventKind kind);

struct GameEvent {
  EventKind kind;
  int32_t racer;
  int32_t gate;
  double value;
};

inline constexpr size_t kEventsReserve = 64;

// How far down the course a racer is.
struct Progress {
  int32_t next = 0;     // the gate owed: 0 the start gate ... the last the finish line
  bool missed = false;  // the owed gate was skied past; nothing counts until it is taken
  bool finished = false;
  double finish_time = 0;  // s, the clock at the line, to the share of the step it was crossed
  int32_t resets = 0;
  int32_t wipeouts = 0;
  int32_t missed_gates = 0;  // gates skied past (each counted once)
  double stuck = 0;          // s going nowhere on his skis
};

// How a racer rides when the bot has his controls (field.hpp deals it): the share of the bot's
// speed plan he dares, and how far right of the piste's centre he holds his line, m.
struct Rider {
  double pace = 1;
  double lane = 0;
};

struct Racer {
  ge::skier::SkierState skier;
  Progress progress;
  Rider rider;
};

struct GameState {
  std::shared_ptr<const ge::mountain::mapgen::Level> level;
  ge::skier::TreeIndex trees;
  ge::skier::SkiSpec spec;
  std::vector<Racer> racers;      // the player's first; sized at create
  std::vector<GameEvent> events;  // this step's, cleared at its start
  WantedState wanted;             // the hunt for the player (wanted.hpp); inactive until raised
  double t = 0;                   // s since the start
  int64_t steps = 0;

  ge::skier::World world() const { return ge::skier::World{level.get(), &trees}; }
};

}  // namespace fall_line::sim
