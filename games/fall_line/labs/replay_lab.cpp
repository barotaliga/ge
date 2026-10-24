// LAB `replay`: A RACE WATCHED AGAIN FROM ANY MOMENT (engine/replay, docs/replays.md § Keyframes
// and the seek). The pilot races seed 38 with a field, its input log is opened by the replay's
// player and played to the end (taking the keyframes as it goes), and then sought: to every
// keyframe kept, to a step just past and just before each, and to a spread of steps between, back
// and forth. Every seek's digest is checked against the run played straight to that step (each
// step's digest is kept on a straight pass first), so `mismatches` must be 0.
//
// What it costs is the second half: how many keyframes the policy keeps and where, the bytes one
// clone allocates (the counting allocator, engine/prof/memory.hpp), a clone's time, and the
// worst seek in steps (deterministic) and in milliseconds (measured). The timeline picture shows
// the run, the keyframes kept and the seeks, each seek's arc from the keyframe it started at.
//
//   make lab LAB=replay ARGS="--seed=38 --rivals=3 --hands=human"
//
// ge:summary Lab: Fall Line's replay sought anywhere lands on the straight run; keyframe costs
// ge:keywords lab replay seek keyframe clone keep cost bytes timeline digest mismatches budget
// ge:skill replay sim labs
// ge:test tests/fall_line_play_test.cpp
#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "core/num_text.hpp"
#include "fall_line/sim/play.hpp"
#include "lab/lab.hpp"
#include "lab/raster.hpp"
#include "play/session.hpp"
#include "prof/memory.hpp"
#include "replay/player.hpp"

