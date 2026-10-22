// LAB `free`: the bot on the free ride, down every piste of each seed's ski area from its head to
// a foot, through the real `step`: THE RUNS table (the run, its colour, the chain of runs it flows
// down, its length, whether the bot reached the foot, the time, wipeouts, resets, the runs the
// free ride found it on, the metres skied, top and mean speed, the digest) and, per seed, the
// ski area from above with every run in its colour and the bot's lines over them. A run the bot
// does not ski to its foot is a failure (exit 1), so a regression in the free ride, the bot or
// the skier on the ski area's runs shows here first.
//
// THE OUTINGS table (`--outings`, the first so many pistes with a lift at their foot): the bot
// down the piste, up the lift at its foot and down another run off its top (outing.hpp) — the
// lift, the run after it, whether he reached that run's foot, the time, the wait in the load zone
// and the ride up, the runs and metres skied, the digest; his line drawn over the map, white
// where a lift carries him. An outing not ridden through is a failure.
//
// ge:summary Lab: the bot on the free ride down every piste of a seed, the runs table, its lines
// ge:keywords lab free free-ride simulate_free bot runs foot arrived chain distance odometer
// ge:keywords outings simulate_outing lift wait ride
// ge:skill sim labs
// ge:test tests/fall_line_free_test.cpp
#include <memory>
#include <string>
#include <vector>

