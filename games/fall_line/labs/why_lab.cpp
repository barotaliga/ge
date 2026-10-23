// LAB `why`: why the bots skied one seed the way they did. The pilot and a field of rivals down
// one seed with every bot's decisions recorded (ai/why.hpp, bot.hpp's `mode`, `line` and
// `speed`), the same run stepped untraced beside it (the digests must agree: recording decides
// nothing), and per racer how its speed was settled (the share of steps it braked, tucked or
// glided), how often it changed its mind (`speed` flips a minute) and how many calls were close
// (a margin under `kClose`). Its charts are the player's speed against the plan's cap with every
// change of mind marked, and the speed decision's margin: the before-and-after for a change to
// a bot. The pilot's decisions are attached as a series file (`why-0.tsv`) for `ge graph`; `ge
// why` reads any one step of the same run from an input log.
//
// ge:summary Lab: every bot's decisions down one seed, traced and untraced alike, flips, charts
// ge:keywords lab why bot decisions trace flips close-call margin brake tuck glide timeline
// ge:keywords rivals digest humanness dither
// ge:skill bots sim labs
// ge:test tests/fall_line_why_test.cpp
#include <cmath>
#include <memory>
#include <string>
#include <vector>

#include "ai/why.hpp"
#include "core/num_text.hpp"
#include "fall_line/sim/bot.hpp"
#include "fall_line/sim/game.hpp"
#include "fall_line/sim/simulate.hpp"
#include "lab/chart.hpp"
#include "lab/lab.hpp"
#include "lab/series.hpp"
#include "mountain/mapgen/generate.hpp"
#include "play/why.hpp"

