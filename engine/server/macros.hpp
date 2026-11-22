// A DEDICATED SERVER'S MACROS AND THEIR LIST, for an operator's scripts and messages:
// - `macrolist [filter]`: each `$` macro, its name in a 16-column field and what it gives now (cut
//   to 63 characters), then "<shown> of <all> macros". The filter is a wildcard on the whole name
//   (`*` any run, `?` one character, `\` the next taken as it is), case kept.
// - `$com_maplist`: the maps the game finds, by name ignoring case, each once, joined by single
//   spaces (the game's `ServerGame::maps`).
// - `console_prefix`: the text put before each line the terminal prints. A leading `<?>` has its
//   `?` made the line's syslog level (6 a normal line, 4 a warning, 3 an error) and the first `@`
//   its letter (A, W, E), for a service manager's journal; `%` runs through the C library's time
//   pattern on the local time (`%H:%M`).
// The clock's macros ($com_date, $com_time, $com_uptime, $com_uptime_long, $random) are the
// console's own (console/clock_macros.hpp); the process adds them with these (main.cpp).
//
// ge:summary A dedicated server's macro list and $com_maplist
// ge:keywords macrolist macros com_maplist wildcard maps dedicated-server server_macro_list
// ge:keywords server_map_list console_prefix terminal-prefix syslog-level
// ge:skill net console
// ge:test tests/server_macros_test.cpp
#pragma once

#include <cstdint>
#include <ctime>
#include <string>
#include <string_view>
#include <vector>

#include "console/console.hpp"

namespace ge::server {

// `macrolist`'s text for the macros on `console` matching `filter` ("" or none: every one).
std::string server_macro_list(const console::Console& console, std::string_view filter);
// `$com_maplist`'s value: `maps` sorted ignoring case, a name met twice (ignoring case) once.
std::string server_map_list(std::vector<std::string> maps);
// A terminal line's level, for `console_prefix`'s `<?>` and `@`.
enum class LineLevel : uint8_t { normal, warning, error };
// The text `console_prefix` (`prefix`) puts before a terminal line of `level`, at the local time
// `now` ("" when the prefix is empty).
std::string terminal_prefix(std::string_view prefix, LineLevel level, const std::tm& now);
// `text` matches the wildcard `pattern` whole (`*`, `?`, `\`), case kept.
bool wildcard_match(std::string_view pattern, std::string_view text);

}  // namespace ge::server
