// A game window's output: the terminal, and a log file when one is named (window_output.hpp).
//
// ge:summary A game window's output sink: the terminal, and a log file appended and flushed
// ge:keywords log-file logfile window output terminal sink flush
// ge:skill agent-interface debug
// ge:test tests/play_window_output_test.cpp
#include "play/window_output.hpp"

#include <cstdio>
#include <filesystem>

#include "core/output.hpp"

namespace ge::play {

namespace {

namespace out = ge::core::output;

std::FILE* g_file = nullptr;  // the log file, or null
std::string g_path;

void window_line(const out::LogLine& line, void*) {
  std::FILE* to = line.level == out::Level::error ? stderr : stdout;
  std::fprintf(to, "%.*s\n", static_cast<int>(line.message.size()), line.message.data());
  if (g_file != nullptr) {
    const std::string text = out::format_line(line);
    std::fprintf(g_file, "%s\n", text.c_str());
    std::fflush(g_file);
  }
}

}  // namespace

bool attach_window_output(const std::string& file, std::string& error) {
  if (g_file != nullptr) {
    std::fclose(g_file);
    g_file = nullptr;
    g_path.clear();
  }
  bool ok = true;
  if (!file.empty()) {
    const std::filesystem::path p(file);
    std::error_code ec;
    if (p.has_parent_path()) std::filesystem::create_directories(p.parent_path(), ec);
    g_file = std::fopen(file.c_str(), "a");
    if (g_file == nullptr) {
      error = "the log file " + file +
              " cannot be opened for appending; next: name a file in a folder you can write";
      ok = false;
    } else {
      g_path = file;
    }
  }
  out::set_output_sink(window_line);  // the lines before this one are replayed into it
  return ok;
}

const std::string& window_log_file() { return g_path; }

}  // namespace ge::play
