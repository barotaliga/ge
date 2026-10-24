// LAB `replay-file`: A RACE KEPT, WATCHED BACK AS IT GOES, AND CLIPPED (engine/replay: file,
// live, index; docs/replays.md § The replay file and the instant replay). The pilot races seed 38
// with a field through a live recorder, which keeps keyframes and files the player's moments as
// marks (a take-off, a landing weighted by its load, a wipeout, a reset, a missed gate, the
// finish). Every `--every` seconds of the race the player watches the last `--back` seconds as an
// instant replay off the live keeper: every step of it is checked against the live run's digest
// at that step, and the live run is then resumed and must go on to the same race.
//
// At the end the race is a replay file: written, read back (it must be the same text), refused by
// the game one version on (the error must name both versions), listed by the index, and clipped
// round its heaviest mark. The clip, loaded into a fresh player, is played from its in to its out
// and every step checked again. `mismatches` must be 0, `round-trip` and `refused` 1.
//
//   make lab LAB=replay-file ARGS="--seed=38 --rivals=3 --hands=human"
//
// ge:summary Lab: Fall Line's race kept as a replay file with marks; instant replays; a clip
// ge:keywords lab replay replay-file file marks clip instant-replay live watch resume version
// ge:keywords round-trip refuse index timeline
// ge:skill replay sim labs
// ge:test tests/replay_live_test.cpp
#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "core/num_text.hpp"
#include "fall_line/sim/play.hpp"
#include "fall_line/sim/state.hpp"
#include "lab/lab.hpp"
#include "lab/raster.hpp"
#include "replay/file.hpp"
#include "replay/index.hpp"
#include "replay/live.hpp"

