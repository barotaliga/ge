// `ge server`: A GAME'S DEDICATED SERVER RUN FROM ITS REPO (docs/servers.md, M53d): the binary a
// game makes with `ge_add_server` (cmake/Server.cmake) built and run headless with its bots, for
// a digest and an input log, or its settings explained.
//
//   ge server [--game=<binary>] [--map=M] [--bots=N] [--for=S]   built, then run offline (no
//       socket) for S seconds of server time (default 60) with its bots; the input log written to
//       runs/<binary>.inputs (--log), the digest and the frames read from its last line
//   ge server --replay=<log>                  the log run again: its digest, or the first frame
//                                             that leaves it (an error)
//   ge server --port=N                        on that UDP port instead of offline, for players
//   ge server cfg [<prefix>]                  every server value with where it was set and what
//                                             it overrode (the binary's --explain); --mod,
//                                             --cfg and --root as the server takes them
//
// Which binary: --game names it (sesv, gesample); without it the repo's own, when its CMake files
// make exactly one (`ge_add_server(... OUTPUT_NAME <name> ...)`, `server_binaries`). Anything
// after `--` goes to the binary as given (`-- +set sv_seed 7`).
//
// ge:summary ge server: a game's dedicated server built and run headless, replayed, explained
// ge:keywords cli ge server dedicated-server headless bots digest input-log replay explain cfg
// ge:keywords run_server run_server_cfg server_binaries server_argv read_server_run ge_add_server
// ge:skill agent-interface net
// ge:test tests/cli_server_test.cpp
#pragma once

#include <filesystem>
#include <string>
#include <utility>
#include <vector>

#include "cli/build.hpp"
#include "cli/finding.hpp"

namespace ge::cli {

struct ServerOptions {
  std::string game;  // the binary's name; "" the repo's one
  std::string map, bots, seconds, port, log, replay;
  std::string prefix;  // `server cfg`: only the values starting with this
  std::string mod, cfg, root;
  std::vector<std::string> passthrough;  // after `--`, to the binary as given
};

// The dedicated binaries the repo's CMake files (CMakeLists.txt, *.cmake) make (`ge_add_server`'s
// OUTPUT_NAME), sorted; the build folders are not searched.
std::vector<std::string> server_binaries(const std::filesystem::path& repo);
// The same with each one's CMake target: (binary, target), sorted by binary.
std::vector<std::pair<std::string, std::string>> server_targets(const std::filesystem::path& repo);

// The arguments a run hands the binary (without the program): offline unless a port is given,
// --for, the map and bots, the input log (or the replay), then the passthrough.
std::vector<std::string> server_argv(const ServerOptions& s, const std::string& log);

// What a run printed, read: the frames and the digest from its `done:` line (or a replay's
// `replay:` line), the input log's path; false when there was no such line.
struct ServerRun {
  std::string frames, digest, log;
  std::string diverged;  // a replay: the frame that left the log ("" none)
};
bool read_server_run(const std::string& output, ServerRun& out);

// The binary at `exe` run with `argv` (output to `log_file`), its answer as an outcome; exposed so
// a test runs a built binary without a build.
Outcome run_server_binary(const std::filesystem::path& repo, const std::filesystem::path& exe,
                          const ServerOptions& s, const std::filesystem::path& log_file);

Outcome run_server(const BuildOptions& o, const ServerOptions& s);
Outcome run_server_cfg(const BuildOptions& o, const ServerOptions& s);

}  // namespace ge::cli
