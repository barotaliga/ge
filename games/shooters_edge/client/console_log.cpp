// Quake II's console log (console_log.hpp).
//
// ge:summary Quake II's console log: the cvars and each printed line appended with its time
// ge:keywords q2 logfile logfile_flush logfile_name logfile_prefix console-log
// ge:skill console shooter
// ge:test tests/shooters_edge_console_log_test.cpp
#include "shooters_edge/client/console_log.hpp"

#include <system_error>

namespace shooters_edge::client {

namespace con = ge::console;

void add_console_log_cvars(con::Cvars& v, ConsoleLog& log) {
  v.bind({.name = "logfile",
          .fallback = "0",
          .flags = con::kArchive,
          .help = "Whether everything the console prints is kept in a log file.",
          .kind = con::CvarKind::integer,
          .min = 0,
          .max = 2,
          .values = "0: no log (the default); 1: kept in logs/<logfile_name>.log; 2: kept, each "
                    "line written at once"},
         &log.mode);
  v.bind({.name = "logfile_flush",
          .fallback = "0",
          .flags = con::kArchive,
          .help = "Whether each line of the log file is written at once.",
          .kind = con::CvarKind::boolean,
          .values = "0: now and then (the default); 1: each line at once"},
         &log.flush);
  v.bind({.name = "logfile_name",
          .fallback = "console",
          .flags = con::kArchive,
          .help = "The log file's name, in the logs folder beside your settings.",
          .kind = con::CvarKind::text,
          .values = "TEXT: the name, .log added (console the default)"},
         &log.name);
  v.bind({.name = "logfile_prefix",
          .fallback = "[%Y-%m-%d %H:%M] ",
          .flags = con::kArchive,
          .help = "The time written before each line of the log file.",
          .kind = con::CvarKind::text,
          .values = "TEXT: a time pattern, %Y-%m-%d the date and %H:%M:%S the time "
                    "([%Y-%m-%d %H:%M] the default); \"\": none"},
         &log.prefix);
}

bool console_log_line(ConsoleLog& log, const std::filesystem::path& dir, std::string_view line,
                      std::time_t now) {
  if (log.mode <= 0 || log.name.empty()) {
    log.file.reset();
    log.open_path.clear();
    return true;
  }
  const std::filesystem::path path = dir / "logs" / (log.name + ".log");
  if (!log.file || path != log.open_path) {
    log.file.reset();
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    std::FILE* f = std::fopen(path.string().c_str(), "ab");
    if (f == nullptr) return false;
    log.file = std::shared_ptr<std::FILE>(f, [](std::FILE* p) { std::fclose(p); });
    log.open_path = path;
  }
  char stamp[128] = {};
  if (!log.prefix.empty()) {
    std::tm parts{};
#if defined(_WIN32)
    localtime_s(&parts, &now);
#else
    localtime_r(&now, &parts);
#endif
    if (std::strftime(stamp, sizeof stamp, log.prefix.c_str(), &parts) == 0) stamp[0] = 0;
  }
  std::fputs(stamp, log.file.get());
  std::fwrite(line.data(), 1, line.size(), log.file.get());
  std::fputc('\n', log.file.get());
  if (log.mode >= 2 || log.flush) std::fflush(log.file.get());
  return true;
}

}  // namespace shooters_edge::client
