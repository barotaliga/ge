// LAB `army`: the army on foot on a real ski area. The hunted man stands still (stood again every
// step, never reset for it) at a spot of the map while the hunt is pinned at its stars and a
// helicopter hovers over him, and the army runs through the real `step` (`step_army`): THE ARMY
// table (per scenario, the lifted men sent and let off at the lift's top, the waves' men, the most
// out at once, how many hold him at the end, the nearest man, the men caught and the most windows
// in a row any was caught, the arrest and when) and, per scenario, the map with every soldier's
// path over it.
//
// Scenarios: `valley` (beside the first lift's bottom station, three stars, the army shooting to
// kill: the lifted squad rides the lift whose top is over him and the waves climb from further
// down the valley), `slope` (beside the highest lift's line, more than halfway up it, four
// stars: the squads ride up past him and ski down at him, the waves climb the runs from below),
// `arrest` (the valley spot with the army not shooting to kill: they run him down).
//
// A man still caught at the end, a scenario whose lifted men are never let off, or an arrest
// that never comes is a failure (exit 1).
//
// ge:summary Lab: the army on foot on seed 38: lift squads, waves, stand-off, arrest, paths
// ge:keywords lab army soldier troops lift-squad wave column stand-off arrest stuck paths
// ge:keywords scenario valley slope
// ge:skill sim labs
// ge:test tests/fall_line_army_test.cpp
#include <memory>
#include <span>
#include <string>
#include <vector>

#include "core/math.hpp"
#include "core/num_text.hpp"
#include "fall_line/sim/army.hpp"
#include "fall_line/sim/army_rules.hpp"
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
namespace ar = fall_line::sim::army;
using ge::core::hypot;
using ge::lab::Rgba;

std::string fixed(double v, int d) { return ge::core::num::to_fixed(v, d); }

enum class Scene : uint8_t { valley, slope, arrest };
constexpr Scene kScenes[] = {Scene::valley, Scene::slope, Scene::arrest};

const char* scene_name(Scene s) {
  return s == Scene::valley ? "valley" : s == Scene::slope ? "slope" : "arrest";
}

// How far from a lift's line he stands, m; up the highest lift's line on the slope, its share.
constexpr double kBesideM = 40;
constexpr double kSlopeUp = 0.6;
constexpr double kSlopeBesideM = 80;
// The helicopter keeping sight of him, m over him.
constexpr double kHoverM = 150;

struct Hunt {
  fs::Spot q;
  int32_t stars = 0;
  int32_t lift = -1;
  int32_t lifted = 0;
  int32_t off = 0;
  int32_t waves = 0;
  int32_t most = 0;
  int32_t holding = 0;
  int32_t out = 0;
  double nearest = 0;
  int32_t caught = 0;  // men caught (two windows or more in a row) at the end
  int32_t worst = 0;   // the most windows in a row any man was caught
  double arrested_at = -1;
  // Every soldier's path, by id, every second; and where each one ended.
  std::vector<std::vector<fs::Spot>> paths;
  std::vector<fs::SoldierFrom> from;
  std::vector<fs::Soldier> last;
};

// Where he stands in a scene.
fs::Spot spot_of(const fs::GameState& st, Scene scene) {
  const auto& lifts = *st.lifts;
  if (scene != Scene::slope) {
    const ml::LiftPlan& p = lifts.front();
    return {p.bottom.x - p.dz * kBesideM, 0, p.bottom.z + p.dx * kBesideM};
  }
  size_t high = 0;
  for (size_t i = 1; i < lifts.size(); i++) {
    const auto top = [&st](const ml::LiftPlan& p) { return st.level->ground_at(p.top.x, p.top.z); };
    if (top(lifts[i]) > top(lifts[high])) high = i;
  }
  const ml::LiftPlan& p = lifts[high];
  const double u = p.length * kSlopeUp;
  return {p.bottom.x + p.dx * u - p.dz * kSlopeBesideM, 0,
          p.bottom.z + p.dz * u + p.dx * kSlopeBesideM};
}

