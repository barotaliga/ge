// Quake II's console log (client/console_log.hpp): lines kept with their time while logfile is on,
// 1 a new file and 2 added to, a change closing and opening it again, nothing while it is off.
//
// ge:summary Tests Quake II's logfile, logfile_name and logfile_prefix on the console's lines
// ge:keywords q2 logfile logfile_flush logfile_name logfile_prefix console-log
// ge:skill console shooter
#include "shooters_edge/client/console_log.hpp"

#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "check.hpp"
#include "console/console.hpp"

namespace cl = shooters_edge::client;
namespace fs = std::filesystem;

namespace {

std::string read(const fs::path& p) {
  std::ifstream in(p, std::ios::binary);
  std::stringstream s;
  s << in.rdbuf();
  return s.str();
}

}  // namespace

TEST(lines_are_kept_with_their_time_while_logfile_is_on) {
  const fs::path dir = fs::temp_directory_path() / "se_console_log_test";
  fs::remove_all(dir);
  ge::console::Console c;
  cl::ConsoleLog log;
  cl::add_console_log_cvars(c.cvars(), log);
  CHECK(log.mode == 0 && log.flush == 0 && log.name == "console" &&
        log.prefix == "[%Y-%m-%d %H:%M] ");
  std::tm t{};
  t.tm_year = 126;
  t.tm_mon = 10;
  t.tm_mday = 21;
  t.tm_hour = 20;
  t.tm_min = 5;
  t.tm_isdst = -1;
  const std::time_t now = std::mktime(&t);
  std::vector<std::string> shown;
  const auto show = [&shown](std::string_view l) { shown.emplace_back(l); };
  const fs::path file = dir / "logs" / "console.log";
  cl::console_log_line(log, dir, "not kept", now, show);
  CHECK(!fs::exists(file) && shown.empty());
  // 1: a new file each time it opens; the opening said on the screen and in the file.
  c.execute("logfile 1; logfile_flush 1");
  cl::console_log_line(log, dir, "Quake II's console", now, show);
  const std::string opened =
      "Logging console to " + (dir / "logs" / "console.log").generic_string();
  CHECK(shown.size() == 1 && shown[0] == opened);
  CHECK(read(file) == "[2026-11-21 20:05] " + opened + "\n[2026-11-21 20:05] Quake II's console\n");
  // 2: the same file added to; the change closes and opens it again.
  shown.clear();
  c.execute("logfile 2; logfile_prefix \"\"");
  cl::console_log_line(log, dir, "added", now, show);
  CHECK(shown.size() == 2 && shown[0] == "Closing console log." && shown[1] == opened);
  CHECK(read(file) == "[2026-11-21 20:05] " + opened +
                          "\n[2026-11-21 20:05] Quake II's console\nClosing console log.\n" +
                          opened + "\nadded\n");
  // 1 again: a new file.
  c.execute("logfile 1");
  cl::console_log_line(log, dir, "fresh", now, show);
  CHECK(read(file) == opened + "\nfresh\n");
  // Off: closed with its last line, nothing more kept.
  c.execute("logfile 0");
  cl::console_log_line(log, dir, "closed", now, show);
  CHECK(log.file->file == nullptr && read(file) == opened + "\nfresh\nClosing console log.\n");
  fs::remove_all(dir);
}