namespace {

namespace fs = fall_line::sim;
namespace gp = ge::play;
namespace gr = ge::replay;
namespace num = ge::core::num;
using Clock = std::chrono::steady_clock;

double ms_since(Clock::time_point t0) {
  return std::chrono::duration<double, std::milli>(Clock::now() - t0).count();
}

// The steps sought: every keyframe kept, a step past and before each, then nine spread over the
// run from its end back to its start, interleaved so the seeks go back and forth.
std::vector<int64_t> marks_of(const std::vector<int64_t>& kept, int64_t length) {
  std::vector<int64_t> out;
  for (int64_t k : kept) {
    out.push_back(k);
    if (k + 1 <= length) out.push_back(k + 1);
    if (k > 0) out.push_back(k - 1);
  }
  for (int i = 0; i <= 8; i++) out.push_back(length - length * ((i * 5) % 9) / 8);
  return out;
}

struct Sought {
  int64_t mark, from, steps;
  double ms;
  bool same;
};

// The run as a bar, the keyframes as ticks, each seek an arc from its keyframe to its mark.
ge::lab::Picture timeline(const std::vector<int64_t>& kept, const std::vector<Sought>& seeks,
                          int64_t length, double hz) {
  constexpr int kW = 1200, kH = 220, kX0 = 40, kX1 = kW - 40, kBar = 150;
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
  for (int64_t k : kept) {
    const double x = x_of(k);
    ge::lab::fill_rect(p, x - 1, kBar - 10, 3, 28, {30, 90, 200, 255});
  }
  for (const Sought& s : seeks) {
    const double a = x_of(s.from), b = x_of(s.mark);
    const double lift = 12 + (b - a) * 0.5;
    const ge::lab::Rgba c =
        s.same ? ge::lab::Rgba{40, 160, 60, 255} : ge::lab::Rgba{220, 30, 30, 255};
    ge::lab::draw_line(p, a, kBar - 12, (a + b) / 2, kBar - 12 - lift, c, 1);
    ge::lab::draw_line(p, (a + b) / 2, kBar - 12 - lift, b, kBar - 12, c, 1);
    ge::lab::fill_disc(p, b, kBar - 12, 2.5, c);
  }
  ge::lab::draw_text(p, kX0, 12,
                     "SECONDS OF THE RUN, BLUE TICKS THE KEYFRAMES KEPT, ARCS THE SEEKS",
                     {40, 40, 40, 255});
  return p;
}

void run(ge::lab::Lab& lab) {
  ge::prof::count_allocations();
  const double seed = lab.arg_num("seed");
  gp::Setup setup;
  std::string error;
  if (!gp::make_setup(fs::kGame, seed, {{"rivals", lab.arg("rivals")}, {"hands", lab.arg("hands")}},
                      setup, error)) {
    lab.fail(error, "Pass --rivals as 0..7 and --hands as none or human (skill: sim).");
    return;
  }
  // The pilot races until the end, or for --seconds (0: the whole race), the session writing the
  // input log as it goes.
  gp::Session session(fs::kGame, setup);
  if (!session.ok()) {
    lab.fail(session.error(), "Pick another --seed (skill: mountain).");
    return;
  }
  const auto most = static_cast<int64_t>(lab.arg_num("seconds") * fs::kGame.hz);
  std::vector<double> input(fs::kGame.axes.size(), 0);
  while (!session.done() && (most <= 0 || session.steps() < most)) {
    session.run().pilot(input);
    session.step(input);
  }
  const gp::InputLog log = session.inputs();
  const int64_t length = log.steps();

  // The straight pass: every step's digest.
  std::vector<uint64_t> digests;
  digests.reserve(static_cast<size_t>(length) + 1);
  {
    auto straight = fs::kGame.open(setup, error);
    digests.push_back(straight->digest());
    for (int64_t i = 0; i < length; i++) {
      straight->step(log.at(i));
      digests.push_back(straight->digest());
    }
  }

  gr::Player player(fs::kGame, log);
  if (!player.ok()) {
    lab.fail(player.error(), "The pilot's log should open; see tests/replay_player_test.cpp.");
    return;
  }
  const auto budget = static_cast<int64_t>(lab.arg_num("budget"));
  const Clock::time_point t_play = Clock::now();
  player.seek(length);
  while (!player.advance(budget)) {
  }
  const double play_ms = ms_since(t_play);
  const std::vector<int64_t> kept = player.keeper().kept_steps();

  // One clone's bytes and time, at the end of the run (where the most is held).
  const int64_t bytes0 = ge::prof::allocations().bytes;
  const Clock::time_point t_clone = Clock::now();
  const auto copy = player.run().clone();
  const double clone_ms = ms_since(t_clone);
  const int64_t clone_bytes = ge::prof::allocations().bytes - bytes0;

  std::vector<Sought> seeks;
  int64_t mismatches = 0, worst_steps = 0, total_steps = 0;
  double worst_ms = 0;
  for (int64_t mark : marks_of(kept, length)) {
    const int64_t before = player.stats().steps;
    const Clock::time_point t0 = Clock::now();
    player.seek(mark);
    while (!player.advance(budget)) {
    }
    const double ms = ms_since(t0);
    const int64_t steps = player.stats().steps - before;
    const int64_t from = mark - steps;  // where the seek started: a keyframe, or the run
    const bool same = player.run().digest() == digests[static_cast<size_t>(mark)];
    mismatches += same ? 0 : 1;
    worst_steps = steps > worst_steps ? steps : worst_steps;
    total_steps += steps;
    worst_ms = ms > worst_ms ? ms : worst_ms;
    seeks.push_back({mark, from, steps, ms, same});
  }

  lab.metric("steps", static_cast<double>(length), "steps");
  lab.metric("seeks", static_cast<double>(seeks.size()), "count");
  lab.metric("mismatches", static_cast<double>(mismatches), "count");
  lab.metric("keyframes", static_cast<double>(kept.size()), "count");
  lab.metric("worst-seek-steps", static_cast<double>(worst_steps), "steps");
  lab.metric(
      "mean-seek-steps",
      seeks.empty() ? 0 : static_cast<double>(total_steps) / static_cast<double>(seeks.size()),
      "steps");
  lab.metric("restores", static_cast<double>(player.stats().restores), "count");
  lab.metric("reopens", static_cast<double>(player.stats().reopens), "count");
  lab.metric("clone-bytes", static_cast<double>(clone_bytes), "B", ge::lab::Det::measured);
  lab.metric("keyframes-bytes", static_cast<double>(clone_bytes) * static_cast<double>(kept.size()),
             "B", ge::lab::Det::measured);
  lab.metric("clone-ms", clone_ms, "ms", ge::lab::Det::measured);
  lab.metric("play-ms", play_ms, "ms", ge::lab::Det::measured);
  lab.metric("step-us", play_ms * 1000 / static_cast<double>(length ? length : 1), "us",
             ge::lab::Det::measured);
  lab.metric("worst-seek-ms", worst_ms, "ms", ge::lab::Det::measured);

  ge::lab::LabTable steps_kept{"The keyframes kept at the end (step, s)", {"step", "s"}, {}};
  for (int64_t k : kept) {
    steps_kept.rows.push_back(
        {std::to_string(k), num::to_fixed(static_cast<double>(k) / fs::kGame.hz, 2)});
  }
  lab.table(std::move(steps_kept));
  ge::lab::LabTable sought{"Every seek: from the keyframe or the run, the steps taken, the digest",
                           {"mark", "from", "steps", "same"},
                           {}};
  for (const Sought& s : seeks) {
    sought.rows.push_back({std::to_string(s.mark), std::to_string(s.from), std::to_string(s.steps),
                           s.same ? "yes" : "NO"});
  }
  lab.table(std::move(sought));
  ge::lab::LabTable times{"Every seek's time", {"mark", "ms"}, {}};
  for (const Sought& s : seeks)
    times.rows.push_back({std::to_string(s.mark), num::to_fixed(s.ms, 2)});
  lab.table(std::move(times), ge::lab::Det::measured);

  const std::string rivals = lab.arg("rivals");
  lab.shot(
      {"timeline",
       "SEED " + num::to_string(seed) + " WATCHED AGAIN: KEYFRAMES AND SEEKS",
       (lab.arg_num("seconds") > 0 ? "The race's first " : "The race's ") +
           num::to_fixed(static_cast<double>(length) / fs::kGame.hz, 1) +
           " s as a bar (seconds under it), the " + std::to_string(kept.size()) +
           " keyframes the keeper holds at the end in blue (dense in the last seconds, thinned "
           "with age), and each of the " +
           std::to_string(seeks.size()) +
           " seeks as an arc from the keyframe (or the run) it started from to the step it "
           "landed on: green when its digest is the straight run's, red when not.",
       "Every arc green; the blue ticks every 2 s in the last 20 s, then sparser; no arc "
       "longer than the gap between two ticks.",
       {{"seed", num::to_string(seed)}, {"rivals", rivals}, {"hands", lab.arg("hands")}}},
      timeline(kept, seeks, length, fs::kGame.hz));
}

const ge::lab::LabSpec kSpec{
    "replay",
    ge::lab::LabKind::sim,
    "game:fall_line",
    "engine/replay (keep, player), games/fall_line/sim/play.cpp (the run's clone)",
    "replay",
    "Does a replay sought to any step land on the run played straight there, and what do the "
    "keyframes cost?",
    {{"seed", "38", "the race map's seed"},
     {"rivals", "3", "bots on the grid beside the pilot (0..7)"},
     {"hands", "human", "every bot's hands: none or human"},
     {"budget", "600", "steps a seek may take a frame"},
     {"seconds", "0", "the race cut to its first this many seconds (0: all of it)"}},
    "--rivals=1 --seconds=60",
    run,
};
GE_LAB(kSpec);

}  // namespace
