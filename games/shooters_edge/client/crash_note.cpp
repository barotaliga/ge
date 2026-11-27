// The window's last words (crash_note.hpp).
//
// ge:summary An uncaught exception's line on stderr and in crash.txt in the settings folder
// ge:keywords crash crash-txt uncaught-exception say_crash
// ge:skill platform shooter
#include "shooters_edge/client/crash_note.hpp"

#include <cstdio>
#include <ctime>
#include <filesystem>
#include <fstream>

#include "platform/window.hpp"

namespace shooters_edge::client {

void say_crash(const std::string& what) {
  const std::string line =
      "shooters_edge: stopped by " + what +
      ". This is a bug: run the same command under lldb or gdb with a breakpoint on __cxa_throw "
      "to see where it was thrown.";
  std::fprintf(stderr, "%s\n", line.c_str());
  const std::string prefs = ge::platform::prefs_dir("gameengine2", "shooters_edge");
  if (prefs.empty()) return;
  const std::filesystem::path at = std::filesystem::path(prefs) / "crash.txt";
  std::ofstream out(at, std::ios::app);
  if (!out) return;
  const std::time_t now = std::time(nullptr);
  char when[32] = {};
  std::strftime(when, sizeof when, "%Y-%m-%d %H:%M:%S", std::localtime(&now));  // NOLINT
  out << when << " " << line << "\n";
  std::fprintf(stderr, "shooters_edge: the line above is kept in %s\n", at.string().c_str());
}

}  // namespace shooters_edge::client