namespace {

namespace fs = fall_line::sim;
namespace gp = ge::play;
namespace gr = ge::replay;
namespace num = ge::core::num;
using Clock = std::chrono::steady_clock;

double ms_since(Clock::time_point t0) {
  return std::chrono::duration<double, std::milli>(Clock::now() - t0).count();
}

// The player's own moments of the last step, filed as marks: the kind is the event's name, the
// weight the moment's worth to a director (a landing by its load).
void file_marks(gr::Live& live) {
  const fs::GameState* s = fs::state_of(live.run());
  if (!s) return;
  for (const fs::GameEvent& e : s->events) {
    if (e.racer != 0) continue;
    switch (e.kind) {
      case fs::EventKind::takeoff:
        live.mark("takeoff", 1);
        break;
      case fs::EventKind::landing:
        live.mark("landing", e.value);
        break;
      case fs::EventKind::wipeout:
        live.mark("wipeout", 3);
        break;
      case fs::EventKind::reset:
        live.mark("reset", 0.5);
        break;
      case fs::EventKind::missed:
        live.mark("missed", 1);
        break;
      case fs::EventKind::finish:
        live.mark("finish", 2);
        break;
      default:
        break;
    }
  }
}

struct Watched {
  int64_t at, from, steps, mismatches;
  double ms;
};

ge::lab::Rgba colour_of(const std::string& kind) {
  if (kind == "wipeout") return {220, 30, 30, 255};
  if (kind == "takeoff") return {30, 90, 200, 255};
  if (kind == "landing") return {40, 160, 200, 255};
  if (kind == "finish") return {40, 160, 60, 255};
  if (kind == "missed") return {230, 140, 20, 255};
  return {120, 120, 120, 255};
}

// The race as a bar: the marks as ticks by kind (taller the heavier), the instant replays as
// brackets over the seconds they showed, the clip shaded.
ge::lab::Picture timeline(const gr::ReplayFile& f, const std::vector<Watched>& watched, double hz) {
  constexpr int kW = 1200, kH = 240, kX0 = 40, kX1 = kW - 40, kBar = 170;
  ge::lab::Picture p = ge::lab::create_picture(kW, kH, {250, 250, 250, 255});
  const int64_t length = f.log.steps();
  const auto x_of = [&](int64_t s) {
    return kX0 + (kX1 - kX0) * static_cast<double>(s) / static_cast<double>(length ? length : 1);
  };
  ge::lab::fill_rect(p, x_of(f.in()), kBar - 60, x_of(f.out()) - x_of(f.in()), 70,
                     {255, 230, 170, 255});
  ge::lab::fill_rect(p, kX0, kBar, kX1 - kX0, 8, {190, 190, 190, 255});
  for (double t = 0; t * hz <= static_cast<double>(length); t += 10) {
    const double x = x_of(static_cast<int64_t>(t * hz));
    ge::lab::draw_line(p, x, kBar + 10, x, kBar + 16, {90, 90, 90, 255}, 1);
    ge::lab::draw_text(p, x - 6, kBar + 22, num::to_fixed(t, 0), {60, 60, 60, 255});
  }
  for (const gr::Mark& m : f.marks) {
    const double x = x_of(m.step);
    const double h = 10 + 12 * (m.weight > 4 ? 4 : m.weight);
    ge::lab::fill_rect(p, x - 1, kBar - h, 3, h, colour_of(m.kind));
  }
  for (const Watched& w : watched) {
    const double a = x_of(w.from), b = x_of(w.at);
    const ge::lab::Rgba c =
        w.mismatches == 0 ? ge::lab::Rgba{40, 40, 40, 255} : ge::lab::Rgba{220, 30, 30, 255};
    ge::lab::draw_line(p, a, kBar - 80, b, kBar - 80, c, 2);
    ge::lab::draw_line(p, a, kBar - 86, a, kBar - 74, c, 1);
    ge::lab::draw_line(p, b, kBar - 86, b, kBar - 74, c, 1);
  }
  ge::lab::draw_text(p, kX0, 12,
                     "SECONDS OF THE RACE; TICKS THE MARKS (RED WIPEOUT, BLUE TAKEOFF, CYAN "
                     "LANDING, ORANGE MISSED, GREEN FINISH); BRACKETS THE INSTANT REPLAYS; "
                     "SHADED THE CLIP",
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
  const auto every = static_cast<int64_t>(lab.arg_num("every") * hz);
  const double back = lab.arg_num("back");
  const auto budget = static_cast<int64_t>(lab.arg_num("budget"));

  // The live race, its digest at every step kept (the live run IS the straight run), and an
  // instant replay of the last `back` seconds every `every`.
  std::vector<uint64_t> digests{live.run().digest()};
  std::vector<Watched> watched;
  std::vector<double> input(fs::kGame.axes.size(), 0);
  while (!live.done() && (most <= 0 || live.steps() < most)) {
    live.run().pilot(input);
    live.step(input);
    file_marks(live);
    digests.push_back(live.run().digest());
    if (every <= 0 || live.steps() % every != 0) continue;
    const uint64_t before = live.run().digest();
    const Clock::time_point t0 = Clock::now();
    std::unique_ptr<gr::Player> replay = live.watch(back);
    if (!replay) {
      lab.fail(live.error(), "The live log should open; see tests/replay_live_test.cpp.");
      return;
    }
    while (!replay->advance(budget)) {
    }
    const double ms = ms_since(t0);
    Watched w{live.steps(), replay->at(), replay->stats().steps, 0, ms};
    // Watched to the end, every step checked against the live run's.
    for (int64_t s = replay->at(); s <= live.steps(); s++) {
      replay->seek(s);
      while (!replay->advance(budget)) {
      }
      w.mismatches += replay->run().digest() == digests[static_cast<size_t>(s)] ? 0 : 1;
    }
    live.resume(*replay);
    w.mismatches += live.run().digest() == before ? 0 : 1;
    watched.push_back(w);
  }

  // The file: written, read back, refused one version on, listed, clipped round its heaviest mark.
  gr::ReplayFile file = live.file();
  const std::string text = gr::to_text(file);
  const auto back_in = gr::parse_replay(text, error);
  const bool round_trip =
      back_in && gr::to_text(*back_in) == text && gr::plays_on(*back_in, fs::kGame, error);
  gp::GameSpec next = fs::kGame;
  next.version = fs::kGame.version + 1;
  std::string refusal;
  const bool refused =
      !gr::plays_on(file, next, refusal) &&
      refusal.find("version " + std::to_string(fs::kGame.version)) != std::string::npos &&
      refusal.find("version " + std::to_string(next.version)) != std::string::npos;
  const gr::Mark* top = nullptr;
  for (const gr::Mark& m : file.marks) {
    if (!top || m.weight > top->weight) top = &m;
  }
  const int64_t length = file.log.steps();
  const auto pad = static_cast<int64_t>(lab.arg_num("clip") * hz);
  const int64_t centre = top ? top->step : length / 2;
  file.clip_in = centre > pad ? centre - pad : 0;
  file.clip_out = centre + pad < length ? centre + pad : length;
  const std::string clipped = gr::to_text(file);
  const auto loaded = gr::parse_replay(clipped, error);
  int64_t clip_mismatches = 0;
  if (loaded && gr::plays_on(*loaded, fs::kGame, error)) {
    gr::Player clip(fs::kGame, loaded->log);
    clip.seek(loaded->in());
    while (!clip.advance(budget)) {
    }
    for (int64_t s = loaded->in(); s <= loaded->out(); s++) {
      clip.seek(s);
      while (!clip.advance(budget)) {
      }
      clip_mismatches += clip.run().digest() == digests[static_cast<size_t>(s)] ? 0 : 1;
    }
  } else {
    lab.fail("the clipped file did not load: " + error, "See tests/replay_file_test.cpp.");
  }
  const gr::IndexEntry row = gr::entry_of(file, "seed-" + num::to_string(seed) + ".ge-replay");
  lab.attach("race.ge-replay", std::vector<uint8_t>(clipped.begin(), clipped.end()));

  int64_t mismatches = clip_mismatches, worst_steps = 0;
  double worst_ms = 0;
  for (const Watched& w : watched) {
    mismatches += w.mismatches;
    worst_steps = w.steps > worst_steps ? w.steps : worst_steps;
    worst_ms = w.ms > worst_ms ? w.ms : worst_ms;
  }
  lab.metric("steps", static_cast<double>(length), "steps");
  lab.metric("marks", static_cast<double>(file.marks.size()), "count");
  lab.metric("mark-kinds", static_cast<double>(row.kinds.size()), "count");
  lab.metric("instant-replays", static_cast<double>(watched.size()), "count");
  lab.metric("mismatches", static_cast<double>(mismatches), "count");
  lab.metric("clip-steps", static_cast<double>(file.out() - file.in()), "steps");
  lab.metric("round-trip", round_trip ? 1 : 0, "bool");
  lab.metric("refused", refused ? 1 : 0, "bool");
  lab.metric("file-bytes", static_cast<double>(clipped.size()), "B");
  lab.metric(
      "bytes-per-minute",
      length ? static_cast<double>(clipped.size()) * 60 * hz / static_cast<double>(length) : 0,
      "B");
  lab.metric("worst-watch-steps", static_cast<double>(worst_steps), "steps");
  lab.metric("worst-watch-ms", worst_ms, "ms", ge::lab::Det::measured);
  if (mismatches) {
    lab.fail(std::to_string(mismatches) + " replayed steps parted from the live race",
             "A clone or an adopted keeper lost state: run tests/replay_live_test.cpp, then the "
             "`replay` lab's seeks (skill: replay).");
  }
  if (!round_trip)
    lab.fail("the replay file did not read back as written: " + error,
             "See tests/replay_file_test.cpp.");
  if (!refused)
    lab.fail("another version was not refused naming both: " + refusal,
             "See replay/file.cpp's plays_on.");
  lab.note("refusal: " + refusal);

  ge::lab::LabTable marks{
      "The marks filed (step, s, kind, weight)", {"step", "s", "kind", "weight"}, {}};
  for (const gr::Mark& m : file.marks) {
    marks.rows.push_back({std::to_string(m.step),
                          num::to_fixed(static_cast<double>(m.step) / hz, 2), m.kind,
                          num::to_fixed(m.weight, 2)});
  }
  lab.table(std::move(marks));
  ge::lab::LabTable shown{"Every instant replay: at, from, steps to land, steps parted",
                          {"at", "from", "steps", "mismatches"},
                          {}};
  for (const Watched& w : watched) {
    shown.rows.push_back({std::to_string(w.at), std::to_string(w.from), std::to_string(w.steps),
                          std::to_string(w.mismatches)});
  }
  lab.table(std::move(shown));
  ge::lab::LabTable times{"Every instant replay's time to land", {"at", "ms"}, {}};
  for (const Watched& w : watched)
    times.rows.push_back({std::to_string(w.at), num::to_fixed(w.ms, 2)});
  lab.table(std::move(times), ge::lab::Det::measured);

  std::string kinds;
  for (const std::string& k : row.kinds) kinds += (kinds.empty() ? "" : ", ") + k;
  lab.shot({"timeline",
            "SEED " + num::to_string(seed) + " KEPT: MARKS, INSTANT REPLAYS, THE CLIP",
            "The race's " + num::to_fixed(static_cast<double>(length) / hz, 1) +
                " s as a bar (seconds under it), the " + std::to_string(file.marks.size()) +
                " marks the player's moments filed (" + (kinds.empty() ? "none" : kinds) +
                ") as ticks taller the heavier, the " + std::to_string(watched.size()) +
                " instant replays as brackets over the seconds each showed (black when every "
                "step matched the live race, red when not), and the clip round the heaviest "
                "mark shaded.",
            "Every bracket black and " + num::to_fixed(back, 0) +
                " s wide ending where it was watched; the shading centred on a tall tick.",
            {{"seed", num::to_string(seed)},
             {"rivals", lab.arg("rivals")},
             {"hands", lab.arg("hands")}}},
           timeline(file, watched, hz));
}

const ge::lab::LabSpec kSpec{
    "replay-file",
    ge::lab::LabKind::sim,
    "game:fall_line",
    "engine/replay (file, live, index), games/fall_line/sim/play.cpp (the run's clone)",
    "replay",
    "Does a race kept as a replay file round-trip, refuse another version, and do its instant "
    "replays and its clip play the live race again?",
    {{"seed", "38", "the race map's seed"},
     {"rivals", "3", "bots on the grid beside the pilot (0..7)"},
     {"hands", "human", "every bot's hands: none or human"},
     {"seconds", "0", "the race cut to its first this many seconds (0: all of it)"},
     {"every", "30", "an instant replay every this many seconds of the race (0: none)"},
     {"back", "30", "how far back each instant replay starts, s"},
     {"clip", "4", "the clip's reach either side of the heaviest mark, s"},
     {"budget", "600", "steps a seek may take a frame"}},
    "--rivals=1 --seconds=60 --every=20 --back=5",
    run,
};
GE_LAB(kSpec);

}  // namespace
