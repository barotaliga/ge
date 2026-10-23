// THE RIDE SCENARIOS: exact situations a skier is staged in, each a hand-built map, a moment to
// stand him at, a scripted input and a length, written ONCE and read by both the tests
// (tests/skier_scenarios_test.cpp holds every one's digest and its expectation) and the `ride`
// lab (the same rows measured and drawn). A scenario never touches the generator: its map is
// `synthetic.hpp`'s.
//
// To add one: a row in `scenarios.cpp`'s table (its id is the `--scenario=` value), then
// `GE_GOLDEN_WRITE=1` the test's golden for the new row, and an expectation in the test if it
// has one right answer.
//
// ge:summary The ride scenarios: staged moments with scripted inputs, run and measured
// ge:keywords scenario scenarios ride staged moment scripted input run_scenario trace
// ge:keywords schuss carve hockey-stop skate kicker tree drop turns rollers measure injuries
// ge:skill skier labs
// ge:test tests/skier_scenarios_test.cpp
#pragma once

#include <cstdint>
#include <functional>
#include <span>
#include <string_view>
#include <vector>

#include "mountain/mapgen/types.hpp"
#include "skier/body.hpp"
#include "skier/skier.hpp"
#include "skier/skis.hpp"

namespace ge::skier {

enum class Stage : uint8_t { slope, strip };

struct Scenario {
  const char* id;
  const char* what;  // what it stages and what a right run shows, one sentence
  Stage stage;
  double pitch;   // rad, the strip's (unused on the slope)
  double packed;  // the strip's snow (unused on the slope)
  double x, z;    // where he stands, m
  double heading;
  double speed;  // m/s along the heading
  double lift;   // m over the snow (0 on it)
  double seconds;
  bool schuss;  // a straight run on a strip: its speed is held to `terminal_speed`
  SkierInput (*input)(double t, const SkierState& s);
};

// Every scenario, in the order the lab lists them.
std::span<const Scenario> scenarios();
const Scenario* find_scenario(std::string_view id);

// The map a scenario is staged on.
mountain::mapgen::Level scenario_level(const Scenario& sc);

// One tenth of a second of a run, for a picture.
struct TraceSample {
  double t, x, z, y, speed, clearance;
  bool on_snow, down;
};

struct ScenarioRun {
  SkierState end;
  std::vector<TraceSample> trace;
  double top_speed = 0;   // m/s
  double distance = 0;    // m travelled over the plan
  double max_g_lat = 0;   // the hardest sideways load, g
  double min_radius = 0;  // m, the tightest path (speed over yaw rate) held on the snow past
                          // 5 m/s; 0 when he never turned
  int32_t flights = 0;
  double longest_flight = 0;  // s
  double max_landing_g = 0;
  double stop_time = -1;  // s when he first stood still (under 0.3 m/s), -1 never
  double wipeout_time = -1;
  Wipeout cause = Wipeout::none;
  int32_t glances = 0;
  uint64_t digest = 0;  // FNV-1a over every step's state
  BodyState body;       // what his body took (`body.hpp`), stepped after every step
  uint64_t body_digest = 0;
  // THE THROWN BODY after the wipeout (`ragdoll.hpp`): how far its spine turned, rad; how far
  // its centre of mass went from where he went down, m (over the plan); the highest that centre
  // rose over the snow, m; and the time from the wipeout until it had lain still a second, s
  // (-1 never).
  double tumble = 0;
  double thrown = 0;
  double thrown_top = 0;
  double lie_time = -1;
  double mortal_time = -1;  // s he was mortally hurt, -1 never
  double death_time = -1;
};

// One step of a run, as it stands after the step: what a per-step trace reads.
struct ScenarioStep {
  double t;  // s, the step's end
  const SkierState& skier;
  const SkierInput& input;
  const SkierEvents& events;
  double ground;  // m, the untouched surface under him
};

// Run `sc` on `spec` at `hz` steps a second, `each` called after every step when given (the ride
// lab's --trace).
ScenarioRun run_scenario(const Scenario& sc, const SkiSpec& spec, double hz = 120,
                         const std::function<void(const ScenarioStep&)>& each = {});

}  // namespace ge::skier