namespace {

namespace fs = fall_line::sim;
namespace gl = ge::lab;
namespace ai = ge::ai;

// A speed call closer than this is a close one, m/s of margin.
constexpr double kClose = 0.25;

std::string fixed(double v, int d) { return ge::core::num::to_fixed(v, d); }

void run(gl::Lab& lab) {
  const double seed = lab.arg_num("seed");
  const double limit = lab.arg_num("limit");
  const auto rivals = static_cast<int32_t>(lab.arg_num("rivals"));
  const std::string k = "seed-" + fixed(seed, 0);
  std::shared_ptr<const ge::mountain::mapgen::Level> level;
  try {
    level = std::make_shared<const ge::mountain::mapgen::Level>(fs::race_map(seed, 1));
  } catch (const ge::mountain::mapgen::GenerateError& e) {
    lab.fail("seed " + fixed(seed, 0) + " built no map: " + e.what(),
             "Run `make lab LAB=analyze ARGS=--seeds=" + fixed(seed, 0) +
                 " --refusals` (skill: mountain).");
    return;
  }
  ai::Why why;
  fs::Driver traced(level, rivals);
  fs::Driver plain(level, rivals);
  traced.set_why(&why);
  const auto steps = static_cast<int64_t>(limit * fs::kHz);
  while (traced.state().steps < steps && !traced.over()) {
    const fs::GameState& st = traced.state();
    traced.step(fs::bot_controls(st, 0, traced.plan(), why.follow(0, st.steps)));
    plain.step(fs::bot_controls(plain.state(), 0, plain.plan()));
  }
  if (traced.digest() != plain.digest()) {
    lab.fail("the traced run's digest " + std::to_string(traced.digest()) +
                 " differs from the "
                 "untraced " +
                 std::to_string(plain.digest()) + " on seed " + fixed(seed, 0) +
                 ": recording a decision changed one",
             "compute the choice before recording it in bot_controls, never branch on `why` "
             "(skill: bots)");
    return;
  }
  const double t_end = traced.state().t;

  gl::LabTable table{"how each racer's speed was settled, seed " + fixed(seed, 0),
                     {"racer", "steps", "brake %", "tuck %", "glide %", "speed flips", "flips/min",
                      "close calls", "resets"},
                     {}};
  const auto racers = static_cast<int32_t>(traced.state().racers.size());
  for (int32_t a = 0; a < racers; a++) {
    int64_t n = 0, brake = 0, tuck = 0, glide = 0, close = 0, resets = 0;
    for (const ai::Decision& d : why.decisions()) {
      if (d.agent != a) continue;
      const std::string name = d.name;
      if (name == "mode" && std::string(d.chosen) == "reset") resets++;
      if (name != "speed") continue;
      n++;
      const std::string c = d.chosen;
      brake += c == "brake";
      tuck += c == "tuck";
      glide += c == "glide";
      close += !std::isnan(d.margin) && d.margin < kClose && c != "glide";
    }
    int64_t flips = 0;
    for (const ge::play::Flip& f : ge::play::why_flips(why, a, 0)) {
      flips += std::string(f.decision) == "speed";
    }
    const double pct = n > 0 ? 100.0 / static_cast<double>(n) : 0;
    const double per_min = t_end > 0 ? static_cast<double>(flips) * 60 / t_end : 0;
    const std::string who = a == 0 ? "player" : "rival-" + std::to_string(a);
    table.rows.push_back({who, std::to_string(n), fixed(static_cast<double>(brake) * pct, 1),
                          fixed(static_cast<double>(tuck) * pct, 1),
                          fixed(static_cast<double>(glide) * pct, 1), std::to_string(flips),
                          fixed(per_min, 1), std::to_string(close), std::to_string(resets)});
    std::string key = k;
    key += ".";
    key += who;
    key += ".";
    lab.metric(key + "brake-share", static_cast<double>(brake) * pct, "%");
    lab.metric(key + "tuck-share", static_cast<double>(tuck) * pct, "%");
    lab.metric(key + "speed-flips-per-min", per_min, "count");
    lab.metric(key + "close-calls", static_cast<double>(close), "count");
  }
  lab.table(table);
  lab.metric(k + ".decisions", static_cast<double>(why.decisions().size()), "count");

  const gl::Series s = ge::play::why_timeline(why, 0, fs::kHz);
  const std::string text = gl::write_series(s);
  lab.attach("why-0.tsv", std::vector<uint8_t>(text.begin(), text.end()));

  std::vector<gl::Mark> flips;
  for (const ge::play::Flip& f : ge::play::why_flips(why, 0, 0)) {
    if (std::string(f.decision) != "speed") continue;
    flips.push_back({static_cast<double>(f.step) / fs::kHz, std::string(f.to)});
  }
  const std::vector<std::pair<std::string, std::string>> inputs{
      {"seed", fixed(seed, 0)}, {"rivals", std::to_string(rivals)}, {"pilot", "bot"}};
  const std::string sub = "seed " + fixed(seed, 0) + ", the player's bot, " + fixed(t_end, 1) +
                          " s, " + std::to_string(flips.size()) + " changes of mind";
  const auto& t = s.find("t_s")->num;
  const auto chart = [&](const std::string& id, const std::string& title,
                         const std::vector<std::pair<std::string, std::string>>& columns,
                         const std::string& y_label, const std::string& caption,
                         const std::string& look_for) {
    gl::LineChart lc;
    lc.frame = {title, sub, "t (s)", y_label};
    for (const auto& [label, column] : columns) {
      lc.traces.push_back({label, t, s.find(column)->num});
    }
    lc.marks = flips;
    const gl::Chart ch = gl::line_chart(lc);
    const std::string svg = gl::to_svg(ch.plot);
    lab.attach(id + ".svg", std::vector<uint8_t>(svg.begin(), svg.end()));
    lab.note(gl::chart_text(ch, 8));
    lab.shot({id, title,
              caption + " Each grey line is a step the speed decision changed, named by what it "
                        "chose then.",
              look_for, inputs},
             gl::to_picture(ch.plot));
  };
  chart("speed-plan", "SPEED AGAINST THE PLAN, SEED " + fixed(seed, 0),
        {{"speed", "speed.speed"}, {"cap", "speed.cap"}}, "m/s",
        "The player's speed and the plan's cap over the stretch ahead, as the bot read them "
        "each step.",
        "speed hugs the cap from below on the straights; brakes where it crosses over; few "
        "flips in a row (a burst of lines is dithering)");
  chart("speed-margin", "HOW CLOSE EACH SPEED CALL WAS, SEED " + fixed(seed, 0),
        {{"margin", "speed.margin"}}, "margin (m/s)",
        "The margin the speed decision chose by: its option's score over the next best not "
        "barred (while it glides, how far the brake and the tuck both are from their "
        "thresholds).",
        "the margin falls to zero where the choice changes and grows away from it; a margin "
        "hugging zero for seconds is a bot on a knife edge");
}

const gl::LabSpec kSpec{
    "why",
    gl::LabKind::drive,
    "game:fall_line",
    "games/fall_line/sim/bot (the decisions), engine/ai/why and engine/play/why",
    "bots",
    "Why did each bot ski the seed the way it did, and how often did it change its mind?",
    {
        {"seed", "38", "the map's seed"},
        {"limit", "600", "the run's time limit, s"},
        {"rivals", "3", "bots on the grid beside the player"},
    },
    "--seed=38 --limit=30",
    run,
};
GE_LAB(kSpec);

}  // namespace
