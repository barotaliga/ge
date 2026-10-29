// LAB `army-vehicles`: the ground waves' carriers, trucks and tanks on a real ski area. The hunted
// man is stood on open snow up the mountain from the army's pad (stood again and healed every
// step, so the hunt runs its length) while the hunt is pinned at its stars, and the army runs
// through the real `step` with its helicopters and paramotors kept home, so the waves' vehicles are
// the hunt's machines on the snow: THE VEHICLES table (per scenario, the carriers, trucks and tanks
// called, when the first set off, the nearest any came to him, the men they put down, the times
// one struck him, the times one had to be put back on its way, those gone and those still out)
// and, per scenario, the ground round him from above with every vehicle's track, and a chart of
// every vehicle's distance from him through the hunt.
//
// Scenarios: `three` (three stars: a carrier and a truck from the first wave), `five` (five stars:
// the tanks join), `home` (five stars, the hunt ended at `--stop` s: every vehicle drives back
// down its way and is gone).
//
// A vehicle put back on its way (stuck), a wave that called none, no man put down at three stars,
// no tank at five, or a vehicle still out at the end of `home` is a failure (exit 1).
//
// ge:summary Lab: the army's ground vehicles on seed 38: the call, the drive, the drop, home
// ge:keywords lab army vehicle army-vehicles carrier truck tank drive unload drop stuck paths
// ge:keywords scenario home strike
// ge:skill sim labs
// ge:test tests/fall_line_army_vehicle_test.cpp
#include <memory>
#include <span>
#include <string>
#include <vector>

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "core/num_text.hpp"
#include "fall_line/sim/army.hpp"
#include "fall_line/sim/army_vehicle.hpp"
#include "fall_line/sim/army_vehicle_rules.hpp"
#include "fall_line/sim/free.hpp"
#include "fall_line/sim/game.hpp"
#include "fall_line/sim/wanted.hpp"
#include "lab/chart.hpp"
#include "lab/lab.hpp"
#include "lab/raster.hpp"
#include "mountain/labs/level_map.hpp"
#include "mountain/mapgen/generate.hpp"
#include "skier/body.hpp"
#include "skier/skier.hpp"

