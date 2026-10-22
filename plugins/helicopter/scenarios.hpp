// THE HELICOPTER SCENARIOS: exact situations the machine is staged in, each a hand-built map
// (`skier/synthetic.hpp`), a moment to stand or hang it at, a scripted hand on its controls (or
// the bot's, to an aim) and a length, written ONCE and read by both the tests
// (tests/helicopter_scenarios_test.cpp holds every one's digest and its expectation) and the
// `heli` lab (the same rows measured and drawn). A scenario never touches the generator.
//
// To add one: a row in `scenarios.cpp`'s table (its id is the `--scenario=` value), then
// `GE_GOLDEN_WRITE=1` the test's golden for the new row, and an expectation in the test if it has
// one right answer.
//
// ge:summary The helicopter scenarios: staged moments with scripted controls, run and measured
// ge:keywords helicopter scenario scenarios staged spool lift-off torque climb hover cruise land
// ge:keywords hard-landing slope tree loop rider run_heli_scenario
// ge:skill helicopter labs
// ge:test tests/helicopter_scenarios_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

#include "helicopter/heli.hpp"
#include "helicopter/pilot.hpp"
#include "mountain/mapgen/types.hpp"

namespace ge::helicopter {

enum class HeliStage : uint8_t { slope, strip };

struct HeliScenario {
  const char* id;
  const char* what;  // what it stages and what a right run shows, one sentence
  HeliStage stage;
  double pitch;  // rad, the strip's (unused on the slope)
  double x, z;   // where it stands or hangs, m
  double heading;
  double speed;   // m/s along the heading
  double height;  // m of its skids over the snow (0: sat on it)
  double spool;   // the rotor's share of its rpm at the start
  double seconds;
  double load;  // kg sat on the skid (0 none), at the skid's place across
  // The hand on the controls at `t` s, or null for the bot flying to `aim`.
  HeliControls (*hand)(double t, const HeliState& h);
  HeliAim aim;
};

// Every scenario, in the order the lab lists them.
std::span<const HeliScenario> heli_scenarios();
const HeliScenario* find_heli_scenario(std::string_view id);

// The map a scenario is staged on.
mountain::mapgen::Level heli_scenario_level(const HeliScenario& sc);

// Where a rider sits across the airframe, body x, m: on the skid, a little out over it.
inline constexpr double kSeatX = 1.1;

// One tenth of a second of a run, for a picture.
struct HeliSample {
  double t, x, z, y, ground, speed, vy, roll, disc_pitch, collective;
  bool grounded;
};

struct HeliRun {
  HeliState end;
  std::vector<HeliSample> trace;
  double top_speed = 0;  // m/s
  double distance = 0;   // m flown over the plan
  double climbed = 0;    // m, the most it rose over where it started
  double top_climb = 0;  // m/s, the fastest it climbed
  double max_roll = 0;   // rad, the fuselage's most either way in the air
  double max_disc = 0;   // rad, the disc's most pitch either way
  double turned = 0;     // rad, the heading's change from the start (signed, unwrapped)
  double closest = -1;   // m, the bot's nearest to its aim (-1 on a scripted hand)
  double lowest = -1;    // m, the skids' least height over the snow in the air after 2 s, -1 never
  double lift_off = -1;  // s, when the skids first left the snow, -1 never
  int32_t landings = 0;
  double hardest_landing = 0;  // m/s down onto the skids
  HeliCrash crash = HeliCrash::none;
  double crash_time = -1;
  double crash_fall = 0;  // m/s, how fast it was coming down when the snow stopped it
  uint64_t digest = 0;    // FNV-1a over every step's state
};

// Run `sc` at `hz` steps a second. The run stops at a crash.
HeliRun run_heli_scenario(const HeliScenario& sc, double hz = 120);

}  // namespace ge::helicopter
