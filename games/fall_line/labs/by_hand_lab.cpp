// LAB `by-hand`: Fall Line played by hand (games/fall_line/controls). The bot skis one seed through
// the play contract; its input log is held on a simulated pad as a session's device events
// (script.hpp), and the session is played back into the hand loop the `fall_line` window runs
// (by_hand.hpp). Every step must read back the bot's own axes, so the hand run finishes on the
// bot's digest; its input log is written and replayed through the play contract (`ge replay`'s
// path) to the digest it records. The lab fails otherwise. Then it charts the pad against the
// axes the run was stepped on, and what the pad's motors were told (felt.hpp), and lists the
// default bindings.
//
// THEN THE THUMBS: the same log played as two thumbs on a phone's screen (thumbs.hpp,
// script.hpp's `touch_session`). A thumb cannot say all a pad can (it brakes or tucks, never
// both; a jump is a tap and a hold, so the tuck lifts before it), so every step must read back
// what the thumbs gave, and the race on them must finish and replay; how far that run strays
// from the bot's is measured, not failed.
//
// It writes four files beside its report: `session.events` (the pad's session, which
// `fall_line --events=<it>` plays in the window), `thumbs.events` (the thumbs' session),
// `run.inputs` (the hand run's log, which `ge replay <it>` plays back) and `bindings.txt` (the
// defaults the player's file starts from).
//
// ge:summary Lab: the bot's run held on a pad and played by hand, its log replayed, charted, felt
// ge:keywords lab by-hand person pad session event-log input-log replay digest rumble felt
// ge:keywords bindings defaults fall-line window events touch thumbs phone touch-session
// ge:skill input labs
// ge:test tests/fall_line_controls_test.cpp
#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "core/num_text.hpp"
#include "fall_line/controls/bindings.hpp"
#include "fall_line/controls/by_hand.hpp"
#include "fall_line/controls/felt.hpp"
#include "fall_line/controls/script.hpp"
#include "fall_line/controls/thumbs.hpp"
#include "fall_line/sim/play.hpp"
#include "fall_line/sim/state.hpp"
#include "input/bindings_text.hpp"
#include "input/event_log.hpp"
#include "lab/chart.hpp"
#include "lab/lab.hpp"
#include "lab/series.hpp"
#include "lab/series_charts.hpp"
#include "play/inputs.hpp"
#include "play/session.hpp"

