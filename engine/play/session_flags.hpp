// THE FLAGS EVERY GAME TAKES, whatever else its command line says: a session's own config and its
// log file. A game window (any game's) hands its argv here first; these are taken out and the
// game parses the rest as before.
//
//   -config <file>   (--config=<file>)   the console's config.cfg is this file: read at start,
//                                         every save written there, autoexec.cfg skipped, the
//                                         player's own settings untouched
//                                         (Console::set_config_file)
//   -logfile <file>  (--logfile=<file>)  every output line appended to it too, flushed as written
//                                         (window_output.hpp), beside the terminal
//
// For runs that must not touch a player's settings (a measurement, a test, an agent at the keys)
// and whose output is read from a file, not a terminal. `~` is the home folder.
//
// ge:summary The flags every game takes: -config (a session's own config) and -logfile (a log file)
// ge:keywords session-flags config logfile log-file command-line set_config_file window-output
// ge:skill agent-interface console
// ge:test tests/play_session_flags_test.cpp
#pragma once

#include <string>

namespace ge::console {
class Console;
}

namespace ge::play {

struct SessionFlags {
  std::string config;   // the session's own config.cfg, or empty: the player's
  std::string logfile;  // the log file, or empty: the terminal alone
};

// Takes the session flags out of argv (argc shrinks; the rest keep their order). False with the
// reason when one lacks its file.
bool take_session_flags(int& argc, char** argv, SessionFlags& out, std::string& error);

// The output attached (the terminal, and the log file when named) and the console's config file
// set (when a console is given and a config named). False with the reason when the log file
// cannot be opened (the terminal attached anyway).
bool apply_session_flags(const SessionFlags& flags, console::Console* con, std::string& error);

}  // namespace ge::play
