// THE SNOWMOBILE SCENARIOS: exact situations the machine is staged in, each a hand-built map
// (`skier/synthetic.hpp`), a moment to stand it at, a scripted hand on its controls (or the bot's,
// to an aim) and a length, written ONCE and read by both the tests
// (tests/snowmobile_scenarios_test.cpp holds every one's digest and its expectation) and the
// `sled` lab (the same rows measured and drawn). A scenario never touches the generator.
//
// To add one: a row in `scenarios.cpp`'s table (its id is the `--scenario=` value), then
// `GE_GOLDEN_WRITE=1` the test's golden for the new row, and an expectation in the test if it has
// one right answer.
//
// ge:summary The snowmobile scenarios: staged moments with scripted controls, run and measured
// ge:keywords snowmobile scenario scenarios staged top-speed sink float launch climb high-mark
// ge:keywords carve turn circle tree drop cliff park pilot run_sled_scenario
// ge:skill snowmobile labs
// ge:test tests/snowmobile_scenarios_test.cpp
#pragma once

#include <cstdint>
#include <functional>
#include <span>
#include <string_view>
#include <vector>

#include "mountain/mapgen/types.hpp"
#include "snowmobile/pilot.hpp"
#include "snowmobile/sled.hpp"

namespace ge::snowmobile {

enum class SledStage : uint8_t { slope, strip };

struct SledScenario {
  const char* id;
  const char* what;  // what it stages and what a right run shows, one sentence
  SledStage stage;
  double pitch;   // rad, the strip's (unused on the slope)
  double packed;  // the strip's snow (unused on the slope)
  double x, z;    // where it stands, m
  double heading;
  double speed;  // m/s along the heading
  double lift;   // m over its rest height (0 on the snow)
  double seconds;
  bool ridden;  // false: left to itself, the parking brake on
  // The hand on the controls at `t` s, or null for the bot riding to `aim`.
  SledControls (*hand)(double t, const SledState& s);
  SledAim aim;
};

// Every scenario, in the order the lab lists them.
std::span<const SledScenario> sled_scenarios();
const SledScenario* find_sled_scenario(std::string_view id);

// The map a scenario is staged on.
mountain::mapgen::Level sled_scenario_level(const SledScenario& sc);

// One tenth of a second of a run, for a picture.
struct SledSample {
  double t, x, z, y, speed, way, roll, sink, slip, rpm;
  bool airborne;
};

struct SledRun {
  SledState end;
  std::vector<SledSample> trace;
  double top_speed = 0;    // m/s
  double distance = 0;     // m travelled over the plan
  double climbed = 0;      // m, the most the CoG rose over where it stood
  double rest_sink = 0;    // m, the belt's mean sink at the end
  double top_slip = 0;     // m/s, the belt's most slip over the snow
  double max_roll = 0;     // rad, the most it rolled either way while ridden on the snow
  double radius = 0;       // m, the path's radius over the run's last half (0 if it never turned)
  double stall_time = -1;  // s, when its way first fell under 1 m/s after a second, -1 never
  double closest = -1;     // m, the bot's nearest to its aim (-1 on a scripted hand)
  int32_t flights = 0;
  double longest_flight = 0;   // s
  double hardest_landing = 0;  // m/s into the snow
  int32_t hits = 0;
  Thrown thrown = Thrown::none;  // the first time the rider was put off
  double thrown_time = -1;
  uint64_t digest = 0;  // FNV-1a over every step's state
};

// The rider every scenario carries, kg: the skier's reference body in his kit.
double scenario_rider();

// One step of a run, as it stands after the step: what a per-step trace reads.
struct SledStep {
  double t;  // s, the step's end
  const SledState& sled;
  const SledControls& controls;
  const SledEvents& events;
};

// Run `sc` at `hz` steps a second, `each` called after every step when given (the sled lab's
// --trace). The run stops at the first throw (the rider is off).
SledRun run_sled_scenario(const SledScenario& sc, double hz = 120,
                          const std::function<void(const SledStep&)>& each = {});

}  // namespace ge::snowmobile
