// The paramotor's scenario table and its runner (scenarios.hpp).
//
// ge:summary The paramotor scenario table and run_para_scenario: stage, script, step, measure
// ge:keywords paramotor scenario table run_para_scenario measure trace digest
// ge:skill paramotor labs
// ge:test tests/paramotor_scenarios_test.cpp
#include "paramotor/scenarios.hpp"

#include <cmath>

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "core/num.hpp"
#include "paramotor/spec.hpp"
#include "skier/skier.hpp"
#include "skier/synthetic.hpp"
#include "skier/trees.hpp"

namespace ge::paramotor {

namespace {

namespace mg = mountain::mapgen;
namespace lm = core::libm;
namespace num = core::num;
constexpr double kMid = skier::kStripSize / 2;
constexpr double kFace = skier::slope::kPitch;  // the strip pitched as the slope is, rad
constexpr double kAcross = core::kPi / 2;       // across the face, +x

ParaControls let_go(double, const ParaState&) { return {}; }
ParaControls full_throttle(double, const ParaState&) { return {1, 0, 0, 0}; }
// Half throttle and the right toggle pulled half way down, held.
ParaControls right_turn(double, const ParaState&) { return {0.5, 0, 0.5, 0}; }
// Both brakes held all the way down for 8 s, then let up: a stall, and the surge out of it.
ParaControls stall(double t, const ParaState&) { return {0, t < 8 ? 1.0 : 0.0, 0, 0}; }
// THE ACRO, the throttle shut: the bar all the way forward (a dive), all the way back (the rears),
// or let go.
ParaControls dive(double t, const ParaState&) { return {0, 0, 0, t < 4 ? 1.0 : 0.0}; }
ParaControls swing_up(double t, const ParaState&) { return {0, 0, 0, t < 8 ? -1.0 : 0.0}; }
ParaControls loop(double t, const ParaState&) {
  return {0, 0, 0, t < 5 ? 1.0 : (t < 8 ? -1.0 : 0.0)};
}
ParaControls tangle(double t, const ParaState&) {
  return {0, 0, 0, t < 2 ? 1.0 : (t < 6 ? -1.0 : 0.0)};
}

constexpr ParaScenario kScenarios[] = {
    {.id = "launch",
     .what = "stood on the 17 deg face, the wing ready over him, full throttle and nothing else: "
             "the wing is let fly within a few seconds and lifts him off the snow",
     .stage = ParaStage::strip,
     .pitch = kFace,
     .x = kMid,
     .z = 300,
     .heading = 0,
     .speed = 0,
     .height = 0,
     .wind_x = 0,
     .wind_z = 0,
     .seconds = 20,
     .drop_at = -1,
     .hand = full_throttle,
     .aim = {}},
    {.id = "glide",
     .what = "hung 200 m over the flat with the throttle shut: it trims near 55 km/h and sinks "
             "at some 3-4 m/s, a glide near 4",
     .stage = ParaStage::strip,
     .pitch = 0,
     .x = kMid,
     .z = 400,
     .heading = 0,
     .speed = 15,
     .height = 200,
     .wind_x = 0,
     .wind_z = 0,
     .seconds = 40,
     .drop_at = -1,
     .hand = let_go,
     .aim = {}},
    {.id = "climb",
     .what = "hung 50 m over the flat on full throttle: it climbs",
     .stage = ParaStage::strip,
     .pitch = 0,
     .x = kMid,
     .z = 400,
     .heading = 0,
     .speed = 15,
     .height = 50,
     .wind_x = 0,
     .wind_z = 0,
     .seconds = 30,
     .drop_at = -1,
     .hand = full_throttle,
     .aim = {}},
    {.id = "turn",
     .what = "200 m up at half throttle, the right toggle held half down: a steady banked turn "
             "to the right, round and round",
     .stage = ParaStage::strip,
     .pitch = 0,
     .x = kMid,
     .z = 400,
     .heading = 0,
     .speed = 15,
     .height = 200,
     .wind_x = 0,
     .wind_z = 0,
     .seconds = 30,
     .drop_at = -1,
     .hand = right_turn,
     .aim = {}},
    {.id = "stall",
     .what = "300 m up, both brakes held down for 8 s, then let up: the wing stalls, sinks fast, "
             "and surges forward as it fills again",
     .stage = ParaStage::strip,
     .pitch = 0,
     .x = kMid,
     .z = 400,
     .heading = 0,
     .speed = 15,
     .height = 300,
     .wind_x = 0,
     .wind_z = 0,
     .seconds = 20,
     .drop_at = -1,
     .hand = stall,
     .aim = {}},
    {.id = "fly-down",
     .what = "the bot skis off an 11 deg face, lifts off and flies down the fall line holding "
             "60 m over the snow",
     .stage = ParaStage::strip,
     .pitch = 0.2,
     .x = kMid,
     .z = 300,
     .heading = 0,
     .speed = 0,
     .height = 0,
     .wind_x = 0,
     .wind_z = 0,
     .seconds = 90,
     .drop_at = -1,
     .hand = nullptr,
     .aim = {}},
    {.id = "land",
     .what = "the bot glides in from 60 m to a spot on the flat and flares: his skis touch down "
             "softly and the wing comes down behind him",
     .stage = ParaStage::strip,
     .pitch = 0,
     .x = kMid,
     .z = 1600,
     .heading = core::kPi,
     .speed = 15,
     .height = 60,
     .wind_x = 0,
     .wind_z = 0,
     .seconds = 60,
     .drop_at = -1,
     .hand = nullptr,
     .aim = {.has_aim = true, .x = kMid, .z = 1100, .height = 60, .land = true}},
    {.id = "ridge",
     .what = "40 m over the 17 deg face, gliding across it with the throttle shut in a 10 m/s "
             "wind blowing up it: ridge lift halves his sink",
     .stage = ParaStage::strip,
     .pitch = kFace,
     .x = 600,
     .z = 1500,
     .heading = kAcross,
     .speed = 15,
     .height = 40,
     .wind_x = 0,
     .wind_z = -10,
     .seconds = 20,
     .drop_at = -1,
     .hand = let_go,
     .aim = {}},
    {.id = "lee",
     .what = "the bot flies across the 17 deg face 40 m up in a 12 m/s wind blowing down it: "
             "the lee's rough air folds the wing and the bot pumps it out",
     .stage = ParaStage::strip,
     .pitch = kFace,
     .x = 600,
     .z = 1500,
     .heading = kAcross,
     .speed = 15,
     .height = 40,
     .wind_x = 0,
     .wind_z = 12,
     .seconds = 40,
     .drop_at = -1,
     .hand = nullptr,
     .aim = {.has_aim = true, .x = 3400, .z = 1500, .height = 40, .land = false}},
    {.id = "crown",
     .what = "the bot flies across the slope 10 m over the snow at the lone tree: the wing meets "
             "its crown and is cut away",
     .stage = ParaStage::slope,
     .pitch = 0,
     .x = 160,
     .z = skier::slope::kTreeZ,
     .heading = kAcross,
     .speed = 15,
     .height = 15,
     .wind_x = 0,
     .wind_z = 0,
     .seconds = 30,
     .drop_at = -1,
     .hand = nullptr,
     .aim = {.has_aim = true, .x = 520, .z = skier::slope::kTreeZ, .height = 10, .land = false}},
    {.id = "drop",
     .what = "2 m over the flat at 15 m/s, the release pressed at once: he lands on his skis and "
             "skis on; the canopy and the motor fall free and lie on the snow",
     .stage = ParaStage::strip,
     .pitch = 0,
     .x = kMid,
     .z = 400,
     .heading = 0,
     .speed = 15,
     .height = 2,
     .wind_x = 0,
     .wind_z = 0,
     .seconds = 10,
     .drop_at = 0.5,
     .hand = let_go,
     .aim = {}},
    {.id = "dive",
     .what = "400 m over the flat, the throttle shut and the bar held all the way forward for 4 s: "
             "the wing pitches over into a dive, steep and past 85 km/h, and stays open",
     .stage = ParaStage::strip,
     .pitch = 0,
     .x = kMid,
     .z = 400,
     .heading = 0,
     .speed = 15,
     .height = 400,
     .wind_x = 0,
     .wind_z = 0,
     .seconds = 8,
     .drop_at = -1,
     .hand = dive,
     .aim = {}},
    {.id = "swing-up",
     .what = "400 m over the flat at a cruise, the bar held all the way back for 8 s: without a "
             "dive's speed the rears only slow and lift it; no loop and no tangle",
     .stage = ParaStage::strip,
     .pitch = 0,
     .x = kMid,
     .z = 400,
     .heading = 0,
     .speed = 15,
     .height = 400,
     .wind_x = 0,
     .wind_z = 0,
     .seconds = 12,
     .drop_at = -1,
     .hand = swing_up,
     .aim = {}},
    {.id = "loop",
     .what = "400 m over the flat, a 5 s dive and then the bar all the way back for 3 s: the wing "
             "swings up and round over him in a LOOP, pulling 5-12 g, and flies on once let go",
     .stage = ParaStage::strip,
     .pitch = 0,
     .x = kMid,
     .z = 400,
     .heading = 0,
     .speed = 15,
     .height = 400,
     .wind_x = 0,
     .wind_z = 0,
     .seconds = 14,
     .drop_at = -1,
     .hand = loop,
     .aim = {}},
    {.id = "tangle",
     .what = "400 m over the flat, a 2 s dive and then the bar all the way back: without the "
             "speed to go over, the lines go slack at the top and he FALLS INTO THE WING, and "
             "falls tangled in it until the snow stops him",
     .stage = ParaStage::strip,
     .pitch = 0,
     .x = kMid,
     .z = 400,
     .heading = 0,
     .speed = 15,
     .height = 400,
     .wind_x = 0,
     .wind_z = 0,
     .seconds = 40,
     .drop_at = -1,
     .hand = tangle,
     .aim = {}},
    {.id = "cut-tangle",
     .what = "the tangle, and the release pressed at 7 s: the bundle is cut away and falls free "
             "of him",
     .stage = ParaStage::strip,
     .pitch = 0,
     .x = kMid,
     .z = 400,
     .heading = 0,
     .speed = 15,
     .height = 400,
     .wind_x = 0,
     .wind_z = 0,
     .seconds = 8,
     .drop_at = 7,
     .hand = tangle,
     .aim = {}},
};

}  // namespace

std::span<const ParaScenario> para_scenarios() { return kScenarios; }

const ParaScenario* find_para_scenario(std::string_view id) {
  for (const ParaScenario& sc : kScenarios) {
    if (id == sc.id) return &sc;
  }
  return nullptr;
}

mg::Level para_scenario_level(const ParaScenario& sc) {
  return sc.stage == ParaStage::slope ? skier::synthetic_slope() : skier::flat_strip(sc.pitch, 1);
}

ParaRun run_para_scenario(const ParaScenario& sc, double hz) {
  const mg::Level level = para_scenario_level(sc);
  const skier::TreeIndex trees = skier::build_tree_index(level);
  const ParaAirField field = make_air_field(level.seed);
  const skier::World sw{&level, &trees};
  const skier::SkiSpec& spec = skier::kReferenceSkis;
  ParaWorld world{&level, &trees, &field, ParaWind{sc.wind_x, sc.wind_z}, 0};
  const double dt = 1 / hz;
  skier::SkierState s = skier::stand_skier(sw, sc.x, sc.z, sc.heading, sc.speed, sc.height);
  ParaState p = ready_para(s);
  if (sc.height > 0) {
    // Hung in the air under the wing, flying: the wing straight over him on taut lines.
    p.mode = ParaMode::flown;
    p.flying = true;
    p.flights = 1;
    p.air_time = spec::launch::kFlight;
    p.x = s.x;
    p.y = s.y + spec::kHarness + spec::wing::kLines;
    p.z = s.z;
    p.agl = sc.height;
    // ...sinking as a trimmed wing glides, so the air meets it at its trim.
    s.vy = p.vy = -sc.speed / 4;
  }
  const double y0 = s.y;
  ParaRun run;
  uint64_t digest = 0xcbf29ce484222325ull;
  const auto steps = static_cast<int64_t>(num::round(sc.seconds * hz));
  const auto every = static_cast<int64_t>(num::round(hz / 10));
  double px = s.x;
  double pz = s.z;
  double last_heading = s.heading;
  double sink_sum = 0, air_sum = 0;
  int64_t air_n = 0;
  bool dropped = false;
  for (int64_t i = 0; i < steps; i++) {
    const double t = static_cast<double>(i) * dt;
    world.now = t;
    if (i % every == 0) {
      run.trace.push_back(ParaSample{t, s.x, s.z, s.y, level.ground_at(s.x, s.z), s.speed, s.vy,
                                     p.bank, p.fold, p.flying});
    }
    const ParaControls c = sc.hand ? sc.hand(t, p) : para_pilot(p, s, world, sc.aim);
    ParaEvents pe;
    if (!dropped && sc.drop_at >= 0 && t >= sc.drop_at) {
      dropped = true;
      drop_para(p, s, ParaCut::drop, pe);
    }
    // In the harness his skis do nothing; on the snow under the rig they are his: the throttle
    // tucks him, the toggles steer, the brakes pivot the skis across. Without it he just skis.
    skier::SkierInput in;
    if (para_rigged(p) && !para_hangs(p)) in = skier::SkierInput{c.steer, c.throttle, c.brake};
    skier::SkierEvents se;
    skier::step_skier(s, in, sw, spec, dt, se);
    step_para(p, s, c, world, spec, dt, pe);
    digest = para_hash(p, skier::skier_hash(s, digest));
    if (pe.launched && run.launch < 0) run.launch = t + dt;
    if (pe.took_off && run.take_off < 0) run.take_off = t + dt;
    if (pe.folded) run.folds++;
    if (pe.looped) run.loops++;
    if (pe.tangled && run.tangle < 0) run.tangle = t + dt;
    if (p.mode == ParaMode::flown) run.top_g = num::max(run.top_g, p.g);
    run.top_sink = num::max(run.top_sink, -s.vy);
    if (p.mode == ParaMode::flown) run.top_airspeed = num::max(run.top_airspeed, p.airspeed);
    if (pe.touched) run.touches++;
    if (se.landed) run.landing_g = num::max(run.landing_g, se.landing_g);
    if (se.wiped_out) run.wiped_out = true;
    if (pe.cut != ParaCut::none && run.cut == ParaCut::none) {
      run.cut = pe.cut;
      run.cut_time = t + dt;
    }
    run.top_speed = num::max(run.top_speed, s.speed);
    run.distance += core::hypot(s.x - px, s.z - pz);
    px = s.x;
    pz = s.z;
    run.climbed = num::max(run.climbed, s.y - y0);
    run.top_climb = num::max(run.top_climb, s.vy);
    if (p.flying) run.max_bank = num::max(run.max_bank, std::fabs(p.bank));
    run.stalled = run.stalled || p.stalled;
    run.deepest_fold = num::max(run.deepest_fold, p.fold);
    run.turned += core::angle_diff(last_heading, s.heading);
    last_heading = s.heading;
    if (p.flying && t >= sc.seconds / 2) {
      sink_sum += -s.vy;
      air_sum += p.airspeed;
      air_n++;
    }
    if (!sc.hand && sc.aim.has_aim) {
      const double d = core::hypot(s.x - sc.aim.x, s.z - sc.aim.z);
      run.closest = run.closest < 0 ? d : num::min(run.closest, d);
    }
  }
  if (air_n > 0) {
    run.sink = sink_sum / static_cast<double>(air_n);
    run.airspeed = air_sum / static_cast<double>(air_n);
  }
  run.end = p;
  run.skier = s;
  run.digest = digest;
  return run;
}

}  // namespace ge::paramotor
