// LAB `lift-ride`: every lift of each seed's ski area ridden from its load zone to its top
// through the real `step`. THE LIFTS table: the lift, its kind, its plan length, its carriers and
// the rope's speed, the headway between carriers at the load point, the wait for one, the ride
// up, the slowest and fastest he was carried, the biggest jump he made in one step while carried,
// and how far from `mapgen::let_go_of`'s point he was let go. A lift that does not let him go
// within the limit, a carried step longer than `kMostJump` (a carrier that leaps on its loop),
// or a let-go anywhere but the lift's own point is a failure (exit 1). The picture: the ski area
// from above with every lift's line, its carriers where the clock puts them at the run's start
// (white on the way up, grey on the way down) and its load zone and let-go point ringed.
//
// ge:summary Lab: every lift of a seed ridden from its load zone to its top: wait, ride, let-go
// ge:keywords lab lift-ride lift carrier carrier_at load-zone wait ride let-go headway jump
// ge:skill sim labs
// ge:test tests/fall_line_lift_ride_test.cpp
#include <memory>
#include <span>
#include <string>
#include <vector>

#include "core/math.hpp"
#include "core/num_text.hpp"
#include "fall_line/sim/free.hpp"
#include "fall_line/sim/game.hpp"
#include "fall_line/sim/lift_ride.hpp"
#include "lab/lab.hpp"
#include "lab/raster.hpp"
#include "mountain/labs/level_map.hpp"
#include "mountain/lifts/lift_loop.hpp"
#include "mountain/mapgen/generate.hpp"
#include "mountain/mapgen/lift_line.hpp"

