// LAB `humanness`: how human the bots look, with each hand profile (fall_line/sim/hands.hpp).
// The pilot and a field of rivals down each seed, once with every profile, every bot traced; per
// racer and axis the humanness metrics of what it pressed (ai/human.hpp: saturation, jumps,
// reversals, the hold, the lag behind its own intent) and of its speed decision (changes of mind
// a minute), each flagged where it falls outside a person's band. Whether every bot still
// finishes with the hands on is the other half: a bot made human that no longer gets down is no
// use. The chart is the player's steer as pressed against what it wanted, profile by profile.
//
// ge:summary Lab: the humanness metrics of every bot, by hand profile, flagged out of band
// ge:keywords lab humanness human-like hands profile saturation jumps reversals lag bands bot
// ge:keywords rivals out-of-band robotic finish
// ge:skill bots sim labs
// ge:test tests/fall_line_hands_test.cpp
#include <array>
#include <memory>
#include <string>
#include <vector>

#include "ai/human.hpp"
#include "ai/why.hpp"
#include "core/num_text.hpp"
#include "fall_line/sim/bot.hpp"
#include "fall_line/sim/game.hpp"
#include "fall_line/sim/hands.hpp"
#include "fall_line/sim/simulate.hpp"
#include "lab/chart.hpp"
#include "lab/lab.hpp"
#include "lab/series.hpp"
#include "mountain/mapgen/generate.hpp"
#include "mountain/mapgen/query.hpp"

