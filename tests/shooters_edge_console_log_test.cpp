// Quake II's console log (client/console_log.hpp): lines appended with their time while logfile is
// on, nothing while it is off, another name another file.
//
// ge:summary Tests Quake II's logfile, logfile_name and logfile_prefix on the console's lines
// ge:keywords q2 logfile logfile_flush logfile_name logfile_prefix console-log
// ge:skill console shooter
#include "shooters_edge/client/console_log.hpp"

#include <fstream>
#include <sstream>
#include <string>

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
  CHECK(log.mode == 0 && log.name == "console" && log.prefix == "[%Y-%m-%d %H:%M] ");
  std::tm t{};
  t.tm_year = 126;
  t.tm_mon = 10;
  t.tm_mday = 21;
  t.tm_hour = 20;
  t.tm_min = 5;
  t.tm_isdst = -1;
  const std::time_t now = std::mktime(&t);
  CHECK(cl::console_log_line(log, dir, "not kept", now));
  CHECK(!fs::exists(dir / "logs" / "console.log"));
  c.execute("logfile 2");
  CHECK(cl::console_log_line(log, dir, "Quake II's console", now));
  CHECK(read(dir / "logs" / "console.log") == "[2026-11-21 20:05] Quake II's console\n");
  c.execute("logfile_prefix \"\"; logfile_name match");
  CHECK(cl::console_log_line(log, dir, "a fresh file", now));
  CHECK(read(dir / "logs" / "match.log") == "a fresh file\n");
  c.execute("logfile 0");
  CHECK(cl::console_log_line(log, dir, "closed", now));
  CHECK(!log.file && read(dir / "logs" / "match.log") == "a fresh file\n");
  fs::remove_all(dir);
}
