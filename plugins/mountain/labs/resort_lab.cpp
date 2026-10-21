// LAB `resort`: ONE SKI AREA drawn and described, from the engine alone: whether a generated
// mountain reads as a ski area somebody laid out (R25–R30) and how its runs are spread. The plan
// from above (every run in its colour, the lanes grey, the lifts, the hub and its wind tunnels,
// the woods, the kickers and cliffs) and the same area with every course picked out in the colour
// it measures; tables of the attempts tried, the lifts, the runs and the courses; and the
// headline numbers a generator change is read by.
//
// ge:summary Lab: one generated ski area from above and its courses, with its lifts, runs, courses
// ge:keywords lab resort ski-area plan courses lifts runs picture woods hub refusals
// ge:skill mountain labs
// ge:test tests/lab_runner_test.cpp
#include <cstdio>
#include <string>

#include "core/math.hpp"
#include "lab/lab.hpp"
#include "mountain/labs/level_map.hpp"
#include "mountain/labs/resort_map.hpp"
#include "mountain/mapgen/grades.hpp"

namespace {

namespace mg = ge::mountain::mapgen;

std::string f1(double v) {
  char b[32];
  std::snprintf(b, sizeof b, "%.1f", v);
  return b;
}

void run(ge::lab::Lab& lab) {
  const int px = static_cast<int>(lab.arg_num("size"));
  for (const double seed : lab.arg_nums("seeds")) {
    const std::string id = "seed-" + std::to_string(static_cast<long long>(seed));
    mg::ResortAttempt a;
    const auto tried = ge::mountain::labs::build_area(lab, seed, a);
    ge::lab::LabTable attempts{id + ": attempts tried", {"attempt", "reached", "refused"}, {}};
    for (const auto& t : tried) {
      attempts.rows.push_back({std::to_string(t.attempt), t.reached.empty() ? "-" : t.reached,
                               t.why.empty() ? "-" : t.why});
    }
    lab.table(std::move(attempts));
    if (tried.empty() || !tried.back().why.empty()) continue;

    ge::lab::LabTable lifts{id + ": lifts", {"lift", "kind", "length m", "rise m"}, {}};
    for (const mg::LiftPlan& l : a.lift_plans) {
      const double rise = ge::core::sample_field(a.ground, l.top.x, l.top.z) -
                          ge::core::sample_field(a.ground, l.bottom.x, l.bottom.z);
      lifts.rows.push_back({l.id, mg::lift_kind_name(l.kind),
                            f1(ge::core::hypot(l.top.x - l.bottom.x, l.top.z - l.bottom.z)),
                            f1(rise)});
    }
    lab.table(std::move(lifts));

    double km[4] = {0, 0, 0, 0};
    double lanes = 0;
    ge::lab::LabTable runs{
        id + ": runs", {"run", "from", "kind", "colour", "length m", "drop m", "into"}, {}};
    for (const mg::BuiltRun& b : a.built) {
      const mg::WalkedRun& w = b.walked;
      const bool road = w.spec.kind == mg::RunKind::road;
      const auto& pts = w.track.points;
      if (road) {
        lanes += w.track.length;
      } else {
        km[static_cast<int>(b.grade)] += w.track.length;
      }
      runs.rows.push_back(
          {w.spec.id, w.spec.from, mg::run_kind_name(w.spec.kind),
           road ? "-" : mg::grade_name(b.grade), f1(w.track.length),
           f1(pts.front().y - pts.back().y),
           w.into ? a.built[size_t(w.into->run)].walked.spec.id + " at " + f1(w.into->s)
                  : "floor"});
    }
    lab.table(std::move(runs));

    ge::lab::LabTable courses{
        id + ": courses", {"course", "colour", "length m", "drop m", "runs skied"}, {}};
    for (const mg::Course& c : a.courses) {
      std::string skied;
      for (const std::string& r : c.runs) skied += (skied.empty() ? "" : " ") + r;
      courses.rows.push_back({c.id, mg::grade_name(c.grade), f1(c.length), f1(c.drop), skied});
    }
    lab.table(std::move(courses));

    lab.metric(id + ".attempt", a.attempt, "count");
    lab.metric(id + ".lifts", static_cast<double>(a.lift_plans.size()), "count");
    lab.metric(id + ".runs", static_cast<double>(a.built.size()), "count");
    lab.metric(id + ".green-km", km[0] / 1000, "km");
    lab.metric(id + ".blue-km", km[1] / 1000, "km");
    lab.metric(id + ".red-km", km[2] / 1000, "km");
    lab.metric(id + ".black-km", km[3] / 1000, "km");
    lab.metric(id + ".lane-km", lanes / 1000, "km");
    lab.metric(id + ".courses", static_cast<double>(a.courses.size()), "count");
    lab.metric(id + ".trees", static_cast<double>(a.trees.size()), "count");
    lab.metric(id + ".off-kickers", static_cast<double>(a.off_kickers.size()), "count");
    lab.metric(id + ".cliffs", static_cast<double>(a.cliffs.size()), "count");
    lab.metric(id + ".tunnel", a.tunnels.empty() ? 0 : a.tunnels[0].length, "m");
    lab.metric(id + ".facing", a.facing, "rad");

    const std::string seed_word = std::to_string(static_cast<long long>(seed));
    lab.shot({id + "-plan", "SEED " + seed_word + " SKI AREA FROM ABOVE",
              "The ski area from above, +x right and +z down: the face shaded by height and lit "
              "from the upper left, the packed snow tinted blue, the woods dark green, every run "
              "in its measured colour and numbered at its top, the lanes grey, the lifts black "
              "with their stations, the hub's edges yellow and its wind tunnels cyan, kickers "
              "orange (X off the runs), cliffs purple, the village magenta. " +
                  std::to_string(a.built.size()) + " runs, " + std::to_string(a.lift_plans.size()) +
                  " lifts, " + std::to_string(a.trees.size()) + " trees.",
              "every run falls from a lift's top to the hub or into another run; no trees on a "
              "run, a lift's line or in the hub; the woods thin up the mountain to bare snow at "
              "the tree line; every finish and valley station inside the yellow hub",
              ge::mountain::labs::area_inputs(lab, a)},
             ge::mountain::labs::draw_area(a, px, false));
    lab.shot({id + "-courses", "SEED " + seed_word + " COURSES",
              "The same area with its runs faint and every course (R28) drawn over them in the "
              "colour its steepest stretch measures, named at its top by the run it starts on. " +
                  std::to_string(a.courses.size()) + " courses.",
              "each course follows its runs down through every junction to the hub without a "
              "kink where it changes runs",
              ge::mountain::labs::area_inputs(lab, a)},
             ge::mountain::labs::draw_area(a, px, true));
  }
}

const ge::lab::LabSpec kSpec{
    "resort",
    ge::lab::LabKind::picture,
    "plugin:mountain",
    "plugins/mountain/mapgen/resort_build.hpp",
    "mountain",
    "What ski area did the generator build for this seed: its runs, lifts, woods and courses?",
    {
        {"seeds", "38", "seeds, comma-separated"},
        {"size", "1024", "the picture's side, px"},
        ge::mountain::labs::kRegionFlag,
        ge::mountain::labs::kAttemptsFlag,
    },
    "--seeds=38 --size=256 --attempts=1",
    run,
};
GE_LAB(kSpec);

}  // namespace
