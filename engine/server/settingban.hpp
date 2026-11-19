// SETTING BANS AND USER INFO BANS (`cvarban_add`, `userinfoban_add`), the same rule for every
// game's server that can ask its clients (docs/servers.md § The settings): a client whose console
// setting, or whose user info key, has a value the administrator named gets the entry's action.
//
// - ASKING A CLIENT: the server stuffs one console line per setting ban (`setting_ban_query`):
//   `cmd <DEL>c <name> $<name>`. A client's console expands `$<name>` to the setting's value (empty
//   when it has none) and sends the line back as a client command, `<DEL>c <name> <value>`
//   (`kSettingAnswer`); the DEL byte keeps a player from typing it by accident. Nothing is waited
//   for: a client that never answers is never judged. A game asks at every `new` (connect and each
//   map) and on the administrator's `cvarban_check`.
// - USER INFO is the server's already: judged at connect (only `kick` entries, refusing the
//   connect), at every `new` and at every change of it, every key against every entry.
// - THE MATCH (`setting_ban_matches`): a leading `!` turns it round; `*` any value but empty; `=N`,
//   `<N`, `>N` compare as numbers (an empty or wordy value is 0); `~text` contains it; `#text` is
//   exactly the rest, whatever it starts with; anything else is the exact value. Text compares
//   ignore case.
// - THE WORDS: `cvarban_add <setting> <match> [log|print|stuff|kick] [text]` (log when none is
//   given; the text is what print shows, stuff runs or kick tells, a note for log),
//   `cvarban_remove <number|setting|all>`, `cvarban_list`, `cvarban_check`; and
//   `userinfoban_add`, `userinfoban_remove`, `userinfoban_list` the same over user info keys. An
//   entry the same as one listed (name, match and action) is refused.
// - THE ACTIONS (the game does them): `log` writes `setting_ban_log_line`; `print` shows the client
//   the text; `stuff` runs it on the client's console; `kick` writes the log line, shows the text
//   and drops the client. Every entry that matches acts, in the order added, until a kick.
//
// ge:summary cvarban_add, userinfoban_add: clients judged by a setting's or user info's value
// ge:keywords cvarban cvarban_add cvarban_remove cvarban_list cvarban_check userinfoban
// ge:keywords userinfoban_add userinfoban_remove userinfoban_list setting-ban userinfo-ban
// ge:keywords setting_ban_matches setting_ban_query setting_ban_command client-setting-check
// ge:skill net
// ge:test tests/server_settingban_test.cpp
#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "server/cmdfilter.hpp"

namespace ge::server {

struct SettingBan {
  std::string name;   // the setting or user info key, as written
  std::string match;  // the match, as written
  FilterDo action = FilterDo::log;
  std::string text;  // print's words, stuff's line, kick's words; log's note
};

struct SettingBans {
  std::vector<SettingBan> entries;
};

// The first word of a client's answer to `setting_ban_query`.
inline constexpr std::string_view kSettingAnswer =
    "\x7f"
    "c";

// Whether `value` matches `match` (the forms above).
bool setting_ban_matches(std::string_view match, std::string_view value);
// The console line that asks a client for `name`'s value.
std::string setting_ban_query(std::string_view name);
// The entries that act on `name` = `value`, in order, the first kick the last.
std::vector<const SettingBan*> setting_ban_hits(const SettingBans& bans, std::string_view name,
                                                std::string_view value);
// An administrator's line when it is one of `kind`'s words (`kind` "cvarban" or "userinfoban";
// `extra` a game's own spellings of add, remove and list): true, its answer in `out`.
// `cvarban_check` is the game's to answer (it needs its clients): false here.
bool setting_ban_command(SettingBans& bans, std::string_view kind, std::string_view line,
                         std::string& out, const char* const (*extra)[3] = nullptr);
// The log's line: `<name>[<address>]: matched cvarban: "<setting>" is "<value>"`.
std::string setting_ban_log_line(std::string_view player, std::string_view address,
                                 std::string_view name, std::string_view value);

}  // namespace ge::server
