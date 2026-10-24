// THE PLAY EXECUTABLE'S CONSOLE COMMANDS: a game's cvars and cfg files, answered without a screen
// over the cvars its spec registers (game.hpp's `cvars`). `ge cvar` and `ge cfg` run them:
//
//   play cvar ls [<prefix>]             every cvar: value, default, kind, range, flags, help
//   play cvar get <name>...             the named ones (an unknown name: the nearest known)
//   play cfg check <file>... [--server] each file exec'd in a fresh console: every problem with
//                                       its file and line, and each name set that nothing reads
//   play cfg exec <file>... [--server]  the same, then every cvar off its default and what the
//              [--cmd=<commands>]       files and the commands printed
//   play cfg dump                       a cfg of every setting a player may change, commented
//
// `--server`: the console is a dedicated server's (the server's cvars may be set; a client's
// refuses them). A file's own `exec` finds files beside it first. Each answers one JSON line.
//
// ge:summary The play executable's cvar and cfg commands: list, get, check, exec, dump
// ge:keywords play cvar cfg ls get check exec dump console cvar_cmd cfg_cmd ge-cvar ge-cfg
// ge:skill console agent-interface
// ge:test tests/play_console_test.cpp
#pragma once

#include <map>
#include <ostream>
#include <string>
#include <vector>

#include "play/game.hpp"

namespace ge::play {

int cvar_cmd(const GameSpec& spec, const std::vector<std::string>& words,
             const std::map<std::string, std::string>& flags, std::ostream& out);
int cfg_cmd(const GameSpec& spec, const std::vector<std::string>& words,
            const std::map<std::string, std::string>& flags, std::ostream& out);

}  // namespace ge::play
