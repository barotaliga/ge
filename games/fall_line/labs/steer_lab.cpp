// LAB `steer`: the steering a bot shares (engine/ai/steer.hpp) on one seed's ski area. THE FAN: an
// army snowmobile (games/fall_line/sim/army_drive) at the first lift's bottom station wanting the
// last lift's top weighs the heading it holds and its fan of headings either side, each walked out
// over the snow and costed; every one drawn as far as it runs, coloured by its cost, the one taken
// in yellow, and the fan as a table. THE GRID: a crowd of soldiers packed round the same spot,
// each one's push off those too near summed over the step's grid and over all of them; the two
// held equal to the bit, and how many each visited.
//
// ge:summary Lab: a snowmobile's fan of headings costed and drawn; the soldiers' grid against all
// ge:keywords lab steer fan heading way_cost choose_way separation neighbours grid keep-apart
// ge:keywords pick_heading snowmobile army
// ge:skill bots labs
// ge:test tests/ai_steer_test.cpp
#include <cmath>
#include <cstdint>
#include <cstring>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "ai/steer.hpp"
#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "core/num_text.hpp"
#include "core/prng.hpp"
#include "fall_line/sim/army_drive.hpp"
#include "fall_line/sim/army_rules.hpp"
#include "fall_line/sim/army_sled_rules.hpp"
#include "fall_line/sim/army_troop.hpp"
#include "fall_line/sim/free.hpp"
#include "lab/lab.hpp"
#include "lab/raster.hpp"
#include "mountain/mapgen/generate.hpp"
#include "mountain/picture/map_picture.hpp"
#include "skier/trees.hpp"

namespace {

namespace fs = fall_line::sim;
namespace gl = ge::lab;
namespace mg = ge::mountain::mapgen;
namespace lm = ge::core::libm;
namespace as = fall_line::sim::army_sled;

// The crowd packed round the spot: how many, and the disc they stand in, m.
constexpr int32_t kCrowd = 96;
constexpr double kCrowdM = 40;

std::string fixed(double v, int d) { return ge::core::num::to_fixed(v, d); }

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
  const std::optional<mg::ResortMap>& resort = level->resort;
  if (!resort.has_value() || resort->lifts.empty()) {
    lab.fail(k + ": the map has no lifts to steer between",
             "Pick a seed whose ski area has lifts: `make lab LAB=free` lists them (skill: sim).");
    return;
  }
  const ge::skier::TreeIndex trees = ge::skier::build_tree_index(*level);
  const mg::Vec3 from = resort->lifts.front().bottom;
  const mg::Vec3 to = resort->lifts.back().top;

  // THE FAN, read as the army's snowmobiles read it, holding the way it wants.
  const fs::DriveRead read{as::kLookM,   as::kStepM,    as::kFan,         as::kSpread,
                           as::kRise,    as::kDrop,     as::kTunnelM,     as::kTrunkM,
                           as::kEdgeM,   as::kTurnCost, as::kBlockedCost, as::kTreeCost,
                           as::kKeepCost};
  const double want = lm::atan2(to.x - from.x, to.z - from.z);
  const double held = want + 0.6;
  const fs::WayChoice choice = fs::choose_way(*level, trees, from.x, from.z, held, want, read);
  constexpr int kPx = 720;
  const double side = 3 * read.look;
  gl::Picture pic = ge::mountain::picture::draw_map(
      *level, kPx, ge::mountain::picture::MapWindow{from.x, from.z, side});
  const double px = kPx / side;
  const auto at = [&](double x, double z) {
    return std::pair{(x - from.x + side / 2) * px, (z - from.z + side / 2) * px};
  };
  gl::LabTable fan{"The fan from lift " + resort->lifts.front().id + "'s bottom toward lift " +
                       resort->lifts.back().id + "'s top",
                   {"heading off want (rad)", "cost", "runs (m)", "trunks near", "taken"},
                   {}};
  double least = 1e300;
  double most = 0;
  std::vector<std::pair<double, fs::WayCost>> ways;
  ways.emplace_back(held, fs::way_cost(*level, trees, from.x, from.z, held, want, held, read));
  for (int32_t i = -read.fan; i <= read.fan; i++) {
    const double h = want + read.spread * i / read.fan;
    ways.emplace_back(h, fs::way_cost(*level, trees, from.x, from.z, h, want, held, read));
  }
  for (const auto& [h, c] : ways) {
    least = std::fmin(least, c.cost);
    most = std::fmax(most, c.cost);
  }
  for (const auto& [h, c] : ways) {
    const bool taken =
        std::fabs(ge::core::angle_diff(h, choice.way)) < 1e-9 && c.cost == choice.cost;
    const double f = most > least ? (c.cost - least) / (most - least) : 0;
    const gl::Rgba colour =
        taken ? gl::Rgba{255, 220, 0}
              : gl::Rgba{static_cast<uint8_t>(255 * f), static_cast<uint8_t>(255 * (1 - f)), 60};
    const auto [x0, y0] = at(from.x, from.z);
    const auto [x1, y1] = at(from.x + lm::sin(h) * c.reach, from.z + lm::cos(h) * c.reach);
    gl::draw_line(pic, x0, y0, x1, y1, colour, taken ? 3 : 1);
    fan.rows.push_back({fixed(ge::core::angle_diff(want, h), 2), fixed(c.cost, 2),
                        fixed(c.reach, 0), std::to_string(c.trees), taken ? "yes" : ""});
  }
  lab.table(fan);
  lab.metric(k + ".fan-cost", choice.cost, "cost");
  lab.metric(k + ".fan-reach", choice.reach, "m");
  lab.metric(k + ".fan-off-want", ge::core::angle_diff(want, choice.way), "rad");
  lab.shot({k + "-fan",
            "SEED " + fixed(seed, 0) + ": AN ARMY SNOWMOBILE'S FAN OF HEADINGS, COSTED",
            "The snow round lift " + resort->lifts.front().id +
                "'s bottom station (+x right, +z "
                "down). From the middle, every heading the machine weighs toward lift " +
                resort->lifts.back().id + "'s top (the way it holds first, " + fixed(0.6, 1) +
                " rad off the way it wants, then " + std::to_string(2 * read.fan + 1) +
                " across the fan), each drawn as far as it runs before a climb, a drop, a "
                "tunnel or the edge stops it, green cheap to red dear; the one taken in yellow.",
            "the yellow line runs nearest the way wanted among the longest; a line stopped short "
            "by a climb, a drop or a tunnel is short and turns red, and on open snow every line "
            "runs its full look and only the turn off the way wanted costs",
            {{"seed", fixed(seed, 0)}, {"look", fixed(read.look, 0) + " m"}}},
           pic);

