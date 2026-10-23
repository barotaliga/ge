// LAB `crowd`: the free ride's crowd on a real ski area. `--count` amateurs are dealt onto seed
// `--seed`'s runs (`deal_crowd`) and the free ride runs through the real `step` for `--limit`
// seconds with the player stood still at the head of his first run: THE CROWD table (how many
// are doing what at the end), THE KINDS table (per kind of skier: how many, the mean speed while
// skiing, the falls and stops a minute, the runs skied out), the lifts (the riders let off at
// each top), the crowd's cost a step, and the map with every amateur's path for the first minute
// and where each one is at the end.
//
// A lift nobody is ever let off the top of, an amateur skiing yet still for a minute, or a place
// that is not a number is a failure (exit 1).
//
// ge:summary Lab: the crowd on seed 38's runs and lifts: what each is doing, the kinds, the lifts
// ge:keywords lab crowd amateurs kinds groups lifts queue ride stuck paths cost
// ge:skill sim labs
// ge:test tests/fall_line_crowd_test.cpp
#include <chrono>
#include <cmath>
#include <memory>
#include <span>
#include <string>
#include <vector>

#include "core/math.hpp"
#include "core/num_text.hpp"
#include "fall_line/sim/crowd.hpp"
#include "fall_line/sim/free.hpp"
#include "fall_line/sim/game.hpp"
#include "fall_line/sim/wanted.hpp"
#include "lab/lab.hpp"
#include "lab/raster.hpp"
#include "lab/stats.hpp"
#include "mountain/labs/level_map.hpp"
#include "mountain/lifts/lift_loop.hpp"
#include "mountain/mapgen/generate.hpp"
#include "skier/skier.hpp"

