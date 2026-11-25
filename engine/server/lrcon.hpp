// A LIMITED REMOTE CONSOLE FOR HELPERS (`lrcon_password`, `lrcon_add`), the same rule for every
// game's server (docs/servers.md § The settings): a second password that runs only the console
// lines an administrator allowed, so a trusted helper can change the map or kick a cheat without
// the whole remote console.
//
// - THE PASSWORD (`lrcon_password`): given where the remote console's password goes. Empty (the
//   default): no limited console. The full password always wins.
// - THE LIST: `lrcon_add <prefix>` (the administrator's line, over the full console) allows every
//   line that starts with that text ("map", "kick ", "sv_locked"); `lrcon_del <prefix>` takes one
//   away, `lrcon_list` shows them. Empty: the password runs nothing.
// - THE REFUSAL: a line no prefix allows answers "This command is not permitted." and runs
//   nothing. A wrong password is the full console's refusal and counts against `sv_rconrate`.
//
// The prefixes are kept in the endpoint (`LimitedRcon`), set at start from nothing and by the
// administrator's lines; `lrcon_command` answers those lines, `lrcon_allows` judges a helper's.
//
// ge:summary lrcon_password, lrcon_add: a helper's remote console that runs only allowed lines
// ge:keywords lrcon lrcon_password lrcon_add lrcon_del lrcon_list limited-rcon helper-console
// ge:keywords lrcon_allows lrcon_command
// ge:skill net
// ge:test tests/server_lrcon_test.cpp
#pragma once

#include <string>
#include <vector>

#include "server/server.hpp"

namespace ge::server {

inline constexpr GameRule kLrconRules[] = {
    {"lrcon_password", "The password of the limited remote console a helper uses.", nullptr,
     "\"\": none (the default); TEXT: the password, which runs only the lines lrcon_add allows",
     false},  // a password: never in the server info
};

inline constexpr const char* kLrconRefusal = "This command is not permitted.\n";

struct LimitedRcon {
  std::string password;               // lrcon_password (empty: none)
  std::vector<std::string> prefixes;  // the lines it may run start with one of these
};

// Whether `line` starts with an allowed prefix.
bool lrcon_allows(const LimitedRcon& l, const std::string& line);

// The administrator's `lrcon_add`, `lrcon_del`, `lrcon_list` (and `extra`'s spellings, a game's
// own words for them: add, del, list) onto `l`; false when the line is none of them.
bool lrcon_command(LimitedRcon& l, const std::string& line, std::string& out,
                   const char* const (*extra)[3] = nullptr);

}  // namespace ge::server