Hunt run_scene(const std::shared_ptr<const mg::Level>& level, Scene scene, double limit) {
  fs::GameState st = fs::create_free_ride(level, 0);
  // Staged: the army on foot alone: its helicopters are the army-flight lab's.
  st.army.send_helis = false;
  Hunt h;
  h.stars = scene == Scene::slope ? 4 : 3;
  const fs::Spot at = spot_of(st, scene);
  st.racers[0].skier = ge::skier::stand_skier(st.world(), at.x, at.z, 0, 0);
  st.wanted.lethal = scene != Scene::arrest;
  fs::set_wanted(st, h.stars);
  h.q = fs::quarry_of(st);
  h.lift = fs::lift_for(st, h.q);
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
      if (e.kind != fs::EventKind::army) continue;
      const auto n = static_cast<int32_t>(e.value);
      const fs::ArmyChange c = fs::army_change(e);
      h.lifted += c == fs::ArmyChange::squad ? n : 0;
      h.off += c == fs::ArmyChange::off ? n : 0;
      h.waves += c == fs::ArmyChange::wave ? n : 0;
      if (c == fs::ArmyChange::arrest) h.arrested_at = st.t;
    }
    const auto out = static_cast<int32_t>(st.army.soldiers.size());
    h.most = out > h.most ? out : h.most;
    for (const fs::Soldier& s : st.army.soldiers)
      h.worst = s.progress.slow > h.worst ? s.progress.slow : h.worst;
    if (st.steps % static_cast<int64_t>(fs::kHz) == 0) {
      for (const fs::Soldier& s : st.army.soldiers) {
        const auto id = static_cast<size_t>(s.id);
        if (h.paths.size() <= id) {
          h.paths.resize(id + 1);
          h.from.resize(id + 1);
        }
        h.paths[id].push_back({s.x, s.y, s.z});
        h.from[id] = s.from;
      }
    }
    if (h.arrested_at >= 0 && st.t > h.arrested_at + 5) break;
  }
  h.out = static_cast<int32_t>(st.army.soldiers.size());
  h.nearest = 1e9;
  for (const fs::Soldier& s : st.army.soldiers) {
    h.holding += s.mode == fs::SoldierMode::hold ? 1 : 0;
    h.caught += s.progress.slow >= 2 ? 1 : 0;
    const double d = hypot(s.x - h.q.x, s.z - h.q.z);
    h.nearest = d < h.nearest ? d : h.nearest;
  }
  h.last = st.army.soldiers;
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
    lab.fail("seed " + fixed(seed, 0) + " v" + fixed(version, 0) + " has no lifts to send men up",
             "Pass a --version that builds a ski area (10 or later; skill: mountain).");
    return;
  }
  ge::lab::LabTable table{
      "army",
      {"scenario", "stars", "lift", "lifted", "let off", "waves' men", "most out", "holding", "out",
       "nearest m", "caught", "worst", "arrest s"},
      {}};
  const double k = px / level->size;
  for (const Scene scene : kScenes) {
    const std::string id = scene_name(scene);
    const Hunt h = run_scene(level, scene, limit);
    table.rows.push_back({id, std::to_string(h.stars), std::to_string(h.lift),
                          std::to_string(h.lifted), std::to_string(h.off), std::to_string(h.waves),
                          std::to_string(h.most), std::to_string(h.holding), std::to_string(h.out),
                          fixed(h.nearest, 1), std::to_string(h.caught), std::to_string(h.worst),
                          h.arrested_at >= 0 ? fixed(h.arrested_at, 2) : "-"});
    lab.metric(id + ".lifted", h.lifted, "men");
    lab.metric(id + ".let-off", h.off, "men");
    lab.metric(id + ".waves", h.waves, "men");
    lab.metric(id + ".most-out", h.most, "men");
    lab.metric(id + ".holding", h.holding, "men");
    lab.metric(id + ".nearest", h.nearest, "m");
    lab.metric(id + ".caught", h.caught, "men");
    lab.metric(id + ".worst-caught", h.worst, "windows");
    if (h.caught > 0) {
      lab.fail(id + ": " + std::to_string(h.caught) + " men still caught at the end",
               "Look at previews/army/" + id +
                   ".png for where the paths stop; read way_toward and watch_progress in "
                   "games/fall_line/sim/army_troop.cpp (skill: sim).");
    }
    if (h.lifted > 0 && h.off == 0 && h.arrested_at < 0) {
      lab.fail(id + ": the lifted squad was never let off at the lift's top",
               "Read ride and wait_at_lift in games/fall_line/sim/army.cpp; run `make lab "
               "LAB=lift-ride` for the lift itself (skill: sim).");
    }
    if (scene == Scene::arrest) {
      lab.metric(id + ".arrest", h.arrested_at, "s");
      if (h.arrested_at < 0) {
        lab.fail("arrest: nobody took him in within " + fixed(limit, 0) + " s",
                 "Look at previews/army/arrest.png; read try_arrest and chase in "
                 "games/fall_line/sim/army.cpp (skill: sim).");
      }
    }
    if (!pictures) continue;
    ge::lab::Picture pic = ge::mountain::labs::draw_map(*level, px);
    for (size_t i = 0; i < h.paths.size(); i++) {
      const auto& path = h.paths[i];
      const Rgba c = h.from[i] == fs::SoldierFrom::lift ? Rgba{0, 200, 255} : Rgba{255, 140, 0};
      for (size_t j = 1; j < path.size(); j++) {
        ge::lab::draw_line(pic, path[j - 1].x * k, path[j - 1].z * k, path[j].x * k, path[j].z * k,
                           c, 1);
      }
    }
    for (const fs::Soldier& s : h.last) {
      const Rgba c = s.mode == fs::SoldierMode::hold     ? Rgba{0, 0, 0}
                     : s.mode == fs::SoldierMode::arrest ? Rgba{255, 255, 255}
                     : s.progress.slow >= 2              ? Rgba{255, 0, 0}
                                                         : Rgba{255, 255, 0};
      ge::lab::fill_disc(pic, s.x * k, s.z * k, 2, c);
    }
    ge::lab::fill_disc(pic, h.q.x * k, h.q.z * k, 5, Rgba{255, 0, 255});
    lab.shot({id,
              "SEED " + fixed(seed, 0) + " ARMY: " + std::string(scene_name(scene)),
              "The map from above (+x right, +z down; the runs red, trees dark green) with the man "
              "hunted (magenta dot) and every soldier's path: light blue a lifted squad's (up its "
              "lift and down at him), orange a ground wave's (up the runs from the valley). Where "
              "each man ended: black holding him in sight, white the arrest, red still caught, "
              "yellow anything else. " +
                  std::to_string(h.lifted) + " lifted, " + std::to_string(h.waves) +
                  " in the waves; nearest " + fixed(h.nearest, 0) + " m.",
              scene == Scene::arrest
                  ? "orange paths converging on the magenta dot and one white dot on it; no red"
                  : "a blue path up a lift's line and back down to him; orange columns up the "
                    "runs from below; black dots round him where each first saw him (a few "
                    "hundred metres on open ground, nearer over a brow); no red",
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
    "army",
    ge::lab::LabKind::scenario,
    "game:fall_line",
    "games/fall_line/sim/army (the army on foot: the lifts' squads, the ground waves, the chase, "
    "the stand-off, the arrest)",
    "sim",
    "On a real ski area, do the soldiers the hunt sends ride the lifts up, climb the runs and "
    "close on a man without getting caught on the way, and how long do they take?",
    {
        {"seed", "38", "the map's seed"},
        {"version", "10", "the generator's version (a ski area: 10 or later)"},
        {"limit", "600", "each scenario's time limit, s"},
        {"px", "768", "the pictures' size, px"},
        {"no-pictures", "", "skip the pictures", true},
    },
    "--px=256 --limit=360",
    run,
};
GE_LAB(kSpec);

}  // namespace
