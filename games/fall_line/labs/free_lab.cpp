// LAB `free`: the bot on the free ride, down every piste of each seed's ski area from its head to
// a foot, through the real `step`: THE RUNS table (the run, its colour, the chain of runs it flows
// down, its length, whether the bot reached the foot, the time, wipeouts, resets, the runs the
// free ride found it on, the metres skied, top and mean speed, the digest) and, per seed, the
// ski area from above with every run in its colour and the bot's lines over them. A run the bot
// does not ski to its foot is a failure (exit 1), so a regression in the free ride, the bot or
// the skier on the ski area's runs shows here first.
//
// ge:summary Lab: the bot on the free ride down every piste of a seed, the runs table, its lines
// ge:keywords lab free free-ride simulate_free bot runs foot arrived chain distance odometer
// ge:skill sim labs
// ge:test tests/fall_line_free_test.cpp
#include <memory>
#include <string>
#include <vector>

#include "core/num_text.hpp"
#include "fall_line/sim/free.hpp"
#include "fall_line/sim/simulate.hpp"
#include "lab/lab.hpp"
#include "lab/raster.hpp"
#include "mountain/labs/level_map.hpp"
#include "mountain/mapgen/generate.hpp"

namespace {

namespace fs = fall_line::sim;
namespace mg = ge::mountain::mapgen;
using ge::lab::Rgba;

std::string fixed(double v, int d) { return ge::core::num::to_fixed(v, d); }

std::string hex(uint64_t u) {
  static const char* kDigits = "0123456789abcdef";
  std::string s(16, '0');
  for (int i = 15; i >= 0; i--, u >>= 4) s[static_cast<size_t>(i)] = kDigits[u & 15];
  return s;
}

const char* grade_word(mg::PisteGrade g) {
  switch (g) {
    case mg::PisteGrade::green:
      return "green";
    case mg::PisteGrade::blue:
      return "blue";
    case mg::PisteGrade::red:
      return "red";
    case mg::PisteGrade::black:
      return "black";
  }
  return "?";
}

Rgba grade_colour(mg::PisteGrade g) {
  switch (g) {
    case mg::PisteGrade::green:
      return {40, 160, 60};
    case mg::PisteGrade::blue:
      return {40, 90, 220};
    case mg::PisteGrade::red:
      return {220, 40, 40};
    case mg::PisteGrade::black:
      return {20, 20, 20};
  }
  return {128, 128, 128};
}

// The run ids the chain from `run` flows down, joined by '>'.
std::string chain_of(const mg::Level& level, const std::vector<mg::Run>& runs, int32_t run) {
  std::string s;
  std::vector<bool> seen(runs.size(), false);
  for (int32_t k = run; k >= 0 && !seen[static_cast<size_t>(k)];) {
    seen[static_cast<size_t>(k)] = true;
    const mg::Run& r = runs[static_cast<size_t>(k)];
    s += (s.empty() ? "" : ">") + r.id;
    if (!r.into) break;
    k = fs::run_index(level, r.into->run);
  }
  return s;
}

void run(ge::lab::Lab& lab) {
  const std::vector<double> seeds = lab.arg_nums("seeds");
  const bool pictures = !lab.arg_bool("no-pictures");
  const auto most = static_cast<size_t>(lab.arg_num("most"));
  const auto version = static_cast<int>(lab.arg_num("version"));
  fs::SimOptions opts;
  opts.max_seconds = lab.arg_num("limit");
  ge::lab::LabTable table{
      "runs",
      {"seed", "run", "colour", "chain", "length m", "foot", "time s", "wipeouts", "resets",
       "runs on", "skied m", "top km/h", "mean km/h", "digest"},
      {}};
  int arrived = 0;
  int ridden = 0;
  for (const double seed : seeds) {
    const std::string k = "seed-" + fixed(seed, 0);
    std::shared_ptr<const mg::Level> level;
    try {
      level = std::make_shared<const mg::Level>(
          fs::free_map(seed, version > 0 ? std::optional<int>(version) : std::nullopt));
    } catch (const mg::GenerateError& e) {
      lab.fail("seed " + fixed(seed, 0) + " built no map: " + e.what(),
               "Run `make lab LAB=analyze ARGS=--seeds=" + fixed(seed, 0) +
                   " --refusals` (skill: mountain).");
      continue;
    }
    if (!level->resort) {
      lab.fail(k + ": the map has no ski area (generator v" + std::to_string(level->version) + ")",
               "Pass --version=8 or later, or leave it at 0 for the current rules (skill: sim).");
      continue;
    }
    const auto& runs = level->resort->runs;
    std::vector<std::vector<fs::LinePoint>> drawn;
    size_t taken = 0;
    for (size_t i = 0; i < runs.size() && (most == 0 || taken < most); i++) {
      const mg::Run& r = runs[i];
      if (r.kind != mg::RunKind::piste) continue;
      taken++;
      ridden++;
      std::vector<std::vector<fs::LinePoint>> lines;
      const fs::RunReport rep = fs::simulate_free(level, seed, static_cast<int32_t>(i), opts,
                                                  pictures ? &lines : nullptr);
      const mg::Track chain = fs::run_chain(*level, static_cast<int32_t>(i));
      const std::string key = k + "." + r.id;
      if (rep.finished) {
        arrived++;
      } else {
        lab.fail(key + ": the bot did not reach a foot (" + fixed(rep.distance, 0) + " m of " +
                     fixed(chain.length, 0) + " in " + fixed(rep.time, 1) + " s)",
                 "Look at previews/free/" + k +
                     ".png for where his line stops, then read games/fall_line/sim/free.cpp and "
                     "bot.cpp (skill: sim).");
      }
      lab.metric(key + ".time", rep.time, "s");
      lab.metric(key + ".wipeouts", rep.wipeouts, "count");
      lab.metric(key + ".resets", rep.resets, "count");
      lab.metric(key + ".skied", rep.distance, "m");
      table.rows.push_back(
          {fixed(seed, 0), r.id, grade_word(r.grade), chain_of(*level, runs, int32_t(i)),
           fixed(chain.length, 0), rep.finished ? "yes" : "NO", fixed(rep.time, 2),
           std::to_string(rep.wipeouts), std::to_string(rep.resets), std::to_string(rep.runs),
           fixed(rep.distance, 0), fixed(rep.top_speed * 3.6, 1), fixed(rep.mean_speed * 3.6, 1),
           hex(rep.digest)});
      if (!lines.empty()) drawn.push_back(std::move(lines[0]));
    }
    if (!pictures) continue;
    constexpr int kPx = 1024;
    ge::lab::Picture pic = ge::mountain::labs::draw_map(*level, kPx);
    const double px = kPx / level->size;
    for (const mg::Run& r : runs) {
      for (size_t i = 1; i < r.points.size(); i++) {
        const auto& a = r.points[i - 1];
        const auto& b = r.points[i];
        ge::lab::draw_line(pic, a.x * px, a.z * px, b.x * px, b.z * px, grade_colour(r.grade), 1);
      }
    }
    for (const auto& line : drawn) {
      for (size_t i = 1; i < line.size(); i++) {
        const auto& a = line[i - 1];
        const auto& b = line[i];
        const Rgba c = b.down      ? Rgba{230, 0, 200}
                       : b.on_snow ? Rgba{0, 200, 230}
                                   : Rgba{255, 160, 0};
        ge::lab::draw_line(pic, a.x * px, a.z * px, b.x * px, b.z * px, c, 2);
      }
    }
    lab.shot({k,
              "SEED " + fixed(seed, 0) + "'S PISTES SKIED ON THE FREE RIDE",
              "The ski area from above (+x right, +z down; the face shaded by height, the woods "
              "dark green) with every run's line in its colour, and over them the bot's line "
              "down each piste from its head to a foot: cyan on the snow, orange in the air, "
              "magenta down after a wipeout. " +
                  std::to_string(arrived) + " of " + std::to_string(ridden) +
                  " pistes so far skied to a foot.",
              "every cyan line starts at a run's head and follows the coloured runs down to a "
              "lift's bottom station or the valley; little magenta",
              {{"seed", fixed(seed, 0)},
               {"version", std::to_string(level->version)},
               {"skis", "reference"},
               {"pilot", "bot"}}},
             pic);
  }
  lab.metric("pistes", ridden, "count");
  lab.metric("arrived", arrived, "count");
  lab.table(std::move(table));
}

const ge::lab::LabSpec kSpec{
    "free",
    ge::lab::LabKind::drive,
    "game:fall_line",
    "games/fall_line/sim (create_free_ride, step_free, plan_free, simulate_free)",
    "sim",
    "Does the bot on the free ride ski every piste of each seed's ski area from its head to a "
    "foot, how fast, and with what on the way (wipeouts, resets)?",
    {
        {"seeds", "38", "seeds, comma-separated"},
        {"version", "10", "the generator's version (8 or later; 0 the current rules)"},
        {"most", "0", "the most pistes ridden a seed, in the map's order (0: every one)"},
        {"limit", "600", "a run's time limit, s"},
        {"no-pictures", "", "skip the map pictures", true},
    },
    "--seeds=38 --most=3",
    run,
};
GE_LAB(kSpec);

}  // namespace
