// CFG FILES: the player's config as data, and the order a game reads its cfgs at startup.
//
// STARTUP (`start`): `default.cfg` (the game's shipped defaults and binds), then `config.cfg`
// (what `cfg_save` last wrote), then `autoexec.cfg` (the player's own, never written by the game),
// then the command line's `+commands`. A file that is not there is skipped silently; every other
// problem is the console's (file and line). A dedicated server reads `server.cfg` in place of the
// first three (`start_server`).
//
// SAVING (`Console::save_text`, the `cfg_save` command) writes, in a stable order a diff can
// read: a header line, `unbindall`, every bind (mode, then key), every alias but the temporary
// ones (by name), every archived cvar (`seta`, by name) and every created user-info key
// (`setinfo`). Values are quoted; a `$` is written `$$` so it reads back as written.
//
// ge:summary Cfg files: the startup order (default, config, autoexec, +commands), saving, help
// ge:keywords cfg config cfg_save config-cfg autoexec default-cfg server-cfg start
// ge:keywords start_server save_text describe help command-line dump template
// ge:skill console
// ge:test tests/console_cfg_test.cpp
#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "console/console.hpp"

namespace ge::console {

inline constexpr std::string_view kDefaultFile = "default.cfg";
inline constexpr std::string_view kConfigFile = "config.cfg";
inline constexpr std::string_view kAutoexecFile = "autoexec.cfg";
inline constexpr std::string_view kServerFile = "server.cfg";

// The client's startup order queued and run; `args` the command line's words.
void start(Console& c, const std::vector<std::string>& args);
// A dedicated server's: server.cfg, then the command line.
void start_server(Console& c, const std::vector<std::string>& args);

// `name` described for `help`: a cvar's value, default, range, flags and help; a command's help;
// an alias's body.
std::string describe(const Console& c, std::string_view name);

// A cfg of every cvar a player may set, each commented with its help and range and set to its
// default (`ge cfg dump`): the starting point for a config of one's own.
std::string dump_text(const Console& c);

// The flags as letters (A archived, U user info, S server info, V server's, C cheat, L latched,
// R read-only, * created).
std::string flags_text(uint32_t flags);

}  // namespace ge::console