  // THE GRID: a crowd packed round the spot, the push apart over the grid and over all of them.
  std::vector<fs::Soldier> crowd;
  ge::core::Rng rng = ge::core::make_rng(static_cast<uint32_t>(seed));
  for (int32_t i = 0; i < kCrowd; i++) {
    fs::Soldier s;
    s.id = i;
    s.mode = fs::SoldierMode::chase;
    const double a = ge::core::next(rng) * 2 * ge::core::kPi;
    const double r = std::sqrt(ge::core::next(rng)) * kCrowdM;
    s.x = from.x + lm::sin(a) * r;
    s.z = from.z + lm::cos(a) * r;
    crowd.push_back(s);
  }
  ge::ai::Neighbours near;
  fs::build_near(near, crowd);
  int64_t by_grid = 0;
  int32_t differ = 0;
  for (const fs::Soldier& s : crowd) {
    ge::ai::Away all;
    for (const fs::Soldier& o : crowd) {
      if (o.id != s.id) ge::ai::add_apart(all, s.x - o.x, s.z - o.z, fs::army::kApartM);
    }
    ge::ai::Away grid;
    near.for_each_near(s.x, s.z, [&](int32_t i) {
      by_grid++;
      const fs::Soldier& o = crowd[static_cast<size_t>(i)];
      if (o.id != s.id) ge::ai::add_apart(grid, s.x - o.x, s.z - o.z, fs::army::kApartM);
    });
    differ += std::memcmp(&all, &grid, sizeof all) == 0 ? 0 : 1;
  }
  if (differ > 0) {
    lab.fail(k + ": " + std::to_string(differ) +
                 " soldiers pushed apart differently over the "
                 "grid than over all of them",
             "Read ai::Neighbours::for_each_near in engine/ai/steer.hpp: it must visit every bot "
             "the full pass reaches, in ascending index (skill: bots).");
  }
  const auto full = static_cast<int64_t>(kCrowd) * kCrowd;
  lab.metric(k + ".grid-visits", static_cast<double>(by_grid), "count");
  lab.metric(k + ".full-visits", static_cast<double>(full), "count");
  lab.table({"Keeping apart: " + std::to_string(kCrowd) + " soldiers within " + fixed(kCrowdM, 0) +
                 " m, each one's neighbours",
             {"pass", "visits", "pushes the same"},
             {{"all of them", std::to_string(full), "-"},
              {"the step's grid", std::to_string(by_grid), differ == 0 ? "yes" : "no"}}});
}

const gl::LabSpec kSpec{
    "steer",
    gl::LabKind::sim,
    "game:fall_line",
    "engine/ai/steer (the fan of headings, separation over a grid) under the army's drive and "
    "troop (games/fall_line/sim/army_drive, army_troop)",
    "bots",
    "Which heading does a bot take from its fan, and does the grid keep soldiers apart as the "
    "full pass does?",
    {
        {"seed", "38", "the map's seed"},
    },
    "--seed=38",
    run,
};
GE_LAB(kSpec);

}  // namespace
