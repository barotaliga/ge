// QUAKE II'S CONSOLE LOG IN THE WINDOW: every line the console prints copied to a text file while
// `logfile` is on, in the folder the settings are saved in (the console's first search directory),
// as `logs/<logfile_name>.log`. The file is the dedicated server's console log (server/
// console_log.hpp), so the names mean in the window what they mean on Quake II's server:
// - `logfile`: 0 no log (the default); 1 a new file each time it opens; 2 or more added to the end.
// - `logfile_flush`: 0 the C library's buffering (the default); 1 each line written at once; 2 or
//   more nothing held at all.
// - `logfile_name` ("console") and `logfile_prefix` ("[%Y-%m-%d %H:%M] ", put before each line
//   as `console_prefix` is: `<?>`, `@` and the local time's `%` patterns; "" none).
// The settings are checked as each line is printed: a change closes the file ("Closing console
// log.") and opens it again ("Logging console to <path>"), those lines shown before the line.
// Registered in Quake II's layer (client/cvar_layers.cpp), so another game's window never writes
// it.
//
// ge:summary Quake II's console log in the window: the server's log under the logfile settings
// ge:keywords q2 logfile logfile_flush logfile_name logfile_prefix console-log log-file
// ge:skill console shooter
// ge:test tests/shooters_edge_console_log_test.cpp
#pragma once

#include <ctime>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <string_view>

#include "console/console.hpp"
#include "server/console_log.hpp"

namespace shooters_edge::client {

struct ConsoleLog {
  int mode = 0;                              // logfile
  int flush = 0;                             // logfile_flush
  std::string name = "console";              // logfile_name: logs/<name>.log
  std::string prefix = "[%Y-%m-%d %H:%M] ";  // logfile_prefix
  std::shared_ptr<ge::server::ConsoleLog> file = std::make_shared<ge::server::ConsoleLog>();
};

// logfile, logfile_flush, logfile_name and logfile_prefix on `v`, bound to `log`.
void add_console_log_cvars(ge::console::Cvars& v, ConsoleLog& log);
// The log brought to its settings under `dir` (each line that says so given to `show`, and written
// to the file when one is open), then `line` written while it is open, stamped with `now`.
void console_log_line(ConsoleLog& log, const std::filesystem::path& dir, std::string_view line,
                      std::time_t now, const std::function<void(std::string_view)>& show = {});

}  // namespace shooters_edge::client
