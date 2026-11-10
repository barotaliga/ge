// THE PROGRAMS A GAME'S CLI STARTS: the window joined to a server or hosting one, the dedicated
// server, each started DETACHED (its own session, so it outlives the CLI and a closed terminal),
// its output and errors written to a log file, and later asked whether it still runs and stopped.
// POSIX (`posix_spawn`, a new session where the C library offers one) on Linux and macOS;
// `CreateProcess`, detached, on Windows.
//
// ge:summary The programs a game's CLI starts: detached, logged, asked after, stopped
// ge:keywords gamecli process spawn detached posix_spawn createprocess pid alive stop kill log
// ge:keywords self_dir program_path executable
// ge:skill agent-interface
// ge:test tests/gamecli_cli_test.cpp
#pragma once

#include <string>
#include <vector>

namespace ge::gamecli {

// `argv` (argv[0] the program's path) started detached in `cwd` ("": the CLI's), its output and
// errors appended to `log`; true with its process id in `pid`, false with `error`.
bool spawn_detached(const std::vector<std::string>& argv, const std::string& cwd,
                    const std::string& log, long& pid, std::string& error);
// Whether process `pid` still runs (a child of this process that ended is reaped).
bool process_alive(long pid);
// Process `pid` asked to stop, then made to after `grace_ms`; false with `error` when it cannot be.
bool process_stop(long pid, int grace_ms, std::string& error);

// The folder of the running program (`argv0` when the platform cannot say), without a trailing
// separator; "" when unknown.
std::string self_dir(const char* argv0);
// `<dir>/<name>` with the platform's executable suffix (".exe" on Windows).
std::string program_path(const std::string& dir, const std::string& name);

}  // namespace ge::gamecli