namespace {

namespace fs = fall_line::sim;
namespace mg = ge::mountain::mapgen;
namespace ml = ge::mountain::lifts;
namespace num = ge::core::num;
using ge::lab::Rgba;

// The longest a carried rider may move in one step, m (the fastest rope covers 0.05 m).
constexpr double kMostJump = 0.5;
// How far from the lift's let-go point he may be let go, m.
constexpr double kLetGoReach = 0.01;

std::string fixed(double v, int d) { return num::to_fixed(v, d); }

const char* kind_word(mg::LiftKind k) {
  switch (k) {
    case mg::LiftKind::gondola:
      return "gondola";
    case mg::LiftKind::chair:
      return "chair";
    case mg::LiftKind::drag:
      return "drag";
  }
  return "?";
}

struct Ride {
  bool off = false;
  double wait = 0;
  double ride = 0;
  double slowest = 1e9;
  double fastest = 0;
  double jump = 0;
  double miss = 0;  // m from the let-go point
};

Ride ride_lift(std::shared_ptr<const mg::Level> level, int32_t lift, double limit) {
  fs::GameState state = fs::create_free_ride(std::move(level));
  fs::wait_for_lift(state, 0, lift);
  Ride r;
  const auto steps = static_cast<int64_t>(num::round(limit * fs::kHz));
  const fs::Controls none{};
  double x = 0;
  double z = 0;
  bool was_carried = false;
  while (state.steps < steps) {
    fs::step(state, std::span<const fs::Controls>(&none, 1));
    const fs::Racer& me = state.racers[0];
    for (const fs::GameEvent& e : state.events) {
      if (e.kind != fs::EventKind::unloaded) continue;
      r.off = true;
      r.ride = e.value;
      r.wait = state.t - e.value;
    }
    if (r.off) {
      const ml::LiftPlan& p = (*state.lifts)[static_cast<size_t>(lift)];
      const mg::LetGo at = mg::let_go_of(p.kind, p.bottom, p.top);
      r.miss = ge::core::hypot(me.skier.x - at.x, me.skier.z - at.z);
      break;
    }
    const bool carried = me.free.lift.phase == fs::LiftPhase::carried;
    if (carried) {
      if (was_carried) r.jump = num::max(r.jump, ge::core::hypot(me.skier.x - x, me.skier.z - z));
      r.slowest = num::min(r.slowest, me.skier.speed);
      r.fastest = num::max(r.fastest, me.skier.speed);
    }
    was_carried = carried;
    x = me.skier.x;
    z = me.skier.z;
  }
  return r;
}

void run(ge::lab::Lab& lab) {
  const std::vector<double> seeds = lab.arg_nums("seeds");
  const bool pictures = !lab.arg_bool("no-pictures");
  const auto version = static_cast<int>(lab.arg_num("version"));
  const double limit = lab.arg_num("limit");
  ge::lab::LabTable table{"lifts",
                          {"seed", "lift", "kind", "length m", "carriers", "rope m/s", "headway s",
                           "wait s", "ride s", "slowest m/s", "fastest m/s", "jump m", "let-go m"},
                          {}};
  int lifts = 0;
  int let_off = 0;
  for (const double seed : seeds) {
    const std::string k = "seed-" + fixed(seed, 0);
    std::shared_ptr<const mg::Level> level;
    try {
      level = std::make_shared<const mg::Level>(
          fs::free_map(seed, version > 0 ? std::optional<int>(version) : std::nullopt));
    } catch (const mg::GenerateError& e) {
      lab.fail("seed " + fixed(seed, 0) + " built no map: " + e.what(),
               "Run `make lab LAB=analyze ARGS=--seeds=" + fixed(seed, 0) +
                   " --refusals` (skill: mountain).");
      continue;
    }
    const std::vector<ml::LiftPlan> plans = ml::lift_plans(*level);
    if (plans.empty()) {
      lab.fail(k + ": the map has no lift (generator v" + std::to_string(level->version) + ")",
               "Pass --version=8 or later, or leave it at 0 for the current rules (skill: sim).");
      continue;
    }
    for (const ml::LiftPlan& p : plans) {
      lifts++;
      const std::string& id = level->resort->lifts[static_cast<size_t>(p.lift)].id;
      std::string key = k + ".";
      key += id;
      const Ride r = ride_lift(level, p.lift, limit);
      const double span = p.clock.empty() ? p.loop : p.span;
      const double headway = span / static_cast<double>(p.carriers) / p.ride.speed;
      if (!r.off) {
        lab.fail(key + ": the lift did not let him go within " + fixed(limit, 0) + " s",
                 "Read plugins/mountain/lifts/lift_loop.cpp (carrier_at, off_at) and "
                 "games/fall_line/sim/lift_ride.cpp (step_lift) (skill: sim).");
      } else {
        let_off++;
      }
      if (r.jump > kMostJump) {
        lab.fail(key + ": a carried step jumped " + fixed(r.jump, 2) + " m",
                 "A carrier leaps on its loop: read loop_spot and loop_at in "
                 "plugins/mountain/lifts/lift_loop.cpp (skill: mountain).");
      }
      if (r.off && r.miss > kLetGoReach) {
        lab.fail(key + ": let go " + fixed(r.miss, 2) + " m from the lift's let-go point",
                 "Read let_go in games/fall_line/sim/lift_ride.cpp (skill: sim).");
      }
      lab.metric(key + ".wait", r.wait, "s");
      lab.metric(key + ".ride", r.ride, "s");
      lab.metric(key + ".jump", r.jump, "m");
      table.rows.push_back({fixed(seed, 0), id, kind_word(p.kind), fixed(p.length, 0),
                            std::to_string(p.carriers), fixed(p.ride.speed, 1), fixed(headway, 2),
                            fixed(r.wait, 2), fixed(r.ride, 1), fixed(r.slowest, 2),
                            fixed(r.fastest, 2), fixed(r.jump, 3), fixed(r.miss, 3)});
    }
    if (!pictures) continue;
    constexpr int kPx = 1024;
    ge::lab::Picture pic = ge::mountain::labs::draw_map(*level, kPx);
    const double px = kPx / level->size;
    for (const ml::LiftPlan& p : plans) {
      ge::lab::draw_line(pic, p.bottom.x * px, p.bottom.z * px, p.top.x * px, p.top.z * px,
                         Rgba{40, 40, 40}, 1);
      for (int32_t c = 0; c < p.carriers; c++) {
        const ml::LoopSpot spot = ml::carrier_at(p, c, 0);
        const ml::Place at = ml::carrier_place(p, spot);
        ge::lab::fill_disc(pic, at.x * px, at.z * px, 1.5,
                           spot.side == 0 ? Rgba{255, 255, 255} : Rgba{150, 150, 150});
      }
      const ml::Place load = ml::load_spot(p);
      const mg::LetGo off = mg::let_go_of(p.kind, p.bottom, p.top);
      ge::lab::draw_ring(pic, load.x * px, load.z * px, 5, Rgba{0, 200, 230});
      ge::lab::draw_ring(pic, off.x * px, off.z * px, 5, Rgba{255, 160, 0});
    }
    lab.shot({k,
              "SEED " + fixed(seed, 0) + "'S LIFTS AND THEIR CARRIERS AT THE START",
              "The ski area from above (+x right, +z down; the face shaded by height, the woods "
              "dark green) with every lift's line, its carriers where the clock puts them at "
              "the run's start (white on the up rope, grey on the down), its load zone ringed "
              "cyan at the bottom and its let-go point ringed orange at the top.",
              "carriers spaced along every line, bunched in the terminals of the chairs and "
              "gondolas; a cyan ring at each bottom, an orange ring near each top",
              {{"seed", fixed(seed, 0)}, {"version", std::to_string(level->version)}}},
             pic);
  }
  lab.metric("lifts", lifts, "count");
  lab.metric("let-off", let_off, "count");
  lab.table(std::move(table));
}

const ge::lab::LabSpec kSpec{
    "lift-ride",
    ge::lab::LabKind::drive,
    "game:fall_line",
    "plugins/mountain/lifts (the lift plan, carrier_at) and games/fall_line/sim (the lift ride)",
    "sim",
    "Does every lift of each seed's ski area take a rider from its load zone, carry him smoothly "
    "up its line and let him go at its own let-go point, and how long is the wait and the ride?",
    {
        {"seeds", "38", "seeds, comma-separated"},
        {"version", "10", "the generator's version (8 or later; 0 the current rules)"},
        {"limit", "1200", "a ride's time limit, s"},
        {"no-pictures", "", "skip the map pictures", true},
    },
    "--seeds=38",
    run,
};
GE_LAB(kSpec);

}  // namespace
