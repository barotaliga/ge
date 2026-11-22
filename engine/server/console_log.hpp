// THE CONSOLE LOG: every line the dedicated server's terminal prints, also written to a file
// (Quake II's `logfile`, `logfile_name`, `logfile_prefix` and `logfile_flush` are its spellings):
// - `con_logfile`: 0 no log (the default); 1 a new file at each opening; 2 or more added to the
// end.
// - `con_logfile_name` ("console"): the file is `logs/<name>.log` in the game's folder.
// - `con_logfile_prefix` ("[%Y-%m-%d %H:%M] "): put before each line as `console_prefix` is
//   (macros.hpp: `<?>`, `@`, the local time's `%` patterns).
// - `con_logfile_flush`: 0 the C library's buffering (the default); 1 each line written at once;
//   2 or more nothing held at all.
// A change to any of them while the log is open closes it ("Closing console log.") and opens it
// again ("Logging console to <path>"); a file that cannot be opened says so and the log stays off.
// main.cpp checks the settings each frame and its terminal's sink writes the lines.
//
// ge:summary The dedicated server's console log: every terminal line into logs/<name>.log
// ge:keywords console-log logfile logfile_name logfile_prefix logfile_flush con_logfile
// ge:keywords con_logfile_name con_logfile_prefix con_logfile_flush log-file buffering
// ge:skill net console
// ge:test tests/server_console_log_test.cpp
#pragma once

#include <cstdio>
#include <ctime>
#include <string>
#include <string_view>

namespace ge::server {

struct ConsoleLog {
  std::FILE* file = nullptr;
  std::string path;  // the open file's path
  // The settings the file was opened with (the next check compares).
  int mode = 0;
  int flush = 0;
  std::string name;
  std::string folder;

  ConsoleLog() = default;
  ConsoleLog(const ConsoleLog&) = delete;
  ConsoleLog& operator=(const ConsoleLog&) = delete;
  ~ConsoleLog();
};

// The log brought to the settings: closed, opened or both when any differs from what it was opened
// with. Each line to print (ending in a newline) is added to `said`; "Closing console log." is
// written into the file before it closes.
void console_log_update(ConsoleLog& log, int mode, std::string_view folder, std::string_view name,
                        int flush, std::string& said);
// One printed line written with `prefix` (terminal_prefix's rules) before it, when the log is open.
void console_log_line(ConsoleLog& log, std::string_view prefix, std::string_view line,
                      const std::tm& now);
// The log closed with its last line written.
void console_log_close(ConsoleLog& log);

}  // namespace ge::server
