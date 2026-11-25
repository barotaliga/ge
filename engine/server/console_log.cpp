// ge:summary The dedicated server's console log: every terminal line into logs/<name>.log
// ge:keywords console-log logfile logfile_flush con_logfile
// ge:skill net console
// ge:test tests/server_console_log_test.cpp
#include "server/console_log.hpp"

#include <filesystem>
#include <string>
#include <system_error>

#include "server/macros.hpp"

namespace ge::server {

ConsoleLog::~ConsoleLog() {
  if (file != nullptr) std::fclose(file);
}

void console_log_close(ConsoleLog& log) {
  if (log.file == nullptr) return;
  std::fputs("Closing console log.\n", log.file);
  std::fclose(log.file);
  log.file = nullptr;
  log.path.clear();
}

void console_log_update(ConsoleLog& log, int mode, std::string_view folder, std::string_view name,
                        int flush, std::string& said) {
  if (mode == log.mode && flush == log.flush && name == log.name && folder == log.folder) return;
  if (log.file != nullptr) said += "Closing console log.\n";
  console_log_close(log);
  log.mode = mode;
  log.flush = flush;
  log.name = name;
  log.folder = folder;
  if (mode <= 0) return;
  const std::filesystem::path dir = std::filesystem::path(folder) / "logs";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  const std::string path = (dir / (std::string(name) + ".log")).generic_string();
  log.file = std::fopen(path.c_str(), mode == 1 ? "w" : "a");
  if (log.file == nullptr) {
    said += "Couldn't open " + path + " for logging.\n";
    return;
  }
  if (flush >= 2) std::setvbuf(log.file, nullptr, _IONBF, 0);  // 1: flushed after each line
  log.path = path;
  said += "Logging console to " + path + "\n";
}

void console_log_line(ConsoleLog& log, std::string_view prefix, std::string_view line,
                      const std::tm& now) {
  if (log.file == nullptr) return;
  std::string text = terminal_prefix(prefix, LineLevel::normal, now);
  text.append(line);
  text.push_back('\n');
  std::fwrite(text.data(), 1, text.size(), log.file);
  if (log.flush == 1) std::fflush(log.file);
}

}  // namespace ge::server
