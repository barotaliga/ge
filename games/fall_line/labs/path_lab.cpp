// LAB `path`: the paths a bot finds (engine/ai/path.hpp) on one seed's ski area, drawn on its
// map. On foot across the mountain from the first lift's bottom station to the last lift's top on
// a coarse grid (each step costing its length, more for every metre it climbs or drops, barred
// where the ground is steeper than a person walks), searched twice, by Dijkstra and by A*, the
// two held to the same cost, how many cells each expanded beside it, and the way string-pulled
// to its fewest clear legs. And every run's OUTING (outing.hpp): the lift and the next run the
// outing's graph search over the runs and the lifts chose, in a table.
//
// ge:summary Lab: a walk across the ski area by Dijkstra and A*, string-pulled; every outing
// ge:keywords lab path search a-star dijkstra grid walk string-pulling expanded cost outing
// ge:keywords graph lifts runs
// ge:skill bots labs
// ge:test tests/ai_path_test.cpp
#include <cmath>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "ai/path.hpp"
#include "core/math.hpp"
#include "core/num_text.hpp"
#include "fall_line/sim/free.hpp"
#include "fall_line/sim/outing.hpp"
#include "lab/lab.hpp"
#include "lab/raster.hpp"
#include "mountain/labs/level_map.hpp"
#include "mountain/lifts/lift_loop.hpp"
#include "mountain/mapgen/generate.hpp"