namespace {

namespace fs = fall_line::sim;
namespace gl = ge::lab;
namespace ai = ge::ai;

// A speed call closer than this is a close one, m/s of margin (the `why` lab's).
constexpr double kClose = 0.25;
// The chart's window, s.
constexpr double kChartSeconds = 12;

std::string fixed(double v, int d) { return ge::core::num::to_fixed(v, d); }

struct AxisInfo {
  const char* name;
  double lo, hi, rest;
  ai::Feel feel;  // a person's: the steer a stick, the tuck and the brake triggers
};
constexpr std::array<AxisInfo, 3> kAxes{{
    {"steer", -1, 1, 0, ai::Feel::stick},
    {"tuck", 0, 1, 0, ai::Feel::trigger},
    {"brake", 0, 1, 0, ai::Feel::trigger},
}};

double axis_of(const fs::Controls& c, size_t k) {
  return k == 0 ? c.ski.steer : k == 1 ? c.ski.tuck : c.ski.brake;
}

// One racer's run: what it pressed and meant at every step.
struct Track {
  std::array<std::vector<double>, 3> pressed, wanted;
};

void run(gl::Lab& lab) {
  const std::vector<double> seeds = lab.arg_nums("seeds");
  const double limit = lab.arg_num("limit");
  const auto rivals = static_cast<int32_t>(lab.arg_num("rivals"));
  const std::array<fs::HandsProfile, 2> profiles{fs::HandsProfile::none, fs::HandsProfile::human};

  gl::LabTable table{
      "what each bot pressed, by hand profile",
      {"seed", "hands", "racer", "axis", "saturated", "jumps", "reversals/min", "hold s", "lag s"},
      {}};
  gl::LabTable flags{"out of a person's band", {"seed", "hands", "racer", "what"}, {}};
  gl::LabTable finish{"does every bot still get down",
                      {"seed", "hands", "racer", "finished", "time s", "missed", "resets",
                       "wipeouts", "speed flips/min"},
                      {}};
  std::array<std::vector<double>, 2> chart_pressed, chart_wanted;
  std::vector<double> chart_t;

  for (const double seed : seeds) {
    std::shared_ptr<const ge::mountain::mapgen::Level> level;
    try {
      level = std::make_shared<const ge::mountain::mapgen::Level>(fs::race_map(seed, 1));
    } catch (const ge::mountain::mapgen::GenerateError& e) {
      lab.fail("seed " + fixed(seed, 0) + " built no map: " + e.what(),
               "Run `make lab LAB=analyze ARGS=--seeds=" + fixed(seed, 0) +
                   " --refusals` (skill: mountain).");
      return;
    }
    for (size_t p = 0; p < profiles.size(); p++) {
      const fs::HandsProfile profile = profiles[p];
      const std::string hands = fs::hands_name(profile);
      ai::Why why;
      fs::Driver d(level, rivals);
      d.set_why(&why);
      d.set_hands(profile, seed);
      fs::BotHands mine(profile, fs::hands_seed(seed, 0));
      const auto racers = d.state().racers.size();
      std::vector<Track> tracks(racers);
      std::vector<std::string> resets;
      const auto steps = static_cast<int64_t>(limit * fs::kHz);
      while (d.state().steps < steps && !d.over()) {
        const fs::GameState& st = d.state();
        ai::Why* w = why.follow(0, st.steps);
        d.step(mine.press(fs::bot_controls(st, 0, d.plan(), w, mine.lead()), w));
        for (const fs::GameEvent& e : d.state().events) {
          if (e.kind != fs::EventKind::reset) continue;
          const auto& k = d.state().racers[static_cast<size_t>(e.racer)].skier;
          const auto hit = ge::mountain::mapgen::nearest_track_point(level->track, k.x, k.z);
          resets.push_back(
              (e.racer == 0 ? std::string("player") : "rival-" + std::to_string(e.racer)) +
              " reset at " + fixed(d.state().t, 1) + " s before gate " + std::to_string(e.gate) +
              ", " + fixed(hit.s, 0) + " m down the piste");
        }
        for (size_t i = 0; i < racers; i++) {
          for (size_t k = 0; k < kAxes.size(); k++) {
            tracks[i].pressed[k].push_back(axis_of(d.controls()[i], k));
          }
        }
      }
      // What each racer meant, off its "hands" decisions (one a step, in step order).
      for (const ai::Decision& dec : why.decisions()) {
        if (std::string(dec.name) != "hands") continue;
        Track& t = tracks[static_cast<size_t>(dec.agent)];
        for (size_t k = 0; k < kAxes.size(); k++) {
          t.wanted[k].push_back(why.fact_of(dec, kAxes[k].name));
        }
      }
      for (const std::string& line : resets) {
        std::string note = "seed " + fixed(seed, 0);
        note += ", ";
        note += hands;
        note += ": ";
        note += line;
        lab.note(note);
      }
      const double seconds = d.state().t;
      const fs::RunReport rep = d.report(seed);
      for (size_t i = 0; i < racers; i++) {
        const std::string who = i == 0 ? "player" : "rival-" + std::to_string(i);
        std::string key = "seed-" + fixed(seed, 0);
        key += ".";
        key += hands;
        key += ".";
        key += who;
        key += ".";
        std::vector<ai::OutOfBand> out;
        for (size_t k = 0; k < kAxes.size(); k++) {
          const AxisInfo& a = kAxes[k];
          const ai::AxisHumanness h = ai::axis_humanness(tracks[i].pressed[k], tracks[i].wanted[k],
                                                         a.lo, a.hi, a.rest, fs::kHz);
          table.rows.push_back({fixed(seed, 0), hands, who, a.name, fixed(h.saturated, 3),
                                fixed(h.jumps, 4), fixed(h.reversals_min, 1), fixed(h.hold_s, 3),
                                fixed(h.lag_s, 3)});
          lab.metric(key + a.name + ".saturated", h.saturated, "share");
          lab.metric(key + a.name + ".jumps", h.jumps, "share");
          lab.metric(key + a.name + ".lag", h.lag_s, "s");
          ai::check_axis(a.name, a.feel, h, out);
        }
        const ai::DecisionHumanness speed =
            ai::decision_humanness(why, static_cast<int32_t>(i), "speed", seconds, kClose);
        ai::check_decision("speed", speed, out);
        lab.metric(key + "speed.flips-per-min", speed.flips_min, "count");
        lab.metric(key + "out-of-band", static_cast<double>(out.size()), "count");
        for (const ai::OutOfBand& o : out) {
          flags.rows.push_back({fixed(seed, 0), hands, who, ai::out_of_band_text(o)});
        }
        const fs::FieldRow& row = rep.field[i];
        finish.rows.push_back({fixed(seed, 0), hands, who, row.finished ? "yes" : "NO",
                               fixed(row.time, 2), std::to_string(row.missed),
                               std::to_string(row.resets), std::to_string(row.wipeouts),
                               fixed(speed.flips_min, 1)});
        if (!row.finished && profile != fs::HandsProfile::none && limit >= 600) {
          std::string what = who + " did not finish seed " + fixed(seed, 0);
          what += " with ";
          what += hands;
          what += " hands";
          lab.fail(what, "Run `make lab LAB=why ARGS=--seed=" + fixed(seed, 0) +
                             "` and read where it went wrong; soften the profile in "
                             "games/fall_line/sim/hands.cpp (skill: bots).");
        }
      }
      if (seed == seeds.front()) {
        // The player's every axis, pressed and meant, for `ge graph`.
        gl::Series ser;
        ser.set("title", "the player's controls with " + hands + " hands, seed " + fixed(seed, 0));
        ser.set("x", "t_s");
        ser.set("unit.t_s", "s");
        const size_t rows = tracks[0].pressed[0].size();
        gl::Column tc;
        tc.name = "t_s";
        for (size_t s = 0; s < rows; s++) tc.num.push_back(static_cast<double>(s) / fs::kHz);
        ser.columns.push_back(std::move(tc));
        for (size_t k = 0; k < kAxes.size(); k++) {
          for (const bool meant : {false, true}) {
            gl::Column c;
            c.name = std::string(kAxes[k].name) + (meant ? ".meant" : ".pressed");
            c.num = meant ? tracks[0].wanted[k] : tracks[0].pressed[k];
            c.num.resize(rows);
            ser.columns.push_back(std::move(c));
          }
        }
        const std::string text = gl::write_series(ser);
        lab.attach("player-" + hands + ".tsv", std::vector<uint8_t>(text.begin(), text.end()));
        const auto n = static_cast<size_t>(kChartSeconds * fs::kHz);
        for (size_t s = 0; s < n && s < tracks[0].pressed[0].size(); s++) {
          if (p == 0) chart_t.push_back(static_cast<double>(s) / fs::kHz);
          chart_pressed[p].push_back(tracks[0].pressed[0][s]);
          chart_wanted[p].push_back(tracks[0].wanted[0][s]);
        }
      }
    }
  }
  lab.table(table);
  lab.table(flags);
  lab.table(finish);

  gl::LineChart lc;
  lc.frame = {"THE PLAYER'S STEER, AS MEANT AND AS PRESSED",
              "seed " + fixed(seeds.front(), 0) + ", the first " + fixed(kChartSeconds, 0) + " s",
              "t (s)", "steer"};
  for (size_t p = 0; p < profiles.size(); p++) {
    const std::string hands = fs::hands_name(profiles[p]);
    chart_pressed[p].resize(chart_t.size());
    chart_wanted[p].resize(chart_t.size());
    lc.traces.push_back({hands + " pressed", chart_t, chart_pressed[p]});
    if (profiles[p] != fs::HandsProfile::none) {
      lc.traces.push_back({hands + " meant", chart_t, chart_wanted[p]});
    }
  }
  const gl::Chart ch = gl::line_chart(lc);
  const std::string svg = gl::to_svg(ch.plot);
  lab.attach("steer.svg", std::vector<uint8_t>(svg.begin(), svg.end()));
  lab.note(gl::chart_text(ch, 8));
  lab.shot({"steer",
            "THE PLAYER'S STEER, AS MEANT AND AS PRESSED",
            "The player's steer over the first seconds of the seed: as the bot with no hands "
            "pressed it, and with human hands what it meant and what the hands pressed.",
            "with no hands the steer moves the step the bot decides; with human hands it "
            "follows what was meant about 0.2 s late, rounded, never jumping",
            {{"seed", fixed(seeds.front(), 0)}, {"rivals", std::to_string(rivals)}}},
           gl::to_picture(ch.plot));
}

const gl::LabSpec kSpec{
    "humanness",
    gl::LabKind::drive,
    "game:fall_line",
    "games/fall_line/sim/hands (the profiles), engine/ai/hands and engine/ai/human",
    "bots",
    "How human do the bots look with each hand profile, and do they still get down?",
    {
        {"seeds", "38,7,101", "the maps' seeds"},
        {"limit", "600", "the run's time limit, s"},
        {"rivals", "3", "bots on the grid beside the player"},
    },
    "--seeds=38 --limit=30",
    run,
};
GE_LAB(kSpec);

}  // namespace
