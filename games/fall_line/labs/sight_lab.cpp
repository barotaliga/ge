// LAB `sight`: what a pursuer sees (engine/ai/sight.hpp under the wanted level's sight line,
// games/fall_line/sim/wanted) from one spot of one seed's ski area: the SHED of a soldier stood at
// the first lift's top, and of a helicopter's crew hovering over the same spot. Every point of a
// grid round him is asked whether he would see a man standing there, and why not: out of range,
// behind the ground, or through the woods (from the snow only); heard within reach whatever is
// between. The shares are a table and the sheds two pictures, each point coloured by its answer.
//
// ge:summary Lab: a soldier's and a helicopter's sight sheds from a lift's top, by what hides
// ge:keywords lab sight shed viewshed line-of-sight ground woods hear range soldier helicopter
// ge:keywords sees_quarry occluder
// ge:skill bots labs
// ge:test tests/ai_sight_test.cpp
#include <array>
#include <cmath>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>

#include "ai/sight.hpp"
#include "core/math.hpp"
#include "core/num_text.hpp"
#include "fall_line/sim/army_rules.hpp"
#include "fall_line/sim/free.hpp"
#include "fall_line/sim/wanted.hpp"
#include "lab/lab.hpp"
#include "lab/raster.hpp"
#include "mountain/mapgen/generate.hpp"
#include "mountain/picture/map_picture.hpp"
#include "skier/trees.hpp"

