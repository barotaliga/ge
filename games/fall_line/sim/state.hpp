// THE STATE OF A RUN: the map, the mode (a race down the course, or the free ride over the ski
// area's runs), the field of racers on it (each a skier, his progress down the course and his
// free ride), the run's clock and this step's events. `create_game` builds it (game.hpp) and
// `step` is the only thing that writes it during a run; a pilot, a lab or a renderer reads it.
//
// Every buffer a step writes is sized at create time: `racers` never grows, `events` is
// reserved for `kEventsReserve` a step and a free ride's `skied` for every run, so a step
// allocates nothing.
//
// ge:summary The state of a run: GameState, Racer, Progress, Controls, the events
// ge:keywords game-state state racer progress controls events event-kind gate missed finish
// ge:keywords reset wipeout landing takeoff field clock wanted mode free-ride onto arrived skied
// ge:keywords lift-ride lift-phase boarded unloaded
// ge:skill sim
// ge:test tests/fall_line_sim_test.cpp
#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "mountain/lifts/lift_loop.hpp"
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
  onto,      // a free ride came onto a run (`gate` the run's index in the ski area)
  arrived,   // a free ride reached a run's foot (`gate` the run, `value` the clock, s)
  boarded,   // a free ride came into a lift's load zone (`gate` the lift's index)
  unloaded,  // a lift let him go at its top (`gate` the lift, `value` the ride's time, s)
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

// What a run is: a RACE down the course's gates to the finish, or the FREE RIDE over the ski
// area's runs (free.hpp), with no gate and no finish line.
enum class Mode : uint8_t { race, free };

// Where a free ride has been (free.hpp): the run under his skis, the last one he was on and how
// far down it, every run skied (in the order first skied), the metres skied, and the foot.
// Where a lift has him (lift_ride.hpp): not at all, waiting in its load zone for a carrier, or
// carried up the line on one.
enum class LiftPhase : uint8_t { none, waiting, carried };
struct OnLift {
  LiftPhase phase = LiftPhase::none;
  int32_t lift = -1;     // the lift's index in the ski area
  int32_t carrier = -1;  // the carrier he is on, once carried
  double since = 0;      // s, the clock when he was taken from the load zone
  int32_t ridden = 0;    // lifts ridden to the top this run
};

struct FreeRide {
  int32_t start = 0;           // the run he was stood at the head of
  int32_t run = -1;            // the run under his skis, or -1 off every run
  int32_t last = -1;           // the last run he was on, or -1 before any
  double last_s = 0;           // m down `last` he was last on it
  std::vector<int32_t> skied;  // reserved for every run at create
  double distance = 0;         // m skied over the plan, on the snow
  bool arrived = false;        // at a run's foot (since the last lift let him go)
  double arrived_at = 0;       // s, the clock when he got there
  OnLift lift;                 // the lift that has him, and the lifts ridden
};

struct Racer {
  ge::skier::SkierState skier;
  Progress progress;
  Rider rider;
  FreeRide free;
};

struct GameState {
  std::shared_ptr<const ge::mountain::mapgen::Level> level;
  Mode mode = Mode::race;
  // A free ride's runs as lines the skier is found on (free.hpp's `run_tracks`), each index built
  // at create; none in a race.
  std::shared_ptr<const std::vector<ge::mountain::mapgen::Track>> runs;
  // A free ride's lifts, planned at create (lift_loop.hpp); none in a race.
  std::shared_ptr<const std::vector<ge::mountain::lifts::LiftPlan>> lifts;
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