namespace {

namespace fs = fall_line::sim;
namespace gl = ge::lab;
namespace ai = ge::ai;
namespace mg = ge::mountain::mapgen;

// The walk's grid, m a cell; its cost for every metre climbed or dropped; the steepest ground a
// person walks (rise over run).
constexpr double kCell = 8;
constexpr double kPerRise = 6;
constexpr double kSteepest = 0.7;

std::string fixed(double v, int d) { return ge::core::num::to_fixed(v, d); }

struct Pt {
  double x, z;
};

void run(gl::Lab& lab) {
  const double seed = lab.arg_num("seed");
  const std::string k = "seed-" + fixed(seed, 0);
  std::shared_ptr<const mg::Level> level;
  try {
    level = std::make_shared<const mg::Level>(fs::free_map(seed, std::nullopt));
  } catch (const mg::GenerateError& e) {
    lab.fail("seed " + fixed(seed, 0) + " built no map: " + e.what(),
             "Run `make lab LAB=analyze ARGS=--seeds=" + fixed(seed, 0) +
                 " --refusals` (skill: mountain).");
    return;
  }
  const std::optional<mg::ResortMap>& has_resort = level->resort;
  if (!has_resort.has_value()) {
    lab.fail(k + ": the map has no ski area to walk across",
             "Pick a seed whose ski area has lifts: `make lab LAB=free` lists them (skill: sim).");
    return;
  }
  const mg::ResortMap& resort = *has_resort;
  if (resort.lifts.empty()) {
    lab.fail(k + ": the map has no lifts to walk between",
             "Pick a seed whose ski area has lifts: `make lab LAB=free` lists them (skill: sim).");
    return;
  }

  // THE WALK: the first lift's bottom station to the last lift's top.
  const ai::Grid grid{static_cast<int32_t>(level->size / kCell),
                      static_cast<int32_t>(level->size / kCell), kCell};
  const auto cell_of = [&](double x, double z) {
    const auto i = static_cast<int32_t>(x / kCell);
    const auto j = static_cast<int32_t>(z / kCell);
    return j * grid.cols + i;
  };
  const auto centre = [&](int32_t c) {
    const int32_t row = c / grid.cols;
    return Pt{(c % grid.cols + 0.5) * kCell, (row + 0.5) * kCell};
  };
  const auto height = [&](int32_t c) {
    const Pt p = centre(c);
    return level->ground_at(p.x, p.z);
  };
  const auto step = [&](int32_t a, int32_t b, double length) {
    const double rise = std::fabs(height(b) - height(a));
    if (rise > kSteepest * length) return ai::kUnreached;
    return length + kPerRise * rise;
  };
  const mg::Lift& first = resort.lifts.front();
  const mg::Lift& last = resort.lifts.back();
  const int32_t from = cell_of(first.bottom.x, first.bottom.z);
  const int32_t to = cell_of(last.top.x, last.top.z);
  ai::Search search;
  const ai::GridPath dijkstra = ai::grid_path(grid, from, to, step, 0, search);
  const ai::GridPath astar = ai::grid_path(grid, from, to, step, 1, search);
  if (astar.cells.empty()) {
    lab.fail(k + ": no walk from lift " + first.id + "'s bottom to lift " + last.id +
                 "'s top under a rise of " + fixed(kSteepest, 1) + " a metre",
             "Look at previews/path/" + k +
                 ".png for the ground in between, or raise kSteepest "
                 "in games/fall_line/labs/path_lab.cpp (skill: bots).");
  }
  if (std::fabs(astar.cost - dijkstra.cost) > 1e-6 * std::fmax(1.0, dijkstra.cost)) {
    lab.fail(k + ": A* found a walk costing " + fixed(astar.cost, 3) + " and Dijkstra " +
                 fixed(dijkstra.cost, 3) + ": the heuristic overestimates",
             "Read grid_path's heuristic in engine/ai/path.cpp: least_per_m must be no more than "
             "any step's cost over its length (skill: bots).");
  }
  std::vector<Pt> pts;
  pts.reserve(astar.cells.size());
  for (const int32_t c : astar.cells) pts.push_back(centre(c));
  // A leg is clear where every 4 m of it stays under the steepest a person walks.
  const auto clear = [&](size_t a, size_t b) {
    const double d = ge::core::hypot(pts[b].x - pts[a].x, pts[b].z - pts[a].z);
    const int n = static_cast<int>(d / 4) + 1;
    double y = level->ground_at(pts[a].x, pts[a].z);
    for (int i = 1; i <= n; i++) {
      const double f = static_cast<double>(i) / n;
      const double y1 = level->ground_at(pts[a].x + (pts[b].x - pts[a].x) * f,
                                         pts[a].z + (pts[b].z - pts[a].z) * f);
      if (std::fabs(y1 - y) > kSteepest * d / n) return false;
      y = y1;
    }
    return true;
  };
  const std::vector<Pt> pulled = ai::string_pull<Pt>(std::span<const Pt>(pts), clear);
  double walked = 0;
  for (size_t i = 1; i < pulled.size(); i++) {
    walked += ge::core::hypot(pulled[i].x - pulled[i - 1].x, pulled[i].z - pulled[i - 1].z);
  }
  lab.metric(k + ".walk-cost", astar.cost, "m");
  lab.metric(k + ".walk-cells", static_cast<double>(astar.cells.size()), "count");
  lab.metric(k + ".walk-legs", static_cast<double>(pulled.size() > 0 ? pulled.size() - 1 : 0),
             "count");
  lab.metric(k + ".walk-pulled-length", walked, "m");
  lab.metric(k + ".dijkstra-expanded", static_cast<double>(dijkstra.expanded), "count");
  lab.metric(k + ".astar-expanded", static_cast<double>(astar.expanded), "count");
  lab.table({"The walk from lift " + first.id + "'s bottom to lift " + last.id + "'s top, " +
                 fixed(kCell, 0) + " m cells",
             {"search", "cost (m)", "cells", "expanded"},
             {{"dijkstra", fixed(dijkstra.cost, 1), std::to_string(dijkstra.cells.size()),
               std::to_string(dijkstra.expanded)},
              {"a*", fixed(astar.cost, 1), std::to_string(astar.cells.size()),
               std::to_string(astar.expanded)},
              {"string-pulled", "-", std::to_string(pulled.size()), "-"}}});

  // THE OUTINGS: the graph search's lift and next run off every run.
  const auto lifts = ge::mountain::lifts::lift_plans(*level);
  gl::LabTable outings{"Every run's outing, as the graph search over runs and lifts chose",
                       {"run", "lift", "next run"},
                       {}};
  int32_t found = 0;
  for (size_t r = 0; r < resort.runs.size(); r++) {
    const fs::Outing o = fs::outing_from(*level, lifts, static_cast<int32_t>(r));
    const bool has = o.lift >= 0;
    found += has ? 1 : 0;
    outings.rows.push_back({resort.runs[r].id,
                            has ? resort.lifts[static_cast<size_t>(o.lift)].id : "-",
                            has ? resort.runs[static_cast<size_t>(o.next)].id : "-"});
  }
  lab.table(outings);
  lab.metric(k + ".outings", static_cast<double>(found), "count");

  // THE PICTURE.
  constexpr int kPx = 1024;
  const double px = kPx / level->size;
  gl::Picture pic = ge::mountain::labs::draw_map(*level, kPx);
  for (const mg::Lift& l : resort.lifts) {
    gl::draw_line(pic, l.bottom.x * px, l.bottom.z * px, l.top.x * px, l.top.z * px,
                  gl::Rgba{255, 255, 255}, 1);
  }
  for (size_t i = 1; i < pts.size(); i++) {
    gl::draw_line(pic, pts[i - 1].x * px, pts[i - 1].z * px, pts[i].x * px, pts[i].z * px,
                  gl::Rgba{255, 160, 0}, 1);
  }
  for (size_t i = 1; i < pulled.size(); i++) {
    gl::draw_line(pic, pulled[i - 1].x * px, pulled[i - 1].z * px, pulled[i].x * px,
                  pulled[i].z * px, gl::Rgba{0, 220, 255}, 2);
  }
  lab.shot({k,
            "SEED " + fixed(seed, 0) + ": A WALK ACROSS THE SKI AREA, FOUND AND STRING-PULLED",
            "The ski area from above (+x right, +z down), every lift a white line. In orange the "
            "cheapest walk on " +
                fixed(kCell, 0) + " m cells from lift " + first.id + "'s bottom station to lift " +
                last.id +
                "'s top, each step costing its length "
                "and " +
                fixed(kPerRise, 0) +
                " m more a metre climbed or dropped, barred over a "
                "rise of " +
                fixed(kSteepest, 1) +
                "; in cyan the same walk string-pulled to its "
                "fewest clear legs. A* expanded " +
                std::to_string(astar.expanded) + " cells, Dijkstra " +
                std::to_string(dijkstra.expanded) + ", for one cost.",
            "the orange walk follows the contours round the steep faces rather than straight "
            "over them; the cyan legs lie on it at its corners and cut across where the ground "
            "allows",
            {{"seed", fixed(seed, 0)}, {"cell", fixed(kCell, 0) + " m"}}},
           pic);
}

const gl::LabSpec kSpec{
    "path",
    gl::LabKind::sim,
    "game:fall_line",
    "engine/ai/path (the heap, the graph and grid searches, string-pulling) and the outing's "
    "graph search (games/fall_line/sim/outing)",
    "bots",
    "What way does a bot find across the mountain, at what cost, and which outing off each run?",
    {
        {"seed", "38", "the map's seed"},
    },
    "--seed=38",
    run,
};
GE_LAB(kSpec);

}  // namespace
