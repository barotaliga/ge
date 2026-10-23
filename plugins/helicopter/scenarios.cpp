// The helicopter's scenario table and its runner (scenarios.hpp).
//
// ge:summary The helicopter scenario table and run_heli_scenario: stage, script, step, measure
// ge:keywords helicopter scenario table run_heli_scenario measure trace digest
// ge:skill helicopter labs
// ge:test tests/helicopter_scenarios_test.cpp
#include "helicopter/scenarios.hpp"

#include <cmath>

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "core/num.hpp"
#include "skier/synthetic.hpp"
#include "skier/trees.hpp"

namespace ge::helicopter {

namespace {

namespace mg = mountain::mapgen;
namespace num = core::num;
constexpr double kMid = skier::kStripSize / 2;
constexpr double kUp = core::kPi;  // up the strip and the slope (both fall down +z)

HeliControls let_go(double, const HeliState&) { return {}; }
// The lever raised to 0.7 over a second and held, nothing else touched: it lifts, and the
// rotor's torque turns the nose left.
HeliControls lever_up(double t, const HeliState&) {
  return {core::clamp(t, 0.0, 1.0) * 0.7, 0, 0, 0};
}
HeliControls full_lever(double, const HeliState&) { return {1, 0, 0, 0}; }
// High up at the hover's lever, the cyclic held full aft: the disc goes over the top.
HeliControls full_aft(double, const HeliState&) { return {0.55, -1, 0, 0}; }

constexpr HeliScenario kScenarios[] = {
    {.id = "spool-up",
     .what = "on the flat with the lever down while the rotor spools up: it stays on its skids",
     .stage = HeliStage::strip,
     .pitch = 0,
     .x = kMid,
     .z = 400,
     .heading = kUp,
     .speed = 0,
     .height = 0,
     .spool = 0,
     .seconds = 8,
     .load = 0,
     .hand = let_go,
     .aim = {}},
    {.id = "lift-off",
     .what = "the lever raised to 0.7 and nothing else: it lifts off and the torque turns the "
             "nose left",
     .stage = HeliStage::strip,
     .pitch = 0,
     .x = kMid,
     .z = 400,
     .heading = kUp,
     .speed = 0,
     .height = 0,
     .spool = 1,
     .seconds = 10,
     .load = 0,
     .hand = lever_up,
     .aim = {}},
    {.id = "full-climb",
     .what = "the lever full up from a hover at 50 m: a climb of some 15-20 m/s",
     .stage = HeliStage::strip,
     .pitch = 0,
     .x = kMid,
     .z = 400,
     .heading = kUp,
     .speed = 0,
     .height = 50,
     .spool = 1,
     .seconds = 12,
     .load = 0,
     .hand = full_lever,
     .aim = {}},
    {.id = "hover",
     .what = "the bot holds a hover 20 m over a spot on the flat: it stays within a metre or two",
     .stage = HeliStage::strip,
     .pitch = 0,
     .x = kMid,
     .z = 400,
     .heading = kUp,
     .speed = 0,
     .height = 20,
     .spool = 1,
     .seconds = 20,
     .load = 0,
     .hand = nullptr,
     .aim = {.x = kMid, .z = 400, .height = 20}},
    {.id = "cruise",
     .what = "the bot flies 1.6 km up the flat at 45 m: it cruises at about 40 m/s and stops "
             "over the aim",
     .stage = HeliStage::strip,
     .pitch = 0,
     .x = kMid,
     .z = 2400,
     .heading = kUp,
     .speed = 0,
     .height = 45,
     .spool = 1,
     .seconds = 90,
     .load = 0,
     .hand = nullptr,
     .aim = {.x = kMid, .z = 800, .height = 45}},
    {.id = "land",
     .what = "the bot flies 400 m across the flat and sets down on the spot, facing up the strip",
     .stage = HeliStage::strip,
     .pitch = 0,
     .x = kMid,
     .z = 800,
     .heading = kUp,
     .speed = 0,
     .height = 30,
     .spool = 1,
     .seconds = 60,
     .load = 0,
     .hand = nullptr,
     .aim = {.x = kMid + 300, .z = 540, .height = 30, .land = true, .has_face = true, .face = kUp}},
    {.id = "drop-lever",
     .what = "the lever thrown down in a hover at 30 m: it falls onto its skids and crashes",
     .stage = HeliStage::strip,
     .pitch = 0,
     .x = kMid,
     .z = 400,
     .heading = kUp,
     .speed = 0,
     .height = 30,
     .spool = 1,
     .seconds = 10,
     .load = 0,
     .hand = let_go,
     .aim = {}},
    {.id = "slope-land",
     .what = "the bot sets down facing up the 17 deg face: its nose meets the snow before the "
             "skids can sit",
     .stage = HeliStage::slope,
     .pitch = 0,
     .x = 200,
     .z = 300,
     .heading = kUp,
     .speed = 0,
     .height = 20,
     .spool = 1,
     .seconds = 40,
     .load = 0,
     .hand = nullptr,
     .aim = {.x = 200, .z = 300, .height = 20, .land = true}},
    {.id = "tree",
     .what = "the bot flown across the face at 8 m, under the lone tree's top: the disc meets its "
             "crown",
     .stage = HeliStage::slope,
     .pitch = 0,
     .x = 180,
     .z = skier::slope::kTreeZ,
     .heading = core::kPi / 2,
     .speed = 0,
     .height = 8,
     .spool = 1,
     .seconds = 40,
     .load = 0,
     .hand = nullptr,
     .aim = {.x = 480, .z = skier::slope::kTreeZ, .height = 8}},
    {.id = "loop",
     .what = "300 m up at a hover's lever, the cyclic held full aft: the disc goes past the "
             "vertical",
     .stage = HeliStage::strip,
     .pitch = 0,
     .x = kMid,
     .z = 400,
     .heading = kUp,
     .speed = 0,
     .height = 300,
     .spool = 1,
     .seconds = 4,
     .load = 0,
     .hand = full_aft,
     .aim = {}},
    {.id = "terrain",
     .what = "the bot flies up the face from its foot to near its top at 45 m over the snow: "
             "it holds its height over the face as it climbs",
     .stage = HeliStage::slope,
     .pitch = 0,
     .x = 200,
     .z = 600,
     .heading = kUp,
     .speed = 0,
     .height = 45,
     .spool = 1,
     .seconds = 60,
     .load = 0,
     .hand = nullptr,
     .aim = {.x = 200, .z = 60, .height = 45}},
    {.id = "rider",
     .what = "the bot hovers with a 90 kg rider on its right skid: the fuselage hangs rolled "
             "toward him",
     .stage = HeliStage::strip,
     .pitch = 0,
     .x = kMid,
     .z = 400,
     .heading = kUp,
     .speed = 0,
     .height = 20,
     .spool = 1,
     .seconds = 15,
     .load = 90,
     .hand = nullptr,
     .aim = {.x = kMid, .z = 400, .height = 20}},
};

}  // namespace

std::span<const HeliScenario> heli_scenarios() { return kScenarios; }

const HeliScenario* find_heli_scenario(std::string_view id) {
  for (const HeliScenario& sc : kScenarios) {
    if (id == sc.id) return &sc;
  }
  return nullptr;
}

mg::Level heli_scenario_level(const HeliScenario& sc) {
  return sc.stage == HeliStage::slope ? skier::synthetic_slope() : skier::flat_strip(sc.pitch, 1);
}

HeliRun run_heli_scenario(const HeliScenario& sc, double hz, ai::Why* why) {
  const mg::Level level = heli_scenario_level(sc);
  const skier::TreeIndex trees = skier::build_tree_index(level);
  const HeliWorld world{&level, &trees};
  const HeliLoad load{sc.load, sc.load > 0 ? kSeatX : 0.0};
  const double dt = 1 / hz;
  HeliState h = stand_heli(level, sc.x, sc.z, sc.heading);
  h.spool = sc.spool;
  if (sc.height > 0) {
    // Hung in the air at the hover's thrust, level, its disc with it.
    h.y += sc.height;
    h.pitch = h.roll = h.disc.pitch = h.disc.roll = 0;
    h.grounded = false;
    h.thrust = heli_mass(load) * 9.81;
  }
  h.vx = core::libm::sin(sc.heading) * sc.speed;
  h.vz = core::libm::cos(sc.heading) * sc.speed;
  const double y0 = h.y;
  HeliRun run;
  uint64_t digest = 0xcbf29ce484222325ull;
  const auto steps = static_cast<int64_t>(num::round(sc.seconds * hz));
  const auto every = static_cast<int64_t>(num::round(hz / 10));
  double px = h.x;
  double pz = h.z;
  double last_heading = h.heading;
  HeliEvents ev;
  for (int64_t i = 0; i < steps; i++) {
    const double t = static_cast<double>(i) * dt;
    if (i % every == 0) {
      run.trace.push_back(HeliSample{t, h.x, h.z, h.y, level.ground_at(h.x, h.z),
                                     core::hypot3(h.vx, h.vy, h.vz), h.vy, h.roll, h.disc.pitch,
                                     h.collective, h.grounded});
    }
    const HeliControls in =
        sc.hand ? sc.hand(t, h)
                : heli_pilot(h, world, load, sc.aim, why ? why->follow(0, i) : nullptr);
    step_heli(h, in, load, 1, world, dt, ev);
    digest = heli_hash(h, digest);
    run.top_speed = num::max(run.top_speed, core::hypot3(h.vx, h.vy, h.vz));
    run.distance += core::hypot(h.x - px, h.z - pz);
    px = h.x;
    pz = h.z;
    run.climbed = num::max(run.climbed, h.y - y0);
    run.top_climb = num::max(run.top_climb, h.vy);
    if (!h.grounded) run.max_roll = num::max(run.max_roll, std::fabs(h.roll));
    run.max_disc = num::max(run.max_disc, std::fabs(h.disc.pitch));
    if (!h.grounded && t >= 2) {
      const double over = h.y - level.ground_at(h.x, h.z);
      run.lowest = run.lowest < 0 ? over : num::min(run.lowest, over);
    }
    run.turned += core::angle_diff(last_heading, h.heading);
    last_heading = h.heading;
    if (!sc.hand) {
      const double d = core::hypot(h.x - sc.aim.x, h.z - sc.aim.z);
      run.closest = run.closest < 0 ? d : num::min(run.closest, d);
    }
    if (ev.lifted && run.lift_off < 0) run.lift_off = t + dt;
    if (ev.landed) {
      run.landings++;
      run.hardest_landing = num::max(run.hardest_landing, ev.sink);
    }
    if (ev.crash != HeliCrash::none) {
      run.crash = ev.crash;
      run.crash_time = t + dt;
      run.crash_fall = ev.fall;
      break;
    }
  }
  run.end = h;
  run.digest = digest;
  return run;
}

}  // namespace ge::helicopter
