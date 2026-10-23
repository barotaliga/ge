// THE HEADLESS RUN: a pilot at the controls of a run from the start to the finish (or the time
// limit), every step's controls snapped and recorded on the control tape, the whole reduced to a
// `RunReport`. With `rivals`, the bot rides each of them on the same grid (each as his dealt
// `Rider`, field.hpp) and the run goes on until the whole field is home; only the player's
// controls are taped, because the rivals' are a pure function of the run. It is what the `sim` lab
// prints a row of, what the tests hold golden, and, with a tape handed in, a replay: a run is a
// pure function of its map and its controls, so the tape played back gives the same digest.
// `simulate_free` is the bot on the free ride, from the head of a run until he reaches a foot;
// `simulate_outing` the bot's outing, down a run, up the lift at its foot and down another.
//
// ge:summary The headless run: simulate a pilot over a map, the RunReport, the tape, replay
// ge:keywords simulate sim run-report report digest tape replay headless time-limit trace
// ge:keywords simulate_on replay_tape rivals field field-row place simulate_free free-ride
// ge:keywords simulate_outing outing lifts
// ge:skill sim
// ge:test tests/fall_line_sim_test.cpp
#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include "fall_line/sim/bot.hpp"
#include "fall_line/sim/hands.hpp"
#include "fall_line/sim/state.hpp"
#include "mountain/mapgen/types.hpp"
#include "racing/tape.hpp"

namespace fall_line::sim {

struct SimOptions {
  double max_seconds = 600;                 // the run's time limit
  std::optional<int> version;               // the generator's
  int32_t rivals = 0;                       // bots on the grid beside the player
  HandsProfile hands = HandsProfile::none;  // every bot's hands, the player's and the rivals'
};

// One racer's run in a field, in field order (the player first).
struct FieldRow {
  double pace = 1;  // his Rider's
  double lane = 0;  // m
  bool finished = false;
  double time = 0;  // s, the finish's clock (or the run's end)
  int32_t place = 0;
  int32_t gates = 0;
  int32_t missed = 0;
  int32_t resets = 0;
  int32_t wipeouts = 0;
  int32_t shoulders = 0;  // times he met another racer
};

// The player's run, as the sim lab's table prints it.
struct RunReport {
  double seed = 0;
  bool finished = false;
  double time = 0;         // s, the finish's clock (or the limit)
  int32_t gates = 0;       // gates taken, the start gate and the finish among them
  int32_t gate_count = 0;  // gates on the course
  int32_t missed = 0;
  int32_t resets = 0;
  int32_t wipeouts = 0;
  int32_t flights = 0;
  double top_speed = 0;   // m/s
  double mean_speed = 0;  // m/s, the piste's length over the time (finished runs)
  double max_landing_g = 0;
  int32_t place = 1;         // the player's, at the run's end
  double distance = 0;       // m skied (the free ride's odometer; 0 in a race)
  int32_t runs = 0;          // runs skied (the free ride's)
  int32_t lifts = 0;         // lifts ridden to the top (the free ride's)
  double lift_wait = 0;      // s waiting in load zones for a carrier
  double lift_ride = 0;      // s carried up lifts
  int32_t sled_rides = 0;    // times the free ride's snowmobile was taken
  double sled_time = 0;      // s on its boards
  int32_t heli_rides = 0;    // times the free ride's helicopter was taken
  double heli_time = 0;      // s sat on its skid
  int32_t para_flights = 0;  // take-offs flown under the free ride's paramotor
  double para_time = 0;      // s hung in its harness
  double climbed = 0;        // m, the most the player rose over where he started
  int64_t steps = 0;
  uint64_t digest = 0;  // FNV-1a over every step's state
  ge::racing::ControlTape tape;
  std::vector<FieldRow> field;  // every racer, the player first
};

// A point of a racer's line, every tenth of a second, for a picture.
struct LinePoint {
  double x, z, speed;
  bool on_snow, down;
  bool lifted;  // a lift has him
  bool sled;    // on the snowmobile's boards
  bool heli;    // on the helicopter's skid
  bool para;    // hung under the paramotor's wing
};

// THE ONE STEP every pilot drives through (the bot in `simulate`, a tape in `replay_tape`, an
// agent in `PlaySession`): the player's controls snapped onto the tape's grid and recorded, the
// rivals' from the bot, `step`, the state folded into the digest, the report's tallies kept.
class Driver {
 public:
  Driver(std::shared_ptr<const ge::mountain::mapgen::Level> level, int32_t rivals);
  // A run already created (a race, or a free ride, whose bot skis his run's chain).
  // `lifts`: on the free ride, the lifts he is to ride before a foot ends the run; `sled_rides`
  // the times he is to have taken the snowmobile (and got off it) before one does, `heli_rides`
  // the helicopter. A ride begun under the paramotor ends only once he has flown it and the rig
  // is off him.
  explicit Driver(GameState state, int32_t lifts = 0, int32_t sled_rides = 0,
                  int32_t heli_rides = 0);
  void step(Controls player);
  // Every racer home (on the free ride, the player at a foot): nothing is left to drive.
  bool over() const;
  const GameState& state() const { return state_; }
  const BotPlan& plan() const { return plan_; }
  // The rivals' bots record their decisions into `why` (null: stop), each as his racer index.
  void set_why(ge::ai::Why* why) { why_ = why; }
  // The rivals press through hands of `profile` (hands.hpp), each off his own stream on `seed`'s
  // map. Set before the first step.
  void set_hands(HandsProfile profile, double seed);
  // What each racer pressed on the last step (the player first): the humanness lab reads it.
  const std::vector<Controls>& controls() const { return controls_; }
  uint64_t digest() const { return hash_; }
  ge::racing::ControlTape tape() const { return recorder_.seal(); }
  // The run so far, as the sim lab's table prints it.
  RunReport report(double seed) const;

