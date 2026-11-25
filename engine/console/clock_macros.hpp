// THE CLOCK'S MACROS: `$com_date` and `$com_time` (today's date and the time of day by
// `com_date_format`, default `%Y-%m-%d`, and `com_time_format`, default `%H:%M`, the C library's
// date letters), `$com_uptime` (how long the program has run: `MM.SS` under an hour, `H:MM.SS`
// under a day, else `D+H:MM.SS`), `$com_uptime_long` (the same in words: "1 day, 2 hours, 5 secs",
// each part only when not zero, "0 secs" at zero), `$random` (a digit 0..9), and the older
// clients' fixed forms `$time` (14:05), `$date` (22.11.26), `$timestamp` (20261122-1405),
// `$dateiso` (2026-11-22_14-05) and `$rand` (0 to 1, six decimals). The console reads
// no clock: the host hands over its own (the wall clock in a window, a fixed one in a test), and
// its uptime becomes the console's clock, the one `sleep` counts on (console.hpp).
//
// ge:summary The clock's macros: $com_date, $com_time, $com_uptime, $com_uptime_long, $random
// ge:keywords macros com_date com_time com_date_format com_time_format com_uptime uptime
// ge:keywords com_uptime_long random clock time-of-day level_clock tenths_clock
// ge:skill console
// ge:test tests/console_more_test.cpp
#pragma once

#include <ctime>
#include <functional>
#include <string>

#include "console/console.hpp"

namespace ge::console {

struct Clock {
  std::function<std::tm()> local;    // the local date and time now
  std::function<double()> uptime_s;  // seconds since the program started
  uint32_t seed = 1;                 // $random's stream
};

// The macros and the two format cvars on `con`.
void add_clock_macros(Console& con, Clock clock);

// How long `seconds` is in the two uptime forms.
std::string uptime_short(double seconds);
std::string uptime_long(double seconds);
// A level's time as a macro says it (`$cl_timer`): `M:SS` under an hour, else `H:M:SS`, the
// minutes never padded ("4:05", "1:4:05").
std::string level_clock(double seconds);
// A position to the tenth (`$cl_demopos`): `M:SS.T` ("3:07.4"), the minutes running past 59.
std::string tenths_clock(double seconds);

}  // namespace ge::console
