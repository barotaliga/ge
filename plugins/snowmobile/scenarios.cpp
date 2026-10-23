// The snowmobile's scenario table and its runner (scenarios.hpp).
//
// ge:summary The snowmobile scenario table and run_sled_scenario: stage, script, step, measure
// ge:keywords snowmobile scenario table run_sled_scenario measure trace digest
// ge:skill snowmobile labs
// ge:test tests/snowmobile_scenarios_test.cpp
#include "snowmobile/scenarios.hpp"

#include <cmath>

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "core/num.hpp"
#include "skier/skis.hpp"
#include "skier/synthetic.hpp"
#include "skier/trees.hpp"
#include "snowmobile/body.hpp"

namespace ge::snowmobile {

namespace {

namespace mg = mountain::mapgen;
namespace num = core::num;
constexpr double kDeg = core::kPi / 180;
constexpr double kMid = skier::kStripSize / 2;
constexpr double kUp = core::kPi;  // up the strip and the slope (both fall down +z)

SledControls flat_out(double, const SledState&) { return {1, 0, 0, 0}; }
SledControls hands_off(double, const SledState&) { return {}; }
SledControls climb(double, const SledState&) { return {1, 0, 0, -0.7}; }
// Held near a cruise of 30 km/h and the bars at full lock: the circle it settles on.
SledControls lock_right(double, const SledState& s) {
  return {core::clamp((8.5 - s.way) * 0.4 + 0.3, 0, 1), 0, 1, 0};
}
// Through powder at a cruise, the bars at most of their lock: rolled onto the inside edge.
SledControls powder_carve(double, const SledState& s) {
  return {core::clamp((12 - s.way) * 0.4 + 0.35, 0, 1), 0, 0.8, 0};
}
SledControls powder_carve_left(double t, const SledState& s) {
  SledControls c = powder_carve(t, s);
  c.steer = -c.steer;
  return c;
}
// Linked turns on the groomer, the bars swung every two seconds.
SledControls weave(double t, const SledState& s) {
  const bool right = static_cast<int>(t / 2) % 2 == 0;
  return {core::clamp((14 - s.way) * 0.4 + 0.3, 0, 1), 0, right ? 0.9 : -0.9, 0};
}
SledControls brake_hard(double, const SledState&) { return {0, 1, 0, 0}; }

constexpr SledScenario kScenarios[] = {
    {"top-speed",
     "flat out on the dead flat groomer from rest: about 140 km/h",
     SledStage::strip,
     0,
     1,
     kMid,
     200,
     0,
     0,
     0,
     70,
     true,
     flat_out,
     {}},
    {"rest-powder",
     "stood in untouched powder with the engine idling: the belt sits 20-30 cm in",
     SledStage::strip,
     0,
     0,
     kMid,
     400,
     0,
     0,
     0,
     5,
     true,
     hands_off,
     {}},
    {"launch-powder",
     "flat out from rest in untouched powder: the belt spins, then planes up",
     SledStage::strip,
     0,
     0,
     kMid,
     400,
     0,
     0,
     0,
     20,
     true,
     flat_out,
     {}},
    {"climb-24",
     "flat out from rest up a 24 deg powder face, the weight forward: it climbs",
     SledStage::strip,
     24 * kDeg,
     0,
     kMid,
     3600,
     kUp,
     0,
     0,
     25,
     true,
     climb,
     {}},
    {"high-mark-35",
     "charged at a 35 deg powder face at 50 km/h: it bogs and stalls on it",
     SledStage::strip,
     35 * kDeg,
     0,
     kMid,
     3600,
     kUp,
     14,
     0,
     20,
     true,
     climb,
     {}},
    {"turn-lock",
     "full lock at 30 km/h on the flat groomer: a circle of some 8-14 m",
     SledStage::strip,
     0,
     1,
     kMid,
     400,
     0,
     8.5,
     0,
     12,
     true,
     lock_right,
     {}},
    {"powder-carve",
     "most of the lock at 43 km/h in flat powder: rolled onto its inside edge",
     SledStage::strip,
     0,
     0,
     kMid,
     400,
     0,
     12,
     0,
     8,
     true,
     powder_carve,
     {}},
    {"powder-carve-left",
     "the same turn to the left: the mirror of the right, rolled left",
     SledStage::strip,
     0,
     0,
     kMid,
     400,
     0,
     12,
     0,
     8,
     true,
     powder_carve_left,
     {}},
    {"weave",
     "linked turns at 50 km/h down a 10 deg groomer: never thrown",
     SledStage::strip,
     10 * kDeg,
     1,
     kMid,
     400,
     0,
     14,
     0,
     20,
     true,
     weave,
     {}},
    {"brake",
     "full brake at 60 km/h on the flat groomer: stopped in some 40 m",
     SledStage::strip,
     0,
     1,
     kMid,
     400,
     0,
     16.7,
     0,
     8,
     true,
     brake_hard,
     {}},
    {"park-slope",
     "left on a 15 deg groomer on its parking brake: it stays where it is",
     SledStage::strip,
     15 * kDeg,
     1,
     kMid,
     400,
     0,
     0,
     0,
     10,
     false,
     hands_off,
     {}},
    {"tree",
     "straight at the lone tree at 15 m/s: the rider thrown off the trunk",
     SledStage::slope,
     0,
     0,
     skier::slope::kTreeX,
     200,
     0,
     15,
     0,
     8,
     true,
     hands_off,
     {}},
    {"drop",
     "dropped 4 m onto the flat groomer at 15 m/s: ridden out",
     SledStage::strip,
     0,
     1,
     kMid,
     400,
     0,
     15,
     4,
     4,
     true,
     hands_off,
     {}},
    {"cliff",
     "dropped 16 m onto the flat groomer at 15 m/s: the rider thrown",
     SledStage::strip,
     0,
     1,
     kMid,
     400,
     0,
     15,
     16,
     5,
     true,
     hands_off,
     {}},
    {"pilot-climb",
     "the bot rides up the slope's powder from its foot to near its top",
     SledStage::slope,
     0,
     0,
     280,
     600,
     kUp,
     0,
     0,
     40,
     true,
     nullptr,
     {280, 60, 14}},
    {"pilot-trees",
     "the bot rides up past the lone tree standing in its line: round it",
     SledStage::slope,
     0,
     0,
     skier::slope::kTreeX,
     330,
     kUp,
     0,
     0,
     30,
     true,
     nullptr,
     {skier::slope::kTreeX, 150, 10}},
};

}  // namespace

std::span<const SledScenario> sled_scenarios() { return kScenarios; }

const SledScenario* find_sled_scenario(std::string_view id) {
  for (const SledScenario& sc : kScenarios) {
    if (id == sc.id) return &sc;
  }
  return nullptr;
}

mg::Level sled_scenario_level(const SledScenario& sc) {
  return sc.stage == SledStage::slope ? skier::synthetic_slope()
                                      : skier::flat_strip(sc.pitch, sc.packed);
}

double scenario_rider() { return skier::kReferenceSkis.mass; }

SledRun run_sled_scenario(const SledScenario& sc, double hz,
                          const std::function<void(const SledStep&)>& each) {
  const mg::Level level = sled_scenario_level(sc);
  const skier::TreeIndex trees = skier::build_tree_index(level);
  const SledWorld world{&level, &trees};
  const double dt = 1 / hz;
  SledState s = stand_sled(level, sc.x, sc.z, sc.heading);
  s.y += sc.lift;
  s.vx = core::libm::sin(sc.heading) * sc.speed;
  s.vz = core::libm::cos(sc.heading) * sc.speed;
  s.running = sc.ridden;
  s.rpm = sc.ridden ? spec::kIdleRpm : 0;
  s.tread_speed = sc.speed;
  derive_sled(s);
  const double y0 = s.y;
  const double rider = scenario_rider();
  SledRun run;
  uint64_t h = 0xcbf29ce484222325ull;
  const auto steps = static_cast<int64_t>(num::round(sc.seconds * hz));
  const auto every = static_cast<int64_t>(num::round(hz / 10));
  const int64_t half = steps / 2;
  double px = s.x;
  double pz = s.z;
  double turned = 0;
  double turn_path = 0;
  double last_heading = s.heading;
  SledEvents ev;
  for (int64_t i = 0; i < steps; i++) {
    const double t = static_cast<double>(i) * dt;
    if (i % every == 0) {
      double sink = 0;
      for (size_t k = 2; k < s.sinks.size(); k++) sink += s.sinks[k];
      run.trace.push_back(SledSample{t, s.x, s.z, s.y, s.speed, s.way, s.roll, sink / 6, s.slip,
                                     s.rpm, s.airborne});
    }
    const SledControls in = sc.hand ? sc.hand(t, s) : sled_pilot(s, world, sc.aim);
    step_sled(s, in, rider, sc.ridden, world, dt, ev);
    h = sled_hash(s, h);
    if (each) each(SledStep{t + dt, s, in, ev});
    run.top_speed = num::max(run.top_speed, s.speed);
    const double step = core::hypot(s.x - px, s.z - pz);
    run.distance += step;
    px = s.x;
    pz = s.z;
    run.climbed = num::max(run.climbed, s.y - y0);
    run.top_slip = num::max(run.top_slip, s.slip);
    if (!s.airborne) run.max_roll = num::max(run.max_roll, std::fabs(s.roll));
    if (i >= half) {
      turned += std::fabs(core::angle_diff(last_heading, s.heading));
      turn_path += step;
    }
    last_heading = s.heading;
    if (run.stall_time < 0 && t > 1 && s.way < 1) run.stall_time = t;
    if (!sc.hand) {
      const double d = core::hypot(s.x - sc.aim.x, s.z - sc.aim.z);
      run.closest = run.closest < 0 ? d : num::min(run.closest, d);
    }
    if (ev.took_off) run.flights++;
    if (ev.landed) {
      run.longest_flight = num::max(run.longest_flight, ev.flight_s);
      run.hardest_landing = num::max(run.hardest_landing, ev.into);
    }
    if (ev.hit > 0) run.hits++;
    if (ev.thrown != Thrown::none) {
      run.thrown = ev.thrown;
      run.thrown_time = t + dt;
      break;
    }
  }
  run.radius = turned > 0.5 ? turn_path / turned : 0;
  double sink = 0;
  for (size_t k = 2; k < s.sinks.size(); k++) sink += s.sinks[k];
  run.rest_sink = sink / 6;
  run.end = s;
  run.digest = h;
  return run;
}

}  // namespace ge::snowmobile
