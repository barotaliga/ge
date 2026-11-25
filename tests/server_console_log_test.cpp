// The dedicated server's console log (engine/server/console_log.hpp): opened at logs/<name>.log,
// written with its prefix, a new file or added to, reopened on a change with its two lines, off.
//
// ge:summary Tests the console log: the file, the prefix, the modes, the reopening
// ge:keywords test console-log logfile logfile_flush con_logfile
// ge:skill net console
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include "check.hpp"
#include "server/console_log.hpp"

namespace gs = ge::server;
namespace fs = std::filesystem;

namespace {

std::string slurp(const fs::path& p) {
  std::ifstream f(p);
  std::stringstream s;
  s << f.rdbuf();
  return s.str();
}

}  // namespace

TEST(the_console_log_writes_every_line_and_reopens_on_a_change) {
  const fs::path root = fs::temp_directory_path() / "ge_server_console_log_test";
  fs::remove_all(root);
  const fs::path file = root / "logs" / "console.log";
  std::tm now{};
  now.tm_year = 126;
  now.tm_mon = 10;
  now.tm_mday = 22;
  now.tm_hour = 9;
  now.tm_min = 5;
  {
    gs::ConsoleLog log;
    std::string said;
    gs::console_log_update(log, 0, root.string(), "console", 0, said);
    CHECK(said.empty() && log.file == nullptr);
    gs::console_log_line(log, "", "nothing", now);  // off: nothing written
    gs::console_log_update(log, 1, root.string(), "console", 1, said);
    CHECK(said == "Logging console to " + file.generic_string() + "\n");
    gs::console_log_line(log, "[%Y-%m-%d %H:%M] ", "first", now);
    CHECK(slurp(file) == "[2026-11-22 09:05] first\n");  // flushed at the line
    said.clear();
    gs::console_log_update(log, 1, root.string(), "console", 1, said);
    CHECK(said.empty());  // nothing changed
    gs::console_log_update(log, 2, root.string(), "console", 2, said);
    CHECK(said == "Closing console log.\nLogging console to " + file.generic_string() + "\n");
    gs::console_log_line(log, "", "second", now);
    CHECK(slurp(file) == "[2026-11-22 09:05] first\nClosing console log.\nsecond\n");
    said.clear();
    gs::console_log_update(log, 1, root.string(), "console", 2, said);  // a new file
    gs::console_log_line(log, "", "third", now);
    CHECK(slurp(file) == "third\n");
    said.clear();
    gs::console_log_update(log, 0, root.string(), "console", 2, said);
    CHECK(said == "Closing console log.\n" && log.file == nullptr);
    CHECK(slurp(file) == "third\nClosing console log.\n");
  }
  fs::remove_all(root);
}
