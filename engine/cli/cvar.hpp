// A GAME'S SETTINGS THROUGH ge: `ge cvar` and `ge cfg` build the game's `play` executable and run
// its console commands (engine/play/console_cmds.hpp) over the cvars the game registers:
//
//   ge cvar ls [<prefix>]          every cvar: value, default, range, flags, help
//   ge cvar get <name>...          the named ones (an unknown one: the nearest name)
//   ge cfg check <file>...         each problem a finding at file:line (unknown commands, refused
//                                  sets, names nothing reads), as a compiler's
//   ge cfg exec <file>...          the same, then every cvar the files moved off its default and
//                                  what they printed (--cmd= runs more after them)
//   ge cfg dump [--out=<file>]     every setting a player may change, at its default, commented:
//                                  a cfg to start one's own from
//
// `--server` runs them in a dedicated server's console (the server's cvars may be set).
//
// ge:summary ge cvar and ge cfg: a game's cvars listed, cfg files checked and run, the dump
// ge:keywords cli ge cvar cfg ls get check exec dump run_cvar run_cfg settings console
// ge:skill console agent-interface
// ge:test tests/cli_cvar_test.cpp
#pragma once

#include <string>
#include <vector>

#include "cli/build.hpp"
#include "cli/outcome.hpp"
#include "play/json_read.hpp"

namespace ge::cli {

struct ConsoleAsk {
  std::string op{};                  // ls, get; check, exec, dump
  std::vector<std::string> words{};  // the prefix, the names or the files
  bool server = false;
  std::string cmd{};  // exec: commands run after the files
  std::string out{};  // dump: the file written (empty: the text printed)
};

Outcome run_cvar(const BuildOptions& o, const ConsoleAsk& ask);
Outcome run_cfg(const BuildOptions& o, const ConsoleAsk& ask);

// The play executable's answer to `cvar` or `cfg` read into an outcome (exposed for the tests).
Outcome console_answer(const BuildOptions& o, const ConsoleAsk& ask, const std::string& command,
                       const play::FlatJson& m, const std::string& line);

}  // namespace ge::cli
