// LAB `tarn-ride`: a skier at the open tarn (games/fall_line/sim/lake.hpp), through the real
// `step`. Seed 38's free-ride map with its tarn laid open (no ice, the bowl dug), and a skier stood
// on the south bank heading north across it at each speed of a ladder, each in a lane of its own,
// nothing pressed: THE RIDES table (the speed, how he met the water, the metres he skimmed, where
// he went in, north of the tarn's middle, the seconds in the water, the metres he swam, how it
// ended, the seconds it took) and the tarn from above with every line over it, white on his skis,
// yellow skimming, blue swimming. The ladder's slow end goes in on the shelf and wades out; its
// middle skims, goes in and swims out; its fast end skims to the far bank. A ride that neither
// climbs out nor is reset in its time is a failure.
//
// ge:summary Lab: a skier onto the open tarn at a ladder of speeds: skimming, in, swimming out
// ge:keywords lab tarn-ride tarn lake skim sank ashore swim step_on_water skim-speed sink-speed
// ge:skill sim labs
// ge:test tests/fall_line_lake_test.cpp
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "core/math.hpp"
#include "core/num_text.hpp"
#include "fall_line/sim/free.hpp"
#include "fall_line/sim/game.hpp"
#include "fall_line/sim/lake.hpp"
#include "lab/lab.hpp"
#include "lab/raster.hpp"
#include "mountain/mapgen/generate.hpp"
#include "mountain/picture/map_picture.hpp"
#include "mountain/tarn/tarn.hpp"
#include "skier/skier.hpp"

