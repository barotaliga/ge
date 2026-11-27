// THE WINDOW'S LAST WORDS: the line an uncaught exception ends the window with, written to stderr
// and kept in `crash.txt` in the game's settings folder (with the time), because a window started
// from a desktop has no terminal to read it in.
//
// ge:summary An uncaught exception's line on stderr and in crash.txt in the settings folder
// ge:keywords crash crash-txt uncaught-exception say_crash exception-text settings-folder
// ge:skill platform shooter
#pragma once

#include <string>

namespace shooters_edge::client {

// `what` ("an uncaught exception: <its text>") said as the window's last line and kept.
void say_crash(const std::string& what);

}  // namespace shooters_edge::client
