// A DEDICATED SERVER'S PROCESS: what every game's `<game>_server` binary runs (cmake/Server.cmake's
// `ge_add_server` links a three-line main that calls `server_main` with the game's ServerGame).
//
// THE SETTINGS, later winning (docs/servers.md § The settings; the base and mod layers and
// `--explain` come with M53b):
// 1. each cvar's fallback (server.hpp's ServerConfig, the game's default port and map);
// 2. `server.cfg` in the server's directory (`--dir`, default the working directory);
// 3. the command line: the flags below, then any `+command args` (`+set sv_hostname "Arena"`).
// A cfg's mistake is printed at its file and line and stops the server before it opens.
//
// THE FLAGS (each a cvar, server.hpp): --name --description --ip --port --maxclients --map --bots
// --seed --password --visibility; and how the process runs: --for --status --realtime --offline
// --log --replay --dir --help.
//
// THE RUN: the banner, a status line every `--status` seconds of server time, the digest at the
// end, the input log written when asked; `--replay` runs a log again and names the first frame
// that leaves it (exit 2). Frames are paced to the wall clock only with `--realtime`; the run
// itself never reads it.
//
// ge:summary A dedicated server's process: flags, server.cfg, the run loop, status, digest, replay
// ge:keywords server_main dedicated-server server-cfg flags command-line port ip name description
// ge:keywords maxclients bots realtime offline replay input-log status digest server_configure
// ge:skill net
// ge:test tests/server_core_test.cpp
#pragma once

#include <string>
#include <vector>

#include "console/console.hpp"
#include "server/server.hpp"

namespace ge::server {

// The settings worked out for `game`: the cvars bound to `config` on `console`, `server.cfg` read
// (from the console's search path), then the command line `args` (flags and `+commands`, without
// the program's name). False with `error` (the flag, file or line and the next action).
bool server_configure(console::Console& console, const std::vector<std::string>& args,
                      ServerConfig& config, const ServerGame& game, std::string& error);

// The flags' help, for `game`.
std::string server_help(const ServerGame& game);

// The whole process: 0 done, 1 refused (a flag, a cfg, a socket), 2 a replay that left its log.
int server_main(int argc, char** argv, const ServerGame& game);

}  // namespace ge::server
