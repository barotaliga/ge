// A WINDOW'S OUTPUT: where a game window's lines go (every line of the one output channel,
// core/output.hpp: the console's `console: ...` lines, status, warnings, errors). On the
// terminal as always (errors on stderr, the rest on stdout), and, when the player or an agent
// names a LOG FILE (a window's `-logfile <file>`), appended to it too, each line as a dump writes
// it ("[step] level channel: message", `format_line`) and flushed as it is written, so a run's
// output is read from a file (a measurement's `framestats`, a crash's last lines) and never
// fished out of a terminal. The file is opened for appending; its folder is made.
//
// MODULE STATE: the open file lives in window_output.cpp, one per process, as the sink does.
//
// ge:summary A game window's output: the terminal, and a log file when one is named (-logfile)
// ge:keywords log-file logfile window output terminal sink console-output flush append
// ge:skill agent-interface debug
// ge:test tests/play_window_output_test.cpp
#pragma once

#include <string>

namespace ge::play {

// The window's sink attached (core/output.hpp `set_output_sink`): the terminal, and `file`
// when it is not empty. False with the reason (the terminal attached anyway) when the file
// cannot be opened for appending.
bool attach_window_output(const std::string& file, std::string& error);

// The log file's path, or empty when none is open.
const std::string& window_log_file();

}  // namespace ge::play
