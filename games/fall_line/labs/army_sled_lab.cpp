// LAB `army-sleds`: the army's snowmobiles on a real ski area. The hunted man stands still (stood
// again every step) at a spot of the map while the hunt is pinned at its stars and a helicopter
// hovers over him, and the army runs through the real `step` (`step_army_sleds` after the men):
// THE SLEDS table (per scenario, the machines called, the most out at once, the short halts, the
// times dug out and stood back on their belts, the nearest any came, when the first came within
// its halt range, the crews off to take him in, the arrest and whose it was, and the machines still
// caught at the end) and, per scenario, the map with every machine's path over it.
//
// Scenarios: `open` (open snow some 350 m up the mountain from the village, three stars, the
// army shooting to kill: the machines drive up and halt side-on to him), `run` (on a run some
// 40% of the way from the village to the top of the runs, five stars: the roster's three and the
// waves' machines climb the mountain at him), `face` (beside the highest lift's line, more than
// halfway up it on a face steeper than a laden machine climbs, four stars: they come as far as the
// face's foot and wait there), `arrest` (the open spot with the army not shooting to kill: a
// machine runs him down and its crew get off and take him in).
//
// A machine still caught at the end, a scenario shooting to kill within reach with no halt, or an
// arrest that never comes is a failure (exit 1).
//
// ge:summary Lab: the army's snowmobiles on seed 38: the call, the drive, the halt, the arrest
// ge:keywords lab army snowmobile sled army-sled halt dismount arrest dug righted paths stuck
// ge:keywords scenario open slope
// ge:skill sim labs
// ge:test tests/fall_line_army_sled_test.cpp
#include <memory>
#include <span>
#include <string>
#include <vector>

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "core/num_text.hpp"
#include "fall_line/sim/army.hpp"
#include "fall_line/sim/army_sled.hpp"
#include "fall_line/sim/army_sled_rules.hpp"
#include "fall_line/sim/free.hpp"
#include "fall_line/sim/game.hpp"
#include "fall_line/sim/wanted.hpp"
#include "lab/lab.hpp"
#include "lab/raster.hpp"
#include "mountain/labs/level_map.hpp"
#include "mountain/lifts/lift_loop.hpp"
#include "mountain/mapgen/generate.hpp"
#include "skier/skier.hpp"

