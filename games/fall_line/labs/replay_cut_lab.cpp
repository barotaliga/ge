// LAB `replay-cut`: A RACE CUT INTO A HIGHLIGHT REEL (engine/replay/director.hpp, Fall Line's
// moments in games/fall_line/sim/moments.hpp; docs/replays.md § The director). The pilot races
// seed 38 with a field through a live recorder; the player's moments are filed as marks (a flight
// back-dated to its take-off), and the director ranks and spaces them into a cut. The cut must
// hold every flight, each shot opening before its lip, no two shots closer than the spacing.
//
// Then the reel is PLAYED: frame by frame at `--fps`, the reel's clock says which shot and step
// each frame shows, and a replay player seeks there in a budget of steps a frame. Every frame
// that lands is checked against the live race's digest at its step (`mismatches` must be 0); the
// frames a cut holds while the seek catches up are counted. A still of the `near` view is taken
// at each shot's mark, captioned with its kind, lens and rate.
//
//   make lab LAB=replay-cut ARGS="--seed=38 --rivals=3 --hands=human"
//
// ge:summary Lab: Fall Line's race cut into a highlight reel by the director, the reel played
// ge:keywords lab replay director cut shot reel highlight flight lip spacing lens rate slow-motion
// ge:keywords held-frames mismatches timeline stills
// ge:skill replay sim labs
// ge:test tests/replay_director_test.cpp
#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "core/num.hpp"
#include "core/num_text.hpp"
#include "fall_line/sim/moments.hpp"
#include "fall_line/sim/play.hpp"
#include "lab/lab.hpp"
#include "lab/raster.hpp"
#include "replay/director.hpp"
#include "replay/live.hpp"
#include "replay/player.hpp"

