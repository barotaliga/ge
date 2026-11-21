// QUAKE II'S CONSOLE LOG: every line the console prints copied to a text file while `logfile` is
// on, in the folder the settings are saved in (the console's first search directory), as
// `logs/<logfile_name>.log`, appended to. `logfile` 1 writes as the lines come and lets the file
// catch up now and then, 2 (or `logfile_flush` 1) puts each line on the disk at once; 0 closes it.
// Each line begins with `logfile_prefix`, a time pattern (`%Y-%m-%d`, `%H:%M` and the rest as the
// C library spells them; "" none). Changing the name or turning it off and on opens the file again.
// Registered in Quake II's layer (client/cvar_layers.cpp), so another game's window never writes
// it.
//
// ge:summary Quake II's console log: printed lines appended to logs/<name>.log with a time prefix
// ge:keywords q2 logfile logfile_flush logfile_name logfile_prefix console-log log-file
// ge:skill console shooter
// ge:test tests/shooters_edge_console_log_test.cpp
#pragma once

#include <cstdio>
#include <ctime>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>

#include "console/console.hpp"

namespace shooters_edge::client {

struct ConsoleLog {
  int mode = 0;                              // logfile: 0 off, 1 on, 2 on and each line flushed
  bool flush = false;                        // logfile_flush: each line flushed
  std::string name = "console";              // logfile_name: logs/<name>.log
  std::string prefix = "[%Y-%m-%d %H:%M] ";  // logfile_prefix: each line's time
  // The file open and where it is (another name opens another).
  std::shared_ptr<std::FILE> file;
  std::filesystem::path open_path;
};

// logfile, logfile_flush, logfile_name and logfile_prefix on `v`, bound to `log`.
void add_console_log_cvars(ge::console::Cvars& v, ConsoleLog& log);
// `line` appended under `dir` while the log is on (`now` stamps it); the file closed while it is
// off. False when the file could not be opened (nothing written).
bool console_log_line(ConsoleLog& log, const std::filesystem::path& dir, std::string_view line,
                      std::time_t now);

}  // namespace shooters_edge::client