namespace {

namespace fs = fall_line::sim;
namespace mg = ge::mountain::mapgen;
namespace ml = ge::mountain::lifts;
namespace as = fall_line::sim::army_sled;
namespace lm = ge::core::libm;
using ge::core::hypot;
using ge::lab::Rgba;

std::string fixed(double v, int d) { return ge::core::num::to_fixed(v, d); }

enum class Scene : uint8_t { open, run, face, arrest };
constexpr Scene kScenes[] = {Scene::open, Scene::run, Scene::face, Scene::arrest};

const char* scene_name(Scene s) {
  return s == Scene::open ? "open" : s == Scene::run ? "run" : s == Scene::face ? "face" : "arrest";
}

// The open spot: this far up the mountain from the village's machines at the least, m, clear of
// every trunk by this much, m.
constexpr double kOpenM = 350;
constexpr double kClearM = 15;
// The run spot: this share of the way up from the village to the top of the runs.
constexpr double kRunUp = 0.4;
// The face spot: up the highest lift's line, its share, and this far beside it, m.
constexpr double kFaceUp = 0.6;
constexpr double kFaceBesideM = 80;
// The helicopter keeping sight of him, m over him.
constexpr double kHoverM = 150;
// The colours the machines' paths are drawn in, by id.
constexpr Rgba kPathColours[] = {{0, 200, 255}, {255, 140, 0},   {0, 230, 120},
                                 {255, 230, 0}, {200, 120, 255}, {255, 90, 90}};

struct Hunt {
  fs::Spot q;
  int32_t stars = 0;
  int32_t called = 0;
  int32_t most = 0;
  int32_t halts = 0;
  int32_t dug = 0;
  int32_t righted = 0;
  int32_t dismounts = 0;
  double nearest = 1e9;
  double first_near = -1;  // s, the first machine within its halt range
  double arrested_at = -1;
  bool by_crew = false;  // the arrest was a snowmobile crew's
  int32_t caught = 0;    // machines caught (two windows or more in a row) at the end
  std::vector<std::vector<fs::Spot>> paths;
  std::vector<fs::ArmySled> last;
};

bool clear_of_trees(const fs::GameState& st, double x, double z) {
  for (const mg::TreeDef& t : st.level->trees) {
    if (hypot(t.x - x, t.z - z) < kClearM + t.radius) return false;
  }
  return true;
}

// Where he stands in a scene.
fs::Spot spot_of(const fs::GameState& st, Scene scene) {
  if (scene == Scene::run) {
    // The point of a run nearest the height `kRunUp` of the way to the runs' top, on open snow.
    double top = st.level->ground_at(st.army.pad_x, st.army.pad_z);
    const double base = top;
    for (const mg::Track& t : *st.runs) {
      for (const mg::TrackPoint& p : t.points) top = p.y > top ? p.y : top;
    }
    const double aim = base + (top - base) * kRunUp;
    fs::Spot best{st.army.pad_x, 0, st.army.pad_z};
    double off = 1e9;
    for (const mg::Track& t : *st.runs) {
      for (size_t i = 0; i < t.points.size(); i += 10) {
        const mg::TrackPoint& p = t.points[i];
        const double e = p.y > aim ? p.y - aim : aim - p.y;
        if (e < off && clear_of_trees(st, p.x, p.z)) {
          off = e;
          best = {p.x, 0, p.z};
        }
      }
    }
    return best;
  }
  if (scene == Scene::face) {
    const auto& lifts = *st.lifts;
    size_t high = 0;
    const auto top = [&st](const ml::LiftPlan& p) { return st.level->ground_at(p.top.x, p.top.z); };
    for (size_t i = 1; i < lifts.size(); i++) {
      if (top(lifts[i]) > top(lifts[high])) high = i;
    }
    const ml::LiftPlan& p = lifts[high];
    const double u = p.length * kFaceUp;
    return {p.bottom.x + p.dx * u - p.dz * kFaceBesideM, 0,
            p.bottom.z + p.dz * u + p.dx * kFaceBesideM};
  }
  // Up the mountain from the machines' spot, the first open snow.
  const fs::ArmyState& a = st.army;
  const double fx = lm::sin(a.pad_heading);
  const double fz = lm::cos(a.pad_heading);
  for (int32_t i = 0; i < 40; ++i) {
    const double r = kOpenM + 10.0 * i;
    for (const double side : {0.0, 60.0, -60.0, 120.0, -120.0}) {
      const double x = a.pad_x + fx * r + fz * side;
      const double z = a.pad_z + fz * r - fx * side;
      if (clear_of_trees(st, x, z)) return {x, 0, z};
    }
  }
  return {a.pad_x + fx * kOpenM, 0, a.pad_z + fz * kOpenM};
}

Hunt run_scene(const std::shared_ptr<const mg::Level>& level, Scene scene, double limit) {
  fs::GameState st = fs::create_free_ride(level, 0);
  Hunt h;
  h.stars = scene == Scene::run ? 5 : scene == Scene::face ? 4 : 3;
  const fs::Spot at = spot_of(st, scene);
  st.racers[0].skier = ge::skier::stand_skier(st.world(), at.x, at.z, 0, 0);
  st.wanted.lethal = scene != Scene::arrest;
  fs::set_wanted(st, h.stars);
  h.q = fs::quarry_of(st);
  const fs::Controls none{};
  while (st.t < limit) {
    fs::WantedState& w = st.wanted;
    if (w.ended == fs::WantedEnd::running && w.stars > 0) {
      w.heat = h.stars;
      std::erase_if(w.pursuers, [](const fs::Pursuer& p) { return p.id == 1; });
      w.pursuers.push_back(
          fs::Pursuer{1, fs::PursuerKind::helicopter, h.q.x, h.q.y + kHoverM, h.q.z});
    }
    // He stands where he was put, even on a slope his skis would slide down.
    st.racers[0].skier = ge::skier::stand_skier(st.world(), at.x, at.z, 0, 0);
    fs::step(st, std::span<const fs::Controls>(&none, 1));
    for (const fs::GameEvent& e : st.events) {
      if (e.kind == fs::EventKind::army && fs::army_change(e) == fs::ArmyChange::arrest) {
        h.arrested_at = st.t;
        for (const fs::Soldier& s : st.army.soldiers) {
          if (s.id == st.army.arrested_by) h.by_crew = s.from == fs::SoldierFrom::sled;
        }
      }
      if (e.kind != fs::EventKind::army_sled) continue;
      const auto c = static_cast<fs::ArmySledChange>(e.gate);
      h.called += c == fs::ArmySledChange::called ? 1 : 0;
      h.halts += c == fs::ArmySledChange::halt ? 1 : 0;
      h.dug += c == fs::ArmySledChange::dug ? 1 : 0;
      h.righted += c == fs::ArmySledChange::righted ? 1 : 0;
      h.dismounts += c == fs::ArmySledChange::dismount ? 1 : 0;
    }
    int32_t out = 0;
    for (const fs::ArmySled& m : st.army.sleds) {
      if (m.mode == fs::ArmySledMode::due) continue;
      out++;
      const double d = hypot(m.body.x - h.q.x, m.body.z - h.q.z);
      h.nearest = d < h.nearest ? d : h.nearest;
      if (h.first_near < 0 && d < as::kHaltRangeM) h.first_near = st.t;
    }
    h.most = out > h.most ? out : h.most;
    if (st.steps % static_cast<int64_t>(fs::kHz) == 0) {
      for (const fs::ArmySled& m : st.army.sleds) {
        if (m.mode == fs::ArmySledMode::due) continue;
        const auto id = static_cast<size_t>(m.id);
        if (h.paths.size() <= id) h.paths.resize(id + 1);
        h.paths[id].push_back({m.body.x, m.body.y, m.body.z});
      }
    }
    if (h.arrested_at >= 0 && st.t > h.arrested_at + 5) break;
  }
  for (const fs::ArmySled& m : st.army.sleds) h.caught += m.slow >= 2 ? 1 : 0;
  h.last = st.army.sleds;
  return h;
}

void run(ge::lab::Lab& lab) {
  const double seed = lab.arg_num("seed");
  const auto version = static_cast<int>(lab.arg_num("version"));
  const double limit = lab.arg_num("limit");
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
  if (!level->resort || level->resort->lifts.empty()) {
    lab.fail("seed " + fixed(seed, 0) + " v" + fixed(version, 0) + " has no ski area",
             "Pass a --version that builds a ski area (10 or later; skill: mountain).");
    return;
  }
  ge::lab::LabTable table{"sleds",
                          {"scenario", "stars", "called", "most out", "halts", "dug", "righted",
                           "nearest m", "in range s", "dismounts", "arrest s", "by", "caught"},
                          {}};
  const double k = px / level->size;
  for (const Scene scene : kScenes) {
    const std::string id = scene_name(scene);
    const Hunt h = run_scene(level, scene, limit);
    const bool within = scene == Scene::open || scene == Scene::run;
    table.rows.push_back({id, std::to_string(h.stars), std::to_string(h.called),
                          std::to_string(h.most), std::to_string(h.halts), std::to_string(h.dug),
                          std::to_string(h.righted), h.nearest < 1e9 ? fixed(h.nearest, 1) : "-",
                          h.first_near >= 0 ? fixed(h.first_near, 2) : "-",
                          std::to_string(h.dismounts),
                          h.arrested_at >= 0 ? fixed(h.arrested_at, 2) : "-",
                          h.arrested_at < 0 ? "-"
                          : h.by_crew       ? "crew"
                                            : "army",
                          std::to_string(h.caught)});
    lab.metric(id + ".called", h.called, "machines");
    lab.metric(id + ".most-out", h.most, "machines");
    lab.metric(id + ".halts", h.halts, "halts");
    lab.metric(id + ".dug", h.dug, "times");
    lab.metric(id + ".righted", h.righted, "times");
    lab.metric(id + ".nearest", h.nearest, "m");
    lab.metric(id + ".in-range", h.first_near, "s");
    lab.metric(id + ".caught", h.caught, "machines");
    if (h.caught > 0) {
      lab.fail(id + ": " + std::to_string(h.caught) + " machines still caught at the end",
               "Look at previews/army-sleds/" + id +
                   ".png for where the paths stop; read steer and watch in "
                   "games/fall_line/sim/army_sled.cpp and choose_way in army_drive.cpp (skill: "
                   "sim).");
    }
    if (within && h.halts == 0) {
      lab.fail(id + ": no machine made a short halt within " + fixed(limit, 0) + " s",
               "Look at previews/army-sleds/" + id +
                   ".png; read chase in games/fall_line/sim/army_sled.cpp (skill: sim).");
    }
    if (scene == Scene::arrest) {
      lab.metric(id + ".arrest", h.arrested_at, "s");
      if (h.arrested_at < 0) {
        lab.fail("arrest: nobody took him in within " + fixed(limit, 0) + " s",
                 "Look at previews/army-sleds/arrest.png; read chase and dismount in "
                 "games/fall_line/sim/army_sled.cpp (skill: sim).");
      }
    }
    if (!pictures) continue;
    ge::lab::Picture pic = ge::mountain::labs::draw_map(*level, px);
    for (size_t i = 0; i < h.paths.size(); i++) {
      const auto& path = h.paths[i];
      const Rgba c = kPathColours[i % std::size(kPathColours)];
      for (size_t j = 1; j < path.size(); j++) {
        ge::lab::draw_line(pic, path[j - 1].x * k, path[j - 1].z * k, path[j].x * k, path[j].z * k,
                           c, 2);
      }
    }
    for (const fs::ArmySled& m : h.last) {
      const Rgba c = m.mode == fs::ArmySledMode::halt    ? Rgba{0, 0, 0}
                     : m.mode == fs::ArmySledMode::empty ? Rgba{255, 255, 255}
                     : m.slow >= 2                       ? Rgba{255, 0, 0}
                                                         : Rgba{255, 255, 0};
      ge::lab::fill_disc(pic, m.body.x * k, m.body.z * k, 3, c);
    }
    ge::lab::fill_disc(pic, h.q.x * k, h.q.z * k, 5, Rgba{255, 0, 255});
    lab.shot({id,
              "SEED " + fixed(seed, 0) + " ARMY SNOWMOBILES: " + std::string(scene_name(scene)),
              "The map from above (+x right, +z down; the runs red, trees dark green) with the man "
              "hunted (magenta dot) and every army snowmobile's path, a colour a machine, from "
              "the village. Where each machine ended: black halted side-on to him, white left "
              "empty by its crew, red still caught, yellow anything else. " +
                  std::to_string(h.called) + " called, " + std::to_string(h.halts) +
                  " halts; nearest " + fixed(h.nearest, 0) + " m.",
              scene == Scene::arrest
                  ? "one path from the village to the magenta dot and a white dot beside it; no red"
              : scene == Scene::face
                  ? "paths from the village that end in yellow dots at the foot of the steep face "
                    "below him, not wandering along it; no red"
                  : "paths from the village up the mountain toward him that stop a hundred metres "
                    "or more short of him, threading the woods rather than crossing them; no red",
              {{"seed", fixed(seed, 0)},
               {"version", lab.arg("version")},
               {"scenario", id},
               {"stars", std::to_string(h.stars)},
               {"limit s", fixed(limit, 0)}}},
             pic);
  }
  lab.table(std::move(table));
}

const ge::lab::LabSpec kSpec{
    "army-sleds",
    ge::lab::LabKind::scenario,
    "game:fall_line",
    "games/fall_line/sim/army_sled (the army's snowmobiles: the call, the drive over the snow, "
    "the short halt, the stand-off, the arrest)",
    "sim",
    "On a real ski area, do the army's snowmobiles get from the village to a man up the mountain "
    "without getting caught on the way, halt side-on to him where they shoot to kill, and run "
    "him down and take him in where they do not?",
    {
        {"seed", "38", "the map's seed"},
        {"version", "10", "the generator's version (a ski area: 10 or later)"},
        {"limit", "420", "each scenario's time limit, s"},
        {"px", "768", "the pictures' size, px"},
        {"no-pictures", "", "skip the pictures", true},
    },
    "--px=256 --limit=240",
    run,
};
GE_LAB(kSpec);

}  // namespace
