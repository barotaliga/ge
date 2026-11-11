// The flags every game takes: -config and -logfile (session_flags.hpp).
//
// ge:summary The session flags taken off a command line and applied: the config file, the log file
// ge:keywords session-flags config logfile command-line
// ge:skill agent-interface console
// ge:test tests/play_session_flags_test.cpp
#include "play/session_flags.hpp"

#include <cstdlib>
#include <string_view>

#include "console/console.hpp"
#include "play/window_output.hpp"

namespace ge::play {

namespace {

std::string with_home(std::string p) {
  if (!p.empty() && p[0] == '~') {
    const char* home = std::getenv("HOME");
    if (home != nullptr) p = std::string(home) + p.substr(1);
  }
  return p;
}

}  // namespace

bool take_session_flags(int& argc, char** argv, SessionFlags& out, std::string& error) {
  int kept = 1;
  for (int i = 1; i < argc; i++) {
    const std::string_view arg = argv[i];
    std::string* to = nullptr;
    std::string_view value;
    bool has_value = false;
    for (const auto& [name, field] : {std::pair{std::string_view("config"), &out.config},
                                      std::pair{std::string_view("logfile"), &out.logfile}}) {
      const std::string dash = "-" + std::string(name), eq = "--" + std::string(name) + "=";
      if (arg == dash || arg == "-" + dash) {
        to = field;
        if (i + 1 < argc) {
          value = argv[++i];
          has_value = true;
        }
      } else if (arg.rfind(eq, 0) == 0) {
        to = field;
        value = arg.substr(eq.size());
        has_value = true;
      }
    }
    if (to == nullptr) {
      argv[kept++] = argv[i];
      continue;
    }
    if (!has_value || value.empty()) {
      error = std::string("the command line's ") + std::string(arg) + " names no file";
      return false;
    }
    *to = with_home(std::string(value));
  }
  argc = kept;
  return true;
}

bool apply_session_flags(const SessionFlags& flags, console::Console* con, std::string& error) {
  const bool ok = attach_window_output(flags.logfile, error);
  if (con != nullptr && !flags.config.empty()) con->set_config_file(flags.config);
  return ok;
}

}  // namespace ge::play