namespace {

namespace fs = fall_line::sim;
namespace mg = ge::mountain::mapgen;
namespace gl = ge::lab;
namespace av = fall_line::sim::army_vehicle;
namespace lm = ge::core::libm;
using ge::core::hypot;
using gl::Rgba;

std::string fixed(double v, int d) { return ge::core::num::to_fixed(v, d); }

enum class Scene : uint8_t { three, five, home };
constexpr Scene kScenes[] = {Scene::three, Scene::five, Scene::home};

const char* scene_name(Scene s) {
  switch (s) {
    case Scene::three:
      return "three";
    case Scene::five:
      return "five";
    case Scene::home:
      return "home";
  }
  return "?";
}

// He stands this far up the mountain from the army's pad, m. The ground round him drawn this wide,
// m (the whole map for `home`).
constexpr double kUpM = 400;
constexpr double kViewM = 1600;
// The colours the tracks are drawn in, by kind: the carrier, the truck, the tank.
constexpr Rgba kKindColours[] = {{0, 200, 255}, {255, 140, 0}, {120, 255, 60}};

struct Track {
  fs::ArmyVehicleKind kind = fs::ArmyVehicleKind::carrier;
  std::vector<fs::Spot> path;  // each second
  std::vector<double> t, away;
};

struct Hunt {
  fs::Spot q;
  int32_t stars = 0;
  int32_t kinds[3] = {};  // called, by kind
  double out = -1;        // the clock the first set off, s
  double nearest = -1;    // the nearest any came to him, m
  int32_t men = 0;        // men put down
  int32_t strikes = 0;    // vehicles that ran him down (he is stood again each step: once each)
  int32_t stuck = 0;
  int32_t gone = 0;
  int32_t left = 0;  // still out at the end
  std::vector<Track> tracks;
};

Hunt run_scene(const std::shared_ptr<const mg::Level>& level, Scene scene, double limit,
               double stop) {
  fs::GameState st = fs::create_free_ride(level, 0);
  st.army.send_helis = false;
  st.army.send_paras = false;
  Hunt h;
  h.stars = scene == Scene::three ? 3 : 5;
  const double x = st.army.pad_x + lm::sin(st.army.pad_heading) * kUpM;
  const double z = st.army.pad_z + lm::cos(st.army.pad_heading) * kUpM;
  st.racers[0].skier = ge::skier::stand_skier(st.world(), x, z, 0, 0);
  fs::set_wanted(st, h.stars);
  h.q = fs::quarry_of(st);
  const fs::Controls none{};
  std::vector<bool> struck;
  while (st.t < limit) {
    fs::WantedState& w = st.wanted;
    if (scene == Scene::home && st.t >= stop && w.ended == fs::WantedEnd::running) {
      w.ended = fs::WantedEnd::escaped;
    }
    if (w.ended == fs::WantedEnd::running && w.stars > 0) w.heat = h.stars;
    st.racers[0].skier = ge::skier::stand_skier(st.world(), x, z, 0, 0);
    st.racers[0].body = ge::skier::BodyState{};
    fs::step(st, std::span<const fs::Controls>(&none, 1));
    for (const fs::GameEvent& e : st.events) {
      if (e.kind != fs::EventKind::army_vehicle) continue;
      const auto c = static_cast<fs::ArmyVehicleChange>(e.gate);
      if (c == fs::ArmyVehicleChange::out && h.out < 0) h.out = st.t;
      if (c == fs::ArmyVehicleChange::strike) {
        const auto id = static_cast<size_t>(e.value);
        if (struck.size() <= id) struck.resize(id + 1);
        h.strikes += struck[id] ? 0 : 1;
        struck[id] = true;
      }
      h.gone += c == fs::ArmyVehicleChange::gone ? 1 : 0;
    }
    const bool second = st.steps % static_cast<int64_t>(fs::kHz) == 0;
    for (const fs::ArmyVehicle& v : st.army.vehicles) {
      if (!fs::on_snow(v)) continue;
      const double away = hypot(v.x - x, v.z - z);
      h.nearest = h.nearest < 0 || away < h.nearest ? away : h.nearest;
      if (!second) continue;
      const auto id = static_cast<size_t>(v.id);
      if (h.tracks.size() <= id) h.tracks.resize(id + 1);
      Track& tr = h.tracks[id];
      tr.kind = v.kind;
      tr.path.push_back({v.x, v.y, v.z});
      tr.t.push_back(st.t);
      tr.away.push_back(away);
    }
  }
  for (const Track& tr : h.tracks) h.kinds[static_cast<size_t>(tr.kind)]++;
  h.men = st.army.unloaded;
  h.stuck = st.army.vehicles_stuck;
  h.left = static_cast<int32_t>(st.army.vehicles.size());
  return h;
}

// The ground round him from above, `view` m wide, `px` square, with every vehicle's track.
gl::Picture ground_round(const mg::Level& level, const Hunt& h, double view, int px) {
  const double k = px / view;
  const int full = static_cast<int>(level.size * k);
  const double x0 = ge::core::clamp(h.q.x - view / 2, 0.0, level.size - view);
  const double z0 = ge::core::clamp(h.q.z - view / 2, 0.0, level.size - view);
  gl::Picture pic = gl::create_picture(px, px, Rgba{0, 0, 0});
  gl::blit(pic, ge::mountain::labs::draw_map(level, full), static_cast<int>(-x0 * k),
           static_cast<int>(-z0 * k));
  const auto sx = [&](double x) { return (x - x0) * k; };
  const auto sz = [&](double z) { return (z - z0) * k; };
  gl::draw_ring(pic, sx(h.q.x), sz(h.q.z), av::kStopM * k + 3, Rgba{255, 255, 255});
  for (const Track& tr : h.tracks) {
    const Rgba c = kKindColours[static_cast<size_t>(tr.kind)];
    for (size_t j = 1; j < tr.path.size(); j++) {
      gl::draw_line(pic, sx(tr.path[j - 1].x), sz(tr.path[j - 1].z), sx(tr.path[j].x),
                    sz(tr.path[j].z), c, 2);
    }
    if (!tr.path.empty()) gl::fill_disc(pic, sx(tr.path.back().x), sz(tr.path.back().z), 4, c);
  }
  gl::fill_disc(pic, sx(h.q.x), sz(h.q.z), 5, Rgba{255, 0, 255});
  return pic;
}

void run(gl::Lab& lab) {
  const double seed = lab.arg_num("seed");
  const auto version = static_cast<int>(lab.arg_num("version"));
  const double limit = lab.arg_num("limit");
  const double stop = lab.arg_num("stop");
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
  const auto& resort = level->resort;
  if (!resort || resort->lifts.empty()) {
    lab.fail("seed " + fixed(seed, 0) + " v" + fixed(version, 0) + " has no ski area",
             "Pass a --version that builds a ski area (10 or later; skill: mountain).");
    return;
  }
  gl::LabTable table{"vehicles",
                     {"scenario", "stars", "carriers", "trucks", "tanks", "out s", "nearest m",
                      "men", "strikes", "stuck", "gone", "left"},
                     {}};
  const auto opt = [](double v, int d) { return v >= 0 ? fixed(v, d) : std::string("-"); };
  for (const Scene scene : kScenes) {
    const std::string id = scene_name(scene);
    const double until = scene == Scene::home ? limit + stop : limit;
    const Hunt h = run_scene(level, scene, until, stop);
    const int32_t called = h.kinds[0] + h.kinds[1] + h.kinds[2];
    table.rows.push_back({id, std::to_string(h.stars), std::to_string(h.kinds[0]),
                          std::to_string(h.kinds[1]), std::to_string(h.kinds[2]), opt(h.out, 1),
                          opt(h.nearest, 0), std::to_string(h.men), std::to_string(h.strikes),
                          std::to_string(h.stuck), std::to_string(h.gone), std::to_string(h.left)});
    lab.metric(id + ".carriers", h.kinds[0], "vehicles");
    lab.metric(id + ".trucks", h.kinds[1], "vehicles");
    lab.metric(id + ".tanks", h.kinds[2], "vehicles");
    lab.metric(id + ".out", h.out, "s");
    lab.metric(id + ".nearest", h.nearest, "m");
    lab.metric(id + ".men", h.men, "men");
    lab.metric(id + ".strikes", h.strikes, "strikes");
    lab.metric(id + ".stuck", h.stuck, "times");
    lab.metric(id + ".gone", h.gone, "vehicles");
    std::string look = "Look at previews/army-vehicles/" + id;
    look += ".png and " + id;
    look += "-away.png; read ";
    if (called == 0 || h.out < 0) {
      lab.fail(id + ": " + std::to_string(called) + " vehicles called, the first out at " +
                   opt(h.out, 1) + " s",
               look +
                   "call_vehicles and set_off in games/fall_line/sim/army_vehicle.cpp and "
                   "send_wave in army.cpp (skill: sim).");
    }
    if (h.stuck > 0) {
      lab.fail(id + ": vehicles put back on their way " + std::to_string(h.stuck) + " times",
               look +
                   "watch and drive_to in games/fall_line/sim/army_vehicle.cpp, and the drive "
                   "read in army_vehicle_rules.hpp (skill: sim).");
    }
    if (scene == Scene::three && h.men == 0) {
      lab.fail(
          "three: no man put down in " + fixed(limit, 0) + " s",
          look + "start_drop and unload in games/fall_line/sim/army_vehicle.cpp (skill: sim).");
    }
    if (scene == Scene::five && h.kinds[2] == 0) {
      lab.fail("five: no tank called in " + fixed(limit, 0) + " s",
               look + "ground_wave's tanks and call_vehicles (skill: sim).");
    }
    if (scene == Scene::home && h.left > 0) {
      lab.fail("home: " + std::to_string(h.left) + " vehicles still out " + fixed(limit, 0) +
                   " s after the hunt ended",
               look + "go_home in games/fall_line/sim/army_vehicle.cpp (skill: sim).");
    }
    if (!pictures) continue;
    const double view = scene == Scene::home ? level->size : kViewM;
    lab.shot(
        {id,
         "SEED " + fixed(seed, 0) + " ARMY VEHICLES: " + id,
         "The ground round the man hunted (magenta dot), " + fixed(view, 0) +
             " m across from above (+x right, +z down; the runs red, trees dark green), the "
             "ring the vehicles stop at (white) and every vehicle's track, a carrier's blue, "
             "a truck's orange and a tank's green, its last place a dot. " +
             std::to_string(h.kinds[0]) + " carriers, " + std::to_string(h.kinds[1]) + " trucks, " +
             std::to_string(h.kinds[2]) + " tanks; " + std::to_string(h.men) + " men put down.",
         scene == Scene::home
             ? "tracks that climb the runs toward the magenta dot, then turn back down them "
               "and stop where they end"
             : "tracks that follow the runs up from below and leave them only near the "
               "magenta dot, never looping in one place",
         {{"seed", fixed(seed, 0)},
          {"version", lab.arg("version")},
          {"scenario", id},
          {"stars", std::to_string(h.stars)},
          {"limit s", fixed(until, 0)}}},
        ground_round(*level, h, view, px));
    gl::LineChart lc;
    lc.frame = {"each vehicle's distance from him, " + id,
                "seed " + fixed(seed, 0) + ", " + std::to_string(h.stars) + " stars", "t (s)",
                "distance (m)"};
    lc.rules = {av::kTankLethalM, av::kDropCarrierM, av::kStopM};
    lc.spikes = false;
    for (size_t i = 0; i < h.tracks.size(); i++) {
      if (h.tracks[i].t.empty()) continue;
      lc.traces.push_back(
          {std::string(fs::army_vehicle_kind_name(h.tracks[i].kind)) + " " + std::to_string(i),
           h.tracks[i].t, h.tracks[i].away});
    }
    if (scene == Scene::home) lc.marks.push_back({stop, "hunt over"});
    const gl::Chart ch = gl::line_chart(lc);
    lab.note(gl::chart_text(ch, 4));
    lab.shot({id + "-away",
              "SEED " + fixed(seed, 0) + " ARMY VEHICLES' DISTANCE: " + id,
              "Each vehicle's distance from him in the plan, every second from when it set off; "
              "dashed rules at a tank's stand-off (" +
                  fixed(av::kTankLethalM, 0) + " m), where a carrier halts to put its men down (" +
                  fixed(av::kDropCarrierM, 0) + " m) and where every vehicle stops short of him (" +
                  fixed(av::kStopM, 0) + " m).",
              "every line falling as it climbs, the carriers flattening near the middle rule, the "
              "tanks near the top one, none under the bottom one",
              {{"seed", fixed(seed, 0)}, {"scenario", id}, {"stars", std::to_string(h.stars)}}},
             gl::to_picture(ch.plot));
  }
  lab.table(std::move(table));
}

const gl::LabSpec kSpec{
    "army-vehicles",
    gl::LabKind::scenario,
    "game:fall_line",
    "games/fall_line/sim/army_vehicle (the ground waves' carriers, trucks and tanks: the call, "
    "the drive, the drop, home)",
    "sim",
    "On a real ski area, do the ground waves' vehicles drive up the runs to him without sticking, "
    "put their men down near him, stand off where they should, and drive home when the hunt is "
    "over?",
    {
        {"seed", "38", "the map's seed"},
        {"version", "10", "the generator's version (a ski area: 10 or later)"},
        {"limit", "300", "each scenario's time limit (home: after the hunt ends), s"},
        {"stop", "200", "when `home`'s hunt ends, s"},
        {"px", "768", "the pictures' size, px"},
        {"no-pictures", "", "skip the pictures", true},
    },
    "--px=256 --limit=150 --stop=100",
    run,
};
GE_LAB(kSpec);

}  // namespace
