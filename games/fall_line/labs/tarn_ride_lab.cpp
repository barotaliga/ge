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
// THE SNOWMOBILE (M24l4): the free ride's machine, ridden, driven onto the same tarn from the same
// bank at each speed of a ladder of its own with nothing pressed (coasting), and once flat out
// (`--sled-flat`): THE MACHINE'S RIDES table and a second picture, white on the snow, orange
// planing, blue its rider swimming where it went under. Slower than it planes it goes straight
// under; coasting at a planing speed it skims and goes under where the water has it below 6 m/s;
// flat out it planes to the far bank with him still on.
//
// ge:summary Lab: a skier onto the open tarn at a ladder of speeds: skimming, in, swimming out
// ge:keywords lab tarn-ride tarn lake skim sank ashore swim step_on_water skim-speed sink-speed
// ge:keywords snowmobile-on-water sled_on_lake sunk-sled
// ge:skill sim labs
// ge:test tests/fall_line_lake_test.cpp
#include <cstdint>
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
#include "fall_line/sim/sled_outing.hpp"
#include "fall_line/sim/sled_ride.hpp"
#include "lab/lab.hpp"
#include "lab/raster.hpp"
#include "mountain/mapgen/generate.hpp"
#include "mountain/picture/map_picture.hpp"
#include "mountain/tarn/tarn.hpp"
#include "skier/skier.hpp"
#include "snowmobile/sled.hpp"

