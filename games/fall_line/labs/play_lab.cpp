// LAB `play`: AN AGENT AT THE CONTROLS. With `--stdio` it runs one Fall Line session under the
// line protocol (games/fall_line/sim/play.hpp): a `hello` line, then one JSON answer a line for
// each JSON line read from stdin, until `end` or the end of input. Run it with the runner's
// `--quiet`, so stdout carries the protocol and nothing else:
//
//   build/debug/labs play --quiet --stdio --seed=38 --rivals=3
//
// With `--script=<file>` it plays that file's lines and keeps every answer as a note. With
// neither, THE BOT PLAYS THROUGH THE PROTOCOL as an agent would (an `act` of `--every` steps on
// the bot's controls, an `observe` every second, then `tape` and `end`), and the session's tape
// is played back to the same digest: the check that an agent's run is a replayable run.
//
// ge:summary Lab: an agent at the controls over stdio (observe, act, tape, end); the bot's check
// ge:keywords lab play agent stdio protocol playtest script session replay
// ge:skill sim agent-interface labs
// ge:test tests/fall_line_play_test.cpp
#include <cstdio>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>

#include "core/num_text.hpp"
#include "fall_line/sim/bot.hpp"
#include "fall_line/sim/game.hpp"
#include "fall_line/sim/play.hpp"
#include "fall_line/sim/simulate.hpp"
#include "lab/lab.hpp"
#include "mountain/mapgen/generate.hpp"

namespace {

namespace fs = fall_line::sim;
namespace mg = ge::mountain::mapgen;

std::string fixed(double v, int d) { return ge::core::num::to_fixed(v, d); }

// The bot's controls as the `act` line an agent would send.
std::string act_line(const fs::Controls& c, int32_t steps) {
  return "{\"op\":\"act\",\"input\":{\"steer\":" + ge::core::num::to_string(c.ski.steer) +
         ",\"tuck\":" + ge::core::num::to_string(c.ski.tuck) +
         ",\"brake\":" + ge::core::num::to_string(c.ski.brake) +
         ",\"jump\":" + (c.ski.jump ? "true" : "false") +
         ",\"reset\":" + (c.reset ? "true" : "false") + "},\"steps\":" + std::to_string(steps) +
         "}";
}

void report(ge::lab::Lab& lab, const fs::PlaySession& session, double seed) {
  const fs::RunReport r = session.driver().report(seed);
  lab.metric("finished", r.finished ? 1 : 0, "bool");
  lab.metric("time", r.time, "s");
  lab.metric("gates", r.gates, "count");
  lab.metric("place", r.place, "place");
  lab.metric("wipeouts", r.wipeouts, "count");
  lab.metric("steps", static_cast<double>(r.steps), "steps");
}

void run(ge::lab::Lab& lab) {
  const double seed = lab.arg_num("seed");
  const auto rivals = static_cast<int32_t>(lab.arg_num("rivals"));
  std::shared_ptr<const mg::Level> level;
  try {
    level = std::make_shared<const mg::Level>(
        fs::race_map(seed, static_cast<int>(lab.arg_num("version"))));
  } catch (const mg::GenerateError& e) {
    lab.fail("seed " + fixed(seed, 0) + " built no map: " + e.what(),
             "Pick another --seed, or run `make lab LAB=analyze ARGS=--seeds=" + fixed(seed, 0) +
                 " --refusals` (skill: mountain).");
    return;
  }
  fs::PlaySession session(level, seed, rivals);

  if (lab.arg_bool("stdio")) {
    std::printf("%s\n", session.hello().c_str());
    std::fflush(stdout);
    std::string line;
    while (!session.ended() && std::getline(std::cin, line)) {
      if (line.empty()) continue;
      std::printf("%s\n", session.handle(line).c_str());
      std::fflush(stdout);
    }
    report(lab, session, seed);
    return;
  }

  const std::string script = lab.arg("script");
  if (!script.empty()) {
    std::ifstream in(script);
    if (!in) {
      lab.fail("cannot read the script " + script,
               "Pass --script=<file> with one protocol line a line (docs/labs.md § An agent "
               "at the controls).");
      return;
    }
    lab.note(session.hello());
    std::string line;
    while (!session.ended() && std::getline(in, line)) {
      if (!line.empty()) lab.note(session.handle(line));
    }
    report(lab, session, seed);
    return;
  }

  // The bot as the agent, through the protocol's own text.
  const auto every = static_cast<int32_t>(lab.arg_num("every"));
  const auto limit = static_cast<int64_t>(lab.arg_num("seconds") * fs::kHz);
  int32_t errors = 0;
  int64_t observed = 0;
  while (!session.driver().over() && session.driver().state().steps < limit) {
    const fs::Controls c = fs::bot_controls(session.driver().state(), 0, session.driver().plan());
    if (session.handle(act_line(c, every)).find("\"op\":\"error\"") != std::string::npos) errors++;
    if (static_cast<double>(session.driver().state().steps - observed) >= fs::kHz) {
      observed = session.driver().state().steps;
      if (session.handle("{\"op\":\"observe\"}").find("\"op\":\"error\"") != std::string::npos) {
        errors++;
      }
    }
  }
  const std::string tape_line = session.handle("{\"op\":\"tape\"}");
  const ge::racing::ControlTape tape = session.driver().tape();
  const std::string end_line = session.handle("{\"op\":\"end\"}");
  lab.note(end_line);
  report(lab, session, seed);
  lab.metric("protocol-errors", errors, "count");
  if (errors > 0) {
    lab.fail(std::to_string(errors) + " of the bot's protocol lines were refused",
             "Run the session by hand with --stdio and read games/fall_line/sim/play.cpp's "
             "reader (skill: sim).");
  }
  const fs::RunReport again = fs::replay_tape(level, seed, tape, rivals);
  const uint64_t digest = session.driver().digest();
  lab.metric("replay-matches", again.digest == digest ? 1 : 0, "bool");
  if (again.digest != digest) {
    lab.fail("the session's tape played back to a different run",
             "The play session and replay_tape must drive the same Driver steps: read "
             "games/fall_line/sim/simulate.cpp and play.cpp (skill: sim).");
  }
  lab.metric("tape-steps", tape.steps, "steps");
  lab.metric("tape-line-bytes", static_cast<double>(tape_line.size()), "bytes");
}

const ge::lab::LabSpec kSpec{
    "play",
    ge::lab::LabKind::drive,
    "game:fall_line",
    "games/fall_line/sim/play (an agent at the controls over a line protocol)",
    "sim",
    "Can an agent play a run over stdio, and is its run taped and replayable like the bot's?",
    {
        {"seed", "38", "the map's seed"},
        {"rivals", "0", "bots on the grid beside the agent"},
        {"version", "1", "the generator's version"},
        {"stdio", "", "play the protocol on stdin and stdout (run with --quiet)", true},
        {"script", "", "play this file's protocol lines and note every answer"},
        {"seconds", "600", "the bot's self-play: the most run time, s"},
        {"every", "12", "the bot's self-play: steps an act holds its input"},
    },
    "--seconds=20 --rivals=3",
    run,
};
GE_LAB(kSpec);

}  // namespace
