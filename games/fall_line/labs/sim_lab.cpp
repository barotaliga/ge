// LAB `sim`: the bot down each seed's map from the start gate to the finish line, through the
// real `step`, every control recorded on the tape: THE RUN REPORT table (finished, time, gates
// taken and missed, resets, wipeouts, flights, top and mean speed, the hardest landing, the
// run's digest) and, per seed, the map from above with the bot's line drawn over the piste. It
// is Fall Line's headless `sim` CLI: `make sim SEEDS=1,2,3`. A seed the bot does not finish is a
// failure (exit 1), so a regression in the skier, the course or the bot shows here first.
//
// ge:summary Lab: the bot over seeds, the run report table, its line on the map; the sim CLI
// ge:keywords lab sim simulate bot run-report digest seeds finish time gates wipeouts cli
// ge:skill sim labs
// ge:test tests/fall_line_sim_test.cpp
#include <memory>
#include <string>
#include <vector>

#include "core/num_text.hpp"
#include "fall_line/sim/game.hpp"
#include "fall_line/sim/simulate.hpp"
#include "lab/lab.hpp"
#include "lab/raster.hpp"
#include "mountain/labs/level_map.hpp"
#include "mountain/mapgen/generate.hpp"

namespace {

namespace fs = fall_line::sim;
using ge::lab::Rgba;

std::string fixed(double v, int d) { return ge::core::num::to_fixed(v, d); }

std::string hex(uint64_t u) {
  static const char* kDigits = "0123456789abcdef";
  std::string s(16, '0');
  for (int i = 15; i >= 0; i--, u >>= 4) s[static_cast<size_t>(i)] = kDigits[u & 15];
  return s;
}

void run(ge::lab::Lab& lab) {
  const std::vector<double> seeds = lab.arg_nums("seeds");
  const bool pictures = !lab.arg_bool("no-pictures");
  fs::SimOptions opts;
  opts.max_seconds = lab.arg_num("limit");
  opts.version = static_cast<int>(lab.arg_num("version"));
  ge::lab::LabTable table{"runs",
                          {"seed", "finish", "time s", "gates", "missed", "resets", "wipeouts",
                           "flights", "top km/h", "mean km/h", "landing g", "digest"},
                          {}};
  int finished = 0;
  double total_time = 0;
  for (const double seed : seeds) {
    const std::string k = "seed-" + fixed(seed, 0);
    std::shared_ptr<const ge::mountain::mapgen::Level> level;
    try {
      level = std::make_shared<const ge::mountain::mapgen::Level>(fs::race_map(seed, opts.version));
    } catch (const ge::mountain::mapgen::GenerateError& e) {
      lab.fail("seed " + fixed(seed, 0) + " built no map: " + e.what(),
               "Run `make lab LAB=analyze ARGS=--seeds=" + fixed(seed, 0) +
                   " --refusals` (skill: mountain).");
      continue;
    }
    std::vector<fs::LinePoint> line;
    const fs::RunReport r = fs::simulate_on(level, seed, opts, pictures ? &line : nullptr);
    if (r.finished) {
      finished++;
      total_time += r.time;
    } else {
      lab.fail(k + ": the bot did not finish (" + std::to_string(r.gates) + " of " +
                   std::to_string(r.gate_count) + " gates in " + fixed(r.time, 1) + " s)",
               "Look at previews/sim/" + k +
                   ".png for where it stopped, then run the ride lab "
                   "for the skier and read games/fall_line/sim/bot.cpp (skills: sim, skier).");
    }
    lab.metric(k + ".time", r.time, "s");
    lab.metric(k + ".gates", r.gates, "count");
    lab.metric(k + ".missed", r.missed, "count");
    lab.metric(k + ".resets", r.resets, "count");
    lab.metric(k + ".wipeouts", r.wipeouts, "count");
    lab.metric(k + ".top-speed", r.top_speed * 3.6, "km/h");
    lab.metric(k + ".mean-speed", r.mean_speed * 3.6, "km/h");
    lab.metric(k + ".landing-g", r.max_landing_g, "g");
    lab.metric(k + ".tape-steps", r.tape.steps, "steps");
    table.rows.push_back({fixed(seed, 0), r.finished ? "yes" : "NO", fixed(r.time, 2),
                          std::to_string(r.gates) + "/" + std::to_string(r.gate_count),
                          std::to_string(r.missed), std::to_string(r.resets),
                          std::to_string(r.wipeouts), std::to_string(r.flights),
                          fixed(r.top_speed * 3.6, 1), fixed(r.mean_speed * 3.6, 1),
                          fixed(r.max_landing_g, 1), hex(r.digest)});
    if (!pictures) continue;
    constexpr int kPx = 768;
    ge::lab::Picture pic = ge::mountain::labs::draw_map(*level, kPx);
    const double px = kPx / level->size;
    for (size_t i = 1; i < line.size(); i++) {
      const auto& a = line[i - 1];
      const auto& b = line[i];
      const Rgba c = b.down ? Rgba{230, 0, 200} : b.on_snow ? Rgba{10, 20, 90} : Rgba{255, 160, 0};
      ge::lab::draw_line(pic, a.x * px, a.z * px, b.x * px, b.z * px, c, 2);
    }
    lab.shot(
        {k,
         "SEED " + fixed(seed, 0) + " SKIED BY THE BOT",
         "The map from above (+x right, +z down; the face shaded by height, the piste red, "
         "gates numbered, kickers orange, trees dark green) with the bot's line over it: "
         "dark blue on the snow, orange in the air, magenta down after a wipeout. " +
             std::string(r.finished ? "Finished" : "DID NOT FINISH") + " in " + fixed(r.time, 1) +
             " s, " + std::to_string(r.gates) + " of " + std::to_string(r.gate_count) +
             " gates, top " + fixed(r.top_speed * 3.6, 0) + " km/h.",
         "the line follows the piste through every gate from START to FINISH; no magenta; "
         "orange only over the kickers",
         {{"seed", fixed(seed, 0)},
          {"version", lab.arg("version")},
          {"skis", "reference"},
          {"pilot", "bot"}}},
        pic);
  }
  lab.metric("finished", finished, "count");
  lab.metric("mean-time", finished ? total_time / finished : 0, "s");
  lab.table(std::move(table));
}

const ge::lab::LabSpec kSpec{
    "sim",
    ge::lab::LabKind::drive,
    "game:fall_line",
    "games/fall_line/sim (create_game, step, course, bot, simulate)",
    "sim",
    "Does the bot ski each seed's map from the start gate to the finish, how fast, and with what "
    "on the way (gates missed, resets, wipeouts, flights)?",
    {
        {"seeds", "1,2,3,4,5,6,7,8,38", "seeds, comma-separated"},
        {"limit", "600", "the run's time limit, s"},
        {"version", "1", "the generator's version"},
        {"no-pictures", "", "skip the map pictures", true},
    },
    "--seeds=38",
    run,
};
GE_LAB(kSpec);

}  // namespace