namespace {

namespace fs = fall_line::sim;
namespace gp = ge::play;
namespace gr = ge::replay;
namespace num = ge::core::num;

ge::lab::Rgba colour_of(const std::string& kind) {
  if (kind == "wipeout") return {220, 30, 30, 255};
  if (kind == "flight") return {30, 90, 200, 255};
  if (kind == "shoulder") return {150, 60, 190, 255};
  if (kind == "finish") return {40, 160, 60, 255};
  if (kind == "missed") return {230, 140, 20, 255};
  return {120, 120, 120, 255};
}

// The race as a bar: every mark a tick by kind (grey when left out), every shot a block over the
// run it shows, paler and taller for slow motion.
ge::lab::Picture timeline(const std::vector<gr::Mark>& marks, const gr::Cut& cut, int64_t length,
                          double hz) {
  constexpr int kW = 1200, kH = 240, kX0 = 40, kX1 = kW - 40, kBar = 170;
  ge::lab::Picture p = ge::lab::create_picture(kW, kH, {250, 250, 250, 255});
  const auto x_of = [&](int64_t s) {
    return kX0 + (kX1 - kX0) * static_cast<double>(s) / static_cast<double>(length ? length : 1);
  };
  ge::lab::fill_rect(p, kX0, kBar, kX1 - kX0, 8, {190, 190, 190, 255});
  for (double t = 0; t * hz <= static_cast<double>(length); t += 10) {
    const double x = x_of(static_cast<int64_t>(t * hz));
    ge::lab::draw_line(p, x, kBar + 10, x, kBar + 16, {90, 90, 90, 255}, 1);
    ge::lab::draw_text(p, x - 6, kBar + 22, num::to_fixed(t, 0), {60, 60, 60, 255});
  }
  for (const gr::Shot& s : cut.shots) {
    ge::lab::Rgba c = colour_of(s.kind);
    const double h = s.rate < 1 ? 70 : 45;
    if (s.rate < 1)
      c = {static_cast<uint8_t>((c.r + 255) / 2), static_cast<uint8_t>((c.g + 255) / 2),
           static_cast<uint8_t>((c.b + 255) / 2), 255};
    const double a = x_of(s.from), b = x_of(s.to);
    ge::lab::fill_rect(p, a, kBar - h, b - a < 2 ? 2 : b - a, h, c);
  }
  std::vector<bool> in_cut(marks.size(), false);
  for (const gr::Shot& s : cut.shots) in_cut[s.mark] = true;
  for (size_t i : cut.covered) in_cut[i] = true;
  for (size_t i = 0; i < marks.size(); i++) {
    const double x = x_of(marks[i].step);
    ge::lab::fill_rect(p, x - 1, kBar - 90, 3, 14,
                       in_cut[i] ? colour_of(marks[i].kind) : ge::lab::Rgba{160, 160, 160, 255});
  }
  ge::lab::draw_text(p, kX0, 12,
                     "SECONDS OF THE RACE  BLOCKS THE SHOTS (TALL AND PALE: SLOW MOTION)  TICKS "
                     "THE MARKS (GREY: LEFT OUT)  BLUE FLIGHT  RED WIPEOUT  PURPLE SHOULDER  "
                     "ORANGE MISSED  GREEN FINISH",
                     {40, 40, 40, 255});
  return p;
}

void run(ge::lab::Lab& lab) {
  const double seed = lab.arg_num("seed");
  gp::Setup setup;
  std::string error;
  if (!gp::make_setup(fs::kGame, seed, {{"rivals", lab.arg("rivals")}, {"hands", lab.arg("hands")}},
                      setup, error)) {
    lab.fail(error, "Pass --rivals as 0..7 and --hands as none or human (skill: sim).");
    return;
  }
  gr::Live live(fs::kGame, setup);
  if (!live.ok()) {
    lab.fail(live.error(), "Pick another --seed (skill: mountain).");
    return;
  }
  const double hz = fs::kGame.hz;
  const auto most = static_cast<int64_t>(lab.arg_num("seconds") * hz);
  fs::Moments moments;
  std::vector<uint64_t> digests{live.run().digest()};
  std::vector<double> input(fs::kGame.axes.size(), 0);
  while (!live.done() && (most <= 0 || live.steps() < most)) {
    live.run().pilot(input);
    live.step(input);
    if (const fs::GameState* s = fs::state_of(live.run())) moments.file(*s, live);
    digests.push_back(live.run().digest());
  }
  const std::vector<gr::Mark>& marks = live.marks();
  const int64_t length = live.steps();
  const std::string problem = gr::rules_problem(fs::shot_rules());
  if (!problem.empty())
    lab.fail(problem, "Fix the table in games/fall_line/sim/moments.cpp (skill: replay).");
  const gr::DirectorPolicy policy = fs::director_policy();
  const gr::Cut cut = gr::plan_cut(marks, fs::shot_rules(), policy, hz, length);

  // Every flight in the cut, its shot opening before its lip; every two shots spaced.
  int64_t flights = 0, flights_cut = 0, early = 0, spacing_faults = 0;
  for (size_t i = 0; i < marks.size(); i++) {
    if (marks[i].kind != "flight") continue;
    flights++;
    for (const gr::Shot& s : cut.shots) {
      if (marks[i].step >= s.from && marks[i].step <= s.to) {
        flights_cut++;
        early += s.from < marks[i].step ? 1 : 0;
        break;
      }
    }
  }
  const auto spacing = static_cast<int64_t>(num::round(policy.spacing_s * hz));
  for (size_t i = 1; i < cut.shots.size(); i++) {
    if (cut.shots[i].from < cut.shots[i - 1].to + spacing) spacing_faults++;
  }

  // The reel played frame by frame off the live race's own keyframes (the instant replay's
  // player, so a cut seeks from a copy the race already took): the step each frame should show,
  // sought in the budget.
  std::unique_ptr<gr::Player> reel = live.watch(0);
  if (!reel) {
    lab.fail(live.error(), "The live log should open; see tests/replay_live_test.cpp.");
    return;
  }
  gr::Player& player = *reel;
  const double fps = lab.arg_num("fps");
  const auto budget = static_cast<int64_t>(lab.arg_num("budget"));
  const double reel_s = gr::reel_seconds(cut, hz);
  int64_t frames = 0, held = 0, worst_held = 0, mismatches = 0, run_held = 0;
  size_t checked = 0;
  for (int64_t f = 0;; f++) {
    const gr::ReelAt at = gr::reel_at(cut, hz, static_cast<double>(f) / fps);
    player.seek(static_cast<int64_t>(at.step));
    if (player.advance(budget)) {
      run_held = 0;
    } else {
      held++;
      run_held++;
      worst_held = run_held > worst_held ? run_held : worst_held;
    }
    frames++;
    if (player.at() != player.target()) continue;
    // Every landed frame is checked against the live race at its step.
    mismatches += player.run().digest() == digests[static_cast<size_t>(player.at())] ? 0 : 1;
    // A still at the first landed frame at or past each shot's mark.
    while (checked < cut.shots.size() &&
           (checked < at.shot ||
            (checked == at.shot && player.at() >= marks[cut.shots[checked].mark].step))) {
      const gr::Shot& s = cut.shots[checked];
      const int64_t m = player.at();
      std::string caption;
      ge::lab::Picture still;
      if (checked == at.shot && checked < static_cast<size_t>(lab.arg_num("stills")) &&
          player.run().shoot("near", still, caption, error)) {
        lab.shot(
            {"shot-" + std::to_string(checked + 1),
             "SHOT " + std::to_string(checked + 1) + ": " + s.kind + ", " + s.lens + ", " +
                 num::to_string(s.rate) + "X",
             "The reel's frame at the shot's mark (" +
                 num::to_fixed(static_cast<double>(m) / hz, 2) + " s), the `near` view: " + caption,
             "The moment the mark names: the skier in the air for a flight, down for a "
             "wipeout, beside a rival for a shoulder.",
             {{"seed", num::to_string(seed)}, {"step", std::to_string(m)}}},
            still);
      }
      checked++;
    }
    if (at.over) break;
  }

  lab.metric("steps", static_cast<double>(length), "steps");
  lab.metric("marks", static_cast<double>(marks.size()), "count");
  lab.metric("shots", static_cast<double>(cut.shots.size()), "count");
  lab.metric("covered", static_cast<double>(cut.covered.size()), "count");
  lab.metric("dropped", static_cast<double>(cut.dropped.size()), "count");
  lab.metric("unruled", static_cast<double>(cut.unruled.size()), "count");
  lab.metric("flights", static_cast<double>(flights), "count");
  lab.metric("flights-cut", static_cast<double>(flights_cut), "count");
  lab.metric("flights-before-lip", static_cast<double>(early), "count");
  lab.metric("spacing-faults", static_cast<double>(spacing_faults), "count");
  lab.metric("reel-seconds", reel_s, "s");
  lab.metric("frames", static_cast<double>(frames), "count");
  lab.metric("held-frames", static_cast<double>(held), "count");
  lab.metric("worst-held-run", static_cast<double>(worst_held), "frames");
  lab.metric("mismatches", static_cast<double>(mismatches), "count");
  if (flights_cut != flights || early != flights) {
    lab.fail(std::to_string(flights - flights_cut) + " flights left out of the cut, " +
                 std::to_string(flights - early) + " opening at or after the lip",
             "Raise the flight's scale or lead in games/fall_line/sim/moments.cpp, or the reel's "
             "most_s (skill: replay).");
  }
  if (spacing_faults)
    lab.fail("shots closer than the spacing", "See replay/director.cpp's plan_cut.");
  if (mismatches) {
    lab.fail(std::to_string(mismatches) + " shots parted from the live race at their mark",
             "Run the `replay` lab's seeks and tests/replay_player_test.cpp (skill: replay).");
  }

  ge::lab::LabTable shots{"The cut (run order)",
                          {"shot", "from s", "to s", "mark s", "kind", "lens", "rate", "score"},
                          {}};
  for (size_t i = 0; i < cut.shots.size(); i++) {
    const gr::Shot& s = cut.shots[i];
    shots.rows.push_back({std::to_string(i + 1), num::to_fixed(static_cast<double>(s.from) / hz, 2),
                          num::to_fixed(static_cast<double>(s.to) / hz, 2),
                          num::to_fixed(static_cast<double>(marks[s.mark].step) / hz, 2), s.kind,
                          s.lens, num::to_fixed(s.rate, 2), num::to_fixed(s.score, 2)});
  }
  lab.table(std::move(shots));
  ge::lab::LabTable all{
      "Every mark and what the director did with it", {"s", "kind", "weight", "fate"}, {}};
  std::vector<std::string> fate(marks.size(), "cut");
  for (size_t i : cut.covered) fate[i] = "covered";
  for (size_t i : cut.dropped) fate[i] = "dropped";
  for (size_t i : cut.unruled) fate[i] = "no rule";
  for (size_t i = 0; i < marks.size(); i++) {
    all.rows.push_back({num::to_fixed(static_cast<double>(marks[i].step) / hz, 2), marks[i].kind,
                        num::to_fixed(marks[i].weight, 2), fate[i]});
  }
  lab.table(std::move(all));

  lab.shot({"timeline",
            "SEED " + num::to_string(seed) + " CUT: " + std::to_string(cut.shots.size()) +
                " SHOTS, " + num::to_fixed(reel_s, 1) + " S OF REEL",
            "The race's " + num::to_fixed(static_cast<double>(length) / hz, 1) +
                " s as a bar, the " + std::to_string(marks.size()) +
                " marks the player's moments filed as ticks above it (grey when the director left "
                "one out), and the " +
                std::to_string(cut.shots.size()) +
                " shots of the cut as blocks over the run each shows, tall and pale where it plays "
                "in slow motion.",
            "Every blue tick (a flight) under a block that starts left of it; blocks never "
            "touching.",
            {{"seed", num::to_string(seed)},
             {"rivals", lab.arg("rivals")},
             {"hands", lab.arg("hands")}}},
           timeline(marks, cut, length, hz));
}

const ge::lab::LabSpec kSpec{
    "replay-cut",
    ge::lab::LabKind::sim,
    "game:fall_line",
    "engine/replay/director.hpp, games/fall_line/sim/moments.hpp",
    "replay",
    "Which moments did the director cut to, when, at what rate, is each shot spaced from the last, "
    "and does the reel play the race again?",
    {{"seed", "38", "the race map's seed"},
     {"rivals", "3", "bots on the grid beside the pilot (0..7)"},
     {"hands", "human", "every bot's hands: none or human"},
     {"seconds", "0", "the race cut to its first this many seconds (0: all of it)"},
     {"fps", "60", "the reel's frames a second"},
     {"budget", "600", "steps a seek may take a frame"},
     {"stills", "6", "stills of the first this many shots"}},
    "--rivals=1 --seconds=60 --stills=1",
    run,
};
GE_LAB(kSpec);

}  // namespace