 private:
  GameState state_;
  BotPlan plan_;
  ge::racing::TapeRecorder recorder_;
  std::vector<Controls> controls_;
  std::vector<int32_t> shoulders_;
  ge::ai::Why* why_ = nullptr;
  std::vector<BotHands> hands_;  // the rivals', by racer index (the player's is the pilot's)
  int32_t lifts_ = 0;
  int32_t sled_rides_ = 0;
  double sled_time_ = 0;
  int32_t heli_rides_ = 0;
  double heli_time_ = 0;
  double para_time_ = 0;
  double y0_ = 0;
  double climbed_ = 0;
  double boarded_at_ = 0;
  double lift_wait_ = 0;
  double lift_ride_ = 0;
  uint64_t hash_ = 0xcbf29ce484222325ull;
  double top_speed_ = 0;
  int32_t flights_ = 0;
  double max_landing_g_ = 0;
};

// The bot down the map built for `seed`.
RunReport simulate(double seed, const SimOptions& opts = {});
// The bot down `level`; `lines`, when given, gets every racer's line, the player's first.
RunReport simulate_on(std::shared_ptr<const ge::mountain::mapgen::Level> level, double seed,
                      const SimOptions& opts, std::vector<std::vector<LinePoint>>* lines = nullptr);
// The bot on the free ride on `level`, from the head of run `run` to a foot; `lines`, when
// given, gets his line.
RunReport simulate_free(std::shared_ptr<const ge::mountain::mapgen::Level> level, double seed,
                        int32_t run, const SimOptions& opts,
                        std::vector<std::vector<LinePoint>>* lines = nullptr);
// The bot's outing on `level` from the head of run `run` (outing.hpp): down to its foot, up the
// lift there and down another run to a foot; `lines`, when given, gets his line. Not finished
// where the run has no lift at its foot.
RunReport simulate_outing(std::shared_ptr<const ge::mountain::mapgen::Level> level, double seed,
                          int32_t run, const SimOptions& opts,
                          std::vector<std::vector<LinePoint>>* lines = nullptr);
// `tape` played back on `level` with `rivals` bots beside it: the same run, step for step (its
// digest is the report's).
// The bot's outing on the free ride's snowmobile (sled_outing.hpp): from beside it, up the run
// `sled_climb_run` picks, and down it on his skis to a foot.
RunReport simulate_sled_outing(std::shared_ptr<const ge::mountain::mapgen::Level> level,
                               double seed, const SimOptions& opts,
                               std::vector<std::vector<LinePoint>>* lines = nullptr);
// The bot's outing on the free ride's helicopter (heli_outing.hpp): from beside its pad, flown to
// the head of the run `heli_drop_run` picks, dropped there and down it on his skis to a foot.
RunReport simulate_heli_outing(std::shared_ptr<const ge::mountain::mapgen::Level> level,
                               double seed, const SimOptions& opts,
                               std::vector<std::vector<LinePoint>>* lines = nullptr);
// The bot's outing under the free ride's paramotor (para_outing.hpp): begun on the summit, flown
// to a gentle stretch of the run he began on, landed there and down the rest on his skis.
RunReport simulate_para_outing(std::shared_ptr<const ge::mountain::mapgen::Level> level,
                               double seed, const SimOptions& opts,
                               std::vector<std::vector<LinePoint>>* lines = nullptr);
RunReport replay_tape(std::shared_ptr<const ge::mountain::mapgen::Level> level, double seed,
                      const ge::racing::ControlTape& tape, int32_t rivals = 0);

}  // namespace fall_line::sim
