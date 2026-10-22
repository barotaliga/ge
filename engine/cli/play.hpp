// PLAYING, REPLAYING AND INSPECTING THROUGH ge: `ge play`, `ge sim`, `ge replay`, `ge state` and
// `ge diverge` build the game's `play` executable (the play contract, engine/play/game.hpp) and
// run it, so an agent tests a game by playing it, without a screen:
//
//   ge play [--seed=S] [-- --<option>=V]   the line protocol on this terminal (engine/play/
//                                          session.hpp): observe, act, pilot, state, inputs, end
//   ge sim [--seeds=A,B] [-- options]      the game's pilot over each seed; each run's input log
//                                          is written to runs/<game>-<seed>.inputs
//   ge replay <log>                        the log played back; a digest it misses is an error
//   ge state <log> [--step=N]              everything that decides the next step, after N steps
//   ge diverge <log-a> <log-b>             the first input and the first step the runs part, and
//                                          the state's fields that differ there
//
// The play executable answers each with one JSON line, which becomes the outcome's `result`; a
// problem it reports (`{"error","where","next"}`) becomes a finding.
//
// ge:summary ge play, sim, replay, state and diverge: the game's play executable run as findings
// ge:keywords cli ge play sim replay state diverge input-log protocol run_play run_sim
// ge:skill agent-interface
// ge:test tests/cli_play_test.cpp
#pragma once

#include <string>
#include <vector>

#include "cli/build.hpp"
#include "cli/outcome.hpp"

namespace ge::cli {

// The game's play executable: build/<preset>/play.
fs::path play_exe(const BuildOptions& o);

// `ge play`: the build, then the protocol on this process's own stdin and stdout. `played` says
// whether the session ran: then the outcome's `exit` is its code and nothing more is printed;
// otherwise the outcome holds the build's findings.
Outcome run_play(const BuildOptions& o, const std::string& seed,
                 const std::vector<std::string>& options, bool& played);
Outcome run_sim(const BuildOptions& o, const std::string& seeds,
                const std::vector<std::string>& options);
Outcome run_replay(const BuildOptions& o, const std::string& log);
Outcome run_state(const BuildOptions& o, const std::string& log, const std::string& step);
Outcome run_diverge(const BuildOptions& o, const std::string& a, const std::string& b);

// The play executable's JSON answer read into an outcome (exposed for the tests): `line` its
// output, `code` its exit code.
Outcome play_answer(const BuildOptions& o, const std::string& command, int code,
                    const std::string& output);

}  // namespace ge::cli
