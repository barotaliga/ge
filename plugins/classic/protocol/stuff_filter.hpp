// WHAT A SERVER MAY RUN ON THE CLIENT: a classic server sends console lines for the client to run
// (stuffed text). With `cl_ignore_stufftext` the player limits them: 0 every line runs; 1 only the
// lines the whitelist file names run, the others dropped silently; 2 or more the same, each line
// dropped said as "Ignored stufftext: <line>".
//
// THE WHITELIST is a text file of patterns, one a line (blank lines skipped, a carriage return
// dropped, a line of 1024 characters or more dropped): `*` any run of characters, `?` one, `\` the
// next character as itself; case counts and the WHOLE line must match. A line is matched as the
// server sent it, before any `$name` in it is expanded, one command at a time (the text split at
// line breaks and `;`).
//
// ALWAYS RUN, whatever the setting: the signon's own steps (`changing`, `precache`, `reconnect`),
// and `cmd` while the whitelist is empty (once it names anything, `cmd` must be named too).
//
// ge:summary Which stuffed console lines a classic client runs: cl_ignore_stufftext and a whitelist
// ge:keywords stufftext stuffed-text cl_ignore_stufftext whitelist stufftext-whitelist wildcard
// ge:keywords server-commands ignored-stufftext
// ge:skill classic net
// ge:test tests/classic_stuff_filter_test.cpp
#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace ge::classic {

struct StuffFilter {
  int level = 0;                      // cl_ignore_stufftext
  std::vector<std::string> patterns;  // the whitelist's lines
};

// Whether `pattern` (`*`, `?`, `\` as above) matches the whole of `text`.
bool stuff_pattern_match(std::string_view pattern, std::string_view text);

// The whitelist file's text as patterns (the rules above).
std::vector<std::string> read_stuff_whitelist(std::string_view text);

// Whether `line` (one command, as sent) may run under `f`.
bool stuff_allowed(const StuffFilter& f, std::string_view line);

// The line said for a dropped command at `f.level` ("" when nothing is said: level 1).
std::string stuff_ignored_line(const StuffFilter& f, std::string_view line);

}  // namespace ge::classic