namespace {

namespace fc = fall_line::controls;
namespace fs = fall_line::sim;
namespace gl = ge::lab;
namespace in = ge::input;
namespace pl = ge::play;

std::string fixed(double v, int d) { return ge::core::num::to_fixed(v, d); }

std::vector<uint8_t> bytes(const std::string& s) { return {s.begin(), s.end()}; }

// The game's bot down `setup`'s map for at most `most` steps, as the session would step it: each
// step's axes snapped, recorded and stepped.
struct BotRun {
  pl::InputLog log;
  uint64_t digest = 0;
  bool over = false;
};

std::optional<BotRun> bot_run(const pl::Setup& setup, int64_t most, std::string& error) {
  auto run = fs::kGame.open(setup, error);
  if (!run) return std::nullopt;
  pl::InputRecorder rec(pl::new_log(fs::kGame, setup));
  std::vector<double> in(fs::kGame.axes.size(), 0);
  for (int64_t i = 0; i < most && !run->over(); ++i) {
    std::fill(in.begin(), in.end(), 0);
    run->pilot(in);
    for (size_t a = 0; a < in.size(); ++a) in[a] = pl::snap_axis(in[a], fs::kGame.axes[a].kind);
    run->step(in);
    rec.record(in);
  }
  return BotRun{rec.log(), run->digest(), run->over()};
}

// A phone held on its side.
constexpr float kAspect = 19.5f / 9;

// The bot's run on the thumbs: read back step by step, skied on to its end, replayed, charted.
void thumbs(gl::Lab& lab, double seed, const std::string& k, const BotRun& bot) {
  const fc::TouchSession session = fc::touch_session(bot.log, kAspect);
  std::string error;
  in::ActionMap map = fc::default_map();
  in::read_bindings(fc::kExactTouchBindings, map, error);
  auto hand = fc::open_hand_run(seed, 0, std::move(map), error);
  if (!hand) {
    lab.fail("the thumbs' hand run did not open: " + error,
             "See games/fall_line/controls/by_hand.hpp.");
    return;
  }
  fc::hand_screen(*hand, kAspect);
  gl::Series s =
      gl::make_series({"t_s", "bot_steer", "steer", "bot_tuck", "tuck", "brake", "jump"});
  s.set("title", "seed " + fixed(seed, 0) + " on the thumbs: the bot's run on a phone's screen");
  s.set("x", "t_s");
  s.set("unit.t_s", "s");
  in::InputState held;
  int64_t differ = 0, off_bot = 0;
  const auto row = [&](int64_t at) {
    const bool logged = at < bot.log.steps();
    const double bot_steer = logged ? bot.log.at(at)[0] : 0,
                 bot_tuck = logged ? bot.log.at(at)[1] : 0;
    gl::add_row(s, {fs::state_of(*hand->run)->t, bot_steer, hand->step_in[0], bot_tuck,
                    hand->step_in[1], hand->step_in[2], hand->step_in[3]});
  };
  for (const in::LoggedFrame& f : session.events.frames) {
    in::update(held, f.q);
    const double now = static_cast<double>(f.t_ns) * 1e-9;
    fc::hand_frame(*hand, held, 1.0 / 120, now);
    const int64_t at = hand->steps;
    fc::hand_step(*hand, held, now);
    const auto& want = session.want[static_cast<size_t>(at)];
    const auto b = bot.log.at(at);
    bool d = false, o = false;
    for (size_t i = 0; i < want.size(); ++i) {
      d = d || hand->step_in[i] != want[i];
      o = o || want[i] != b[i];
    }
    differ += d ? 1 : 0;
    off_bot += o ? 1 : 0;
    row(at);
  }
  // A finished bot's run: the thumbs held where they last were, skied on to the end.
  const int64_t played = hand->steps;
  while (bot.over && fc::hand_playing(*hand)) {
    in::update(held, {});
    fc::hand_frame(*hand, held, 1.0 / 120, 0);
    const int64_t at = hand->steps;
    fc::hand_step(*hand, held, 0);
    row(at);
  }
  const pl::InputLog log = fc::hand_log(*hand);
  const auto replayed = pl::replay(fs::kGame, log, error);
  const std::string t = k + ".thumbs";
  lab.metric(t + ".steps", static_cast<double>(hand->steps), "steps");
  lab.metric(t + ".steps-after-the-session", static_cast<double>(hand->steps - played), "steps");
  lab.metric(t + ".steps-off-the-thumbs", static_cast<double>(differ), "count");
  lab.metric(t + ".steps-the-thumbs-change", static_cast<double>(off_bot), "count");
  lab.metric(t + ".jumps", session.jumps, "count");
  lab.metric(t + ".jumps-dropped", session.dropped, "count");
  lab.metric(t + ".finished", hand->run->over() ? 1 : 0, "count");
  if (differ != 0) {
    lab.fail(std::to_string(differ) + " steps on the thumbs read back other axes than they gave",
             "See games/fall_line/controls/thumbs.hpp and engine/input/touch.cpp.");
  }
  if (bot.over && !hand->run->over()) {
    lab.fail("the race on the thumbs did not finish in " + std::to_string(hand->steps) + " steps",
             "Chart it (thumbs.steer, thumbs.tuck) against the bot's; see controls/script.hpp.");
  }
  if (!replayed || replayed->digest != log.digest.value_or(0)) {
    lab.fail("the thumbs' log does not replay to its digest" + (error.empty() ? "" : ": " + error),
             "Replay the thumbs' input log with `ge replay` (skill: agent-interface).");
  }
  lab.attach("thumbs.events", bytes(in::write_events(session.events)));
  const std::string sub = "seed " + fixed(seed, 0) + ", " + std::to_string(hand->steps) +
                          " steps on the thumbs, " + std::to_string(off_bot) +
                          " steps where they ask other than the bot" +
                          (hand->run->over() ? ", finished" : "");
  const gl::SeriesCharts out{
      "thumbs.", t,  sub, {{"seed", fixed(seed, 0)}, {"pilot", "the bot, on two thumbs"}},
      {},        {}, ""};
  gl::chart_series(lab, s, out,
                   {{"steer",
                     "THE EDGE THUMB AND THE BOT'S STEER, SEED " + fixed(seed, 0),
                     "-1 left .. 1 right",
                     {"bot_steer", "steer"},
                     "The bot's steer and the steer the edge thumb gave, every step.",
                     "the lines lie on each other but for the thumb's first step"},
                    {"tuck",
                     "THE TUCK LEVER: TUCK, BRAKE AND THE JUMP, SEED " + fixed(seed, 0),
                     "0 .. 1",
                     {"bot_tuck", "tuck", "brake", "jump"},
                     "The bot's tuck, and the tuck, brake and jump the lever's thumb gave: one "
                     "thumb tucks or brakes, and a jump is a tap then a hold, so the tuck lifts "
                     "for a quarter second before it.",
                     "the tuck on the bot's but where it brakes or jumps"}});
}

void run(gl::Lab& lab) {
  const double seed = lab.arg_num("seed");
  const auto most = static_cast<int64_t>(lab.arg_num("limit") * fs::kGame.hz);
  const std::string k = "seed-" + fixed(seed, 0);
  pl::Setup setup;
  std::string error;
  if (!pl::make_setup(fs::kGame, seed, {}, setup, error)) {
    lab.fail("the setup did not open: " + error, "next: pass --seed=<a number>");
    return;
  }
  const auto bot = bot_run(setup, most, error);
  if (!bot) {
    lab.fail("the bot's run on seed " + fixed(seed, 0) + " did not open: " + error,
             "Run `make sim SEEDS=" + fixed(seed, 0) + "` (skill: sim).");
    return;
  }
  const pl::InputLog& script_log = bot->log;
  const in::EventLog session = fc::pad_session(script_log);
  in::EventLog read;
  if (!in::read_events(in::write_events(session), read, error)) {
    lab.fail("the pad's session does not read back: " + error,
             "Fix engine/input/event_log.cpp (skill: input).");
    return;
  }

  in::ActionMap map = fc::default_map();
  in::read_bindings(fc::kExactPadBindings, map, error);
  auto hand = fc::open_hand_run(seed, 0, std::move(map), error);
  if (!hand) {
    lab.fail("the hand run did not open: " + error, "See games/fall_line/controls/by_hand.hpp.");
    return;
  }
  gl::Series s = gl::make_series({"t_s", "pad_x", "steer", "trigger", "tuck", "low", "high"});
  s.set("title", "seed " + fixed(seed, 0) + " by hand: the bot's run held on a simulated pad");
  s.set("x", "t_s");
  s.set("unit.t_s", "s");
  std::vector<gl::Mark> marks;
  in::InputState held;
  int64_t differ = 0;
  for (const in::LoggedFrame& f : read.frames) {
    in::update(held, f.q);
    const double now = static_cast<double>(f.t_ns) * 1e-9;
    fc::hand_frame(*hand, held, 1.0 / 120, now);
    const int64_t at = hand->steps;
    const int before = hand->pulses;
    fc::hand_step(*hand, held, now);
    const auto want = script_log.at(at);
    for (size_t i = 0; i < want.size(); ++i) {
      if (hand->step_in[i] != want[i]) {
        ++differ;
        break;
      }
    }
    const in::Slot* pad = nullptr;
    for (const in::Slot& slot : held.slots) {
      if (slot.active && slot.info.kind == in::DeviceKind::gamepad) pad = &slot;
    }
    float low = 0, high = 0;
    if (pad) {
      if (const in::DeviceFeedback* d = hand->felt.find(pad->info.id)) {
        in::rumble_at(*d, now, low, high);
      }
    }
    const double t = fs::state_of(*hand->run)->t;
    gl::add_row(
        s, {t, pad ? pad->axes[static_cast<int>(in::PadAxis::left_x)] : 0.0, hand->step_in[0],
            pad ? pad->axes[static_cast<int>(in::PadAxis::right_trigger)] : 0.0, hand->step_in[1],
            low, high});
    if (hand->pulses > before) {
      for (const fs::GameEvent& e : fs::state_of(*hand->run)->events) {
        if (e.racer == 0 && fc::pulse_for(e, 0).seconds > 0) {
          marks.push_back({t, fs::event_name(e.kind)});
          break;
        }
      }
    }
  }
  const pl::InputLog log = fc::hand_log(*hand);
  const auto replayed = pl::replay(fs::kGame, log, error);
  lab.metric(k + ".steps", static_cast<double>(hand->steps), "steps");
  lab.metric(k + ".frames", static_cast<double>(read.frames.size()), "count");
  lab.metric(k + ".steps-off-the-bots", static_cast<double>(differ), "count");
  lab.metric(k + ".pulses", static_cast<double>(hand->pulses), "count");
  lab.metric(k + ".finished", hand->run->over() ? 1 : 0, "count");
  if (differ != 0) {
    lab.fail(
        std::to_string(differ) + " steps read back other axes than the bot's",
        "See games/fall_line/controls/script.hpp (the pad's shape) and engine/play/devices.cpp.");
  }
  if (hand->run->digest() != bot->digest) {
    lab.fail("the hand run's digest " + pl::hex64(hand->run->digest()) + " is not the bot's " +
                 pl::hex64(bot->digest),
             "Run tests/fall_line_controls_test and diff the two logs with `ge diverge`.");
  }
  if (!replayed || replayed->digest != log.digest.value_or(0)) {
    lab.fail(
        "the hand run's log does not replay to its digest" + (error.empty() ? "" : ": " + error),
        "Run `ge replay previews/by-hand/run.inputs` (skill: agent-interface).");
  }
  lab.attach("session.events", bytes(in::write_events(session)));
  lab.attach("run.inputs", bytes(pl::to_text(log)));
  lab.attach("bindings.txt", bytes(fc::kDefaultBindings));

  gl::LabTable binds{"FALL LINE'S DEFAULT BINDINGS", {"context", "action", "source"}, {}};
  const in::ActionMap defaults = fc::default_map();
  for (const in::Context& c : defaults.contexts) {
    for (const in::Binding& b : c.bindings) {
      binds.rows.push_back({c.name, defaults.defs[b.action].name, in::source_text(b.source)});
    }
  }
  lab.table(binds);
  thumbs(lab, seed, k, *bot);

  const std::string sub = "seed " + fixed(seed, 0) + ", " + std::to_string(hand->steps) +
                          " steps by hand, " + std::to_string(hand->pulses) + " pulses felt" +
                          (hand->run->over() ? ", finished" : "");
  const gl::SeriesCharts out{"",
                             k,
                             sub,
                             {{"seed", fixed(seed, 0)}, {"pilot", "the bot, on a pad"}},
                             marks,
                             {},
                             "Grey lines are the moments a pulse started."};
  gl::chart_series(lab, s, out,
                   {{"steer",
                     "THE PAD'S STICK AND THE STEER, SEED " + fixed(seed, 0),
                     "-1 left .. 1 right",
                     {"pad_x", "steer"},
                     "The pad's left stick, x, and the steer axis the run was stepped on, every "
                     "step: the bot's run held on a pad and read back through the bindings.",
                     "the two lines lie on each other: the device path adds and loses nothing"},
                    {"tuck",
                     "THE RIGHT TRIGGER AND THE TUCK, SEED " + fixed(seed, 0),
                     "0 .. 1",
                     {"trigger", "tuck"},
                     "The pad's right trigger and the tuck axis, every step.",
                     "the two lines lie on each other"},
                    {"felt",
                     "WHAT THE PAD FELT, SEED " + fixed(seed, 0),
                     "motor 0 .. 1",
                     {"low", "high"},
                     "The pad's two rumble motors, every step: the low (heavy) one and the high "
                     "(buzz), each pulse picked off the run's events (controls/felt.hpp).",
                     "a pulse at each hard landing and on any wipeout or trunk; quiet between"}});
}

const gl::LabSpec kSpec{
    "by-hand",
    gl::LabKind::drive,
    "game:fall_line",
    "games/fall_line/controls (the bindings, the hand loop, the felt) over engine/input and "
    "engine/play",
    "input",
    "Does a person's pad, read through Fall Line's bindings, ski exactly the run its axes say, "
    "is the run's log the run again, and does the same race finish on a phone's two thumbs?",
    {
        {"seed", "38", "the map's seed"},
        {"limit", "600", "the longest stretch held on the pad, s"},
    },
    "--seed=38 --limit=20",
    run,
};
GE_LAB(kSpec);

}  // namespace
