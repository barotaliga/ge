// THE PARAMOTOR SCENARIOS: exact situations the rig is staged in, each a hand-built map
// (`skier/synthetic.hpp`), a moment to stand the skier at (the wing ready over him) or hang him at
// (the wing flying), a wind, a scripted hand on the controls (or the bot's, to an aim) and a
// length, written ONCE and read by both the tests (tests/paramotor_scenarios_test.cpp holds every
// one's digest and its expectation) and the `para` lab (the same rows measured and drawn). A
// scenario never touches the generator.
//
// To add one: a row in `scenarios.cpp`'s table (its id is the `--scenario=` value), then
// `GE_GOLDEN_WRITE=1` the test's golden for the new row, and an expectation in the test if it has
// one right answer.
//
// ge:summary The paramotor scenarios: staged moments with scripted controls, run and measured
// ge:keywords paramotor para scenario scenarios staged launch glide climb turn stall surge
// ge:keywords flare land ridge-lift lee rough fold crown drop run_para_scenario acro dive
// ge:keywords swing-up loop tangle cut-tangle
// ge:skill paramotor labs
// ge:test tests/paramotor_scenarios_test.cpp
#pragma once

#include <cstdint>
#include <functional>
#include <span>
#include <string_view>
#include <vector>

#include "mountain/mapgen/types.hpp"
#include "paramotor/para.hpp"
#include "paramotor/pilot.hpp"

namespace ge::paramotor {

enum class ParaStage : uint8_t { slope, strip };

struct ParaScenario {
  const char* id;
  const char* what;  // what it stages and what a right run shows, one sentence
  ParaStage stage;
  double pitch;  // rad, the strip's (unused on the slope)
  double x, z;   // where the skier stands or hangs, m
  double heading;
  double speed;           // m/s along the heading
  double height;          // m of his skis over the snow (0: stood on it, the wing ready over him)
  double wind_x, wind_z;  // the wind 10 m over open snow, m/s
  double seconds;
  double drop_at;  // s the release is pressed at (-1 never)
  // The hand on the controls at `t` s, or null for the bot flying to `aim`.
  ParaControls (*hand)(double t, const ParaState& p);
  ParaAim aim;
};

// Every scenario, in the order the lab lists them.
std::span<const ParaScenario> para_scenarios();
const ParaScenario* find_para_scenario(std::string_view id);

// The map a scenario is staged on.
mountain::mapgen::Level para_scenario_level(const ParaScenario& sc);

// One tenth of a second of a run, for a picture.
struct ParaSample {
  double t, x, z, y, ground, speed, vy, bank, fold;
  bool flying;
};

struct ParaRun {
  ParaState end;
  skier::SkierState skier;
  std::vector<ParaSample> trace;
  double launch = -1;    // s the wing was let fly, -1 never
  double take_off = -1;  // s the skis left the snow under it, -1 never
  double top_speed = 0;  // m/s, the pilot's
  double distance = 0;   // m over the plan
  double climbed = 0;    // m, the most the pilot rose over where he started
  double top_climb = 0;  // m/s
  double sink = 0;       // m/s, the pilot's mean sink over the second half of the run in the air
  double airspeed = 0;   // m/s, the wing's mean over the second half of the run in the air
  double max_bank = 0;   // rad, the wing's most either way
  double turned = 0;     // rad, the pilot's heading's change from the start (signed, unwrapped)
  bool stalled = false;  // stalled at some point
  int32_t folds = 0;
  double deepest_fold = 0;
  int32_t touches = 0;      // the skis back on the snow under the wing
  double landing_g = 0;     // the hardest landing's load, g
  bool wiped_out = false;   // the skier went down
  double closest = -1;      // m, the bot's nearest to its aim (-1 on a scripted hand)
  int32_t loops = 0;        // whole turns the lines swung round him (acro.hpp)
  double tangle = -1;       // s he fell into his wing, -1 never
  double top_g = 0;         // the most he hung on the flying wing, g
  double top_sink = 0;      // m/s, the pilot's fastest fall
  double top_airspeed = 0;  // m/s, the flying wing's most
  ParaCut cut = ParaCut::none;
  double cut_time = -1;
  uint64_t digest = 0;  // FNV-1a over every step's wing and skier
};

// One step of a run, as it stands after the step: what a per-step trace reads.
struct ParaStep {
  double t;  // s, the step's end
  const ParaState& wing;
  const skier::SkierState& skier;
  const ParaControls& controls;
  const ParaEvents& events;
  double ground;  // m, the snow under the skier
};

// Run `sc` at `hz` steps a second; `each`, when given, is called after every step (the para lab's
// --trace).
ParaRun run_para_scenario(const ParaScenario& sc, double hz = 120,
                          const std::function<void(const ParaStep&)>& each = {});

}  // namespace ge::paramotor
