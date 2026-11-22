// A DEDICATED SERVER'S PROCESS: what every game's `<game>_server` binary runs (cmake/Server.cmake's
// `ge_add_server` links a three-line main that calls `server_main` with the game's ServerGame).
//
// THE SETTINGS, later winning (config.hpp, docs/servers.md § The settings):
// 1. each cvar's fallback (server.hpp's ServerConfig, the game's default port and map);
// 2. the base game directory's `server.cfg` (`<root>/<base>/server.cfg`: `--root`, default the
//    working directory; `--base`, default the game's);
// 3. each mod's `server.cfg`, lowest first (`--mod=a,b`);
// 4. a profile (`--cfg=duel`: duel.cfg from the highest directory that has one);
// 5. the command line: the flags below, then any `+command args` (`+set sv_hostname "Arena"`).
// A cfg's mistake is printed at its file and line and stops the server before it opens.
// `--explain[=prefix]` prints every server value, where it was set and what it overrode, and
// exits.
//
// THE FLAGS (each a cvar, server.hpp): --name --description --ip --port --maxclients --map --bots
// --seed --password --visibility --motd --admin; where the settings are read: --root --base --mod
// --cfg --explain; and how the process runs: --for --status --realtime --offline --log --replay
// --record --commands --help.
//
// THE CONSOLE AT RUN TIME: with `--commands=PATH` the server runs each line added to that file
// (a launcher's way in: spawn.hpp's hosted_command), a few times a second. `mod <dirs>` there
// switches the mods: the layers read again (server_switch_mods) and the match restarted under
// them, on the same port.
//
// THE FEATURES (feature.hpp): a game on the core's own session gets the administrator's
// (admin.hpp: status, kick, bans in a file, rcon over the session, the log), their cvars read
// with the rest; a game with its own protocol answers its own clients and gets none.
//
// THE RUN: the banner, a status line every `--status` seconds of server time, the digest at the
// end, the input log written when asked, the match recorded as a replay file with `--record=PATH`
// (record.hpp); `--replay` runs a log again and names the first frame
// that leaves it (exit 2). Frames are paced to the wall clock only with `--realtime`; the run
// itself never reads it. With no `--for`, `--offline` or `--replay` the process is a server people
// play on: paced to the wall clock until it is stopped.
//
// ge:summary A dedicated server's process: flags, server.cfg, the run loop, status, digest, replay
// ge:keywords server_main dedicated-server server-cfg flags command-line port ip name description
// ge:keywords maxclients bots realtime offline replay input-log status digest server_configure
// ge:keywords root base mod profile explain mod-switch server_switch_mods commands-file
// ge:skill net
// ge:test tests/server_core_test.cpp
#pragma once

#include <string>
#include <vector>

#include "console/console.hpp"
#include "server/config.hpp"
#include "server/feature.hpp"
#include "server/server.hpp"

namespace ge::server {

// The settings worked out for `game`: the cvars bound to `config` on `console`, the layers read
// (config.hpp: through the console's reader, `--root` setting its search path) with every value's
// source kept in `cfg` (which, like `config`, must outlive the console), then the command line
// `args` (flags and `+commands`, without the program's name). False with `error` (the flag, file
// or line and the next action).
// With `features`, theirs are attached (their cvars and commands) before the layers are read.
bool server_configure(console::Console& console, const std::vector<std::string>& args,
                      ServerConfig& config, ServerCfg& cfg, const ServerGame& game,
                      std::string& error, Features* features = nullptr);

// THE MOD SWITCH (`mod arena,ctf` on the server's console): every server value back to its
// fallback, the search path remounted over `mods` (lowest first; none: the base game alone),
// `sv_mod` set, and the layers read again with the command line kept in `cfg`, so a mod's
// server.cfg takes over what it sets and the command line still wins. The running match is the
// caller's to restart under `config`. False with `error` when a layer refuses; the mods before
// are then read again and kept. With `fs_autoexec` 0 only the search path and `sv_mod` change:
// no value is reset and no layer read.
bool server_switch_mods(console::Console& console, ServerCfg& cfg, const ServerConfig& config,
                        const std::vector<std::string>& mods, std::string& error);

// The flags' help, for `game`.
std::string server_help(const ServerGame& game);

// The whole process: 0 done, 1 refused (a flag, a cfg, a socket), 2 a replay that left its log.
int server_main(int argc, char** argv, const ServerGame& game);

}  // namespace ge::server