namespace {

namespace fs = fall_line::sim;
namespace gl = ge::lab;
namespace mg = ge::mountain::mapgen;
namespace tn = ge::mountain::tarn;

std::string fixed(double v, int d) { return ge::core::num::to_fixed(v, d); }

// Where he stands, in radii south of the tarn's middle (past its south shore).
constexpr double kFrom = 1.25;

enum class Phase : uint8_t { skis, skim, swim };

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

// How a ride is made: a skier on his skis, or the snowmobile ridden coasting or flat out.
enum class On : uint8_t { skis, sled, sled_flat };

Ride ride(const std::shared_ptr<const mg::Level>& level, double speed, double off, double limit,
          On on) {
  fs::GameState state = fs::create_free_ride(level, 0);
  const mg::MapLake& lake = state.level->lakes[0];
  fs::Controls c{};
  if (on == On::skis) {
    state.racers[0].skier =
        ge::skier::stand_skier(state.world(), lake.x + off, lake.z - kFrom * lake.radius, 0, speed);
  } else {
    // Taken on its spot, then stood on the bank heading north at the speed, its belt turning so.
    fs::stand_by_sled(state, 0);
    fs::Controls take;
    take.machine = true;
    fs::step(state, std::span<const fs::Controls>(&take, 1));
    ge::snowmobile::SledState& b = state.sled.body;
    const double rpm = b.rpm;
    b = ge::snowmobile::stand_sled(*state.level, lake.x + off, lake.z - kFrom * lake.radius, 0);
    b.vz = speed;
    b.speed = speed;
    b.way = speed;
    b.tread_speed = speed;
    b.running = true;
    b.rpm = rpm;
    if (on == On::sled_flat) c.ski.tuck = 1;
  }
  const bool sled = on != On::skis;
  Ride r;
  double px = state.racers[0].skier.x, pz = state.racers[0].skier.z;
  for (int i = 0; i < static_cast<int>(limit * fs::kHz); i++) {
    fs::step(state, std::span<const fs::Controls>(&c, 1));
    const fs::FreeRide& f = state.racers[0].free;
    const ge::skier::SkierState& s = state.racers[0].skier;
    const double d = ge::core::hypot(s.x - px, s.z - pz);
    const bool skimming = f.skimming || (sled && state.sled.skimming);
    if (skimming) r.skimmed += d;
    if (f.swimming && d < 5) r.swum += d;
    px = s.x;
    pz = s.z;
    if (i % 12 == 0)
      r.line.push_back({s.x, s.z, f.swimming ? Phase::swim : skimming ? Phase::skim : Phase::skis});
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
    // The machine planed across onto the far bank with him on: over.
    if (sled && r.met == "skimmed" && !r.went_in && !state.sled.skimming && fs::on_sled(state, 0)) {
      r.end = "far bank";
      break;
    }
    // Across on his skis and stopped on the far bank: over.
    if (!sled && r.met == "skimmed" && !fs::on_water(state, 0) && s.speed < 0.5) {
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
  const std::vector<double> sled_speeds = lab.arg_nums("sled-speeds");
  const double sled_flat = lab.arg_num("sled-flat");
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
  const auto picture = [&] {
    gl::Picture pic = ge::mountain::picture::draw_map(
        *level, kPx, ge::mountain::picture::MapWindow{lake.x, cz, side});
    const std::vector<ge::water::Point> shore = tn::shore_of(lake);
    const double px = kPx / side;
    for (size_t i = 0; i < shore.size(); i++) {
      const ge::water::Point& a = shore[i];
      const ge::water::Point& b = shore[(i + 1) % shore.size()];
      gl::draw_line(pic, (a.x - lake.x + side / 2) * px, (a.z - cz + side / 2) * px,
                    (b.x - lake.x + side / 2) * px, (b.z - cz + side / 2) * px,
                    gl::Rgba{40, 90, 200}, 2);
    }
    return pic;
  };
  const double px = kPx / side;
  const auto at = [&](double x, double z) {
    return std::pair{(x - lake.x + side / 2) * px, (z - cz + side / 2) * px};
  };
  // Each ride of a ladder in its own lane, side by side, west to east in its order: its row, its
  // metrics and its line on `pic` (`skim` the colour planing).
  const auto ladder = [&](gl::Picture& pic, gl::LabTable& table, const std::vector<On>& ons,
                          const std::vector<double>& vs, const std::string& tag, gl::Rgba skim) {
    const double lane = 0.9 * lake.radius / static_cast<double>(vs.size());
    for (size_t n = 0; n < vs.size(); n++) {
      const double v = vs[n];
      const double off = (static_cast<double>(n) - 0.5 * static_cast<double>(vs.size() - 1)) * lane;
      Ride r = ride(level, v, off, limit, ons[n]);
      const std::string how = ons[n] == On::sled_flat ? " flat out" : "";
      if (r.end == "still in" || r.end == "stopped short") {
        std::string what = k;
        what += ": ";
        what += tag;
        what += " at ";
        what += fixed(v, 0);
        what += " m/s";
        what += how;
        what += " the ride ended ";
        what += r.end;
        lab.fail(what,
                 "Onto the open tarn it skims across, or goes in and climbs out or is reset: "
                 "read games/fall_line/sim/lake.cpp (skill: sim).");
      }
      for (size_t i = 1; i < r.line.size(); i++) {
        const Mark& a = r.line[i - 1];
        const Mark& b = r.line[i];
        const gl::Rgba c = b.phase == Phase::swim   ? gl::Rgba{60, 170, 255}
                           : b.phase == Phase::skim ? skim
                                                    : gl::Rgba{255, 255, 255};
        const auto [x0, y0] = at(a.x, a.z);
        const auto [x1, y1] = at(b.x, b.z);
        gl::draw_line(pic, x0, y0, x1, y1, c, 2);
      }
      if (!r.line.empty()) {
        const auto [tx, ty] = at(r.line.front().x, r.line.front().z);
        gl::draw_text(pic, tx - 6, ty + 6, fixed(v, 0) + (how.empty() ? "" : "!"),
                      gl::Rgba{255, 255, 255});
      }
      const std::string key = k + tag + ".v" + fixed(v, 0) + (how.empty() ? "" : "-flat");
      lab.metric(key + ".skimmed", r.skimmed, "m");
      lab.metric(key + ".in-water", r.in_water, "s");
      lab.metric(key + ".swum", r.swum, "m");
      table.rows.push_back({fixed(v, 0) + how, r.met, fixed(r.skimmed, 1),
                            r.went_in ? fixed(r.in_at, 1) : "-", fixed(r.in_water, 1),
                            fixed(r.swum, 1), r.end, fixed(r.time, 1)});
    }
  };
  const std::vector<std::string> columns{"speed m/s",  "met it", "skimmed m", "in at z m",
                                         "in water s", "swum m", "end",       "time s"};
  {
    gl::Picture pic = picture();
    gl::LabTable table{"Onto seed " + fixed(seed, 0) + "'s open tarn from the south bank (" +
                           fixed(lake.radius, 0) + " m round, " + fixed(lake.depth, 1) + " m deep)",
                       columns,
                       {}};
    ladder(pic, table, std::vector<On>(speeds.size(), On::skis), speeds, "", gl::Rgba{255, 220, 0});
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
  if (!sled_speeds.empty()) {
    gl::Picture pic = picture();
    gl::LabTable table{"The snowmobile onto seed " + fixed(seed, 0) +
                           "'s open tarn from the south bank, coasting (and once flat out)",
                       columns,
                       {}};
    std::vector<On> ons(sled_speeds.size(), On::sled);
    std::vector<double> vs = sled_speeds;
    if (sled_flat > 0) {
      ons.push_back(On::sled_flat);
      vs.push_back(sled_flat);
    }
    ladder(pic, table, ons, vs, ".sled", gl::Rgba{255, 140, 0});
    lab.table(std::move(table));
    lab.shot({k + "-sled",
              "SEED " + fixed(seed, 0) + ": THE SNOWMOBILE ONTO THE OPEN TARN",
              "The tarn from above as before. The free ride's machine ridden from the south bank "
              "heading north, coasting at each speed of its ladder (the number at its foot, m/s) "
              "and once flat out (marked !), each in a lane of its own: white on the snow, orange "
              "planing, blue its rider swimming where it went under.",
              "the slow ones go straight under at the shore and he wades out; the coasting ones "
              "are orange out over the water and blue where they fell under 6 m/s; the one flat "
              "out is orange all the way and white again on the north bank",
              {{"seed", fixed(seed, 0)}, {"from", fixed(kFrom, 2) + " radii south"}}},
             pic);
  }
}

const gl::LabSpec kSpec{
    "tarn-ride",
    gl::LabKind::drive,
    "game:fall_line",
    "games/fall_line/sim/lake (enter_lake, step_on_water, sled_on_lake: skimming, the swimmer, "
    "ashore, the snowmobile planing and going under) on the free ride's tarn laid open",
    "sim",
    "Onto the open tarn at each speed: does he skim, how far, where does he go in, and does he "
    "swim out (or is he reset)?",
    {
        {"seed", "38", "the map's seed"},
        {"speeds", "8,12,15,17,20,25,30,45", "the speeds onto the water, m/s, comma-separated"},
        {"limit", "120", "a ride's time limit, s"},
        {"sled-speeds", "6,12,15,18", "the snowmobile's speeds onto the water coasting, m/s"},
        {"sled-flat", "12", "the snowmobile's speed onto the water flat out, m/s (0: none)"},
    },
    "--seed=38 --speeds=8,17,45 --sled-speeds=6,17 --sled-flat=12",
    run,
};
GE_LAB(kSpec);

}  // namespace
