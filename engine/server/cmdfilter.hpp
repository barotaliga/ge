// COMMANDS A SERVER REFUSES FROM ITS CLIENTS (`filtercmd_add`), the same rule for every game's
// server (docs/servers.md § The settings): a client's console line whose first word is on the list
// never reaches the game, and the server does what the entry says instead.
//
// - THE WORDS (the administrator's, over the console): `filtercmd_add <command> [ignore|log|print|
//   stuff|kick] [text]` adds or replaces the entry for that command; `print` takes the text to
//   show the client and `stuff` the line to run on the client's console (for the others the text
//   is a note for the list). `filtercmd_remove <command|number|all>` takes entries away;
//   `filtercmd_list` shows them, numbered from 1.
// - THE MATCH: the line's first word, ignoring case. The game names the words its own protocol
//   needs (a signon's) and never filters them, so a careless entry cannot lock players out.
// - THE ACTIONS: `ignore` drops the line; `log` drops it and writes the log line
//   (`cmdfilter_log_line`); `print` drops it and shows the client the text; `stuff` drops it and
//   runs the text on the client's console; `kick` drops it and the client.
//
// ge:summary filtercmd_add: client commands a server refuses, and what it does instead
// ge:keywords filtercmd filtercmd_add filtercmd_remove filtercmd_list command-filter banned-command
// ge:keywords cmdfilter_match cmdfilter_command client-command
// ge:skill net
// ge:test tests/server_cmdfilter_test.cpp
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace ge::server {

enum class FilterDo : uint8_t { ignore, log, print, stuff, kick };

struct CmdFilter {
  std::string command;  // lower case
  FilterDo action = FilterDo::ignore;
  std::string text;  // print's words, stuff's line, or a note
};

struct CmdFilters {
  std::vector<CmdFilter> entries;
};

// The entry whose command is `line`'s first word (any case); null when none is.
const CmdFilter* cmdfilter_match(const CmdFilters& filters, std::string_view line);
// An administrator's line when it is one of the words above (or `extra`'s spellings, a game's own
// words for them: add, remove, list): true, its answer in `out`.
bool cmdfilter_command(CmdFilters& filters, std::string_view line, std::string& out,
                       const char* const (*extra)[3] = nullptr);
// The log's line for a refused command: `<name>[<address>]: issued banned command: <command>`.
std::string cmdfilter_log_line(std::string_view name, std::string_view address,
                               std::string_view line);
// The action's name ("ignore", "log", ...).
const char* filter_do_name(FilterDo action);

}  // namespace ge::server