#include "core/num_text.hpp"
#include "fall_line/sim/free.hpp"
#include "fall_line/sim/outing.hpp"
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
  const auto outings = static_cast<size_t>(lab.arg_num("outings"));
  fs::SimOptions outing_opts;
  outing_opts.max_seconds = lab.arg_num("outing-limit");
  ge::lab::LabTable outing_table{"outings",
                                 {"seed", "run", "lift", "then", "foot", "time s", "wait s",
                                  "ride s", "wipeouts", "resets", "runs on", "skied m", "digest"},
                                 {}};
  int arrived = 0;
  int ridden = 0;
  int outings_done = 0;
  int outings_ridden = 0;
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
    std::vector<std::vector<fs::LinePoint>> outing_lines;
    {
      const auto lifts = ge::mountain::lifts::lift_plans(*level);
      size_t rode = 0;
      for (size_t i = 0; i < runs.size() && rode < outings; i++) {
        if (runs[i].kind != mg::RunKind::piste) continue;
        const fs::Outing o = fs::outing_from(*level, lifts, static_cast<int32_t>(i));
        if (o.lift < 0) continue;
        rode++;
        outings_ridden++;
        std::vector<std::vector<fs::LinePoint>> lines;
        const fs::RunReport rep = fs::simulate_outing(level, seed, static_cast<int32_t>(i),
                                                      outing_opts, pictures ? &lines : nullptr);
        const std::string& lift = level->resort->lifts[static_cast<size_t>(o.lift)].id;
        const std::string& then = runs[static_cast<size_t>(o.next)].id;
        const std::string key = k + ".outing." + runs[i].id;
        if (rep.finished) {
          outings_done++;
        } else {
          std::string what = key + ": the bot's outing down run ";
          what += runs[i].id;
          what += ", up ";
          what += lift;
          what += " and down run ";
          what += then;
          what += " did not reach a foot (";
          what += std::to_string(rep.lifts);
          what += " lifts ridden in ";
          what += fixed(rep.time, 1);
          what += " s)";
          lab.fail(what, "Look at previews/free/" + k +
                             "-outings.png for where his line stops, then read "
                             "games/fall_line/sim/outing.cpp and lift_ride.cpp (skill: sim).");
        }
        lab.metric(key + ".time", rep.time, "s");
        lab.metric(key + ".wait", rep.lift_wait, "s");
        lab.metric(key + ".ride", rep.lift_ride, "s");
        outing_table.rows.push_back(
            {fixed(seed, 0), runs[i].id, lift, then, rep.finished ? "yes" : "NO",
             fixed(rep.time, 2), fixed(rep.lift_wait, 1), fixed(rep.lift_ride, 1),
             std::to_string(rep.wipeouts), std::to_string(rep.resets), std::to_string(rep.runs),
             fixed(rep.distance, 0), hex(rep.digest)});
        if (!lines.empty()) outing_lines.push_back(std::move(lines[0]));
      }
    }
    if (!pictures) continue;
    constexpr int kPx = 1024;
    const double px = kPx / level->size;
    const auto base = [&] {
      ge::lab::Picture pic = ge::mountain::labs::draw_map(*level, kPx);
      for (const mg::Run& r : runs) {
        for (size_t i = 1; i < r.points.size(); i++) {
          const auto& a = r.points[i - 1];
          const auto& b = r.points[i];
          ge::lab::draw_line(pic, a.x * px, a.z * px, b.x * px, b.z * px, grade_colour(r.grade), 1);
        }
      }
      return pic;
    };
    const auto draw = [&](ge::lab::Picture& pic, const std::vector<fs::LinePoint>& line) {
      for (size_t i = 1; i < line.size(); i++) {
        const auto& a = line[i - 1];
        const auto& b = line[i];
        const Rgba c = b.lifted    ? Rgba{255, 255, 255}
                       : b.down    ? Rgba{230, 0, 200}
                       : b.on_snow ? Rgba{0, 200, 230}
                                   : Rgba{255, 160, 0};
        ge::lab::draw_line(pic, a.x * px, a.z * px, b.x * px, b.z * px, c, 2);
      }
    };
    if (!outing_lines.empty()) {
      ge::lab::Picture pic = base();
      for (const auto& line : outing_lines) draw(pic, line);
      lab.shot({k + "-outings",
                "SEED " + fixed(seed, 0) + "'S OUTINGS: A PISTE, THE LIFT AT ITS FOOT, ANOTHER RUN",
                "The ski area from above (+x right, +z down) with every run's line in its colour, "
                "and over them the bot's line on each outing: cyan on the snow down the piste and "
                "across to the lift's load zone, white carried up the lift, cyan again from the "
                "top down the ramp and the next run to a foot; orange in the air, magenta down.",
                "each cyan line ends at a lift's bottom station, a white line runs straight up "
                "the lift, and a cyan line leaves its top down a run to a foot",
                {{"seed", fixed(seed, 0)},
                 {"version", std::to_string(level->version)},
                 {"skis", "reference"},
                 {"pilot", "bot"}}},
               pic);
    }
    ge::lab::Picture pic = base();
    for (const auto& line : drawn) draw(pic, line);
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
  lab.metric("outings", outings_ridden, "count");
  lab.metric("outings-done", outings_done, "count");
  lab.table(std::move(table));
  if (outings > 0) lab.table(std::move(outing_table));
}

const ge::lab::LabSpec kSpec{
    "free",
    ge::lab::LabKind::drive,
    "game:fall_line",
    "games/fall_line/sim (create_free_ride, step_free, plan_free, simulate_free, the lift ride "
    "and the outing: simulate_outing)",
    "sim",
    "Does the bot on the free ride ski every piste of each seed's ski area from its head to a "
    "foot, how fast, and with what on the way (wipeouts, resets)? And does it ride an outing: "
    "down a piste, up the lift at its foot and down another run to a foot?",
    {
        {"seeds", "38", "seeds, comma-separated"},
        {"version", "10", "the generator's version (8 or later; 0 the current rules)"},
        {"most", "0", "the most pistes ridden a seed, in the map's order (0: every one)"},
        {"limit", "600", "a run's time limit, s"},
        {"outings", "1",
         "the most outings ridden a seed, from the pistes with a lift at their foot"},
        {"outing-limit", "1800", "an outing's time limit, s"},
        {"no-pictures", "", "skip the map pictures", true},
    },
    "--seeds=38 --most=3",
    run,
};
GE_LAB(kSpec);

}  // namespace