namespace {

namespace fs = fall_line::sim;
namespace fc = fall_line::sim::crowd;
namespace mg = ge::mountain::mapgen;
namespace ml = ge::mountain::lifts;
using ge::lab::Rgba;

std::string fixed(double v, int d) { return ge::core::num::to_fixed(v, d); }

constexpr size_t kModes = 7;
// How long an amateur may ski without moving before he counts as stuck, s, and how slow, m/s.
constexpr double kStuckS = 60;
constexpr double kStuckSpeed = 0.3;
// How many amateurs' paths the picture draws, and for how long, s.
constexpr size_t kPaths = 60;
// The most an amateur may move in a step, m: faster than anyone skis or a lift runs.
constexpr double kJumpM = 1;
constexpr double kPathS = 60;

Rgba mode_colour(fs::AmateurMode m) {
  switch (m) {
    case fs::AmateurMode::ski:
      return {255, 255, 0};
    case fs::AmateurMode::stop:
      return {255, 140, 0};
    case fs::AmateurMode::down:
      return {255, 0, 0};
    case fs::AmateurMode::queue:
      return {0, 200, 255};
    case fs::AmateurMode::ride:
      return {255, 0, 255};
    case fs::AmateurMode::skate:
      return {255, 255, 255};
    case fs::AmateurMode::unseen:
      return {0, 0, 0};
  }
  return {0, 0, 0};
}

struct KindTally {
  int32_t count = 0;
  double speed = 0;  // summed over the steps skiing
  double skiing = 0;
  int32_t falls = 0;
  int32_t stops = 0;
  int32_t runs = 0;
};

void run(ge::lab::Lab& lab) {
  const double seed = lab.arg_num("seed");
  const auto version = static_cast<int>(lab.arg_num("version"));
  const double limit = lab.arg_num("limit");
  const auto count = static_cast<int32_t>(lab.arg_num("count"));
  const int px = static_cast<int>(lab.arg_num("px"));
  const bool pictures = !lab.arg_bool("no-pictures");
  std::shared_ptr<const mg::Level> level;
  try {
    level = std::make_shared<const mg::Level>(fs::free_map(seed, version));
  } catch (const mg::GenerateError& e) {
    lab.fail("seed " + fixed(seed, 0) + " built no map: " + e.what(),
             "Run `make lab LAB=analyze ARGS=--seeds=" + fixed(seed, 0) +
                 " --refusals` (skill: mountain).");
    return;
  }
  fs::GameState st = fs::create_free_ride(level, 0);
  fs::deal_crowd(st, count);
  const fs::CrowdState& crowd = st.crowd;
  if (!crowd.active) {
    lab.fail("seed " + fixed(seed, 0) + " v" + fixed(version, 0) + " dealt no crowd",
             "Read deal_crowd in games/fall_line/sim/crowd.cpp: a free ride with runs deals one "
             "(skill: sim).");
    return;
  }
  // THE NETWORK as the crowd reads it: each run's colour, where it goes from its end, how far that
  // end is from the lift at its foot.
  ge::lab::LabTable net{
      "runs", {"run", "colour", "length m", "top", "into", "foot lift", "foot m"}, {}};
  constexpr const char* kColours[] = {"green", "blue", "red", "black", "road"};
  for (size_t i = 0; i < crowd.net.size(); i++) {
    const fs::CrowdRun& r = crowd.net[i];
    const auto& end = (*st.runs)[i].points.back();
    std::string foot = "-";
    std::string far = "-";
    if (r.foot >= 0 && level->resort) {
      const ml::LiftPlan& p = (*st.lifts)[static_cast<size_t>(r.foot)];
      foot = level->resort->lifts[static_cast<size_t>(r.foot)].id;
      far = fixed(ge::core::hypot(p.bottom.x - end.x, p.bottom.z - end.z), 0);
    }
    const std::string id = level->resort ? level->resort->runs[i].id : std::to_string(i);
    const std::string into =
        r.into >= 0 && level->resort ? level->resort->runs[static_cast<size_t>(r.into)].id : "-";
    net.rows.push_back(
        {id, kColours[r.grade], fixed(r.length, 0), r.top ? "yes" : "no", into, foot, far});
  }
  lab.table(std::move(net));
  const ge::skier::SkierState stood = st.racers[0].skier;
  const size_t n = crowd.amateurs.size();
  const size_t lifts = crowd.queues.size();
  std::vector<fs::AmateurMode> was(n);
  std::vector<int32_t> run_was(n);
  std::vector<double> still(n, 0);
  std::vector<KindTally> kinds(fc::kKindCount);
  std::vector<int32_t> let_off(lifts, 0);
  std::vector<double> first_up(lifts, -1);  // when each lift first took anyone up, s
  std::vector<std::vector<fs::Spot>> paths(n < kPaths ? n : kPaths);
  for (size_t i = 0; i < n; i++) {
    was[i] = crowd.amateurs[i].mode;
    run_was[i] = crowd.amateurs[i].run;
    kinds[static_cast<size_t>(crowd.amateurs[i].kind)].count++;
  }
  int32_t stuck = 0;
  int32_t bad = 0;
  int32_t jumps = 0;  // an amateur moved further in a step than anyone skis or rides
  std::string first_jump;
  std::vector<double> px0(n), pz0(n);
  for (size_t i = 0; i < n; i++) {
    px0[i] = crowd.amateurs[i].x;
    pz0[i] = crowd.amateurs[i].z;
  }
  int32_t bumps = 0;
  std::vector<double> cost;
  cost.reserve(static_cast<size_t>(limit * fs::kHz) + 1);
  const fs::Controls none{};
  while (st.t < limit) {
    st.racers[0].skier = stood;
    const auto t0 = std::chrono::steady_clock::now();
    fs::step(st, std::span<const fs::Controls>(&none, 1));
    cost.push_back(
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count());
    for (const fs::GameEvent& e : st.events) bumps += e.kind == fs::EventKind::bump ? 1 : 0;
    const bool second = st.steps % static_cast<int64_t>(fs::kHz) == 0;
    for (size_t i = 0; i < n; i++) {
      const fs::Amateur& a = crowd.amateurs[i];
      KindTally& k = kinds[static_cast<size_t>(a.kind)];
      if (a.mode == fs::AmateurMode::down && was[i] != fs::AmateurMode::down) k.falls++;
      if (a.mode == fs::AmateurMode::stop && was[i] != fs::AmateurMode::stop) k.stops++;
      if (a.mode == fs::AmateurMode::ride && was[i] != fs::AmateurMode::ride &&
          first_up[static_cast<size_t>(a.lift)] < 0) {
        first_up[static_cast<size_t>(a.lift)] = st.t;
      }
      if (a.mode == fs::AmateurMode::skate && was[i] == fs::AmateurMode::ride) {
        let_off[static_cast<size_t>(a.lift)]++;
      }
      const bool on_run = a.mode == fs::AmateurMode::ski || a.mode == fs::AmateurMode::stop ||
                          a.mode == fs::AmateurMode::down;
      if (on_run && a.run != run_was[i]) k.runs++;
      if ((was[i] == fs::AmateurMode::ski || was[i] == fs::AmateurMode::stop) &&
          (a.mode == fs::AmateurMode::queue || a.mode == fs::AmateurMode::unseen)) {
        k.runs++;
      }
      if (a.mode == fs::AmateurMode::ski) {
        k.speed += a.speed;
        k.skiing += 1;
        still[i] = a.speed < kStuckSpeed ? still[i] + fs::kDt : 0;
        if (still[i] >= kStuckS && still[i] < kStuckS + fs::kDt) stuck++;
      } else {
        still[i] = 0;
      }
      if (!std::isfinite(a.x) || !std::isfinite(a.z) || !std::isfinite(a.y)) bad++;
      const double moved = ge::core::hypot(a.x - px0[i], a.z - pz0[i]);
      // Up a lift unseen he comes back at a run's head: the one place he is meant to.
      if (moved > kJumpM && was[i] != fs::AmateurMode::unseen) {
        if (jumps++ == 0) {
          first_jump = "amateur " + std::to_string(a.id) + " " + fs::amateur_mode_name(was[i]) +
                       " -> " + fs::amateur_mode_name(a.mode) + " on run " +
                       std::to_string(run_was[i]) + " -> " + std::to_string(a.run) + " moved " +
                       fixed(moved, 1) + " m at " + fixed(st.t, 2) + " s";
        }
      }
      px0[i] = a.x;
      pz0[i] = a.z;
      was[i] = a.mode;
      run_was[i] = a.run;
      if (second && i < paths.size() && st.t <= kPathS) paths[i].push_back({a.x, a.y, a.z});
    }
  }
  std::vector<int32_t> modes(kModes, 0);
  for (const fs::Amateur& a : crowd.amateurs) modes[static_cast<size_t>(a.mode)]++;
  ge::lab::LabTable table{"crowd", {"doing", "amateurs", "share %"}, {}};
  for (size_t m = 0; m < kModes; m++) {
    const char* name = fs::amateur_mode_name(static_cast<fs::AmateurMode>(m));
    table.rows.push_back(
        {name, std::to_string(modes[m]), fixed(100.0 * modes[m] / static_cast<double>(n), 1)});
    lab.metric(std::string("end.") + name, modes[m], "amateurs");
  }
  lab.table(std::move(table));
  const double minutes = limit / 60;
  ge::lab::LabTable kind_table{
      "kinds", {"kind", "amateurs", "mean speed km/h", "falls/min", "stops/min", "runs"}, {}};
  for (size_t k = 0; k < kinds.size(); k++) {
    const KindTally& t = kinds[k];
    if (t.count == 0) continue;
    const char* name = fc::kind_name(static_cast<fc::Kind>(k));
    const double mean = t.skiing > 0 ? t.speed / t.skiing * 3.6 : 0;
    kind_table.rows.push_back({name, std::to_string(t.count), fixed(mean, 1),
                               fixed(t.falls / minutes / t.count, 3),
                               fixed(t.stops / minutes / t.count, 3), std::to_string(t.runs)});
    lab.metric(std::string("kind.") + name + ".speed", mean, "km/h");
    lab.metric(std::string("kind.") + name + ".falls", t.falls, "count");
  }
  lab.table(std::move(kind_table));
  ge::lab::LabTable lift_table{
      "lifts", {"lift", "kind", "ride s", "first up s", "let off", "still queued"}, {}};
  int32_t rides = 0;
  int32_t idle = 0;
  for (size_t k = 0; k < lifts; k++) {
    const fs::CrowdQueue& q = crowd.queues[k];
    const ml::LiftPlan& plan = (*st.lifts)[k];
    // The ride up, s: the line at the rope's speed, and half again for the terminals.
    const double ride = plan.length / plan.ride.speed * 1.5;
    lift_table.rows.push_back(
        {level->resort ? level->resort->lifts[k].id : std::to_string(k),
         level->resort ? mg::lift_kind_name(level->resort->lifts[k].kind) : "-", fixed(ride, 0),
         first_up[k] >= 0 ? fixed(first_up[k], 1) : "-", std::to_string(let_off[k]),
         std::to_string(q.issued - q.served)});
    rides += let_off[k];
    // A lift that took riders up long enough ago to have let them off its top, and never did.
    if (first_up[k] >= 0 && first_up[k] + ride < limit && let_off[k] == 0) idle++;
  }
  lab.table(std::move(lift_table));
  lab.metric("amateurs", static_cast<double>(n), "count");
  lab.metric("lift-rides", rides, "count");
  lab.metric("bumps", bumps, "count");
  lab.metric("stuck", stuck, "amateurs");
  lab.metric("jumps", jumps, "count");
  if (jumps > 0) {
    lab.fail(std::to_string(jumps) + " times an amateur jumped more than " + fixed(kJumpM, 0) +
                 " m in a step; the first: " + first_jump,
             "Read run_out in games/fall_line/sim/crowd.cpp and let_go and skate_off in "
             "crowd_lift.cpp for the change named (skill: sim).");
  }
  const ge::lab::Summary c = ge::lab::summarize(cost);
  lab.metric("step.median", c.p50, "ms", ge::lab::Det::measured);
  lab.metric("step.p95", c.p95, "ms", ge::lab::Det::measured);
  if (bad > 0) {
    lab.fail(std::to_string(bad) + " amateur places were not numbers",
             "Run `make test T=fall_line_crowd` and read sample_run and place_on_run in "
             "games/fall_line/sim/crowd.cpp (skill: sim).");
  }
  if (idle > 0) {
    lab.fail(std::to_string(idle) +
                 " lifts took riders up and let nobody off their top in a "
                 "ride's time",
             "Read ride and let_go in games/fall_line/sim/crowd_lift.cpp; the lifts table names "
             "the lift (skill: sim).");
  }
  if (stuck > 0) {
    lab.fail(
        std::to_string(stuck) + " amateurs skied yet stood still for " + fixed(kStuckS, 0) + " s",
        "Look at previews/crowd/map.png for where they are; read move in "
        "games/fall_line/sim/crowd.cpp (skill: sim).");
  }
  if (!pictures) return;
  ge::lab::Picture pic = ge::mountain::labs::draw_map(*level, px);
  const double k = px / level->size;
  for (const auto& path : paths) {
    for (size_t j = 1; j < path.size(); j++) {
      ge::lab::draw_line(pic, path[j - 1].x * k, path[j - 1].z * k, path[j].x * k, path[j].z * k,
                         Rgba{0, 120, 255}, 1);
    }
  }
  for (const fs::Amateur& a : crowd.amateurs) {
    ge::lab::fill_disc(pic, a.x * k, a.z * k, 1.5, mode_colour(a.mode));
  }
  lab.shot(
      {"map",
       "SEED " + fixed(seed, 0) + " THE CROWD AFTER " + fixed(limit, 0) + " S",
       "The map from above (+x right, +z down; the runs red, trees dark green) with the first " +
           std::to_string(paths.size()) +
           " amateurs' paths over the first minute (blue) and "
           "every amateur at the end: yellow skiing, orange stopped, red down, light blue in a "
           "lift's queue, magenta riding a lift, white skating off its top, none up one unseen. " +
           std::to_string(n) + " amateurs; " + std::to_string(rides) + " lift rides.",
       "yellow dots strung down every run, thickest on the greens and blues; light blue "
       "clusters at the lifts' bottom stations and magenta lines up the lifts; blue paths "
       "weaving down the runs in turns",
       {{"seed", fixed(seed, 0)},
        {"version", lab.arg("version")},
        {"count", std::to_string(n)},
        {"limit s", fixed(limit, 0)}}},
      pic);
}

const ge::lab::LabSpec kSpec{
    "crowd",
    ge::lab::LabKind::scenario,
    "game:fall_line",
    "games/fall_line/sim/crowd (the free ride's amateurs: the deal, the runs, the lifts, the "
    "player met)",
    "sim",
    "Do the crowd's amateurs ski every run and ride the lifts back up without getting stuck, in "
    "the shares a ski area has them, and what does a step of them cost?",
    {
        {"seed", "38", "the map's seed"},
        {"version", "10", "the generator's version (a ski area: 10 or later)"},
        {"count", "400", "how many amateurs to deal"},
        {"limit", "600", "how long to run, s"},
        {"px", "768", "the picture's size, px"},
        {"no-pictures", "", "skip the picture", true},
    },
    "--px=256 --limit=240 --count=200",
    run,
};
GE_LAB(kSpec);

}  // namespace
