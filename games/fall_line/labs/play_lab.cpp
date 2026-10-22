// LAB `play`: AN AGENT AT THE CONTROLS. Fall Line on the engine's play session
// (engine/play/session.hpp, games/fall_line/sim/play.hpp). With `--stdio` it runs one session
// under the line protocol: a `hello` line, then one JSON answer a line for each JSON line read
// from stdin, until `end` or the end of input. Run it with the runner's `--quiet`, so stdout
// carries the protocol and nothing else (`ge play` runs the same session from the game's `play`
// executable):
//
//   build/debug/labs play --quiet --stdio --seed=38 --rivals=3
//
// With `--script=<file>` it plays that file's lines and keeps every answer as a note. With
// neither, THE BOT PLAYS THROUGH THE PROTOCOL as an agent would (an `act` of `--every` steps on
// the bot's controls, an `observe` every second, then `inputs` and `end`), and the session's
// input log is played back to the same digest: the check that an agent's run is a replayable run.
//
// ge:summary Lab: an agent at the controls over stdio (observe, act, inputs, end); the bot's check
// ge:keywords lab play agent stdio protocol playtest script session replay inputs
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
#include "play/session.hpp"

namespace {

namespace fs = fall_line::sim;
namespace gp = ge::play;

// The bot's controls as the `act` line an agent would send.
std::string act_line(const fs::Controls& c, int32_t steps) {
  return "{\"op\":\"act\",\"input\":{\"steer\":" + ge::core::num::to_string(c.ski.steer) +
         ",\"tuck\":" + ge::core::num::to_string(c.ski.tuck) +
         ",\"brake\":" + ge::core::num::to_string(c.ski.brake) +
         ",\"jump\":" + (c.ski.jump ? "true" : "false") +
         ",\"reset\":" + (c.reset ? "true" : "false") + "},\"steps\":" + std::to_string(steps) +
         "}";
}

// A number out of the report's JSON, by its key.
double field(const std::string& json, const std::string& key) {
  const size_t at = json.find("\"" + key + "\":");
  if (at == std::string::npos) return 0;
  const std::string rest = json.substr(at + key.size() + 3);
  if (rest.rfind("true", 0) == 0) return 1;
  return std::strtod(rest.c_str(), nullptr);
}

void report(ge::lab::Lab& lab, const gp::Session& session) {
  const std::string r = session.run().report();
  lab.metric("finished", field(r, "finished"), "bool");
  lab.metric("time", field(r, "time"), "s");
  lab.metric("gates", field(r, "gates"), "count");
  lab.metric("place", field(r, "place"), "place");
  lab.metric("wipeouts", field(r, "wipeouts"), "count");
  lab.metric("steps", static_cast<double>(session.steps()), "steps");
}

void run(ge::lab::Lab& lab) {
  const double seed = lab.arg_num("seed");
  gp::Setup setup;
  std::string error;
  if (!gp::make_setup(fs::kGame, seed,
                      {{"rivals", lab.arg("rivals")}, {"version", lab.arg("version")}}, setup,
                      error)) {
    lab.fail(error, "Pass --rivals and --version as numbers (skill: sim).");
    return;
  }
  gp::Session session(fs::kGame, setup);
  if (!session.ok()) {
    lab.fail(session.error(), "Pick another --seed (skill: mountain).");
    return;
  }

  if (lab.arg_bool("stdio")) {
    std::printf("%s\n", session.hello().c_str());
    std::fflush(stdout);
    std::string line;
    while (!session.ended() && std::getline(std::cin, line)) {
      if (line.empty()) continue;
      std::printf("%s\n", session.handle(line).c_str());
      std::fflush(stdout);
    }
    report(lab, session);
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
    report(lab, session);
    return;
  }

  // The bot as the agent, through the protocol's own text, its moves read off a shadow driver
  // stepped on the same controls.
  const auto every = static_cast<int32_t>(lab.arg_num("every"));
  const auto limit = static_cast<int64_t>(lab.arg_num("seconds") * fs::kHz);
  const auto rivals = static_cast<int32_t>(lab.arg_num("rivals"));
  fs::Driver shadow(std::make_shared<const ge::mountain::mapgen::Level>(
                        fs::race_map(seed, static_cast<int>(lab.arg_num("version")))),
                    rivals);
  int32_t errors = 0;
  int64_t observed = 0;
  while (!session.done() && session.steps() < limit) {
    const fs::Controls c = fs::bot_controls(shadow.state(), 0, shadow.plan());
    const int64_t before = session.steps();
    if (session.handle(act_line(c, every)).find("\"op\":\"error\"") != std::string::npos) errors++;
    fs::Controls held = c;
    for (int64_t i = before; i < session.steps(); i++) {
      shadow.step(held);
      held.reset = false;
    }
    if (static_cast<double>(session.steps() - observed) >= fs::kHz) {
      observed = session.steps();
      if (session.handle("{\"op\":\"observe\"}").find("\"op\":\"error\"") != std::string::npos) {
        errors++;
      }
    }
  }
  const std::string inputs_line = session.handle("{\"op\":\"inputs\"}");
  const gp::InputLog log = session.inputs();
  const std::string end_line = session.handle("{\"op\":\"end\"}");
  lab.note(end_line);
  report(lab, session);
  lab.metric("protocol-errors", errors, "count");
  if (errors > 0) {
    lab.fail(std::to_string(errors) + " of the bot's protocol lines were refused",
             "Run the session by hand with --stdio and read engine/play/session.cpp's ops "
             "(skill: agent-interface).");
  }
  const auto again = gp::replay(fs::kGame, log, error);
  const bool matches = again && log.digest && again->digest == *log.digest;
  lab.metric("replay-matches", matches ? 1 : 0, "bool");
  if (!matches) {
    lab.fail("the session's input log played back to a different run" +
                 (error.empty() ? std::string() : ": " + error),
             "The session and replay must step the same snapped inputs: read "
             "engine/play/session.cpp (skill: agent-interface).");
  }
  lab.metric("input-log-steps", static_cast<double>(log.steps()), "steps");
  lab.metric("input-log-runs", static_cast<double>(log.runs.size()), "count");
  lab.metric("inputs-line-bytes", static_cast<double>(inputs_line.size()), "bytes");
}

const ge::lab::LabSpec kSpec{
    "play",
    ge::lab::LabKind::drive,
    "game:fall_line",
    "games/fall_line/sim/play on engine/play (an agent at the controls over a line protocol)",
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
