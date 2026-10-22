// LAB `trace`: the player's skier step by step down one seed, the bot at the controls, written as
// a SERIES file (engine/lab/series.hpp: one row a step, the speed, the edge, the lean, the tuck,
// the sideways load, the sink, the air and the gate owed) and drawn as charts against the run's
// clock with every gate marked, every flight shaded and every spike ringed. It is the
// before-and-after for any change to the skier, the snow or the bot: run it with --tag=before,
// change the code, run it again, and `ge graph` the two trace.tsv files on one chart. Run it
// with `--log=course=debug,skier=debug` and log.txt holds the same run's events by step.
//
// ge:summary Lab: the player's skier step by step on one seed, a series file and its charts
// ge:keywords lab trace series speed edge lean tuck load g-lat sink air gate charts spikes
// ge:keywords debug timeline per-step
// ge:skill sim debug labs
// ge:test tests/fall_line_sim_test.cpp
#include <memory>
#include <string>
#include <vector>

#include "core/num_text.hpp"
#include "fall_line/sim/bot.hpp"
#include "fall_line/sim/game.hpp"
#include "fall_line/sim/simulate.hpp"
#include "lab/chart.hpp"
#include "lab/lab.hpp"
#include "lab/series.hpp"
#include "mountain/mapgen/generate.hpp"

namespace {

namespace fs = fall_line::sim;
namespace gl = ge::lab;

constexpr double kDeg = 57.29577951308232;

std::string fixed(double v, int d) { return ge::core::num::to_fixed(v, d); }

void run(gl::Lab& lab) {
  const double seed = lab.arg_num("seed");
  const double limit = lab.arg_num("limit");
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
  fs::Driver d(level, 0);
  gl::Series s =
      gl::make_series({"t_s", "speed", "edge", "lean", "tuck", "g_lat", "sink", "air", "gate"});
  s.set("title", "the player's skier on seed " + fixed(seed, 0) + ", the bot at the controls");
  s.set("x", "t_s");
  s.set("unit.t_s", "s");
  s.set("unit.speed", "km/h");
  s.set("unit.edge", "deg");
  s.set("unit.lean", "deg");
  s.set("unit.g_lat", "g");
  s.set("unit.sink", "m");
  std::vector<gl::Mark> gates;
  std::vector<gl::Band> flights;
  double flight_from = -1;
  const auto steps = static_cast<int64_t>(limit * fs::kHz);
  while (d.state().steps < steps && !d.over()) {
    d.step(fs::bot_controls(d.state(), 0, d.plan()));
    const fs::GameState& st = d.state();
    const fs::Racer& r = st.racers.front();
    const ge::skier::SkierState& sk = r.skier;
    gl::add_row(s, {st.t, sk.speed * 3.6, sk.edge * kDeg, sk.lean * kDeg, sk.tuck, sk.g_lat,
                    sk.sink, sk.on_snow ? 0.0 : 1.0, static_cast<double>(r.progress.next)});
    for (const fs::GameEvent& e : st.events) {
      if (e.racer != 0) continue;
      if (e.kind == fs::EventKind::gate) gates.push_back({st.t, "G" + std::to_string(e.gate)});
      if (e.kind == fs::EventKind::missed)
        gates.push_back({st.t, "MISS " + std::to_string(e.gate)});
      if (e.kind == fs::EventKind::finish) gates.push_back({st.t, "FINISH"});
    }
    if (!sk.on_snow && flight_from < 0) flight_from = st.t;
    if (sk.on_snow && flight_from >= 0) {
      flights.push_back({flight_from, st.t, "AIR"});
      flight_from = -1;
    }
  }
  const fs::RunReport rep = d.report(seed);
  // Whether the bot finishes is the sim lab's question; a trace of a stretch is still a trace.
  if (!rep.finished) {
    lab.note(k + ": traced the first " + fixed(rep.time, 1) + " s (" + std::to_string(rep.gates) +
             " of " + std::to_string(rep.gate_count) + " gates); --limit=600 traces the whole run");
  }
  const std::string text = gl::write_series(s);
  lab.attach("trace.tsv", std::vector<uint8_t>(text.begin(), text.end()));
  lab.metric(k + ".steps", static_cast<double>(s.rows()), "steps");
  lab.metric(k + ".time", rep.time, "s");
  lab.metric(k + ".flights", static_cast<double>(flights.size()), "count");

  const std::vector<std::pair<std::string, std::string>> inputs{
      {"seed", fixed(seed, 0)}, {"version", "1"}, {"pilot", "bot"}, {"skis", "reference"}};
  const auto& t = s.find("t_s")->num;
  const std::string sub = "seed " + fixed(seed, 0) + ", the bot, " + fixed(rep.time, 1) + " s, " +
                          std::to_string(rep.gates) + "/" + std::to_string(rep.gate_count) +
                          " gates";
  const auto chart = [&](const std::string& id, const std::string& title,
                         const std::vector<std::string>& columns, const std::string& y_label,
                         const std::string& caption, const std::string& look_for) {
    gl::LineChart lc;
    lc.frame = {title, sub, "t (s)", y_label};
    for (const std::string& c : columns) lc.traces.push_back({c, t, s.find(c)->num});
    lc.marks = gates;
    lc.bands = flights;
    const gl::Chart ch = gl::line_chart(lc);
    const std::string svg = gl::to_svg(ch.plot);
    lab.attach(id + ".svg", std::vector<uint8_t>(svg.begin(), svg.end()));
    lab.metric(k + "." + id + ".spikes", static_cast<double>(ch.findings.size()), "count");
    lab.note(gl::chart_text(ch, 8));
    lab.shot({id, title,
              caption + " Grey lines are gates (G<n>), grey bands flights, red rings "
                        "spikes (a step out of line with its neighbours).",
              look_for, inputs},
             gl::to_picture(ch.plot));
  };
  chart("speed", "SPEED DOWN SEED " + fixed(seed, 0), {"speed"}, "speed (km/h)",
        "The skier's speed, vertical included, every step of the run.",
        "speed builds off the start and dips into each gate's turn; no ring outside a landing");
  chart("load", "SIDEWAYS LOAD DOWN SEED " + fixed(seed, 0), {"g_lat"}, "g_lat (g)",
        "The sideways load the snow put on the skier, + to his right: each turn is a swing to one "
        "side.",
        "a swing a turn, alternating sides; rings only at landings or a caught edge");
  chart("edge", "EDGE AND LEAN DOWN SEED " + fixed(seed, 0), {"edge", "lean"}, "angle (deg)",
        "The skis' edge and the body's lean into the turn, + right, in degrees.",
        "lean follows edge through every turn; no one-step jumps (a ring is a snap)");
}

const gl::LabSpec kSpec{
    "trace",
    gl::LabKind::drive,
    "game:fall_line",
    "games/fall_line/sim (the player's skier through step) and plugins/skier",
    "debug",
    "What did the player's skier do, step by step, down one seed, and where did it jump?",
    {
        {"seed", "38", "the map's seed"},
        {"limit", "600", "the run's time limit, s"},
    },
    "--seed=38 --limit=40",
    run,
};
GE_LAB(kSpec);

}  // namespace
