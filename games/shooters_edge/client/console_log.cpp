// Quake II's console log (console_log.hpp).
//
// ge:summary Quake II's console log: the cvars, the log kept to them, each printed line written
// ge:keywords q2 logfile logfile_flush logfile_name logfile_prefix console-log
// ge:skill console shooter
// ge:test tests/shooters_edge_console_log_test.cpp
#include "shooters_edge/client/console_log.hpp"

namespace shooters_edge::client {

namespace con = ge::console;

void add_console_log_cvars(con::Cvars& v, ConsoleLog& log) {
  v.bind({.name = "logfile",
          .fallback = "0",
          .flags = con::kArchive,
          .help = "Whether everything the console prints is kept in a log file.",
          .kind = con::CvarKind::integer,
          .min = 0,
          .max = 9,
          .values = "0: no log (the default); 1: kept in logs/<logfile_name>.log, a new file each "
                    "time it opens; 2 or more: added to the end of the file"},
         &log.mode);
  v.bind({.name = "logfile_flush",
          .fallback = "0",
          .flags = con::kArchive,
          .help = "How soon each line of the log file reaches the disk.",
          .kind = con::CvarKind::integer,
          .min = 0,
          .max = 9,
          .values = "0: now and then (the default); 1: each line at once; 2 or more: nothing held "
                    "back at all"},
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
          .help = "What is written before each line of the log file.",
          .kind = con::CvarKind::text,
          .values = "TEXT: a time pattern, %Y-%m-%d the date and %H:%M:%S the time, <?> the "
                    "line's kind and @ the seconds ([%Y-%m-%d %H:%M] the default); \"\": none"},
         &log.prefix);
}

void console_log_line(ConsoleLog& log, const std::filesystem::path& dir, std::string_view line,
                      std::time_t now, const std::function<void(std::string_view)>& show) {
  std::tm parts{};
#if defined(_WIN32)
  localtime_s(&parts, &now);
#else
  localtime_r(&now, &parts);
#endif
  std::string said;
  ge::server::console_log_update(*log.file, log.name.empty() ? 0 : log.mode, dir.string(), log.name,
                                 log.flush, said);
  for (size_t at = 0; at < said.size();) {
    const size_t end = said.find('\n', at);
    const std::string_view one(said.data() + at,
                               (end == std::string::npos ? said.size() : end) - at);
    if (show) show(one);
    if (one != "Closing console log.")  // the closed file had it; the new one starts with its own
      ge::server::console_log_line(*log.file, log.prefix, one, parts);
    at = end == std::string::npos ? said.size() : end + 1;
  }
  ge::server::console_log_line(*log.file, log.prefix, line, parts);
}

}  // namespace shooters_edge::client