namespace {

namespace fs = fall_line::sim;
namespace gl = ge::lab;
namespace mg = ge::mountain::mapgen;
namespace tn = ge::mountain::tarn;

std::string fixed(double v, int d) { return ge::core::num::to_fixed(v, d); }

// Where he stands, in radii south of the tarn's middle (past its south shore).
constexpr double kFrom = 1.25;

enum class Phase { skis, skim, swim };

struct Mark {
  double x, z;
  Phase phase;
};

struct Ride {
  std::string met = "-", end = "still in";
  bool went_in = false;
  double skimmed = 0, in_at = 0, in_water = 0, swum = 0, time = 0;
  std::vector<Mark> line;
};

Ride ride(std::shared_ptr<const mg::Level> level, double speed, double off, double limit) {
  fs::GameState state = fs::create_free_ride(level, 0);
  const mg::MapLake& lake = state.level->lakes[0];
  state.racers[0].skier =
      ge::skier::stand_skier(state.world(), lake.x + off, lake.z - kFrom * lake.radius, 0, speed);
  Ride r;
  const fs::Controls none{};
  double px = state.racers[0].skier.x, pz = state.racers[0].skier.z;
  for (int i = 0; i < static_cast<int>(limit * fs::kHz); i++) {
    fs::step(state, std::span<const fs::Controls>(&none, 1));
    const fs::FreeRide& f = state.racers[0].free;
    const ge::skier::SkierState& s = state.racers[0].skier;
    const double d = ge::core::hypot(s.x - px, s.z - pz);
    if (f.skimming) r.skimmed += d;
    if (f.swimming && d < 5) r.swum += d;
    px = s.x;
    pz = s.z;
    if (i % 12 == 0)
      r.line.push_back({s.x, s.z,
                        f.swimming   ? Phase::swim
                        : f.skimming ? Phase::skim
                                     : Phase::skis});
    bool done = false;
    for (const fs::GameEvent& e : state.events) {
      if (e.kind == fs::EventKind::skim && r.met == "-") r.met = "skimmed";
      if (e.kind == fs::EventKind::sank) {
        if (r.met == "-") r.met = "in";
        r.went_in = true;
        r.in_at = s.z - lake.z;
      }
      if (e.kind == fs::EventKind::ashore) {
        r.end = "ashore";
        r.in_water = e.value;
        done = true;
      }
      if (e.kind == fs::EventKind::reset) {
        r.end = "reset";
        done = true;
      }
    }
    r.time = state.t;
    if (f.swimming) r.in_water = f.swim.in_water;
    if (done) break;
    // Across on his skis and stopped on the far bank: over.
    if (r.met == "skimmed" && !fs::on_water(state, 0) && s.speed < 0.5) {
      r.end = "far bank";
      break;
    }
    if (r.met == "-" && s.speed < 0.1 && i > fs::kHz) {
      r.end = "stopped short";
      break;
    }
  }
  return r;
}

void run(gl::Lab& lab) {
  const double seed = lab.arg_num("seed");
  const double limit = lab.arg_num("limit");
  const std::vector<double> speeds = lab.arg_nums("speeds");
  const std::string k = "seed-" + fixed(seed, 0);
  mg::Level built;
  try {
    built = fs::free_map(seed, 10);
  } catch (const mg::GenerateError& e) {
    lab.fail("seed " + fixed(seed, 0) + " built no map: " + e.what(),
             "Run `make lab LAB=analyze ARGS=--seeds=" + fixed(seed, 0) +
                 " --refusals` (skill: mountain).");
    return;
  }
  if (built.lakes.empty()) {
    lab.fail(k + ": the free ride laid no tarn",
             "Read find_tarn in plugins/mountain/tarn/tarn.hpp (skill: mountain).");
    return;
  }
  // The tarn the free ride lays, opened: no ice, the bowl dug under it.
  mg::MapLake lake = built.lakes[0];
  built.lakes.clear();
  {
    mg::GenerateOptions opts;
    opts.version = 10;
    mg::Level fresh = mg::generate_level(seed, opts);
    lake.ice = 0;
    tn::dig_basin(fresh, lake, tn::kTarnDepth, false);
    fresh.lakes.push_back(lake);
    built = std::move(fresh);
  }
  const auto level = std::make_shared<const mg::Level>(std::move(built));

  constexpr int kPx = 720;
  const double side = 3.2 * lake.radius;
  const double cz = lake.z + 0.15 * lake.radius;
  gl::Picture pic = ge::mountain::picture::draw_map(
      *level, kPx, ge::mountain::picture::MapWindow{lake.x, cz, side});
  const double px = kPx / side;
  const auto at = [&](double x, double z) {
    return std::pair{(x - lake.x + side / 2) * px, (z - cz + side / 2) * px};
  };
  const std::vector<ge::water::Point> shore = tn::shore_of(lake);
  for (size_t i = 0; i < shore.size(); i++) {
    const ge::water::Point& a = shore[i];
    const ge::water::Point& b = shore[(i + 1) % shore.size()];
    const auto [x0, y0] = at(a.x, a.z);
    const auto [x1, y1] = at(b.x, b.z);
    gl::draw_line(pic, x0, y0, x1, y1, gl::Rgba{40, 90, 200}, 2);
  }
  gl::LabTable table{
      "Onto seed " + fixed(seed, 0) + "'s open tarn from the south bank (" + fixed(lake.radius, 0) +
          " m round, " + fixed(lake.depth, 1) + " m deep)",
      {"speed m/s", "met it", "skimmed m", "in at z m", "in water s", "swum m", "end", "time s"},
      {}};
  const double lane = 0.9 * lake.radius / static_cast<double>(speeds.size());
  for (size_t n = 0; n < speeds.size(); n++) {
    const double v = speeds[n];
    // Each ride in its own lane, side by side, west to east by speed.
    const double off =
        (static_cast<double>(n) - 0.5 * static_cast<double>(speeds.size() - 1)) * lane;
    Ride r = ride(level, v, off, limit);
    if (r.end == "still in" || r.end == "stopped short") {
      lab.fail(k + ": at " + fixed(v, 0) + " m/s the ride ended " + r.end,
               "A skier onto the open tarn skims across, or goes in and climbs out or is reset: "
               "read games/fall_line/sim/lake.cpp (skill: sim).");
    }
    for (size_t i = 1; i < r.line.size(); i++) {
      const Mark& a = r.line[i - 1];
      const Mark& b = r.line[i];
      const gl::Rgba c = b.phase == Phase::swim   ? gl::Rgba{60, 170, 255}
                         : b.phase == Phase::skim ? gl::Rgba{255, 220, 0}
                                                  : gl::Rgba{255, 255, 255};
      const auto [x0, y0] = at(a.x, a.z);
      const auto [x1, y1] = at(b.x, b.z);
      gl::draw_line(pic, x0, y0, x1, y1, c, 2);
    }
    if (!r.line.empty()) {
      const auto [tx, ty] = at(r.line.front().x, r.line.front().z);
      gl::draw_text(pic, tx - 6, ty + 6, fixed(v, 0), gl::Rgba{255, 255, 255});
    }
    const std::string key = k + ".v" + fixed(v, 0);
    lab.metric(key + ".skimmed", r.skimmed, "m");
    lab.metric(key + ".in-water", r.in_water, "s");
    lab.metric(key + ".swum", r.swum, "m");
    table.rows.push_back({fixed(v, 0), r.met, fixed(r.skimmed, 1),
                          r.went_in ? fixed(r.in_at, 1) : "-", fixed(r.in_water, 1),
                          fixed(r.swum, 1), r.end, fixed(r.time, 1)});
  }
  lab.table(std::move(table));
  lab.shot({k + "-rides",
            "SEED " + fixed(seed, 0) + ": ONTO THE OPEN TARN AT A LADDER OF SPEEDS",
            "The tarn from above (+x right, +z down the picture), its shore in blue. Each ride "
            "from the south bank heading north across it, nothing pressed, each in a lane of its "
            "own west to east by speed (the number at its foot, m/s): white on his skis, yellow "
            "skimming, blue swimming.",
            "the slow rides go in on the shelf and wade straight out; the fast ones are yellow "
            "a long way out, then blue where the water's drag has them under 8 m/s; the fastest "
            "is yellow all the way and white again on the north bank",
            {{"seed", fixed(seed, 0)}, {"from", fixed(kFrom, 2) + " radii south"}}},
           pic);
}

const gl::LabSpec kSpec{
    "tarn-ride",
    gl::LabKind::drive,
    "game:fall_line",
    "games/fall_line/sim/lake (enter_lake, step_on_water: skimming, the swimmer, ashore) on the "
    "free ride's tarn laid open",
    "sim",
    "Onto the open tarn at each speed: does he skim, how far, where does he go in, and does he "
    "swim out (or is he reset)?",
    {
        {"seed", "38", "the map's seed"},
        {"speeds", "8,12,15,17,20,25,30,45", "the speeds onto the water, m/s, comma-separated"},
        {"limit", "120", "a ride's time limit, s"},
    },
    "--seed=38 --speeds=8,17,45",
    run,
};
GE_LAB(kSpec);

}  // namespace
