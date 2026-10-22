// PLAYING, REPLAYING AND INSPECTING THROUGH ge: `ge play`, `ge sim`, `ge replay`, `ge state`,
// `ge diverge` and `ge shot` build the game's `play` executable (the play contract,
// engine/play/game.hpp) and run it, so an agent tests a game by playing it, without a screen:
//
//   ge play [--seed=S] [-- --<option>=V]   the line protocol on this terminal (engine/play/
//                                          session.hpp): observe, act, pilot, state, inputs, end
//   ge sim [--seeds=A,B] [-- options]      the game's pilot over each seed; each run's input log
//                                          is written to runs/<game>-<seed>.inputs
//   ge replay <log>                        the log played back; a digest it misses is an error
//   ge state <log> [--step=N]              everything that decides the next step, after N steps
//   ge diverge <log-a> <log-b>             the first input and the first step the runs part, and
//                                          the state's fields that differ there
//   ge shot [<log>] [--step=N] [--view=V]  a still of the run at step N, drawn on the CPU and
//                                          captioned, written to runs/; without a log, the pilot's
//                                          run of --seed
//
// The play executable answers each with one JSON line, which becomes the outcome's `result`; a
// problem it reports (`{"error","where","next"}`) becomes a finding.
//
// ge:summary ge play, sim, replay, state, diverge and shot: the play executable run as findings
// ge:keywords cli ge play sim replay state diverge shot still input-log protocol run_play run_sim
// ge:skill agent-interface
// ge:test tests/cli_play_test.cpp
#pragma once

#include <string>
#include <vector>

#include "cli/build.hpp"
#include "cli/outcome.hpp"
#include "play/json_read.hpp"

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
// `ge shot`: the play executable's `shot` with `flags` ("--name=value": --seed, --step, --view,
// --gpu, --mode, --size) and the game's options; `log` empty: the pilot's run of --seed.
Outcome run_shot(const BuildOptions& o, const std::string& log,
                 const std::vector<std::string>& flags, const std::vector<std::string>& options);
// `ge record`: the play executable's `record` with `flags` ("--name=value") and the game's options.
Outcome run_record(const BuildOptions& o, const std::string& log,
                   const std::vector<std::string>& flags, const std::vector<std::string>& options);

// The play executable's JSON answer read into an outcome (exposed for the tests): `output` what
// it printed, `code` its exit code.
Outcome play_answer(const BuildOptions& o, const std::string& command, int code,
                    const std::string& output);

// The answer line of `output` read into `m` (arrays as path.0, path.1, ...): false, with a
// finding in `out`, when there is none or it is an error answer (its paths made the repo's).
bool read_answer(const BuildOptions& o, int code, const std::string& output, play::FlatJson& m,
                 std::string& line, Outcome& out);
// The build, then the play executable run with `args` (each word quoted, a leading space), its
// answer read as read_answer does. The perf commands (perf.hpp) run on it.
bool play_command(const BuildOptions& o, const std::string& args, play::FlatJson& m,
                  std::string& line, Outcome& out);
// A key of a flat answer as text: a string's own text, any other value's JSON ("" if absent).
std::string text_of(const play::FlatJson& m, const std::string& key);

}  // namespace ge::cli
