// The scenario table and its runner (scenarios.hpp).
//
// ge:summary The scenario table and run_scenario: stage, script, step and measure
// ge:keywords scenario table run_scenario measure trace digest
// ge:skill skier labs
// ge:test tests/skier_scenarios_test.cpp
#include "skier/scenarios.hpp"

#include "core/math.hpp"
#include "skier/synthetic.hpp"
#include "skier/trees.hpp"

namespace ge::skier {

namespace {

constexpr double kDeg = core::kPi / 180;
// The seed the body's risk draws hash in every scenario.
constexpr uint32_t kBodySeed = 1;
constexpr double kMid = kStripSize / 2;

SkierInput tuck(double, const SkierState&) { return SkierInput{0, 1, 0, false}; }
SkierInput tall(double, const SkierState&) { return SkierInput{}; }
SkierInput carve_half(double, const SkierState&) { return SkierInput{0.5, 0, 0, false}; }
SkierInput carve_full(double, const SkierState&) { return SkierInput{1, 0, 0, false}; }
SkierInput brake(double, const SkierState&) { return SkierInput{0, 0, 1, false}; }
SkierInput turns(double t, const SkierState&) {
  const bool right = static_cast<int>(t / 1.5) % 2 == 0;
  return SkierInput{right ? 0.8 : -0.8, 0, 0, false};
}
SkierInput pop_at_lip(double, const SkierState& s) {
  return SkierInput{0, 0, 0, s.z < slope::kKickerLip - 0.6};
}
SkierInput pop_now(double t, const SkierState&) { return SkierInput{0, 0, 0, t < 0.8}; }
SkierInput spin_off(double t, const SkierState& s) {
  return SkierInput{!s.on_snow && t < 0.6 ? 1.0 : 0.0, 0, 0, false};
}

constexpr Scenario kScenarios[] = {
    {"tuck-schuss", "a full tuck down a 20 deg groomer from rest: the top speed meets the terminal",
     Stage::strip, 20 * kDeg, 1, kMid, 20, 0, 0, 0, 90, true, tuck},
    {"tall-schuss", "stood tall down the same groomer: a lower terminal than the tuck's",
     Stage::strip, 20 * kDeg, 1, kMid, 20, 0, 0, 0, 90, true, tall},
    {"powder-schuss", "a tuck down 20 deg of untouched powder: slower, the skis planing up",
     Stage::strip, 20 * kDeg, 0, kMid, 20, 0, 0, 0, 90, true, tuck},
    {"carve", "half the steer held at 15 m/s on a 15 deg groomer: a clean arc, no skid",
     Stage::strip, 15 * kDeg, 1, kMid, 400, 0, 15, 0, 6, false, carve_half},
    {"carve-hard", "full steer at 20 m/s on 15 deg: the edge's grip holds the bend", Stage::strip,
     15 * kDeg, 1, kMid, 400, 0, 20, 0, 4, false, carve_full},
    {"hockey-stop", "full brake at 15 m/s on a 10 deg groomer: stopped in some 25 m", Stage::strip,
     10 * kDeg, 1, kMid, 400, 0, 15, 0, 6, false, brake},
    {"stand-still", "the brake held at rest on a 10 deg groomer: he stays where he is",
     Stage::strip, 10 * kDeg, 1, kMid, 400, 0, 0, 0, 4, false, brake},
    {"skate-flat", "the tuck held at rest on the dead flat: he skates and poles up to a jog",
     Stage::strip, 0, 1, kMid, 400, 0, 0, 0, 12, false, tuck},
    {"turns", "steer swung +-0.8 every 1.5 s down a 20 deg groomer: linked turns, no wipeout",
     Stage::strip, 20 * kDeg, 1, kMid, 20, 0, 12, 0, 20, false, turns},
    {"powder-turns", "the same turns in powder: wider and slower", Stage::strip, 20 * kDeg, 0, kMid,
     20, 0, 12, 0, 20, false, turns},
    {"kicker", "straight over K1 at 14 m/s: the ramp kicks him into the air, landed clean",
     Stage::slope, 0, 0, slope::kCentre, 280, 0, 14, 0, 9, false, tall},
    {"kicker-pop", "the jump loaded up the ramp and let go at the lip: a longer flight",
     Stage::slope, 0, 0, slope::kCentre, 280, 0, 14, 0, 9, false, pop_at_lip},
    {"pop", "a jump loaded 0.8 s at 10 m/s on a 10 deg groomer: a hop of a few tenths",
     Stage::strip, 10 * kDeg, 1, kMid, 400, 0, 10, 0, 3, false, pop_now},
    {"rollers", "straight through the rollers at 20 m/s: skips over the crests, no wipeout",
     Stage::slope, 0, 0, slope::kCentre, 120, 0, 20, 0, 7, false, tall},
    {"tree", "straight at the lone tree at 12 m/s: a wipeout on its trunk", Stage::slope, 0, 0,
     slope::kTreeX, 200, 0, 12, 0, 8, false, tall},
    {"drop", "dropped 2 m onto the flat at 10 m/s: a firm landing, ridden away", Stage::strip, 0, 1,
     kMid, 400, 0, 10, 2, 3, false, tall},
    {"drop-harsh", "dropped 7 m onto the flat at 10 m/s: the legs fold, a wipeout", Stage::strip, 0,
     1, kMid, 400, 0, 10, 7, 8, false, tall},
    {"tree-slow", "straight at the lone tree from 6 m short of it at 6 m/s: bruised, not broken",
     Stage::slope, 0, 0, slope::kTreeX, 254, 0, 6, 0, 4, false, tall},
    {"tree-fast", "straight at the lone tree at 20 m/s: the skis stop on it and he goes on into it",
     Stage::slope, 0, 0, slope::kTreeX, 200, 0, 20, 0, 12, false, tall},
    {"tree-side", "past the lone tree at 15 m/s, half a metre off its axis: his side meets it",
     Stage::slope, 0, 0, slope::kTreeX + 0.5, 200, 0, 15, 0, 8, false, tall},
    {"drop-across", "dropped 3 m at 10 m/s, spun right in the air: landed across, a ski catches",
     Stage::strip, 0, 1, kMid, 400, 0, 10, 3, 8, false, spin_off},
    {"drop-high", "dropped 12 m onto the flat at 10 m/s: the legs and the spine break",
     Stage::strip, 0, 1, kMid, 400, 0, 10, 12, 10, false, tall},
};

}  // namespace

std::span<const Scenario> scenarios() { return kScenarios; }

const Scenario* find_scenario(std::string_view id) {
  for (const Scenario& sc : kScenarios) {
    if (id == sc.id) return &sc;
  }
  return nullptr;
}

mountain::mapgen::Level scenario_level(const Scenario& sc) {
  return sc.stage == Stage::slope ? synthetic_slope() : flat_strip(sc.pitch, sc.packed);
}

ScenarioRun run_scenario(const Scenario& sc, const SkiSpec& spec, double hz,
                         const std::function<void(const ScenarioStep&)>& each) {
  const mountain::mapgen::Level level = scenario_level(sc);
  const TreeIndex trees = build_tree_index(level);
  const World world{&level, &trees};
  const double dt = 1 / hz;
  const auto steps = static_cast<int64_t>(core::num::round(sc.seconds * hz));
  const auto every = static_cast<int64_t>(core::num::round(hz / 10));
  ScenarioRun run;
  SkierState s = stand_skier(world, sc.x, sc.z, sc.heading, sc.speed, sc.lift);
  run.top_speed = s.speed;
  uint64_t h = 0xcbf29ce484222325ull;
  SkierEvents ev;
  BodyEvents bev;
  double from_x = s.x;
  double from_z = s.z;
  for (int64_t i = 0; i < steps; i++) {
    const double t = static_cast<double>(i) * dt;
    if (i % every == 0) {
      run.trace.push_back(TraceSample{t, s.x, s.z, s.y, s.speed,
                                      s.y - (level.ground_at(s.x, s.z) - s.sink), s.on_snow,
                                      s.down});
    }
    const double x0 = s.x;
    const double z0 = s.z;
    const SkierInput in = sc.input(t, s);
    step_skier(s, in, world, spec, dt, ev);
    const double now = t + dt;
    step_body(run.body, s, ev, world, spec, BodyClock{now, dt, i, kBodySeed}, bev);
    h = skier_hash(s, h);
    if (each) each(ScenarioStep{now, s, in, ev, level.ground_at(s.x, s.z)});
    if (bev.mortal) run.mortal_time = now;
    if (bev.died) run.death_time = now;
    run.distance += core::hypot(s.x - x0, s.z - z0);
    run.top_speed = core::num::max(run.top_speed, s.speed);
    const double g_lat = s.g_lat < 0 ? -s.g_lat : s.g_lat;
    if (!s.down) run.max_g_lat = core::num::max(run.max_g_lat, g_lat);
    const double yaw = s.yaw_rate < 0 ? -s.yaw_rate : s.yaw_rate;
    if (s.on_snow && !s.down && s.speed > 5 && yaw > 1e-3) {
      const double r = s.speed / yaw;
      if (run.min_radius == 0 || r < run.min_radius) run.min_radius = r;
    }
    if (ev.landed) {
      run.flights++;
      run.longest_flight = core::num::max(run.longest_flight, ev.flight_s);
      run.max_landing_g = core::num::max(run.max_landing_g, ev.landing_g);
    }
    if (ev.glanced) run.glances++;
    if (ev.wiped_out && run.cause == Wipeout::none) {
      run.cause = ev.cause;
      run.wipeout_time = now;
      from_x = s.x;
      from_z = s.z;
    }
    if (s.thrown.active) {
      run.tumble = s.thrown.tumble;
      run.thrown = core::hypot(s.thrown.x - from_x, s.thrown.z - from_z);
      run.thrown_top =
          core::num::max(run.thrown_top, s.thrown.y - level.ground_at(s.thrown.x, s.thrown.z));
      if (run.lie_time < 0 && s.thrown.still >= 1 && run.wipeout_time >= 0) {
        run.lie_time = now - run.wipeout_time;
      }
    }
    if (run.stop_time < 0 && s.speed < 0.3 && (sc.speed > 0 || now > 1)) run.stop_time = now;
  }
  run.end = s;
  run.digest = h;
  run.body_digest = body_hash(run.body, 0xcbf29ce484222325ull);
  return run;
}

}  // namespace ge::skier