namespace {

namespace fs = fall_line::sim;
namespace gl = ge::lab;
namespace mg = ge::mountain::mapgen;
namespace wd = fall_line::sim::wanted;

// The grid the shed is read on, m; the helicopter's height over the spot, m.
constexpr double kCell = 10;
constexpr double kHover = 60;

std::string fixed(double v, int d) { return ge::core::num::to_fixed(v, d); }

// What a point's answer is, and its colour.
enum class Answer : uint8_t { heard, seen, range, ground, woods };
constexpr std::array<const char*, 5> kNames{"heard", "seen", "out of range", "behind the ground",
                                            "through the woods"};
constexpr std::array<gl::Rgba, 5> kColours{gl::Rgba{255, 255, 255}, gl::Rgba{40, 220, 60},
                                           gl::Rgba{0, 0, 0, 0}, gl::Rgba{200, 40, 40},
                                           gl::Rgba{30, 90, 200}};

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
    lab.fail(k + ": the map has no lift to stand the pursuer at",
             "Pick a seed whose ski area has lifts: `make lab LAB=free` lists them (skill: sim).");
    return;
  }
  const ge::skier::TreeIndex trees = ge::skier::build_tree_index(*level);
  const mg::Vec3 top = resort->lifts.front().top;
  const double ground = level->ground_at(top.x, top.z);

  struct Eye {
    const char* name;
    fs::PursuerKind kind;
    double up;
  };
  const std::array<Eye, 2> eyes{Eye{"soldier", fs::PursuerKind::soldier, fs::army::kEyeM},
                                Eye{"helicopter", fs::PursuerKind::helicopter, kHover}};
  gl::LabTable table{"What each pursuer sees from lift " + resort->lifts.front().id +
                         "'s top, a point every " + fixed(kCell, 0) + " m",
                     {"pursuer", "range (m)", "heard", "seen", "behind the ground",
                      "through the woods", "seen of in range"},
                     {}};
  for (const Eye& eye : eyes) {
    const fs::Pursuer p{0, eye.kind, top.x, ground + eye.up, top.z};
    const double range = wd::kSightM[static_cast<size_t>(eye.kind)];
    const bool low = fs::looks_from_snow(eye.kind);
    std::array<int32_t, 5> count{};
    constexpr int kPx = 720;
    const double side = 2 * range + 2 * kCell;
    const ge::mountain::picture::MapWindow window{top.x, top.z, side};
    gl::Picture pic = ge::mountain::picture::draw_map(*level, kPx, window);
    const double px = kPx / side;
    const auto n = static_cast<int32_t>(range / kCell);
    for (int32_t j = -n; j <= n; j++) {
      for (int32_t i = -n; i <= n; i++) {
        const double x = top.x + i * kCell;
        const double z = top.z + j * kCell;
        if (x < 0 || z < 0 || x > level->size || z > level->size) continue;
        const fs::Spot q{x, level->ground_at(x, z), z};
        const ge::ai::Point3 at{q.x, q.y + wd::kChestM, q.z};
        const ge::ai::Point3 from{p.x, p.y, p.z};
        const double d =
            std::sqrt((at.x - from.x) * (at.x - from.x) + (at.y - from.y) * (at.y - from.y) +
                      (at.z - from.z) * (at.z - from.z));
        Answer a = Answer::seen;
        if (d <= wd::kCloseM) {
          a = Answer::heard;
        } else if (d > range) {
          a = Answer::range;
        } else if (!ge::ai::ground_clear(
                       from, at, wd::kLineStepM, wd::kLineClearM,
                       [&](double gx, double gz) { return level->ground_at(gx, gz); })) {
          a = Answer::ground;
        } else if (low && fs::through_woods(*level, trees, fs::Spot{p.x, p.y, p.z},
                                            fs::Spot{at.x, at.y, at.z})) {
          a = Answer::woods;
        }
        const bool sees = fs::sees_quarry(*level, trees, p, q, false);
        if (sees != (a == Answer::heard || a == Answer::seen)) {
          lab.fail(k + ": the " + eye.name + "'s shed and sees_quarry disagree at (" + fixed(x, 0) +
                       ", " + fixed(z, 0) + ")",
                   "Read sees_quarry in games/fall_line/sim/wanted.cpp against ai::sees in "
                   "engine/ai/sight.hpp: the order of the tests changed (skill: bots).");
          return;
        }
        count[static_cast<size_t>(a)]++;
        if (a != Answer::range) {
          gl::fill_rect(pic, (x - top.x + side / 2) * px - 1, (z - top.z + side / 2) * px - 1, 3, 3,
                        kColours[static_cast<size_t>(a)]);
        }
      }
    }
    gl::draw_ring(pic, kPx / 2.0, kPx / 2.0, range * px, gl::Rgba{255, 255, 255});
    gl::fill_disc(pic, kPx / 2.0, kPx / 2.0, 4, gl::Rgba{255, 200, 0});
    const int32_t in_range = count[0] + count[1] + count[3] + count[4];
    const double share = in_range > 0 ? static_cast<double>(count[0] + count[1]) / in_range : 0;
    const std::string e = eye.name;
    lab.metric(k + "." + e + ".seen", count[1], "count");
    lab.metric(k + "." + e + ".ground", count[3], "count");
    lab.metric(k + "." + e + ".woods", count[4], "count");
    lab.metric(k + "." + e + ".seen-share", share, "ratio");
    table.rows.push_back({e, fixed(range, 0), std::to_string(count[0]), std::to_string(count[1]),
                          std::to_string(count[3]), std::to_string(count[4]),
                          fixed(100 * share, 1) + "%"});
    lab.shot({k + "-" + e,
              "SEED " + fixed(seed, 0) + ": WHAT A " + (low ? "SOLDIER" : "HELICOPTER'S CREW") +
                  " SEES FROM LIFT " + resort->lifts.front().id + "'S TOP",
              "The map round the top of lift " + resort->lifts.front().id +
                  " (+x right, +z down), "
                  "the pursuer at the yellow dot, " +
                  fixed(eye.up, 1) +
                  " m over the snow, his "
                  "range the white ring (" +
                  fixed(range, 0) + " m). A point every " + fixed(kCell, 0) +
                  " m, coloured by whether he would see a man standing there: green seen, white "
                  "heard (within " +
                  fixed(wd::kCloseM, 0) +
                  " m), red behind the ground, blue "
                  "through the woods.",
              low ? "red falls in the lee of every ridge and over the far side of the top; blue "
                    "fills the woods below the tree line; green runs down the open runs"
                  : "from aloft almost everything in range is green: the ground hides only what "
                    "lies tight under a steep face, and the woods hide nothing",
              {{"seed", fixed(seed, 0)}, {"pursuer", e}, {"cell", fixed(kCell, 0) + " m"}}},
             pic);
  }
  lab.table(table);
}

const gl::LabSpec kSpec{
    "sight",
    gl::LabKind::sim,
    "game:fall_line",
    "engine/ai/sight (the ground line, hearing, range, the occluder) under the wanted level's "
    "sight line (games/fall_line/sim/wanted)",
    "bots",
    "What does a soldier, and a helicopter's crew, see from a lift's top, and what hides the rest?",
    {
        {"seed", "38", "the map's seed"},
    },
    "--seed=38",
    run,
};
GE_LAB(kSpec);

}  // namespace
